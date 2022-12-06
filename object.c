#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"
#include "virtualization.h"

#define ALLOCATE_OBJECT (type, oType) \
    (type*)allocateObject(sizeof(type), oType)

static Obj* allocateObject (size_t size, ObjectT type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

OString* copyString (const char* chars, int len) {
    char* heapChars = ALLOCATE(char, length + 1);

    memcpy(heapChars, chars, len);
    heapChars[len] = '\0';

    return allocateString(heapChars, len);
}

static OString* allocateString (char* chars, int len) {
    OString* string = ALLOCATE_OBJECT(OString, O_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}