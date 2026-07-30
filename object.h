#ifndef dis_object_h
#define dis_object_h

#include "header.h"
#include "sequence.h"
#include "value.h"

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)
#define IS_STRING(value)    isObjType(value, O_STRING)
#define IS_OPERATION(value) isObjType(value, O_OPERATION)
#define IS_FORM(value)      isObjType(value, O_FORM)
#define IS_BUFFER(value)    isObjType(value, O_BUFFER)
#define IS_NATIVE(value)    isObjType(value, O_NATIVE)
#define AS_STRING(value)    ((OString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((OString*)AS_OBJECT(value))->chars)
#define AS_OPERATION(value) ((OOperation*)AS_OBJECT(value))
#define AS_FORM(value)      ((OForm*)AS_OBJECT(value))
#define AS_BUFFER(value)    ((OBuffer*)AS_OBJECT(value))
#define AS_NATIVE(value)    ((ONative*)AS_OBJECT(value))

typedef enum {
    O_PILOT,
    O_ENUM,
    O_STRING,
    O_OPERATION,
    O_OBJ,
    O_FORM,
    O_BUFFER,
    O_NATIVE,
} ObjectT;

// the explicit width of a single form field - the layout is the type
typedef enum {
    W_U8, W_U16, W_U32, W_U64,
    W_I8, W_I16, W_I32, W_I64,
    W_F32, W_F64
} WidthT;

struct Obj{
    ObjectT type;
    struct Obj* next;
};

typedef struct {
    Obj object;
    int arity;
    Sequence sequence;
    OString* name;
    // where the operation was written, for a runtime traceback
    const char* file;
} OOperation;

struct OString{
    Obj object;
    int length;
    char* chars;
    uint32_t hash;
};

// a member of a form layout - offsets are packed, no padding is ever inserted
typedef struct {
    OString* name;
    WidthT width;
    int offset;
} FormField;

// the layout descriptor - declared once, shared by every buffer of that form
typedef struct {
    Obj object;
    OString* name;
    FormField* fields;
    int fieldCount;
    int stride;
} OForm;

// a flat linear region of raw bytes - 'count' packed elements of 'form'.
// 'capacity' is what was actually allocated, so growth can amortise; everything
// below 'count' is live and initialised, everything above it is not addressable
typedef struct {
    Obj object;
    OForm* form;
    uint8_t* bytes;
    int count;
    int capacity;
} OBuffer;

// a native reports its own failure through runtimeErr and answers false - the
// vm then unwinds exactly as it would for any other runtime error
typedef bool (*NativeOp)(int args, Value* argv, Value* result);

// an operation implemented in C. the name is static storage, never freed
typedef struct {
    Obj object;
    NativeOp op;
    int arity;
    const char* name;
} ONative;

OString* genString (char* chars, int len);
OString* copyString (const char* chars, int len);
OOperation* newOperation ();
ONative* newNative (NativeOp op, int arity, const char* name);
int widthSize (WidthT width);
bool findWidth (const char* chars, int len, WidthT* width);
OForm* newForm (OString* name, const FormField* fields, int fieldCount);
OBuffer* newBuffer (OForm* form, int count);
FormField* findField (OForm* form, OString* name);
double readWidth (const uint8_t* slot, WidthT width);
void writeWidth (uint8_t* slot, WidthT width, double value);
void printObject (Value value);

static inline bool isObjType(Value value, ObjectT type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif