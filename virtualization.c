#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "header.h"
#include "virtualization.h"
#include "compiler.h"
#include "sequence.h"
#include "memory.h"
#include "object.h"
#include "debug.h"

Virtualizer vm;

static void resetStack () { vm.stackHead = vm.stack; }
void push (Value value) { *vm.stackHead = value; vm.stackHead++; }
Value pop () { vm.stackHead--; return *vm.stackHead; }
static Value peek (int dist) { return vm.stackHead[-1 - dist]; }
static bool isFalse(Value value) { return IS_NONE(value) || (IS_BOOLEAN(value) && !AS_BOOLEAN(value)); }

static void concatenation() {
    OString* latter = AS_STRING(pop());
    OString* prior = AS_STRING(pop());

    int len = prior->length + latter->length;
    
    char* chars = ALLOCATE(char, len + 1);

    memcpy(chars, prior->chars, prior->length);
    memcpy(chars + prior->length, latter->chars, latter->length);

    OString* newString = genString(chars, len);
    push(OBJECT_VALUE(newString));
}

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
    vm.objectHead = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    return;
}

void freeVM () {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
    return;
}

static Interpretation elucidate() {
    #define READ_INSTRUCTION() (*vm.instruction++)
    #define READ_VALUE() (vm.sequence->constants.values[READ_INSTRUCTION()])
    #define READ_STRING() AS_STRING(READ_VALUE())
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
            case SIG_POP: pop(); break;
            case SIG_GLOBAL_RETURN: {
                OString* name = READ_STRING();
                Value val;

                if (!getItem(&vm.globals, name, &val)) {
                    runtimeErr("Global Return Failed: Undefined variable '%s'.", name->chars);
                    return RUNTIME_ERROR;
                }

                push(val);
                break;
            }
            case SIG_GLOBAL_ASSIGN: {
                OString* name = READ_STRING();

                if (setTable(&vm.globals, name, peek(0))) {
                    delItem(&vm.globals, name);
                    runtimeErr("Global Assignment Failed: Undefined variable %s", name->chars);
                    return RUNTIME_ERROR;
                }
                break;
            }
            case OP_GLOBAL: {
                OString* name = READ_STRING();
                setTable(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_EQUAL_TO: {
                Value b = pop();
                Value a = pop();
                push(BOOLEAN_VALUE(equalValues(a, b)));
                break;
            }
            case OP_GREATER_THAN: {
                BINARY_OP(BOOLEAN_VALUE, >);
                break;
            }
            case OP_LESS_THAN: {
                BINARY_OP(BOOLEAN_VALUE, <);
                break;
            }
            case SIG_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenation();
                } else if (IS_NUMERAL(peek(0)) && IS_NUMERAL(peek(1))) {
                    double latter = AS_NUMERAL(pop());
                    double prior = AS_NUMERAL(pop());
                    push(NUMERAL_VALUE(prior + latter));
                } else {
                    runtimeErr("Operands must be of the same type.");
                    return RUNTIME_ERROR;
                }
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
            case SIG_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case SIG_RETURN: {
                #ifdef DEBUG_TRACE_EXECUTION
                printf("\n");
                #endif

                // printf("return ");
                // printValue(pop());
                // printf("\n");

                return INTERPRETED;
            }
        }
    }

    #undef READ_INSTRUCTION
    #undef READ_VALUE
    #undef READ_STRING
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
