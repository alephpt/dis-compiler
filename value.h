#ifndef dis_value_h
#define dis_value_h

#include "header.h"

typedef double Value;

typedef struct {
    int allocated;
    int inventory;
    Value* values;
}  Values;

void initValues (Values* arr);
void writeValues (Values* arr, Value val);
void freeValues (Values* arr);
void printValue (Value value);

#endif