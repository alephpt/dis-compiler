
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
        case O_OPERATION: {
            OOperation* op = (OOperation*)o;
            freeSequence(&op->sequence);
            FREE(OOperation, o);
            break;
        }
        case O_FORM: {
            OForm* form = (OForm*)o;
            FREE_ARRAY(FormField, form->fields, form->fieldCount);
            FREE(OForm, o);
            break;
        }
        case O_BUFFER: {
            // what was allocated is the capacity, which growth may have pushed
            // out past the live count
            OBuffer* buffer = (OBuffer*)o;
            FREE_ARRAY(uint8_t, buffer->bytes, (size_t)buffer->capacity * (size_t)buffer->form->stride);
            FREE(OBuffer, o);
            break;
        }
        case O_NATIVE: {
            // the name is static storage from the natives table - nothing to free
            FREE(ONative, o);
            break;
        }
        default: {
            FREE(Obj, o);
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