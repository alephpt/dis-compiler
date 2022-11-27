#include <stdio.h>
#include "debug.h"
#include "value.h"

static int instruct (const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int instructValue (const char* name, Sequence* sequence, int offset) {
    uint8_t value = sequence->code[offset + 1];
    printf("%-16s %4d   '", name, value);
    printValue(sequence->constants.values[value]);
    printf("'\n");

    return offset + 2;
}

int stripCommand (Sequence* seq, int offset) {
    printf("\033[0m");
    printf(" %04d ", offset);

    if (offset > 0) {
        if (seq->line[offset] != seq->line[offset - 1]) {
            printf("%4d    ", seq->line[offset]);
        } else
        #ifdef DEBUG_TRACE_EXECUTION
        if (seq->code[offset] == OP_VALUE ||
            seq->code[offset] == SIG_NEG ||
            seq->code[offset] == SIG_DIV ||
            seq->code[offset] == SIG_ADD ||
            seq->code[offset] == SIG_MULT ||
            seq->code[offset] == SIG_SUB
            ) {
            printf("\033[90m");
            printf("   ╠──  ");
            printf("\033[0m");
        } else
        #endif
        if (seq->line[offset] != seq->line[offset + 2]) {
            printf("\033[90m");
            printf("   ╚──  ");
            printf("\033[0m");
        }
        else {
            printf("\033[90m");
            printf("   ║    ");
            printf("\033[0m");

        }
    } else {
        printf("%4d    ", seq->line[offset]);
    }

    uint8_t instruction = seq->code[offset];

    switch (instruction) {
        case OP_VALUE:
            return instructValue("OP_VALUE", seq, offset);
        case SIG_ADD:
            return instruct("SIG_ADD", offset);
        case SIG_SUB:
            return instruct("SIG_SUB", offset);
        case SIG_MULT:
            return instruct("SIG_MULT", offset);
        case SIG_DIV:
            return instruct("SIG_DIV", offset);
        case SIG_NEG:
            return instruct("SIG_NEG", offset);
        case SIG_RETURN:
            return instruct("SIG_RETURN", offset);
        default:
            printf("Unknown Instruction %d\n", instruction);
            return offset + 1;
    }

    return -1;
}


void stripSequence (Sequence* sequence, const char* name) {
    printf("        ./~     %s    ~\\.\n", name);
    printf(" Byte  Line   Code              Index   Value \n");
    printf("└────┴──────┴─────────────────┴───────┴───────┘\n");

    for (int offset = 0; offset < sequence->inventory;) {
        offset = stripCommand(sequence, offset);
    }

    printf("\n");

    return;
}