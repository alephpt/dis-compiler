#include <float.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "virtualization.h"


#define ALLOCATE_OBJECT(type, oType) \
    (type*)allocateObject(sizeof(type), oType)

static Obj* allocateObject (size_t size, ObjectT type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objectHead;
    vm.objectHead = object;
    return object;
}


// STRING STUFF //

static OString* allocateString (char* chars, int len, uint32_t hash) {
    OString* string = ALLOCATE_OBJECT(OString, O_STRING);
    string->length = len;
    string->chars = chars;
    string->hash = hash;
    setTable(&vm.strings, string, NONE_VALUE);
    return string;
}

uint32_t hashString (const char* chars, int len) {
    uint32_t hash = 2166136261u;
    for (int key = 0; key < len; key++) {
        hash ^= (uint8_t)chars[key];
        hash *= 16777619;
    }
    return hash;
}

OString* genString (char* chars, int len) {
    uint32_t hash = hashString(chars, len);

    OString* intern = findString(&vm.strings, chars, len, hash);

    if (intern != NULL) {
        FREE_ARRAY(char, chars, len + 1);
        return intern;
    }

    return allocateString(chars, len, hash);
}

OString* copyString (const char* chars, int len) {
    uint32_t hash = hashString(chars, len);

    OString* intern = findString(&vm.strings, chars, len, hash);
    
    if (intern != NULL) { return intern; }

    char* heapChars = ALLOCATE(char, len + 1);

    memcpy(heapChars, chars, len);
    heapChars[len] = '\0';

    return allocateString(heapChars, len, hash);
}


 // FUNCTION STUFF //

 OOperation* newOperation () {
    OOperation* op = ALLOCATE_OBJECT (OOperation, O_OPERATION);
    op->arity = 0;
    op->name = NULL;
    op->file = "?";
    initSequence(&op->sequence);
    return op;
 }

ONative* newNative (NativeOp op, int arity, const char* name) {
    ONative* native = ALLOCATE_OBJECT(ONative, O_NATIVE);
    native->op = op;
    native->arity = arity;
    native->name = name;
    return native;
}

static void printOperation (OOperation* op) {
    if (op->name == NULL) {
        printf("<script>");
        return;
    }

    printf("<op %s>", op->name->chars);
    return;
}


 // FORM STUFF //

static const char* widthNames[] = {
    "u8", "u16", "u32", "u64",
    "i8", "i16", "i32", "i64",
    "f32", "f64"
};

static const int widthSizes[] = { 1, 2, 4, 8, 1, 2, 4, 8, 4, 8 };

int widthSize (WidthT width) { return widthSizes[width]; }

bool findWidth (const char* chars, int len, WidthT* width) {
    for (int i = 0; i < (int)(sizeof(widthSizes) / sizeof(int)); i++) {
        const char* name = widthNames[i];

        if ((int)strlen(name) != len || memcmp(name, chars, len) != 0) { continue; }

        *width = (WidthT)i;
        return true;
    }

    return false;
}

// fields arrive named and typed - the offsets and stride are derived here so
// that packing stays a single authority
OForm* newForm (OString* name, const FormField* fields, int fieldCount) {
    OForm* form = ALLOCATE_OBJECT(OForm, O_FORM);
    form->name = name;
    form->fieldCount = fieldCount;
    form->fields = ALLOCATE(FormField, fieldCount);
    form->stride = 0;

    for (int i = 0; i < fieldCount; i++) {
        form->fields[i] = fields[i];
        form->fields[i].offset = form->stride;
        form->stride += widthSize(fields[i].width);
    }

    return form;
}

OBuffer* newBuffer (OForm* form, int count) {
    OBuffer* buffer = ALLOCATE_OBJECT(OBuffer, O_BUFFER);
    size_t bytes = (size_t)count * (size_t)form->stride;

    buffer->form = form;
    buffer->count = count;
    buffer->capacity = count;
    buffer->bytes = (bytes == 0) ? NULL : ALLOCATE(uint8_t, bytes);

    if (buffer->bytes != NULL) { memset(buffer->bytes, 0, bytes); }

    return buffer;
}

// field names are interned, so identity is a pointer comparison
FormField* findField (OForm* form, OString* name) {
    for (int i = 0; i < form->fieldCount; i++) {
        if (form->fields[i].name == name) { return &form->fields[i]; }
    }

    return NULL;
}


 // WIDTH CONVERSION //

// doubles outside the range of the target integer are undefined in C, so every
// conversion is clamped before it is narrowed
static int64_t clampSigned (double value) {
    if (value != value) { return 0; }
    if (!(value > -9223372036854775808.0)) { return INT64_MIN; }
    if (!(value < 9223372036854775808.0)) { return INT64_MAX; }
    return (int64_t)value;
}

static uint64_t clampUnsigned (double value) {
    if (value != value) { return 0; }
    if (value < 0) { return (uint64_t)clampSigned(value); }
    if (!(value < 18446744073709551616.0)) { return UINT64_MAX; }
    return (uint64_t)value;
}

// only a finite double beyond the range of a float needs clamping - f32 holds an
// infinity exactly, so an infinity is carried across untouched
static float clampFloat (double value) {
    if (value > FLT_MAX && value <= DBL_MAX) { return FLT_MAX; }
    if (value < -FLT_MAX && value >= -DBL_MAX) { return -FLT_MAX; }
    return (float)value;
}

// slots are unaligned by design - packed layouts put f32 on odd offsets
#define READ_SLOT(type) { type raw; memcpy(&raw, slot, sizeof(type)); return (double)raw; }
#define WRITE_SLOT(type, converted) { type raw = (type)(converted); memcpy(slot, &raw, sizeof(type)); return; }

double readWidth (const uint8_t* slot, WidthT width) {
    switch (width) {
        case W_U8:  READ_SLOT(uint8_t);
        case W_U16: READ_SLOT(uint16_t);
        case W_U32: READ_SLOT(uint32_t);
        case W_U64: READ_SLOT(uint64_t);
        case W_I8:  READ_SLOT(int8_t);
        case W_I16: READ_SLOT(int16_t);
        case W_I32: READ_SLOT(int32_t);
        case W_I64: READ_SLOT(int64_t);
        case W_F32: READ_SLOT(float);
        case W_F64: READ_SLOT(double);
    }
    return 0;
}

void writeWidth (uint8_t* slot, WidthT width, double value) {
    switch (width) {
        case W_U8:  WRITE_SLOT(uint8_t, clampUnsigned(value));
        case W_U16: WRITE_SLOT(uint16_t, clampUnsigned(value));
        case W_U32: WRITE_SLOT(uint32_t, clampUnsigned(value));
        case W_U64: WRITE_SLOT(uint64_t, clampUnsigned(value));
        case W_I8:  WRITE_SLOT(int8_t, clampSigned(value));
        case W_I16: WRITE_SLOT(int16_t, clampSigned(value));
        case W_I32: WRITE_SLOT(int32_t, clampSigned(value));
        case W_I64: WRITE_SLOT(int64_t, clampSigned(value));
        case W_F32: WRITE_SLOT(float, clampFloat(value));
        case W_F64: WRITE_SLOT(double, value);
    }
    return;
}

#undef READ_SLOT
#undef WRITE_SLOT

void printObject (Value value) {
    switch (OBJECT_TYPE(value)) {
        case O_OPERATION: {
            printOperation(AS_OPERATION(value));
            break;
        }
        case O_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case O_FORM:
            printf("<form %s>", AS_FORM(value)->name->chars);
            break;
        case O_NATIVE:
            printf("<native %s>", AS_NATIVE(value)->name);
            break;
        case O_BUFFER: {
            OBuffer* buffer = AS_BUFFER(value);
            printf("<%s[%d]>", buffer->form->name->chars, buffer->count);
            break;
        }
        default:
            printf("<object>");
            break;
    }
    return;
}
