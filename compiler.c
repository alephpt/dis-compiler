#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "header.h"
#include "compiler.h"
#include "memory.h"
#include "numeral.h"
#include "scanner.h"

typedef void (*PType)(bool assignable);

typedef struct {
    Token prev;
    Token current;
    bool erroneous;
    bool panic;
} Parser;

typedef enum {
    P_NONE,
    P_ASSIGN,
    P_LOG,
    P_OR,
    P_AND,
    P_EQUALS,
    P_COMPARE,
    P_TERM,
    P_FACTOR,
    P_UNARY,
    P_CALL,
    P_PRIMARY,
} Precedence;

typedef struct {
    PType prefix;
    PType infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int scope;
} LocalT;

// declared layouts outlive a single compilation so the repl can keep using them
typedef struct {
    OString* name;
    OForm* form;
} FormT;

typedef enum {
    TYPE_SCRIPT,
    TYPE_OPERATION
} OperationT;

typedef struct Compiler {
    struct Compiler* enclosing;
    OOperation* operation;
    OperationT type;
    LocalT locals[UINT8_COUNT];
    int localCount;
    int localScope;
} Compiler;

// every source pulled in by an include. the buffers stay alive for the whole
// compilation because token start pointers alias them, and parser.prev can
// still be pointing into a file one token after the scanner has left it
typedef struct {
    char* source;
    size_t bytes;
    const char* path;
} Included;

Parser parser;
Compiler* current = NULL;
Included included[INCLUDE_DEPTH_MAX * 16];
int includedCount = 0;
FormT declaredForms[UINT8_COUNT];
int declaredFormCount = 0;
// a bare form name allocates a single instance only as the value of a definition
bool formInstantiable = false;
static void expression();
static int translateEscapes(const char* raw, int rawLength, char* target);
static void declaration();
static void definition();
static void statement();
static ParseRule* getRule(TType type);
static void precedence(Precedence precede);
static void andComp(bool assignable);
static void orComp(bool assignable);
static void numeral(bool assignable);
static void string(bool assignable);
static void variable(bool assignable);
static void grouping(bool assignable);
static void unary(bool assignable);
static void binary(bool assignable);
static void literal(bool assignable);
static void call(bool assignable);
static void indexed(bool assignable);
static void member(bool assignable);

ParseRule rules[] = {
    [T_L_PAR]            =             {grouping,      NULL,       P_NONE},
    [T_R_PAR]            =             {NULL,          NULL,       P_NONE},
    [T_L_BRACK]          =             {NULL,          indexed,    P_CALL},
    [T_R_BRACK]          =             {NULL,          NULL,       P_NONE},
    [T_L_BRACE]          =             {NULL,          NULL,       P_NONE},
    [T_R_BRACE]          =             {NULL,          NULL,       P_NONE},
    [T_COMMA]            =             {NULL,          NULL,       P_NONE},
    [T_REF]              =             {NULL,          NULL,       P_NONE},
    [T_DEREF]            =             {NULL,          NULL,       P_NONE},
    [T_HASH]             =             {NULL,          NULL,       P_NONE},
    [T_OPEN]             =             {NULL,          NULL,       P_NONE},
    [T_UNDER]            =             {NULL,          NULL,       P_NONE},
    [T_PERIOD]           =             {NULL,          NULL,       P_NONE},
    [T_PARAM_END]        =             {NULL,          NULL,       P_NONE},
    [T_CLOSE]            =             {NULL,          NULL,       P_NONE},
    [T_ID]               =             {variable,      NULL,       P_NONE},
    [T_EXECUTE]          =             {NULL,          call,       P_CALL},
    [T_LOG]              =             {NULL,          NULL,       P_NONE},
    [T_WRITE]            =             {NULL,          NULL,       P_NONE},
    [T_MINUS]            =             {unary,         binary,     P_TERM},
    [T_PLUS]             =             {NULL,          binary,     P_TERM},
    [T_WHACK]            =             {NULL,          binary,     P_FACTOR},
    [T_STAR]             =             {NULL,          binary,     P_FACTOR},
    [T_MOD]              =             {NULL,          binary,     P_FACTOR},
    [T_POWER]            =             {NULL,          NULL,       P_NONE},
    [T_INCREMENT]        =             {NULL,          NULL,       P_NONE},
    [T_DECREMENT]        =             {NULL,          NULL,       P_NONE},
    // compound assignment is a statement form, handled by the 'as' loop update -
    // never an infix expression. binary() has no case for these, so routing them
    // through it would consume an operand and emit nothing, corrupting the stack
    [T_PLUS_EQ]          =             {NULL,          NULL,       P_NONE},
    [T_MINUS_EQ]         =             {NULL,          NULL,       P_NONE},
    [T_EQ_PLUS]          =             {NULL,          NULL,       P_NONE},
    [T_EQ_MINUS]         =             {NULL,          NULL,       P_NONE},
    [T_AND_OP]           =             {NULL,          andComp,    P_AND},
    [T_OR_OP]            =             {NULL,          orComp,     P_OR},
    [T_GREATER]          =             {NULL,          binary,     P_COMPARE},
    [T_LESSER]           =             {NULL,          binary,     P_COMPARE},
    [T_GTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_LTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_EQEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_INEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_NOT]              =             {unary,         NULL,       P_NONE},
    [T_ASSIGN]           =             {NULL,          NULL,       P_NONE},
    [T_L_OUT]            =             {NULL,          NULL,       P_NONE},
    [T_R_OUT]            =             {NULL,          NULL,       P_NONE},
    [T_COMMENT]          =             {NULL,          NULL,       P_NONE},
    [T_DOLLAR]           =             {NULL,          NULL,       P_NONE},
    [T_BWHACK]           =             {NULL,          NULL,       P_NONE},
    [T_BITWISE]          =             {NULL,          NULL,       P_NONE},
    [T_QUEST]            =             {NULL,          NULL,       P_NONE},
    [T_INDEX]            =             {NULL,          NULL,       P_NONE},
    [T_DEFINE]           =             {NULL,          NULL,       P_NONE},
    [T_INCLUDE]          =             {NULL,          NULL,       P_NONE},
    [T_PILOT]            =             {NULL,          NULL,       P_NONE},
    [T_PARENT]           =             {NULL,          NULL,       P_NONE},
    [T_GLOBAL]           =             {NULL,          NULL,       P_NONE},
    [T_SELF]             =             {NULL,          NULL,       P_NONE},
    [T_THIS]             =             {NULL,          NULL,       P_NONE},
    [T_PUBLIC]           =             {NULL,          NULL,       P_NONE},
    [T_PRIVATE]          =             {NULL,          NULL,       P_NONE},
    [T_MEMBER]           =             {NULL,          member,     P_CALL},
    [T_RETURN]           =             {NULL,          NULL,       P_NONE},
    [T_OP]               =             {NULL,          NULL,       P_NONE},
    [T_OBJ]              =             {NULL,          NULL,       P_NONE},
    [T_ENUM]             =             {NULL,          NULL,       P_NONE},
    [T_FORM]             =             {NULL,          NULL,       P_NONE},
    [T_PAIR]             =             {NULL,          NULL,       P_NONE},
    [T_WIDTH]            =             {NULL,          NULL,       P_NONE},
    [T_STRING]           =             {string,        NULL,       P_NONE},
    [T_BINARY]           =             {numeral,       NULL,       P_NONE},
    [T_DECIMAL]          =             {numeral,       NULL,       P_NONE},
    [T_OCTAL]            =             {numeral,       NULL,       P_NONE},
    [T_HEXADECIMAL]      =             {numeral,       NULL,       P_NONE},
    [T_AS]               =             {NULL,          NULL,       P_NONE},
    [T_WHILE]            =             {NULL,          NULL,       P_NONE},
    [T_WHEN]             =             {NULL,          NULL,       P_NONE},
    [T_OR]               =             {NULL,          NULL,       P_NONE},
    [T_ELSE]             =             {NULL,          NULL,       P_NONE},
    [T_NONE]             =             {literal,       NULL,       P_NONE},
    [T_TRUE]             =             {literal,       NULL,       P_NONE},
    [T_FALSE]            =             {literal,       NULL,       P_NONE},
    [T_EOF]              =             {NULL,          NULL,       P_NONE},
    [T_ERR]              =             {NULL,          NULL,       P_NONE},
    [T_SEMIC]            =             {NULL,          NULL,       P_NONE},
    [T_EQ]                =            {NULL,          NULL,       P_NONE}
};

static void initCompiler (Compiler* compiler, OperationT type) {
    compiler->enclosing = current;
    compiler->operation = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->localScope = 0;
    compiler->operation = newOperation();
    compiler->operation->file = scannerFile();
    current = compiler;
    
    if (type != TYPE_SCRIPT) {
        current->operation->name = copyString(parser.prev.start, parser.prev.length);
    }

    LocalT* local = &current->locals[current->localCount++];
    local->scope = 0;
    local->name.start = "";
    local->name.length = 0;
    return;
}

static void err(Token* token, const char* message) {
    if (parser.panic) { return; }
    parser.panic = true;

    fprintf(stderr, "ERR - [%s: line %d]:", token->file, token->line);

    if (token->type == T_EOF) {
        fprintf(stderr, " End of File.");
    } else if (token->type == T_ERR) {

    } else {
        fprintf(stderr, " '%.*s'", token->length, token->start);
    }

    fprintf(stderr, " - %s\n", message);
    parser.erroneous = true;
    return;
}

static Sequence* currentSequence() { return &current->operation->sequence; }
static void currentErr (const char* message) { err(&parser.current, message); return; }
static void prevErr (const char* message) { err(&parser.prev, message); return; }
static ParseRule* getRule (TType type) { return &rules[type]; }
static bool check (TType type) { return parser.current.type == type; }
static void byteEmitter (uint8_t byte) { writeSequence(currentSequence(), byte, parser.prev.line); return; }
static void emitBytes (uint8_t byte1, uint8_t byte2) { byteEmitter(byte1); byteEmitter(byte2); return; }
static void returnEmitter() { byteEmitter(OP_NONE); byteEmitter(SIG_RETURN); return; }

static OOperation* closeCompilation() { 
    returnEmitter();
    OOperation* op = current->operation;

    #ifdef DEBUG_PRINT_CODE
    if (!parser.erroneous) {
        stripSequence(currentSequence(), op->name != NULL ? op->name->chars : "<script>");
    }
    #endif

    current = current->enclosing;
    
    return op; 
}

static int genValue (Value val) {
    int value = addValue(currentSequence(), val);

    if (value > UINT16_MAX) {
        prevErr("Too many values in one chunk.");
        return 0;
    }

    return value;
}

// a constant index that does not fit in a byte switches the instruction to its
// wide form rather than failing - the narrow form stays the common case
static void constEmitter (uint8_t op8, uint8_t op16, int index) {
    if (index <= UINT8_MAX) {
        emitBytes(op8, (uint8_t)index);
        return;
    }

    byteEmitter(op16);
    byteEmitter((uint8_t)((index >> 8) & 0xff));
    byteEmitter((uint8_t)(index & 0xff));
    return;
}

static int identifier (Token* name) {
    return genValue(OBJECT_VALUE(copyString(name->start, name->length)));
}

static bool identifiersMatch(Token* a, Token* b) {
    return (a->length != b->length) ? false : memcmp(a->start, b->start, a->length) == 0;
}

static void stepThrough () {
    parser.prev = parser.current;

    for (;;) {
        parser.current = scanToken();
        
        if (parser.current.type != T_ERR) { break; }

        currentErr(parser.current.start);
    }
    return;
}

static bool match (TType type) {
    if (!check(type)) { return false; }
    
    stepThrough();
    return true;
}

static void forceConsume (TType t, const char* message) {
    if (parser.current.type == t) {
        stepThrough();
        return;
    }

    currentErr(message);
    return;
}

static void valueEmitter (Value value) { constEmitter(OP_VALUE, OP_VALUE_16, genValue(value)); return; }

static int jumpEmitter (uint8_t signal) {
    byteEmitter(signal);
    byteEmitter(0xff);
    byteEmitter(0xff);
    return currentSequence()->inventory - 2;
}

static void loopEmitter (int start) {
    byteEmitter(SIG_LOOP);

    int offset = currentSequence()->inventory - start + 2;
    if (offset > UINT16_MAX) { prevErr("Loop Body Too Large."); }

    byteEmitter((offset >> 8) & 0xff);
    byteEmitter(offset & 0xff);
    return;
}


static void landJump (int offset) {
    int jumpSize = currentSequence()->inventory - offset - 2;

    if (jumpSize > UINT16_MAX) {
        prevErr("Jump size too large");
    }

    currentSequence()->code[offset] = (jumpSize >> 8) & 0xff;
    currentSequence()->code[offset + 1] = jumpSize & 0xff;
    return;
}   

static void rebase () {
    parser.panic = false;

    while (parser.current.type != T_EOF) {
        if (parser.prev.type == T_PERIOD) { return; }

        switch (parser.current.type) {
            case T_OBJ:
            case T_OP:
            case T_FORM:
            case T_INCLUDE:
            case T_DEFINE:
            case T_AS:
            case T_WHEN:
            case T_WHILE:
            case T_LOG:
            case T_WRITE:
            case T_RETURN:
                return;
            default:
                ;
        }

        stepThrough();
    }
    return;
}

static void precedence (Precedence precede) {
    stepThrough();

    PType prefix = getRule(parser.prev.type)->prefix;

    if (prefix == NULL) {
        prevErr("Expression expected.");
        return;
    }

    bool assignable = precede <= P_ASSIGN; 
    prefix(assignable);

    while (precede <= getRule(parser.current.type)->precedence) {
        stepThrough();
        PType infix = getRule(parser.prev.type)->infix;

        if (infix == NULL) {
            prevErr("Operator expected.");
            return;
        }

        infix(assignable);
    }

    if (assignable && match(T_ASSIGN)) {
        prevErr("Invalid assignment target.");
    }
    return;
}

static void beginScope () {
    current->localScope++; 
    return;
}

static void endScope () {
    current->localScope--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].scope > current->localScope) {
        byteEmitter(SIG_POP);
        current->localCount--;
    }

    return;
}

static void scope () {
    while (!check(T_CLOSE) && !check(T_EOF)) {
        declaration();
    }

    forceConsume(T_CLOSE, "Expected closing '^' at end of scope.");

    // '^' closes a body and returns - an operation body may carry a result on
    // the same line, ie. 'op adder <- a, b : $ ^(a + b)'
    if (current->type != TYPE_SCRIPT &&
        parser.current.line == parser.prev.line &&
        getRule(parser.current.type)->prefix != NULL) {
        expression();
        match(T_PERIOD);
        byteEmitter(SIG_RETURN);
    }

    return;
}

