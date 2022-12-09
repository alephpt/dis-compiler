#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "value.h"
#include "object.h"

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

// Todo, Compare 3 Values
bool equalValues (Value a, Value b) {
    if (a.type != b.type) { return false; }
    switch (a.type) {
        case V_BOOLEAN:     return AS_BOOLEAN(a) == AS_BOOLEAN(b);
        case V_NONE:        return true;
        case V_NUMERAL:     return AS_NUMERAL(a) == AS_NUMERAL(b);
        case V_OBJECT:      return AS_OBJECT(a) == AS_OBJECT(b);
        default:            
            return false;
    }
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
    switch(val.type) {
        case V_BOOLEAN:
            printf(AS_BOOLEAN(val) ? "true" : "false");
            break;
        case V_NONE:
            printf("none");
            break;
        case V_NUMERAL:
            printf("%g", AS_NUMERAL(val));
            break;
        case V_OBJECT:
            printObject(val);
            break;
    }
}