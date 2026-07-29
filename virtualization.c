#include <errno.h>
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

Virtualizer vm;

static void resetStack () { vm.stackHead = vm.stack; vm.frameCount = 0; }
void push (Value value) { *vm.stackHead = value; vm.stackHead++; }
Value pop () { vm.stackHead--; return *vm.stackHead; }
static Value peek (int dist) { return vm.stackHead[-1 - dist]; }
static bool isFalse (Value value) { return IS_NONE(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)); }

static void runtimeErr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        OOperation* op = frame->operation;
        size_t instructor = frame->instruction - op->sequence.code - 1;

        fprintf(stderr, "[line %d] in ", op->sequence.line[instructor]);

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

static bool callValue (Value called, int args) {
    if (IS_OBJECT(called)) {
        switch (OBJECT_TYPE(called)) {
            case O_OPERATION:
                return call(AS_OPERATION(called), args);
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

void initVM () {
    resetStack();
    vm.objectHead = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    return;
}

void freeVM () {
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
                Value val = READ_VALUE();
                push(val);
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
                OString* name = READ_STRING();
                Value val;

                if (!getItem(&vm.globals, name, &val)) {
                    runtimeErr("Global Return Failed: Undefined variable '%s'.", name->chars);
                    return RUNTIME_ERROR;
                }

                push(val);
                break;
            }
            case SIG_GLOBAL_ASSIGN: {
                OString* name = READ_STRING();

                if (setTable(&vm.globals, name, peek(0))) {
                    delItem(&vm.globals, name);
                    runtimeErr("Global Assignment Failed: Undefined variable %s", name->chars);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_GLOBAL: {
                OString* name = READ_STRING();
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
                OString* name = READ_STRING();
                uint8_t* slot;
                WidthT width;

                if (!IS_NUMERAL(peek(0))) {
                    runtimeErr("Form members hold numerals only.");
                    return RUNTIME_ERROR;
                }

                Value value = pop();

                if (!memberSlot(name, &slot, &width)) { return RUNTIME_ERROR; }

                writeWidth(slot, width, AS_NUMERAL(value));
                push(value);
                break;
            }
            case SIG_MEMBER_RETURN: {
                OString* name = READ_STRING();
                uint8_t* slot;
                WidthT width;

                if (!memberSlot(name, &slot, &width)) { return RUNTIME_ERROR; }

                push(NUMERAL_VALUE(readWidth(slot, width)));
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
    #undef READ_SHORT
    #undef BINARY_OP
}

Interpretation interpret (const char* source) {
    OOperation* op = compile(source);

    if (op == NULL) { return COMPILE_ERROR; }

    push(OBJECT_VALUE(op));
    if (!call(op, 0)) { return RUNTIME_ERROR; }

    return elucidate();
}
