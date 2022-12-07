#ifndef dis_object_h
#define dis_object_h

#include "header.h"
#include "value.h"


#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)
#define IS_STRING(value)    isObjType(value, O_STRING)
#define AS_STRING(value)    ((OString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((OString*)AS_OBJECT(value))->chars)   

typedef enum {
    O_PILOT,
    O_ENUM,
    O_STRING,
    O_OBJ,
} ObjectT;

struct Obj{
    ObjectT type;
    struct Obj* next;
};

struct OString{
    Obj object;
    int length;
    char* chars;
};

OString* genString (char* chars, int len);
OString* copyString (const char* chars, int len);
void printObject (Value value);

static inline bool isObjType(Value value, ObjectT type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif