#include <stdio.h>
#include "header.h"
#include "virt.h"
#include "compiler.h"
#include "sequence.h"
#include "debug.h"

Virtualizer vm;

static void resetStack () {
    vm.stackHead = vm.stack;
}

void push (Value value) {
    *vm.stackHead = value;
    vm.stackHead++;
}

Value pop () {
    vm.stackHead--;
    return *vm.stackHead;
}

void initVM () {
    resetStack();
    return;
}

void freeVM () {
    return;
}

static Interpretation elucidate() {
    #define READ_INSTRUCTION() (*vm.instruction++)
    #define READ_VALUE() (vm.sequence->constants.values[READ_INSTRUCTION()])
    #define BINARY_OP(op) { do { double b = pop(); double a = pop(); push(a op b); } while (false); }
    
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        int offset = (int)(vm.instruction - vm.sequence->code);

        for (Value* slot = vm.stack; slot < vm.stackHead; slot++) {
            if (slot == vm.stack) { 
                if (vm.sequence->line[offset] != vm.sequence->line[offset - 1]) {
                    printf("\033[90m");
                    printf(" stack   ╚══╬┤ [ ");
                } else {
                    printf("\033[90m");
                    printf(" stack   ╠══╬┤ [ "); 
                }
            } else {
                printf("─[ ");
            }

            printValue(*slot);
            printf(" ]");
        }

        if (vm.stackHead != vm.stack) { 
            printf(" ├╣\n"); 
            printf("\033[0m");        
        }

        stripCommand(vm.sequence, offset);
        #endif

        uint8_t instructor;
        switch (instructor = READ_INSTRUCTION()) {
            case OP_VALUE: {
                Value val = READ_VALUE();
                push(val);
                break;
            }
            case SIG_MULT: {
                BINARY_OP(*);
                break;
            }
            case SIG_DIV: {
                BINARY_OP(/);
                break;
            }
            case SIG_ADD: {
                BINARY_OP(+);
                break;
            }
            case SIG_SUB: {
                BINARY_OP(-);
                break;
            }
            case SIG_NEG: {
                push(-pop());
                break;
            }
            case SIG_RETURN: {
                #ifdef DEBUG_TRACE_EXECUTION
                printf("\n");
                #endif

                printf("return ");
                printValue(pop());
                printf("\n");

                return INTERPRETED;
            }
        }
    }

    #undef READ_INSTRUCTION
    #undef READ_VALUE
    #undef BINARY_OP
}

Interpretation interpret (const char* source) {
    Sequence sequence;
    initSequence(&sequence);

    if (!compile(source, &chunk))
    {
        freeSequence(&sequence);
        return COMPILE_ERROR;
    }

    vm.sequence = &sequence;
    vm.instruction = vm.sequence->code;

    Interpretation connotation = elucidate();

    freeSequence(&sequence);

    return connotation;
}
