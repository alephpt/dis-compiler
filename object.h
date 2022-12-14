#ifndef dis_object_h
#define dis_object_h

#include "header.h"
#include "sequence.h"
#include "value.h"

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)
#define IS_STRING(value)    isObjType(value, O_STRING)
#define IS_OPERATION(value) isObjType(value, O_OPERATION)
#define AS_STRING(value)    ((OString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((OString*)AS_OBJECT(value))->chars)
#define AS_OPERATION(value) ((OOperation*)AS_OBJECT(value))

typedef enum {
    O_PILOT,
    O_ENUM,
    O_STRING,
    O_OPERATION,
    O_OBJ,
} ObjectT;

struct Obj{
    ObjectT type;
    struct Obj* next;
};

typedef struct {
    Obj object;
    int arity;
    Sequence sequence;
    OString* name;
} OOperation;

struct OString{
    Obj object;
    int length;
    char* chars;
    uint32_t hash;
};

OString* genString (char* chars, int len);
OString* copyString (const char* chars, int len);
OOperation* newOperation ();
void printObject (Value value);

static inline bool isObjType(Value value, ObjectT type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif