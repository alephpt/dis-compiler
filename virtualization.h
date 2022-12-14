#ifndef dis_virt_h
#define dis_virt_h
#define FRAME_MAX 128
#define STACK_MAX (FRAME_MAX * UINT8_COUNT)

#include "sequence.h"
#include "table.h"
#include "object.h"
#include "value.h"

typedef struct {
    OOperation* operation;
    uint8_t* instruction;
    Value* slot;
} CallFrame;

typedef struct {
    Sequence* sequence;
    uint8_t* instruction;
    Value stack[STACK_MAX];
    Value* stackHead;
    CallFrame frames[FRAME_MAX];
    int frameCount;
    Obj* objectHead;
    Table globals;
    Table strings;
} Virtualizer;

typedef enum {
    INTERPRETED,
    COMPILE_ERROR,
    RUNTIME_ERROR
} Interpretation;

extern Virtualizer vm;

void initVM();
void freeVM();

Interpretation interpret(const char* source);
void push(Value value);
Value pop();

#endif