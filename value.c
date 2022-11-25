#include <stdio.h>
#include "memory.h"
#include "value.h"

void initValues (Values* arr) {
    arr->values = NULL;
    arr->allocated = 0;
    arr->inventory = 0;

    return;
}

void freeValues (Values* arr) {
    FREE_ARRAY(Value, arr->values, arr->allocated);
    initValues(arr);

    return;
}

void writeValues (Values* arr, Value val) {
    if (arr->allocated < arr->inventory + 1) {
        int current_limit = arr->allocated;
        arr->allocated = EXPAND_LIMITS(current_limit);
        arr->values = EXPAND_ARRAY(Value, arr->values, current_limit, arr->allocated);
    }

    arr->values[arr->inventory] = val;
    arr->inventory++;

    return;
}

void printValue (Value val) {
    printf("%g", val);
}