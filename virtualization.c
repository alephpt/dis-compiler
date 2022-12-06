#include <stdio.h>
#include <stdarg.h>
#include "header.h"
#include "virtualization.h"
#include "compiler.h"
#include "sequence.h"
#include "debug.h"

Virtualizer vm;

static void resetStack () { vm.stackHead = vm.stack; }
void push (Value value) { *vm.stackHead = value; vm.stackHead++; }
Value pop () { vm.stackHead--; return *vm.stackHead; }
static Value peek (int dist) { return vm.stackHead[-1 - dist]; }
static bool isFalse(Value value) { return IS_NONE(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)); }

static void runtimeErr(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruct = vm.instruction - vm.sequence->code - 1;
    int line = vm.sequence->line[instruct];
    fprintf(stderr, "[line %d] in script\n", line);
    resetStack;
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
    #define BINARY_OP(valueType, op) { \
        do { \
            if(!IS_NUMERAL(peek(0)) || !IS_NUMERAL(peek(1))) { \
                runtimeErr("Operands must be numeral types."); \
                return RUNTIME_ERROR; \
            } \
            double b = AS_NUMERAL(pop()); \
            double a = AS_NUMERAL(pop()); \
            push(valueType(a op b)); \
            } while (false); \
        }
    
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
            case OP_NONE: push(NONE_VALUE); break;
            case OP_TRUE: push(BOOLEAN_VALUE(true)); break;
            case OP_FALSE: push(BOOLEAN_VALUE(false)); break;
            case OP_ISEQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOLEAN_VALUE(equalValues(a, b)));
                break;
            }
            case OP_ISGREATER: {
                BINARY_OP(BOOLEAN_VALUE, >);
                break;
            }
            case OP_ISLESSER: {
                BINARY_OP(BOOLEAN_VALUE, <);
                break;
            }
            case SIG_ADD: {
                BINARY_OP(NUMERAL_VALUE, +);
                break;
            }
            case SIG_SUB: {
                BINARY_OP(NUMERAL_VALUE, -);
                break;
            }
            case SIG_MULT: {
                BINARY_OP(NUMERAL_VALUE, *);
                break;
            }
            case SIG_DIV: {
                BINARY_OP(NUMERAL_VALUE, /);
                break;
            }
            case SIG_NOT: {
                push(BOOLEAN_VALUE(isFalse(pop())));
                break;
            }
            case SIG_NEG: {
                if(!IS_NUMERAL(peek(0))) {
                    //TODO: Reverse string/Array
                    runtimeErr("Operand must be a number.");
                    return RUNTIME_ERROR;
                    
                }
                push(NUMERAL_VALUE(-AS_NUMERAL(pop())));
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

    if (!compile(source, &sequence))
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
