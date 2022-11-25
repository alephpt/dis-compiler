#ifndef dis_sequence_h
#define dis_sequence_h

#include "header.h"
#include "value.h"

// instructions
typedef enum {
    OP_VALUE,
    SIG_ADD,
    SIG_SUB,
    SIG_MULT,
    SIG_DIV,
    SIG_RETURN,
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
