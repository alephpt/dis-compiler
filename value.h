#ifndef dis_value_h
#define dis_value_h

#include "header.h"

typedef enum {
    V_BOOLEAN,
    V_NUMERAL,
    V_NONE,
} ValueT;

typedef struct {
    ValueT type;
    union {
        bool boolean;
        double numeral;
    } as;
} Value;

#define BOOLEAN_VALUE(value)    ((Value){V_BOOLEAN, {.boolean = value}})
#define NONE_VALUE               ((Value){V_NONE, {.numeral = 0}})
// ADD HEXIDECIMAL, BINARY, OCTAL
#define NUMERAL_VALUE(value)    ((Value){V_NUMERAL, {.numeral = value}})
#define AS_BOOLEAN(value)       ((value).as.boolean)
#define AS_NUMERAL(value)       ((value).as.numeral)
#define IS_BOOLEAN(value)       ((value).type == V_BOOLEAN)
#define IS_NUMERAL(value)       ((value).type == V_NUMERAL)
#define IS_NONE(value)          ((value).type == V_NONE)

typedef struct {
    int allocated;
    int inventory;
    Value* values;
}  Values;

void initValues (Values* arr);
void writeValues (Values* arr, Value val);
bool equalValues (Value a, Value b);
void freeValues (Values* arr);
void printValue (Value value);

#endif