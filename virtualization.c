#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "header.h"
#include "virtualization.h"
#include "compiler.h"
#include "sequence.h"
#include "memory.h"
#include "object.h"
#include "debug.h"
#include "display.h"

Virtualizer vm;

static void resetStack () { vm.stackHead = vm.stack; vm.frameCount = 0; }
void push (Value value) { *vm.stackHead = value; vm.stackHead++; }
Value pop () { vm.stackHead--; return *vm.stackHead; }
static Value peek (int dist) { return vm.stackHead[-1 - dist]; }
static bool isFalse (Value value) { return IS_NONE(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)); }

void runtimeErr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        OOperation* op = frame->operation;
        size_t instructor = frame->instruction - op->sequence.code - 1;

        fprintf(stderr, "[%s: line %d] in ", op->file, op->sequence.line[instructor]);

        if (op->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", op->name->chars);
        }
    }

    resetStack();
}

static bool call (OOperation* op, int args) {
    if (args != op->arity) {
        runtimeErr("Expected %d arguements, but only received %d.", op->arity, args);
        return false;
    }

    if (vm.frameCount >= FRAME_MAX) {
        runtimeErr("Stack Overflow Suka.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->operation = op;
    frame->instruction = op->sequence.code;
    frame->slot = vm.stackHead - args - 1;
    return true;
}

// a native runs to completion inside the caller's frame - no CallFrame is pushed
static bool callNative (ONative* native, int args) {
    Value result;

    if (args != native->arity) {
        runtimeErr("Expected %d arguements, but only received %d.", native->arity, args);
        return false;
    }

    if (!native->op(args, vm.stackHead - args, &result)) { return false; }

    // the arguments and the native itself leave together
    vm.stackHead -= args + 1;
    push(result);
    return true;
}

static bool callValue (Value called, int args) {
    if (IS_OBJECT(called)) {
        switch (OBJECT_TYPE(called)) {
            case O_OPERATION:
                return call(AS_OPERATION(called), args);
            case O_NATIVE:
                return callNative(AS_NATIVE(called), args);
            default:
                break;
        }
    }
    runtimeErr("Can Only Call Operations and Objects.");
    return false;
}

// a linear region is capped so that the byte count never overflows an int
#define BUFFER_BYTE_MAX 2147483647.0

// consumes the staged form and size, leaving a zeroed linear region behind
static bool allocateBuffer () {
    if (!IS_NUMERAL(peek(0)) || !IS_FORM(peek(1))) {
        runtimeErr("Buffer allocation requires a form and a numeral size.");
        return false;
    }

    double size = AS_NUMERAL(pop());
    OForm* form = AS_FORM(pop());

    if (!(size >= 0) || size > BUFFER_BYTE_MAX || size != (double)(int64_t)size) {
        runtimeErr("Buffer size must be a whole numeral of zero or greater.");
        return false;
    }

    if (size * (double)form->stride > BUFFER_BYTE_MAX) {
        runtimeErr("Buffer of '%s' is too large to allocate.", form->name->chars);
        return false;
    }

    push(OBJECT_VALUE(newBuffer(form, (int)size)));
    return true;
}

// consumes the staged buffer and index, resolving them to one raw field slot
static bool memberSlot (OString* name, uint8_t** slot, WidthT* width) {
    if (!IS_NUMERAL(peek(0)) || !IS_BUFFER(peek(1))) {
        runtimeErr("Only form buffers carry members.");
        return false;
    }

    double index = AS_NUMERAL(pop());
    OBuffer* buffer = AS_BUFFER(pop());
    FormField* field = findField(buffer->form, name);

    if (field == NULL) {
        runtimeErr("Undefined member '%s' of form '%s'.", name->chars, buffer->form->name->chars);
        return false;
    }

    if (!(index >= 0) || !(index < (double)buffer->count) || index != (double)(int64_t)index) {
        runtimeErr("Index out of bounds - '%s[%d]' has no element %g.",
                   buffer->form->name->chars, buffer->count, index);
        return false;
    }

    *slot = buffer->bytes + ((size_t)index * (size_t)buffer->form->stride) + field->offset;
    *width = field->width;
    return true;
}

// a string contributes its own bytes, a buffer its whole packed region
static const void* streamBytes (Value value, size_t* length) {
    if (IS_STRING(value)) {
        OString* string = AS_STRING(value);
        *length = (size_t)string->length;
        return string->chars;
    }

    OBuffer* buffer = AS_BUFFER(value);
    *length = (size_t)buffer->count * (size_t)buffer->form->stride;
    return buffer->bytes;
}

// 'write -> path, value, ... .' - the path sits under every value on the stack.
// one open, one pass, one close, truncating whatever was at the path before
static bool writeStream (int values) {
    Value path = peek(values);

    if (!IS_STRING(path)) {
        runtimeErr("Write requires a string path.");
        return false;
    }

    // every value is checked before the file is touched, so a bad argument can
    // never leave a truncated or half written file behind
    for (int i = values - 1; i >= 0; i--) {
        if (!IS_STRING(peek(i)) && !IS_BUFFER(peek(i))) {
            runtimeErr("Write requires strings or buffers.");
            return false;
        }
    }

    const char* target = AS_CSTRING(path);
    FILE* file = fopen(target, "wb");

    if (file == NULL) {
        runtimeErr("Write could not open '%s' - %s.", target, strerror(errno));
        return false;
    }

    for (int i = values - 1; i >= 0; i--) {
        size_t length;
        const void* bytes = streamBytes(peek(i), &length);

        if (length == 0) { continue; }

        if (fwrite(bytes, 1, length, file) != length) {
            runtimeErr("Write failed on '%s' - %s.", target, strerror(errno));
            fclose(file);
            return false;
        }
    }

    if (fclose(file) != 0) {
        runtimeErr("Write could not close '%s' - %s.", target, strerror(errno));
        return false;
    }

    // the path and every value leave together - a write yields nothing
    vm.stackHead -= values + 1;
    return true;
}

// the narrow and wide forms of an instruction differ only in how the constant
// index is read, so the work itself lives here and both cases share it
static bool globalReturn (OString* name) {
    Value val;

    if (!getItem(&vm.globals, name, &val)) {
        runtimeErr("Global Return Failed: Undefined variable '%s'.", name->chars);
        return false;
    }

    push(val);
    return true;
}

static bool globalAssign (OString* name) {
    if (setTable(&vm.globals, name, peek(0))) {
        delItem(&vm.globals, name);
        runtimeErr("Global Assignment Failed: Undefined variable %s", name->chars);
        return false;
    }

    return true;
}

static bool memberAssign (OString* name) {
    uint8_t* slot;
    WidthT width;

    if (!IS_NUMERAL(peek(0))) {
        runtimeErr("Form members hold numerals only.");
        return false;
    }

    Value value = pop();

    if (!memberSlot(name, &slot, &width)) { return false; }

    writeWidth(slot, width, AS_NUMERAL(value));
    push(value);
    return true;
}

static bool memberReturn (OString* name) {
    uint8_t* slot;
    WidthT width;

    if (!memberSlot(name, &slot, &width)) { return false; }

    push(NUMERAL_VALUE(readWidth(slot, width)));
    return true;
}

static void concatenation () {
    OString* latter = AS_STRING(pop());
    OString* prior = AS_STRING(pop());

    int len = prior->length + latter->length;
    
    char* chars = ALLOCATE(char, len + 1);

    memcpy(chars, prior->chars, prior->length);
    memcpy(chars + prior->length, latter->chars, latter->length);
    chars[len] = '\0';

    OString* newString = genString(chars, len);
    push(OBJECT_VALUE(newString));
}

 // NATIVE OPERATIONS //

// every arithmetic native has the same shape - check the arguments, hand them to
// the C library, wrap the answer back up as a numeral
#define NATIVE_UNARY(id, label, call) \
    static bool native##id (int args, Value* argv, Value* result) { \
        (void)args; \
        if (!IS_NUMERAL(argv[0])) { \
            runtimeErr("%s requires a numeral.", label); \
            return false; \
        } \
        *result = NUMERAL_VALUE(call(AS_NUMERAL(argv[0]))); \
        return true; \
    }

#define NATIVE_BINARY(id, label, call) \
    static bool native##id (int args, Value* argv, Value* result) { \
        (void)args; \
        if (!IS_NUMERAL(argv[0]) || !IS_NUMERAL(argv[1])) { \
            runtimeErr("%s requires numerals.", label); \
            return false; \
        } \
        *result = NUMERAL_VALUE(call(AS_NUMERAL(argv[0]), AS_NUMERAL(argv[1]))); \
        return true; \
    }

NATIVE_UNARY(Sqrt, "sqrt", sqrt)
NATIVE_UNARY(Floor, "floor", floor)
NATIVE_UNARY(Sin, "sin", sin)
NATIVE_UNARY(Cos, "cos", cos)
NATIVE_UNARY(Tan, "tan", tan)
NATIVE_UNARY(Abs, "abs", fabs)

NATIVE_BINARY(Atan2, "atan2", atan2)
NATIVE_BINARY(Pow, "pow", pow)
NATIVE_BINARY(Min, "min", fmin)
NATIVE_BINARY(Max, "max", fmax)

// 'grow -> buf, n.' extends a buffer by n elements and answers the new count.
// the allocation doubles so that appending one at a time stays amortised flat
static bool nativeGrow (int args, Value* argv, Value* result) {
    (void)args;

    if (!IS_BUFFER(argv[0])) {
        runtimeErr("grow requires a buffer.");
        return false;
    }

    if (!IS_NUMERAL(argv[1])) {
        runtimeErr("grow requires a numeral count.");
        return false;
    }

    OBuffer* buffer = AS_BUFFER(argv[0]);
    double by = AS_NUMERAL(argv[1]);

    // the magnitude is settled before anything is narrowed - converting an out
    // of range double to an integer is undefined, and the result of one would
    // sail straight past every check below it
    if (!(by >= 0)) {
        runtimeErr("grow requires a whole count of zero or greater.");
        return false;
    }

    if (by > BUFFER_BYTE_MAX) {
        runtimeErr("grow count is larger than any buffer can be.");
        return false;
    }

    if (by != (double)(int64_t)by) {
        runtimeErr("grow requires a whole count of zero or greater.");
        return false;
    }

    int stride = buffer->form->stride;
    double target = (double)buffer->count + by;

    if (target * (double)stride > BUFFER_BYTE_MAX) {
        runtimeErr("Buffer of '%s' is too large to grow.", buffer->form->name->chars);
        return false;
    }

    int wanted = (int)target;

    if (wanted > buffer->capacity) {
        // the doubling is chosen and capped entirely in doubles, so the one
        // narrowing at the end is always of a value known to fit
        double doubled = (double)buffer->capacity * 2;
        double roomy = (doubled > target) ? doubled : target;

        if (roomy * (double)stride > BUFFER_BYTE_MAX) { roomy = target; }

        int capacity = (int)roomy;

        buffer->bytes = (uint8_t*)reallocate(buffer->bytes,
                                             (size_t)buffer->capacity * (size_t)stride,
                                             (size_t)capacity * (size_t)stride);
        buffer->capacity = capacity;
    }

    // whatever the growth just exposed starts zeroed, like a fresh allocation
    if (wanted > buffer->count) {
        memset(buffer->bytes + ((size_t)buffer->count * (size_t)stride),
               0, (size_t)(wanted - buffer->count) * (size_t)stride);
    }

    buffer->count = wanted;
    *result = NUMERAL_VALUE(buffer->count);
    return true;
}

static bool nativeCount (int args, Value* argv, Value* result) {
    (void)args;

    if (!IS_BUFFER(argv[0])) {
        runtimeErr("count requires a buffer.");
        return false;
    }

    *result = NUMERAL_VALUE(AS_BUFFER(argv[0])->count);
    return true;
}

 // RASTER CORE //

// the packed pixel value, and the whole numbers that index into a surface. both
// settle their range in doubles before narrowing anything
static bool wholePixel (Value value, const char* what, uint32_t* out) {
    if (!IS_NUMERAL(value)) {
        runtimeErr("%s must be a numeral.", what);
        return false;
    }

    double v = AS_NUMERAL(value);

    if (!(v >= 0) || v > 4294967295.0 || v != (double)(int64_t)v) {
        runtimeErr("%s must be a whole number from 0 to 4294967295.", what);
        return false;
    }

    *out = (uint32_t)v;
    return true;
}

static bool wholeCoord (Value value, const char* what, int* out) {
    if (!IS_NUMERAL(value)) {
        runtimeErr("%s must be a numeral.", what);
        return false;
    }

    double v = AS_NUMERAL(value);

    if (!(v >= -2147483648.0) || !(v <= 2147483647.0) || v != (double)(int64_t)v) {
        runtimeErr("%s must be a whole number.", what);
        return false;
    }

    *out = (int)v;
    return true;
}

static bool pixelSurface (Value value, const char* what, OBuffer** out) {
    if (!IS_BUFFER(value)) {
        runtimeErr("%s requires a buffer.", what);
        return false;
    }

    OBuffer* buffer = AS_BUFFER(value);

    if (buffer->form->stride != 4) {
        runtimeErr("%s requires a buffer of four byte elements, but '%s' is %d.",
                   what, buffer->form->name->chars, buffer->form->stride);
        return false;
    }

    *out = buffer;
    return true;
}

// 'clear -> fb, rgba.' paints every element and answers how many it touched
static bool nativeClear (int args, Value* argv, Value* result) {
    (void)args;

    OBuffer* fb;
    uint32_t rgba;

    if (!pixelSurface(argv[0], "clear", &fb)) { return false; }
    if (!wholePixel(argv[1], "The clear colour", &rgba)) { return false; }

    uint8_t* bytes = (uint8_t*)&rgba;

    // an empty surface owns no allocation at all, and memset will not take a
    // null pointer even for a length of zero
    if (fb->count == 0) {
        *result = NUMERAL_VALUE(0);
        return true;
    }

    // when all four bytes agree the whole surface is one memset
    if (bytes[0] == bytes[1] && bytes[1] == bytes[2] && bytes[2] == bytes[3]) {
        memset(fb->bytes, bytes[0], (size_t)fb->count * 4);
    } else {
        uint32_t* pixels = (uint32_t*)fb->bytes;

        for (int i = 0; i < fb->count; i++) { pixels[i] = rgba; }
    }

    *result = NUMERAL_VALUE(fb->count);
    return true;
}

// 'hspan -> fb, w, y, x0, x1, rgba.' fills one clamped run of a scanline and
// answers how many pixels it actually wrote
static bool nativeHspan (int args, Value* argv, Value* result) {
    (void)args;

    OBuffer* fb;
    uint32_t rgba;
    int w, y, x0, x1;

    if (!pixelSurface(argv[0], "hspan", &fb)) { return false; }
    if (!wholeCoord(argv[1], "The hspan width", &w)) { return false; }
    if (!wholeCoord(argv[2], "The hspan row", &y)) { return false; }
    if (!wholeCoord(argv[3], "The hspan start", &x0)) { return false; }
    if (!wholeCoord(argv[4], "The hspan end", &x1)) { return false; }
    if (!wholePixel(argv[5], "The hspan colour", &rgba)) { return false; }

    if (w <= 0) {
        runtimeErr("The hspan width must be one or more.");
        return false;
    }

    // a row outside the surface writes nothing rather than reaching past it
    if (y < 0 || y >= fb->count / w) {
        *result = NUMERAL_VALUE(0);
        return true;
    }

    if (x0 < 0) { x0 = 0; }
    if (x1 > w - 1) { x1 = w - 1; }

    if (x0 > x1) {
        *result = NUMERAL_VALUE(0);
        return true;
    }

    uint32_t* pixels = (uint32_t*)fb->bytes + ((size_t)y * (size_t)w);

    for (int x = x0; x <= x1; x++) { pixels[x] = rgba; }

    *result = NUMERAL_VALUE(x1 - x0 + 1);
    return true;
}

// 'read -> path, buf.' is the inverse of write - it fills as much of the buffer
// as the file has bytes for, and answers how many that was. it never resizes,
// so the caller grows first and asks 'count' second
static bool nativeRead (int args, Value* argv, Value* result) {
    (void)args;

    if (!IS_STRING(argv[0])) {
        runtimeErr("Read requires a string path.");
        return false;
    }

    if (!IS_BUFFER(argv[1])) {
        runtimeErr("Read requires a buffer to read into.");
        return false;
    }

    const char* path = AS_CSTRING(argv[0]);
    OBuffer* buffer = AS_BUFFER(argv[1]);
    FILE* file = fopen(path, "rb");

    if (file == NULL) {
        runtimeErr("Read could not open '%s' - %s.", path, strerror(errno));
        return false;
    }

    // a directory opens happily but will not seek, and would otherwise look
    // exactly like an empty file
    if (fseek(file, 0L, SEEK_END) != 0) {
        char probe;

        errno = 0;
        fread(&probe, 1, 1, file);

        int reason = errno;

        fclose(file);
        runtimeErr("Read failed on '%s' - %s.", path, strerror(reason == 0 ? EIO : reason));
        return false;
    }

    long size = ftell(file);

    if (size < 0) {
        int reason = errno;

        fclose(file);
        runtimeErr("Read failed on '%s' - %s.", path, strerror(reason));
        return false;
    }

    rewind(file);

    // only what the buffer can actually address is filled
    size_t room = (size_t)buffer->count * (size_t)buffer->form->stride;
    size_t wanted = ((size_t)size < room) ? (size_t)size : room;
    size_t got = (wanted == 0) ? 0 : fread(buffer->bytes, 1, wanted, file);

    if (got < wanted || ferror(file)) {
        int reason = errno;

        fclose(file);
        runtimeErr("Read failed on '%s' - %s.", path, strerror(reason == 0 ? EIO : reason));
        return false;
    }

    fclose(file);
    *result = NUMERAL_VALUE((double)got);
    return true;
}

// one row per native - the whole registration cost of adding another
static const NativeEntry natives[] = {
    { "sqrt",  nativeSqrt,  1 },
    { "floor", nativeFloor, 1 },
    { "sin",   nativeSin,   1 },
    { "cos",   nativeCos,   1 },
    { "tan",   nativeTan,   1 },
    { "abs",   nativeAbs,   1 },
    { "atan2", nativeAtan2, 2 },
    { "pow",   nativePow,   2 },
    { "min",   nativeMin,   2 },
    { "max",   nativeMax,   2 },
    { "grow",  nativeGrow,  2 },
    { "count", nativeCount, 1 },
    { "clear", nativeClear, 2 },
    { "hspan", nativeHspan, 6 },
    { "read",  nativeRead,  2 },
};

static void defineTable (const NativeEntry* table, int count) {
    for (int i = 0; i < count; i++) {
        OString* name = copyString(table[i].name, (int)strlen(table[i].name));
        ONative* native = newNative(table[i].op, table[i].arity, table[i].name);

        setTable(&vm.globals, name, OBJECT_VALUE(native));
    }

    return;
}

static void defineNatives () {
    int count = 0;
    const NativeEntry* fromDisplay = displayNatives(&count);

    defineTable(natives, (int)(sizeof(natives) / sizeof(NativeEntry)));

    // built without sdl this contributes only 'clock', so the window natives
    // are simply absent and naming one is an ordinary undefined variable
    defineTable(fromDisplay, count);
    return;
}

void initVM () {
    resetStack();
    vm.objectHead = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    // interning needs both tables standing, so the natives land last
    defineNatives();
    return;
}

void freeVM () {
    displayShutdown();
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
    return;
}

static Interpretation elucidate () {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

    #define READ_INSTRUCTION() (*frame->instruction++)
    #define READ_VALUE() (frame->operation->sequence.constants.values[READ_INSTRUCTION()])
    #define READ_STRING() AS_STRING(READ_VALUE())
    #define READ_VALUE_16() (frame->operation->sequence.constants.values[READ_SHORT()])
    #define READ_STRING_16() AS_STRING(READ_VALUE_16())
    #define READ_SHORT() \
        (frame->instruction += 2, \
        (uint16_t)((frame->instruction[-2] << 8) | frame->instruction[-1]))
    #define BINARY_OP(valueType, operand) { \
        do { \
            if(!IS_NUMERAL(peek(0)) || !IS_NUMERAL(peek(1))) { \
                runtimeErr("Operands must be numeral types."); \
                return RUNTIME_ERROR; \
            } \
            double b = AS_NUMERAL(pop()); \
            double a = AS_NUMERAL(pop()); \
            push(valueType(a operand b)); \
            } while (false); \
        }
    
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        Sequence* traced = &frame->operation->sequence;
        int offset = (int)(frame->instruction - traced->code);

        for (Value* slot = vm.stack; slot < vm.stackHead; slot++) {
            if (slot == vm.stack) {
                if (offset > 0 && traced->line[offset] != traced->line[offset - 1]) {
                    printf("\033[90m");
                    printf(" stack   ╚══╬┤ [ ");
                } else {
                    printf("\033[90m");
                    printf(" stack   ╠══╬┤ [ ");
                }
            } else {
                printf("─[ ");
            }

            printValue(*slot);
            printf(" ]");
        }

        if (vm.stackHead != vm.stack) {
            printf(" ├╣\n");
            printf("\033[0m");
        }

        stripCommand(traced, offset);
        #endif

        uint8_t instructor;
        switch (instructor = READ_INSTRUCTION()) {
            case OP_VALUE: {
                push(READ_VALUE());
                break;
            }
            case OP_VALUE_16: {
                push(READ_VALUE_16());
                break;
            }
            case OP_NONE: push(NONE_VALUE); break;
            case OP_TRUE: push(BOOLEAN_VALUE(true)); break;
            case OP_FALSE: push(BOOLEAN_VALUE(false)); break;
            case SIG_POP: pop(); break;
            case SIG_LOCAL_RETURN: {
                uint8_t local = READ_INSTRUCTION();
                push(frame->slot[local]);
                break;
            }
            case SIG_LOCAL_ASSIGN: {
                uint8_t local = READ_INSTRUCTION();
                frame->slot[local] = peek(0);
                break;
            }
            case SIG_GLOBAL_RETURN: {
                if (!globalReturn(READ_STRING())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_GLOBAL_RETURN_16: {
                if (!globalReturn(READ_STRING_16())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_GLOBAL_ASSIGN: {
                if (!globalAssign(READ_STRING())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_GLOBAL_ASSIGN_16: {
                if (!globalAssign(READ_STRING_16())) { return RUNTIME_ERROR; }
                break;
            }
            case OP_GLOBAL: {
                OString* name = READ_STRING();
                setTable(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GLOBAL_16: {
                OString* name = READ_STRING_16();
                setTable(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_EQUAL_TO: {
                Value b = pop();
                Value a = pop();
                push(BOOLEAN_VALUE(equalValues(a, b)));
                break;
            }
            case OP_GREATER_THAN: {
                BINARY_OP(BOOLEAN_VALUE, >);
                break;
            }
            case OP_LESS_THAN: {
                BINARY_OP(BOOLEAN_VALUE, <);
                break;
            }
            case SIG_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenation();
                } else if (IS_NUMERAL(peek(0)) && IS_NUMERAL(peek(1))) {
                    double latter = AS_NUMERAL(pop());
                    double prior = AS_NUMERAL(pop());
                    push(NUMERAL_VALUE(prior + latter));
                } else {
                    runtimeErr("Operands must be of the same type.");
                    return RUNTIME_ERROR;
                }
                break;
            }
            case SIG_SUB: {
                BINARY_OP(NUMERAL_VALUE, -);
                break;
            }
            case SIG_MULT: {
                BINARY_OP(NUMERAL_VALUE, *);
                break;
            }
            case SIG_DIV: {
                BINARY_OP(NUMERAL_VALUE, /);
                break;
            }
            case SIG_MOD: {
                // fmod carries the sign of the dividend, and x % 0 gives NaN the
                // same way x / 0 gives inf - neither is an error here
                if (!IS_NUMERAL(peek(0)) || !IS_NUMERAL(peek(1))) {
                    runtimeErr("Operands must be numeral types.");
                    return RUNTIME_ERROR;
                }

                double divisor = AS_NUMERAL(pop());
                double dividend = AS_NUMERAL(pop());
                push(NUMERAL_VALUE(fmod(dividend, divisor)));
                break;
            }
            case SIG_NOT: {
                push(BOOLEAN_VALUE(isFalse(pop())));
                break;
            }
            case SIG_NEG: {
                if(!IS_NUMERAL(peek(0))) {
                    //TODO: Reverse string/Array
                    runtimeErr("Operand must be a number.");
                    return RUNTIME_ERROR;
                    
                }
                push(NUMERAL_VALUE(-AS_NUMERAL(pop())));
                break;
            }
            case SIG_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case SIG_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->instruction += offset;
                break;
            }
            case SIG_EXECUTE: {
                uint16_t offset = READ_SHORT();
                if (isFalse(peek(0))) { frame->instruction += offset; }
                break;
            }
            case SIG_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->instruction -= offset;
                break;
            }
            case SIG_ALLOCATE: {
                if (!allocateBuffer()) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_MEMBER_ASSIGN: {
                if (!memberAssign(READ_STRING())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_MEMBER_ASSIGN_16: {
                if (!memberAssign(READ_STRING_16())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_MEMBER_RETURN: {
                if (!memberReturn(READ_STRING())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_MEMBER_RETURN_16: {
                if (!memberReturn(READ_STRING_16())) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_WRITE: {
                int values = READ_INSTRUCTION();
                if (!writeStream(values)) { return RUNTIME_ERROR; }
                break;
            }
            case SIG_CALL: {
                int args = READ_INSTRUCTION();
                if (!callValue(peek(args), args)) {
                    return RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case SIG_RETURN: {
                #ifdef DEBUG_TRACE_EXECUTION
                printf("\n");
                #endif

                Value result = pop();
                vm.frameCount--;

                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRETED;
                }

                vm.stackHead = frame->slot;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

    #undef READ_INSTRUCTION
    #undef READ_VALUE
    #undef READ_STRING
    #undef READ_VALUE_16
    #undef READ_STRING_16
    #undef READ_SHORT
    #undef BINARY_OP
}

Interpretation interpret (const char* source, const char* path) {
    OOperation* op = compile(source, path);

    if (op == NULL) { return COMPILE_ERROR; }

    push(OBJECT_VALUE(op));
    if (!call(op, 0)) { return RUNTIME_ERROR; }

    return elucidate();
}
