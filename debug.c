#include <stdio.h>
#include "debug.h"

static int instruct (const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int instructByte (const char* name, Sequence* sequence, int offset) {
    uint8_t byte = sequence->code[offset + 1];
    printf("%-16s %4d\n", name, byte);
    return offset + 2;
}

static int instructJump (const char* name, int sign, Sequence* sequence, int offset) {
    uint16_t jump = (uint16_t)(sequence->code[offset + 1] << 8);
    jump |= sequence->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);

    return offset + 3;
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
            printf("%4d \033[90m──\033[0m ", seq->line[offset]);
        } else
        #ifdef DEBUG_TRACE_EXECUTION
        if (seq->code[offset] == OP_VALUE ||
            seq->code[offset] == OP_TRUE ||
            seq->code[offset] == OP_FALSE ||
            seq->code[offset] == OP_NONE ||
            seq->code[offset] == OP_EQUAL_TO ||
            seq->code[offset] == OP_LESS_THAN ||
            seq->code[offset] == OP_GREATER_THAN ||
            seq->code[offset] == SIG_NEG ||
            seq->code[offset] == SIG_DIV ||
            seq->code[offset] == SIG_ADD ||
            seq->code[offset] == SIG_MULT ||
            seq->code[offset] == SIG_SUB
            ) {
            printf("\033[90m");
            printf("   ╠─── ");
            printf("\033[0m");
        } else
        #endif
        if (seq->line[offset] != seq->line[offset + 2]) {
            printf("\033[90m");
            printf("   ╚─── ");
            printf("\033[0m");
        }
        else {
            printf("\033[90m");
            printf("   ║    ");
            printf("\033[0m");

        }
    } else {
        printf("%4d \033[90m──\033[0m ", seq->line[offset]);
    }

    uint8_t instruction = seq->code[offset];

    switch (instruction) {
        case OP_VALUE:
            return instructValue("OP_VALUE", seq, offset);
        case OP_NONE:
            return instruct("OP_NONE", offset);
        case OP_TRUE:
            return instruct("OP_TRUE", offset);
        case OP_FALSE:
            return instruct("OP_FALSE", offset);
        case SIG_POP:
            return instruct("SIG_POP", offset);
        case SIG_LOCAL_ASSIGN:
            return instructByte("SIG_LOCAL_RETURN", seq, offset);
        case SIG_LOCAL_RETURN:
            return instructByte("SIG_LOCAL_ASSIGN", seq, offset);
        case SIG_GLOBAL_RETURN:
            return instructValue("SIG_GLOBAL_RETURN", seq, offset);
        case SIG_GLOBAL_ASSIGN:
            return instructValue("SIG_GLOBAL_ASSIGN", seq, offset);
        case OP_GLOBAL:
            return instructValue("OP_GLOBAL", seq, offset);
        case OP_EQUAL_TO:
            return instruct("OP_EQUAL_TO", offset);
        case OP_GREATER_THAN:
            return instruct("OP_GREATER_THAN", offset);
        case OP_LESS_THAN:
            return instruct("OP_LESS_THAN", offset);
        case SIG_ADD:
            return instruct("SIG_ADD", offset);
        case SIG_SUB:
            return instruct("SIG_SUB", offset);
        case SIG_MULT:
            return instruct("SIG_MULT", offset);
        case SIG_DIV:
            return instruct("SIG_DIV", offset);
        case SIG_NOT:
            return instruct("SIG_NOT", offset);
        case SIG_NEG:
            return instruct("SIG_NEG", offset);
        case SIG_PRINT:
            return instruct("SIG_PRINT", offset);
        case SIG_ELSE:
            return instructJump("SIG_ELSE", 1, seq, offset);
        case SIG_OR:
            return instructJump("SIG_OR", 1, seq, offset);
        case SIG_WHEN:
            return instructJump("SIG_WHEN", 1, seq, offset);
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