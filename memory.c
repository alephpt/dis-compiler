
#include <stdlib.h>
#include "memory.h"
#include "virtualization.h"

void* reallocate (void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void* res = realloc(pointer, new_size);

    if (res == NULL) {
        exit(1);
    }

    return res;
}

static void freeObj (Obj* o) {
    switch (o->type) {
        case O_STRING: {
            OString* s = (OString*)o;
            FREE_ARRAY(char, s->chars, s->length + 1);
            FREE(OString, o);
            break;
        }
    }
    return;
}

void freeObjects () {
    Obj* o = vm.objectHead;
    while (o != NULL) {
        Obj* n = o->next;
        freeObj(o);
        o = n;
    }
    return;
}