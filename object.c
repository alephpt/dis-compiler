#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "value.h"
#include "virtualization.h"


#define ALLOCATE_OBJECT(type, oType) \
    (type*)allocateObject(sizeof(type), oType)

static Obj* allocateObject (size_t size, ObjectT type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}


static OString* allocateString (char* chars, int len) {
    OString* string = ALLOCATE_OBJECT(OString, O_STRING);
    string->length = len;
    string->chars = chars;
    return string;
}

OString* genString (char* chars, int len) {
    return allocateString(chars, len);
}

OString* copyString (const char* chars, int len) {
    char* heapChars = ALLOCATE(char, len + 1);

    memcpy(heapChars, chars, len);
    heapChars[len] = '\0';

    return allocateString(heapChars, len);
}

void printObject (Value value) {
    switch (OBJECT_TYPE(value)) {
        case O_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
