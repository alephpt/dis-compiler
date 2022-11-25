#include <stdio.h>
#include "debug.h"
#include "value.h"

void stripSequence (Sequence* sequence, const char* name) {
    printf(" ./~ %s ~\\. \n", name);

    for (int offset = 0; offset < sequence->inventory;) {
        offset = stripCommand(sequence, offset);
    }

    return;
}

static int instruct (const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int instructValue (const char* name, Sequence* sequence, int offset) {
    uint8_t value = sequence->code[offset + 1];
    printf("%-16s %4d '", name, value);
    printValue(sequence->constants.values[value]);
    printf("'\n");

    return offset + 2;
}

int stripCommand (Sequence* sequence, int offset) {
    printf("%04d ", offset);

    uint8_t instruction = sequence->code[offset];

    switch (instruction) {
        case OP_VALUE:
            return instructValue("OP_VALUE", sequence, offset);
        case SIG_RETURN:
            return instruct("SIG_RETURN", offset);
        default:
            printf("Unknown Instruction %d\n", instruction);
            return offset + 1;
    }

    return -1;
}