#ifndef dis_value_h
#define dis_value_h

#include "header.h"

typedef struct Obj Obj;
typedef struct OString OString;
//  TODO: typedef struct Numeral Numeral;

typedef enum {
    V_BOOLEAN,
    V_NUMERAL,
    V_OBJECT,
    V_PAIR,
    V_NONE,
} ValueT;

// TODO: add numeral type conversion to/from binary, hex, octal
typedef struct {
    ValueT type;
    union {
        bool boolean;
        double numeral;
        Obj* object;
    } as;
} Value;

typedef struct {
    int allocated;
    int inventory;
    Value* values;
} Values;

#define NONE_VALUE              ((Value){V_NONE, {.numeral = 0}})
#define BOOLEAN_VALUE(value)    ((Value){V_BOOLEAN, {.boolean = value}})
#define NUMERAL_VALUE(value)    ((Value){V_NUMERAL, {.numeral = value}})
#define OBJECT_VALUE(obj)       ((Value){V_OBJECT, {.object = (Obj*)obj}})
// ADD HEXIDECIMAL, BINARY, OCTAL
#define AS_BOOLEAN(value)       ((value).as.boolean)
#define AS_NUMERAL(value)       ((value).as.numeral)
#define AS_OBJECT(value)        ((value).as.object)
#define IS_NONE(value)          ((value).type == V_NONE)
#define IS_BOOLEAN(value)       ((value).type == V_BOOLEAN)
#define IS_NUMERAL(value)       ((value).type == V_NUMERAL)
#define IS_OBJECT(value)        ((value).type == V_OBJECT)

void initValues (Values* arr);
void writeValues (Values* arr, Value val);
bool equalValues (Value a, Value b);
void freeValues (Values* arr);
void printValue (Value value);

#endif