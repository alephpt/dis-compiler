#ifndef dis_sequence_h
#define dis_sequence_h

#include "header.h"
#include "value.h"

// instructions
typedef enum {
    OP_VALUE,
    OP_NONE,
    OP_TRUE,
    OP_FALSE,
    OP_GLOBAL,
    OP_EQUAL_TO,
    OP_LESS_THAN,
    OP_GREATER_THAN,
    SIG_POP,
    SIG_NOT,
    SIG_NEG,
    SIG_ADD,
    SIG_SUB,
    SIG_MULT,
    SIG_DIV,
    SIG_MOD,
    SIG_LOOP,
    SIG_JUMP,
    SIG_EXECUTE,
    SIG_LOCAL_ASSIGN,
    SIG_LOCAL_RETURN,
    SIG_GLOBAL_ASSIGN,
    SIG_GLOBAL_RETURN,
    SIG_PRINT,
    SIG_CALL,
    SIG_RETURN,
    SIG_ALLOCATE,
    SIG_MEMBER_ASSIGN,
    SIG_MEMBER_RETURN,
    SIG_WRITE,
    // wide forms, carrying a two byte constant index. only the operations that
    // reach into the constant pool need them - local slots and argument counts
    // are still bounded by 255 and stay narrow
    OP_VALUE_16,
    OP_GLOBAL_16,
    SIG_GLOBAL_ASSIGN_16,
    SIG_GLOBAL_RETURN_16,
    SIG_MEMBER_ASSIGN_16,
    SIG_MEMBER_RETURN_16,
} OpCode;

// data
typedef struct {
    int allocated;
    int inventory;
    uint8_t* code;
    int* line;
    Values constants;
} Sequence;

void initSequence (Sequence* sequence);
void freeSequence (Sequence* sequence);
void writeSequence (Sequence* sequence, uint8_t code, int line);
int addValue (Sequence* sequence, Value value);

#endif
