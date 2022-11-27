#ifndef dis_virt_h
#define dis_virt_h
#define STACK_MAX 512

#include "sequence.h"
#include "value.h"

typedef struct {
    Sequence* sequence;
    uint8_t* instruction;
    Value stack[STACK_MAX];
    Value* stackHead;
} Virtualizer;

typedef enum {
    INTERPRETED,
    COMPILE_ERROR,
    RUNTIME_ERROR
} Interpretation;


void initVM();
void freeVM();

Interpretation interpret(Sequence* sequence);
void push(Value value);
Value pop();

#endif