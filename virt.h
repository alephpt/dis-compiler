#ifndef dis_virt_h
#define dis_virt_h

#include "sequence.h"

typedef enum {
    INTERPRETED,
    COMPILE_ERROR,
    RUNTIME_ERROR
} Interpretation;

typedef struct {
    Sequence* sequence;
    uint8_t* instruction;
} Virtualizer;

void initVM();
void freeVM();

Interpretation interpret(Sequence* sequence);

#endif