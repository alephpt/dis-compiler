#include <stdlib.h>
#include "sequence.h"
#include "memory.h"

// initialize blank struct for block of a code
void initSequence (Sequence* sequence) {
    sequence->allocated = 0;
    sequence->inventory = 0;
    sequence->code = NULL;
    initValues(&sequence->constants);

    return;
}

// free struct be removing pointer and reinitializing values
void freeSequence (Sequence* sequence) {
    FREE_ARRAY(uint8_t, sequence->code, sequence->allocated);
    freeValues(&sequence->constants);
    initSequence(sequence);

    return;
}

// append new code to end of block if the allocated sequence size isn't fulfilled
void writeSequence (Sequence* sequence, uint8_t code) {
    if(sequence->allocated < sequence->inventory + 1) {
        int current_limit = sequence->allocated;
        sequence->allocated = EXPAND_LIMITS(current_limit);
        sequence->code = EXPAND_ARRAY(uint8_t, sequence->code, current_limit, sequence->allocated);
    }

    sequence->code[sequence->inventory] = code;
    sequence->inventory++;

    return;
}

int addValue (Sequence* sequence, Value value) {
    writeValues(&sequence->constants, value);
    return sequence->constants.inventory - 1;
}