static int findLocality(Compiler* c, Token* name) {
    for (int i = c->localCount - 1; i >= 0; i--) {
        LocalT* local = &c->locals[i];

        if (identifiersMatch(name, &local->name)) { 
            if (local->scope == -1) {
                prevErr("Can't read local variable in it's own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void variableName (Token name, bool assignable) {
    // a local names a stack slot, which is always narrow. a global names a
    // constant, which may have to reach past 255
    int local = findLocality(current, &name);
    int var = (local != -1) ? local : identifier(&name);
    bool assigning = assignable && match(T_ASSIGN);

    if (assigning) { expression(); }

    if (local != -1) {
        emitBytes(assigning ? SIG_LOCAL_ASSIGN : SIG_LOCAL_RETURN, (uint8_t)var);
        return;
    }

    if (assigning) {
        constEmitter(SIG_GLOBAL_ASSIGN, SIG_GLOBAL_ASSIGN_16, var);
        return;
    }

    constEmitter(SIG_GLOBAL_RETURN, SIG_GLOBAL_RETURN_16, var);
    return;
}

static void initializeDefinition () {
    if (current->localScope == 0) return;
    current->locals[current->localCount - 1].scope = current->localScope;
    return;
}

static void defineVariable (int variable) {
    if (current->localScope > 0) {
        initializeDefinition();
        return;
    }
    // TODO: implement check for global keyword and resort
    constEmitter(OP_GLOBAL, OP_GLOBAL_16, variable);
}

static void defineLocal (Token name) {
    if (current->localCount == UINT8_COUNT) {
        prevErr("Local variable limit exceeded.");
        return;
    }

    LocalT* local = &current->locals[current->localCount++];
    local->name = name;
    local->scope = -1;
    return;
}


static void declareDefinition () {
    if (current->localScope == 0) { return; }

    Token* name = &parser.prev;

    for (int i = current->localCount - 1; i >= 0; i--) {
        LocalT* local = &current->locals[i];

        if (local->scope != -1 && local->scope < current->localScope) {
            break;
        }

        if (identifiersMatch(name, &local->name)) {
            prevErr("Variable with that name already exists in the same scope.");
        }
    }

    defineLocal(*name);
    return;
}

static int parseDefinition (const char* message) {
    forceConsume(T_ID, message);
    
    declareDefinition();
    if (current->localScope > 0) { return 0; }
    
    return identifier(&parser.prev);
}

static OForm* findForm (Token* name) {
    for (int i = 0; i < declaredFormCount; i++) {
        OString* declared = declaredForms[i].name;

        if (declared->length == name->length &&
            memcmp(declared->chars, name->start, name->length) == 0) {
            return declaredForms[i].form;
        }
    }

    return NULL;
}

static void registerForm (OString* name, OForm* form) {
    for (int i = 0; i < declaredFormCount; i++) {
        if (declaredForms[i].name == name) {
            declaredForms[i].form = form;
            return;
        }
    }

    if (declaredFormCount == UINT8_COUNT) {
        prevErr("Declared form limit exceeded.");
        return;
    }

    declaredForms[declaredFormCount].name = name;
    declaredForms[declaredFormCount].form = form;
    declaredFormCount++;
    return;
}

// 'name <- width.' - one packed field per statement, held in declaration order
static int formFields (FormField* fields) {
    int count = 0;

    while (!check(T_CLOSE) && !check(T_EOF)) {
        WidthT width = W_U8;

        forceConsume(T_ID, "Expected field name in form body.");
        OString* name = copyString(parser.prev.start, parser.prev.length);

        for (int i = 0; i < count; i++) {
            if (fields[i].name == name) { prevErr("Field with that name already exists in this form."); }
        }

        forceConsume(T_ASSIGN, "Expected '<-' after field name.");
        forceConsume(T_WIDTH, "Expected a width - u8, u16, u32, u64, i8, i16, i32, i64, f32 or f64.");

        if (!parser.panic && !findWidth(parser.prev.start, parser.prev.length, &width)) {
            prevErr("Unknown field width.");
        }

        forceConsume(T_PERIOD, "Expected '.' after field declaration.");

        // a failed field consumes nothing, so unwind rather than spin
        if (parser.panic) { return count; }

        if (count == UINT8_COUNT) {
            prevErr("Form field limit exceeded.");
            return count;
        }

        fields[count].name = name;
        fields[count].width = width;
        fields[count].offset = 0;
        count++;
    }

    return count;
}

static void formDeclaration () {
    FormField fields[UINT8_COUNT];

    forceConsume(T_ID, "Expected form name.");
    OString* name = copyString(parser.prev.start, parser.prev.length);
    int global = genValue(OBJECT_VALUE(name));

    forceConsume(T_ASSIGN, "Expected '<-' after form name.");
    forceConsume(T_OPEN, "Expected '$' before form body.");

    int count = formFields(fields);

    if (parser.panic) { return; }

    forceConsume(T_CLOSE, "Expected '^' at the end of a form body.");

    if (count == 0) {
        prevErr("A form requires at least one field.");
        return;
    }

    OForm* form = newForm(name, fields, count);
    registerForm(name, form);

    // a layout is always bound globally - the type outlives any local scope
    constEmitter(OP_VALUE, OP_VALUE_16, genValue(OBJECT_VALUE(form)));
    constEmitter(OP_GLOBAL, OP_GLOBAL_16, global);
    return;
}

static void operate (OperationT type) {
    Compiler compile;
    initCompiler(&compile, type);
    beginScope();

    forceConsume(T_ASSIGN, "Expected '<-' after operation name.");

    if (!check(T_PARAM_END)) {
        do {
            current->operation->arity++;

            if (current->operation->arity > 255) {
                currentErr("You cannot have more than 255 variables. Try rethinking your implementation.");
            }

            int constant = parseDefinition("Expected a parameer name.");
            defineVariable(constant);
        } while (match(T_COMMA));
    }

    forceConsume(T_PARAM_END, "Expected ':' after parementers.");
    forceConsume(T_OPEN, "Expected '$' before operation body.");
    scope();

    OOperation* op = closeCompilation();
    constEmitter(OP_VALUE, OP_VALUE_16, genValue(OBJECT_VALUE(op)));
    return;
}

static void operation () {
    int global = parseDefinition("Expected operation name.");
    initializeDefinition();
    operate(TYPE_OPERATION);
    defineVariable(global);
    return;
}

static void expression () {
    precedence(P_ASSIGN);
    return;
}

static void andComp(bool assignable) {
    int jumpIfFalse = jumpEmitter(SIG_EXECUTE);
    byteEmitter(SIG_POP);
    precedence(P_AND);
    landJump(jumpIfFalse);
    return;
}

static void orComp(bool assignable) {
    int jumpIfFalse = jumpEmitter(SIG_EXECUTE);
    int jumpIfTrue = jumpEmitter(SIG_JUMP);
    landJump(jumpIfFalse);
    byteEmitter(SIG_POP);
    precedence(P_OR);
    landJump(jumpIfTrue);
    return;
}

static void expressionStatement () {
    expression();
    forceConsume(T_PERIOD, "Expected '.' at the end of the expression.");
    byteEmitter(SIG_POP);
    return;
}

static void printStatement () {
    forceConsume(T_EXECUTE, "Expected '->' after 'log'.");
    expression();
    forceConsume(T_PERIOD, "Expected '.' after log expression.");
    byteEmitter(SIG_PRINT);
    return;
}

// 'write -> path, value, ... .' - the path leads, every value follows it in the
// order it was written.
//
// ',' carries no precedence, so an ordinary expression ends at one. A '->' call
// does not: arguments() consumes commas itself and swallows the rest of the list
// as its own arguments. A call anywhere but the final position has to be
// parenthesised - '(f -> x), next' - or it eats 'next'.
static void writeStatement () {
    uint8_t values = 0;

    forceConsume(T_EXECUTE, "Expected '->' after 'write'.");
    expression();
    forceConsume(T_COMMA, "Expected ',' after the write path.");

    do {
        expression();

        if (values == 255) {
            prevErr("255 value limit exceeded in one write.");
            break;
        }

        values++;
    } while (match(T_COMMA));

    forceConsume(T_PERIOD, "Expected '.' after the write values.");
    emitBytes(SIG_WRITE, values);
    return;
}

// the loop variable is read and written by name - a local slot when the loop
// declared it, the global binding when the loop only borrows an existing one
static void loopVarLoad (Token name) {
    int local = findLocality(current, &name);

    if (local != -1) {
        emitBytes(SIG_LOCAL_RETURN, (uint8_t)local);
        return;
    }

    constEmitter(SIG_GLOBAL_RETURN, SIG_GLOBAL_RETURN_16, identifier(&name));
    return;
}

static void loopVarStore (Token name) {
    int local = findLocality(current, &name);

    if (local != -1) {
        emitBytes(SIG_LOCAL_ASSIGN, (uint8_t)local);
        return;
    }

    constEmitter(SIG_GLOBAL_ASSIGN, SIG_GLOBAL_ASSIGN_16, identifier(&name));
    return;
}

// 'def i <- 0' makes the loop own the variable, 'i <- 0' borrows one that exists
static bool asInitializer (Token* loopVar) {
    bool declared = match(T_DEFINE);

    forceConsume(T_ID, "Expected a loop variable in the 'as' initializer.");
    *loopVar = parser.prev;

    if (declared) { declareDefinition(); }

    forceConsume(T_ASSIGN, "Expected '<-' after the 'as' loop variable.");
    expression();

    if (declared) {
        // the initializer's value stays put - it is the slot of the new local
        initializeDefinition();
    } else {
        // an assignment yields its value, and nothing here consumes it
        loopVarStore(*loopVar);
        byteEmitter(SIG_POP);
    }

    forceConsume(T_PERIOD, "Expected '.' after the 'as' initializer.");
    return declared;
}

// '++' '--' '+= expr' '-= expr' - every form reads, steps and stores the loop var
static void asUpdate (Token loopVar) {
    if (match(T_INCREMENT) || match(T_DECREMENT)) {
        TType step = parser.prev.type;

        loopVarLoad(loopVar);
        valueEmitter(NUMERAL_VALUE(1));
        byteEmitter(step == T_INCREMENT ? SIG_ADD : SIG_SUB);
        loopVarStore(loopVar);
        byteEmitter(SIG_POP);
        return;
    }

    if (match(T_PLUS_EQ) || match(T_MINUS_EQ)) {
        TType step = parser.prev.type;

        loopVarLoad(loopVar);
        expression();
        byteEmitter(step == T_PLUS_EQ ? SIG_ADD : SIG_SUB);
        loopVarStore(loopVar);
        byteEmitter(SIG_POP);
        return;
    }

    currentErr("Expected '++', '--', '+= expr' or '-= expr' in the 'as' update.");
    return;
}

// the left side of the condition is implied - it is always the loop variable,
// so 'as, def i <- 0.(++) < 7:' reads as 'i < 7'
static int asCondition (Token loopVar) {
    ParseRule* rule = getRule(parser.current.type);

    loopVarLoad(loopVar);

    if (rule->infix != binary ||
        rule->precedence < P_EQUALS || rule->precedence > P_COMPARE) {
        currentErr("Expected a comparison after the 'as' update.");
        return -1;
    }

    stepThrough();
    binary(false);

    return jumpEmitter(SIG_EXECUTE);
}

// 'as, init.(update) [comparison] : statement'
//
//          init
//          SIG_JUMP ─────────► cond      (the update is skipped on pass one)
//  update: <update>
//  cond:   <loop var> <rhs> <compare>
//          SIG_EXECUTE ──────► exit
//          SIG_POP
//          <body>
//          SIG_LOOP ─────────► update
//  exit:   SIG_POP
//
// with no update the loop returns to cond, and with no condition it returns to
// the body and runs forever
static void asStatement () {
    Token loopVar;
    int exitJump = -1;

    // the loop owns its scope, so a declared loop variable is a true local even
    // when the loop sits at the top level
    beginScope();
    forceConsume(T_COMMA, "Expected ',' after 'as'.");

    asInitializer(&loopVar);

    forceConsume(T_L_PAR, "Expected '(' before the 'as' update.");

    bool stepped = !check(T_R_PAR);
    int updateJump = stepped ? jumpEmitter(SIG_JUMP) : -1;
    int loopTarget = currentSequence()->inventory;

    if (stepped) {
        asUpdate(loopVar);
        landJump(updateJump);
    }

    forceConsume(T_R_PAR, "Expected ')' after the 'as' update.");

    if (!check(T_PARAM_END)) {
        exitJump = asCondition(loopVar);

        if (exitJump != -1) { byteEmitter(SIG_POP); }
    }

    forceConsume(T_PARAM_END, "Expected ':' after the 'as' clauses.");

    // a malformed header leaves nothing coherent to attach a body to
    if (parser.panic) {
        endScope();
        return;
    }

    statement();
    loopEmitter(loopTarget);

    if (exitJump != -1) {
        landJump(exitJump);
        byteEmitter(SIG_POP);
    }

    endScope();
    return;
}

static int orStatement (int when) {
    landJump(when);
    byteEmitter(SIG_POP);
    endScope();

    beginScope();
    forceConsume(T_COMMA, "Expected ',' after 'or'.");
    expression();
    forceConsume(T_PARAM_END, "Expected ':' after conditional 'or' expression.");

    int jumper = jumpEmitter(SIG_EXECUTE);
    byteEmitter(SIG_POP);

    statement();

    return jumper;
}

static void whenStatement () {
    beginScope();
    forceConsume(T_COMMA, "Expected ',' after 'when'.");
    expression();
    forceConsume(T_PARAM_END, "Expected ':' after conditional 'when' expression.");

    int jumpIfFalse = jumpEmitter(SIG_EXECUTE);
    byteEmitter(SIG_POP);

    statement();

    while (match(T_OR)) {
        int jumpStatus = orStatement(jumpIfFalse);
        jumpIfFalse = jumpStatus;
    }

    int elseJump = jumpEmitter(SIG_JUMP);

    landJump(jumpIfFalse);
    byteEmitter(SIG_POP);
    endScope();

    if (match(T_ELSE)) { 
        beginScope();
        forceConsume(T_PARAM_END, "Expected ':' after 'else'.");
        statement(); 
        endScope();
    }

    landJump(elseJump);
    return;
}

static void whileStatement () {
    int loopStart = currentSequence()->inventory;
    beginScope();
    forceConsume(T_COMMA, "Expected ',' after 'while'.");
    expression();
    forceConsume(T_PARAM_END, "Expected ':' after conditional 'while' expression.");

    int exitIfFalse = jumpEmitter(SIG_EXECUTE);
    byteEmitter(SIG_POP);

    statement();
    loopEmitter(loopStart);

    landJump(exitIfFalse);
    byteEmitter(SIG_POP);
    endScope();
    return;
}

static void returnStatement () {
    if (current->type == TYPE_SCRIPT) {
        prevErr("Can't return from top level code.");
    }

    if (match(T_PERIOD)) {
        returnEmitter();
        return;
    }

    expression();
    forceConsume(T_PERIOD, "Expected '.' after return value.");
    byteEmitter(SIG_RETURN);
    return;
}

// ------------------------------------------------------------- include ----

// a relative include is resolved against the file doing the including, so a
// library can refer to its own neighbours no matter where dis was run from
static void resolveInclude (const char* base, const char* rel, char* out, size_t cap) {
    const char* slash = (base == NULL) ? NULL : strrchr(base, '/');

    // no directory part means the repl, or a file named from the working
    // directory - either way the include resolves against the working directory
    if (rel[0] == '/' || slash == NULL) {
        snprintf(out, cap, "%s", rel);
        return;
    }

    snprintf(out, cap, "%.*s%s", (int)(slash - base) + 1, base, rel);
    return;
}

// the canonical path is what include-once compares, which is also what stops a
// cycle - a file that is already open simply is not opened again
static bool alreadyIncluded (const char* canonical) {
    for (int i = 0; i < includedCount; i++) {
        if (strcmp(included[i].path, canonical) == 0) { return true; }
    }

    return false;
}

static char* readSource (const char* path, size_t* bytes) {
    FILE* file = fopen(path, "rb");

    if (file == NULL) { return NULL; }

    // a directory opens happily and reports a length of zero, so without this it
    // would read as an empty file and quietly include nothing at all. the probe
    // read is only there to make the system name the real reason
    if (fseek(file, 0L, SEEK_END) != 0) {
        char probe;

        errno = 0;
        fread(&probe, 1, 1, file);

        int reason = errno;

        fclose(file);
        errno = (reason == 0) ? EIO : reason;
        return NULL;
    }

    long size = ftell(file);
    rewind(file);

    if (size < 0) {
        int reason = errno;

        fclose(file);
        errno = reason;
        return NULL;
    }

    char* buffer = ALLOCATE(char, (size_t)size + 1);
    size_t read = fread(buffer, sizeof(char), (size_t)size, file);

    // a short read means the path was never really a readable file - a directory
    // opens happily and then refuses to be read, and silently including nothing
    // would be worse than saying so
    if (read < (size_t)size || ferror(file)) {
        int reason = errno;

        FREE_ARRAY(char, buffer, (size_t)size + 1);
        fclose(file);
        errno = (reason == 0) ? EIO : reason;
        return NULL;
    }

    fclose(file);
    buffer[read] = '\0';
    *bytes = (size_t)size + 1;
    return buffer;
}

// 'include -> "path".' - textual, top level only, and worth no bytecode at all
static void includeDeclaration () {
    char resolved[PATH_MAX];
    char canonical[PATH_MAX];
    char raw[PATH_MAX];

    if (current->type != TYPE_SCRIPT || current->localScope > 0) {
        prevErr("'include' is only allowed at the top level of a file.");
        return;
    }

    forceConsume(T_EXECUTE, "Expected '->' after 'include'.");
    forceConsume(T_STRING, "Expected a quoted path after 'include ->'.");

    Token quoted = parser.prev;
    int rawLength = quoted.length - 2;

    if (rawLength < 0 || rawLength >= PATH_MAX) {
        prevErr("Include path is too long.");
        return;
    }

    int length = translateEscapes(quoted.start + 1, rawLength, raw);

    if (length < 0) { return; }

    raw[length] = '\0';

    if (!check(T_PERIOD)) {
        currentErr("Expected '.' after the include path.");
        return;
    }

    resolveInclude(quoted.file, raw, resolved, sizeof(resolved));

    if (realpath(resolved, canonical) == NULL) {
        prevErr(strerror(errno));
        return;
    }

    // a repeat, whether a diamond or an outright cycle, is simply skipped
    if (!alreadyIncluded(canonical)) {
        if (includedCount == (int)(sizeof(included) / sizeof(Included))) {
            prevErr("Too many included files.");
            return;
        }

        size_t bytes = 0;
        char* source = readSource(resolved, &bytes);

        if (source == NULL) {
            prevErr(strerror(errno));
            return;
        }

        // the displayed name stays relative so diagnostics do not depend on
        // where the tree happens to live, while the canonical path keys the set
        OString* shown = copyString(resolved, (int)strlen(resolved));

        included[includedCount].source = source;
        included[includedCount].bytes = bytes;
        included[includedCount].path = copyString(canonical, (int)strlen(canonical))->chars;
        includedCount++;

        // the push has to happen before the '.' is stepped over, otherwise the
        // parser has already read the next token out of the parent file
        if (!pushSource(source, shown->chars)) {
            prevErr("Includes are nested too deeply.");
            return;
        }
    }

    stepThrough();
    return;
}

static void definition () {
    int variable = parseDefinition("Expected variable name.");

    if (match(T_ASSIGN)) {
        formInstantiable = true;
        expression();
        formInstantiable = false;
    } else {
        // TODO: Implement 'DEAD' type;
        byteEmitter(OP_NONE);
    }

    forceConsume(T_PERIOD, "Expected '.' after Variable declaration.");

    defineVariable(variable);
    return;
}
 
static void statement () {
    if (match(T_LOG)) { printStatement(); } else
    if (match(T_WRITE)) { writeStatement(); } else
    if (match(T_AS)) { asStatement(); } else
    if (match(T_WHEN)) { whenStatement(); } else
    if (match(T_WHILE)) { whileStatement(); } else
    if (match(T_RETURN)) { returnStatement(); } else
    if (match(T_OPEN)) {
        beginScope();
        scope();
        endScope();
    }
    else { expressionStatement(); }
    return;
}

static void declaration () {
    if (match(T_INCLUDE)) { includeDeclaration(); } else
    if (match(T_OP)) { operation(); } else
    if (match(T_FORM)) { formDeclaration(); } else
    if (match(T_DEFINE)) { definition(); }
    else { statement(); }

    if (parser.panic) { rebase(); }
    return;
}

static void literal (bool assignable) {
    (void)assignable;
    switch (parser.prev.type) {
        case T_FALSE: byteEmitter(OP_FALSE); break;
        case T_NONE: byteEmitter(OP_NONE); break;
        case T_TRUE: byteEmitter(OP_TRUE); break;
        default: return;
    }
    return;
}

static void numeral (bool assignable) {
    NumeralT base = N_DENARY;

    switch (parser.prev.type) {
        case T_BINARY:      base = N_BINARY; break;
        case T_OCTAL:       base = N_OCTAL; break;
        case T_HEXADECIMAL: base = N_HEXADECIMAL; break;
        default: break;
    }

    valueEmitter(NUMERAL_VALUE(parseNumeral(parser.prev.start, parser.prev.length, base)));
    return;
}

// escapes are resolved once, here, so the value carries real bytes and nothing
// downstream has to know an escape ever existed. returns -1 on an unknown one
static int translateEscapes (const char* raw, int rawLength, char* target) {
    int length = 0;

    for (int i = 0; i < rawLength; i++) {
        if (raw[i] != '\\' || i + 1 == rawLength) {
            target[length++] = raw[i];
            continue;
        }

        switch (raw[++i]) {
            case 'n':  target[length++] = '\n'; break;
            case 'r':  target[length++] = '\r'; break;
            case '0':  target[length++] = '\0'; break;
            case '\\': target[length++] = '\\'; break;
            case '"':  target[length++] = '"'; break;
            // a trailing '\' continues the line and carries its newline through
            case '\n': target[length++] = '\n'; break;
            default:
                prevErr("Unknown escape in string - use \\n, \\r, \\0, \\\\ or \\\".");
                return -1;
        }
    }

    return length;
}

static void string (bool assignable) {
    // the token span carries its quotes, and translation never grows the text
    int rawLength = parser.prev.length - 2;

    if (rawLength < 0) { rawLength = 0; }

    char* chars = ALLOCATE(char, rawLength + 1);
    int length = translateEscapes(parser.prev.start + 1, rawLength, chars);

    if (length < 0) {
        FREE_ARRAY(char, chars, rawLength + 1);
        return;
    }

    chars[length] = '\0';

    if (length != rawLength) {
        chars = EXPAND_ARRAY(char, chars, rawLength + 1, length + 1);
    }

    valueEmitter(OBJECT_VALUE(genString(chars, length)));
    return;
}

static void grouping (bool assignable) {
    expression();
    forceConsume(T_R_PAR, "Expected ')' at end of expression.");
    return;
}

static void unary (bool assignable) {
    TType opType = parser.prev.type;

    precedence(P_UNARY);

    switch (opType) {
        case T_NOT: byteEmitter(SIG_NOT); break;
        case T_MINUS: byteEmitter(SIG_NEG); break;
        default: return;
    }
    return;
}

// 'Pixel[n]' allocates n packed elements - 'Pixel' alone is the layout itself.
// the layout is loaded through its global binding rather than baked in as a
// constant, so rebinding the name is honoured like any other name in the language
static void formName (Token name) {
    bool instantiable = formInstantiable;
    formInstantiable = false;

    constEmitter(SIG_GLOBAL_RETURN, SIG_GLOBAL_RETURN_16, identifier(&name));

    if (match(T_L_BRACK)) {
        expression();
        forceConsume(T_R_BRACK, "Expected ']' after buffer size.");
        byteEmitter(SIG_ALLOCATE);
        return;
    }

    // 'def v <- Vec4.' is a single instance - a buffer of one
    if (instantiable && check(T_PERIOD)) {
        valueEmitter(NUMERAL_VALUE(1));
        byteEmitter(SIG_ALLOCATE);
    }

    return;
}

static void variable (bool assignable) {
    Token name = parser.prev;

    // a local or a parameter shadows a declared layout - resolution order is
    // the same for a form name as it is for every other name
    if (findLocality(current, &name) == -1 && findForm(&name) != NULL) {
        formName(name);
        return;
    }

    variableName(name, assignable);
}

// resolves '::field' against the buffer and index already staged on the stack
static void fieldAccess (bool assignable) {
    forceConsume(T_ID, "Expected a field name after '::'.");
    int field = identifier(&parser.prev);

    if (assignable && match(T_ASSIGN)) {
        expression();
        constEmitter(SIG_MEMBER_ASSIGN, SIG_MEMBER_ASSIGN_16, field);
        return;
    }

    constEmitter(SIG_MEMBER_RETURN, SIG_MEMBER_RETURN_16, field);
    return;
}

static void indexed (bool assignable) {
    expression();
    forceConsume(T_R_BRACK, "Expected ']' after a buffer index.");
    forceConsume(T_MEMBER, "Expected '::' after a buffer index.");
    fieldAccess(assignable);
    return;
}

static void member (bool assignable) {
    // an unindexed member addresses the first element
    valueEmitter(NUMERAL_VALUE(0));
    fieldAccess(assignable);
    return;
}

static void binary (bool assignable) {
    TType opType = parser.prev.type;
    ParseRule* rule = getRule(opType);
    precedence((Precedence)(rule->precedence + 1));

    switch (opType) {
        case T_INEQ:    emitBytes(OP_EQUAL_TO, SIG_NOT); break;
        case T_EQEQ:    byteEmitter(OP_EQUAL_TO); break;
        case T_GREATER: byteEmitter(OP_GREATER_THAN); break;
        case T_GTOE:    emitBytes(OP_LESS_THAN, SIG_NOT); break;
        case T_LESSER:  byteEmitter(OP_LESS_THAN); break;
        case T_LTOE:    emitBytes(OP_GREATER_THAN, SIG_NOT); break;
        case T_PLUS:    byteEmitter(SIG_ADD); break;
        case T_MINUS:   byteEmitter(SIG_SUB); break;
        case T_STAR:    byteEmitter(SIG_MULT); break;
        case T_WHACK:   byteEmitter(SIG_DIV); break;
        case T_MOD:     byteEmitter(SIG_MOD); break;
        default: return;
    }
    return;
}

static uint8_t arguments() {
    uint8_t args = 0;
    if (!check(T_PERIOD)) {
        do { 
            expression();
            if (args >= 255) {
                prevErr("255 Argument Limit Exceeded.");
            }
            args++; 
        } while (match(T_COMMA));
    }
    return args;
}

static void call (bool assignable) {
    uint8_t args = arguments();
    emitBytes(SIG_CALL, args);
}

// the include set and its buffers belong to one compilation and no longer
static void releaseIncludes () {
    for (int i = 0; i < includedCount; i++) {
        // the root file owns no buffer here - it belongs to whoever read it
        if (included[i].source == NULL) { continue; }

        FREE_ARRAY(char, included[i].source, included[i].bytes);
    }

    includedCount = 0;
    return;
}

OOperation* compile(const char* source, const char* path) {
    Compiler compiler;
    char canonical[PATH_MAX];

    initScanner(source, path);
    releaseIncludes();

    // the root counts as included, so a file that includes itself is skipped
    if (realpath(path, canonical) != NULL) {
        included[includedCount].source = NULL;
        included[includedCount].bytes = 0;
        included[includedCount].path = copyString(canonical, (int)strlen(canonical))->chars;
        includedCount++;
    }

    initCompiler(&compiler, TYPE_SCRIPT);

    parser.erroneous = false;
    parser.panic = false;

    stepThrough();

    while (!match(T_EOF)) {
        declaration();
    }
 
    OOperation* operation = closeCompilation();

    // every token pointer is dead once parsing is done, so the sources can go
    releaseIncludes();

    return parser.erroneous ? NULL : operation;
}