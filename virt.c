#include <stdio.h>
#include "header.h"
#include "virt.h"
#include "debug.h"

Virtualizer vm;

void initVM () {
    return;
}

void freeVM () {
    return;
}

static Interpretation run() {
    #define READ_INSTRUCTION() (*vm.instruction++)
    #define READ_VALUE() (vm.sequence->constants.values[READ_INSTRUCTION()])

    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        stripCommand(vm.sequence, (int)(vm.instruction - vm.sequence->code));
        #endif

        uint8_t instructor;
        switch (instructor = READ_INSTRUCTION()) {
            case OP_VALUE: {
                Value val = READ_VALUE();
                printValue(val);
                printf("\n");
                break;
            }
            case SIG_RETURN: {
                return INTERPRETED;
            }
        }
    }

    #undef READ_INSTRUCTION
    #undef READ_VALUE
}

Interpretation interpret (Sequence* sequence) {
    vm.sequence = sequence;
    vm.instruction = vm.sequence->code;
    return run();
}