#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "header.h"
#include "compiler.h"
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

typedef struct {
    LocalT locals[UINT8_COUNT];
    int localCount;
    int localScope;
} Compiler;

Parser parser;
Compiler* current = NULL;
Sequence* compilingSequence;
static void expression();
static void declaration();
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

ParseRule rules[] = {
    [T_L_PAR]            =             {grouping,      NULL,       P_NONE},
    [T_R_PAR]            =             {NULL,          NULL,       P_NONE},
    [T_L_BRACK]          =             {NULL,          NULL,       P_NONE},
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
    [T_EXECUTE]          =             {NULL,          NULL,       P_NONE},
    [T_LOG]              =             {NULL,          NULL,       P_LOG},
    [T_MINUS]            =             {unary,         binary,     P_TERM},
    [T_PLUS]             =             {NULL,          binary,     P_TERM},
    [T_WHACK]            =             {NULL,          binary,     P_FACTOR},
    [T_STAR]             =             {NULL,          binary,     P_FACTOR},
    [T_MOD]              =             {NULL,          NULL,       P_NONE},
    [T_POWER]            =             {NULL,          NULL,       P_NONE},
    [T_INCREMENT]        =             {NULL,          NULL,       P_NONE},
    [T_DECREMENT]        =             {NULL,          NULL,       P_NONE},
    [T_PLUS_EQ]          =             {NULL,          binary,     P_NONE},
    [T_MINUS_EQ]         =             {NULL,          binary,     P_NONE},
    [T_EQ_PLUS]          =             {NULL,          binary,     P_NONE},
    [T_EQ_MINUS]         =             {NULL,          binary,     P_NONE},
    [T_AND_OP]           =             {NULL,          andComp,    P_AND},
    [T_OR_OP]            =             {NULL,          orComp,     P_OR},
    [T_GREATER]          =             {NULL,          binary,     P_COMPARE},
    [T_LESSER]           =             {NULL,          binary,     P_COMPARE},
    [T_GTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_LTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_EQEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_INEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_NOT]              =             {unary,         NULL,       P_NONE},
    [T_ASSIGN]           =             {NULL,          NULL,       P_ASSIGN},
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
    [T_MEMBER]           =             {NULL,          NULL,       P_NONE},
    [T_RETURN]           =             {NULL,          NULL,       P_NONE},
    [T_OP]               =             {NULL,          NULL,       P_NONE},
    [T_OBJ]              =             {NULL,          NULL,       P_NONE},
    [T_ENUM]             =             {NULL,          NULL,       P_NONE},
    [T_FORM]             =             {NULL,          NULL,       P_NONE},
    [T_PAIR]             =             {NULL,          NULL,       P_NONE},
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

static Sequence* currentSequence() { return compilingSequence; }

static void initCompiler (Compiler* compiler) {
    compiler->localCount = 0;
    compiler->localScope = 0;
    current = compiler;
    return;
}

static void err(Token* token, const char* message) {
    if (parser.panic) { return; }
    parser.panic = true;

    fprintf(stderr, "ERR - [line %d]:", token->line);

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

static void currentErr (const char* message) { err(&parser.current, message); return; }
static void prevErr (const char* message) { err(&parser.prev, message); return; }
static ParseRule* getRule (TType type) { return &rules[type]; }
static bool check (TType type) { return parser.current.type == type; }
static void byteEmitter (uint8_t byte) { writeSequence(currentSequence(), byte, parser.prev.line); return; }
static void emitBytes (uint8_t byte1, uint8_t byte2) { byteEmitter(byte1); byteEmitter(byte2); return; }
static void returnEmitter() { byteEmitter(SIG_RETURN); return; }
static void closer() { returnEmitter(); return; }

static uint8_t genValue (Value val) {
    int value = addValue(currentSequence(), val);

    if (value > UINT8_MAX) {
        prevErr("Too many values in one chunk.");
        return 0;
    }

    return (uint8_t)value;
}

static uint8_t identifier (Token* name) { 
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

static void valueEmitter (Value value) { emitBytes(OP_VALUE, genValue(value)); return; }

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
            case T_DEFINE:
            case T_AS:
            case T_WHEN:
            case T_WHILE:
            case T_LOG:
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
    uint8_t sigAssign, sigReturn;
    int var = findLocality(current, &name); 
    
    if (var != -1) {
        sigReturn = SIG_LOCAL_RETURN;
        sigAssign = SIG_LOCAL_ASSIGN;
    } else {
        var = identifier(&name);
        sigReturn = SIG_GLOBAL_RETURN;
        sigAssign = SIG_GLOBAL_ASSIGN;
    }

    if (assignable && match(T_ASSIGN)) {
        expression();
        emitBytes(sigAssign, (uint8_t)var);
    } else {
        emitBytes(sigReturn, (uint8_t)var);
    }
}

static void initializeDefinition () {
    current->locals[current->localCount - 1].scope = current->localScope;
    return;
}

static void defineVariable (uint8_t variable) {
    if (current->localScope > 0) {
        initializeDefinition();
        return;
    }
    // TODO: implement check for global keyword and resort
    emitBytes(OP_GLOBAL, variable);
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

static uint8_t parseDefinition (const char* message) {
    forceConsume(T_ID, message);
    
    declareDefinition();
    if (current->localScope > 0) { return 0; }
    
    return identifier(&parser.prev);
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

static void definition();

static void asStatement () {
    int exitLoop = -1;
    Token var;
    beginScope();
    forceConsume(T_COMMA, "Expected ',' after 'as'.");

    // define/declare variable
    if (match(T_L_BRACK)) {
        // nothing
    } else if (match(T_DEFINE)) {
        var = parser.current;
        definition();
        forceConsume(T_L_PAR, "Expected '[' before 'as' iterator.");
    } else {
        var = parser.current;
        expressionStatement();
        forceConsume(T_L_PAR, "Expected '[' before 'as' iterator.");
    }

    int beginLoop = currentSequence()->inventory;

    if (!match(T_R_PAR)) {
        int bodyJump = jumpEmitter(SIG_JUMP);
        int increment = currentSequence()->inventory;

        // determine iterator
        expression();
        byteEmitter(SIG_POP);
        
        forceConsume(T_R_PAR, "Expected ']' after 'as' iterator.");

        loopEmitter(beginLoop);
        beginLoop = increment;
        landJump(bodyJump);
    }

    // check comparisons
    if (!match(T_PARAM_END)) {
        // expression
        expression();
        forceConsume(T_PARAM_END, "Expected ':' after 'as' conditions.");
        exitLoop = jumpEmitter(SIG_EXECUTE);
        byteEmitter(SIG_POP);
    }

    
    // execute body
    statement();

    loopEmitter(beginLoop);

    if (exitLoop != -1) {
        landJump(exitLoop);
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

static void definition () {
    uint8_t variable = parseDefinition("Expected variable name.");

    if (match(T_ASSIGN)) {
        expression();
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
    if (match(T_AS)) { asStatement(); } else
    if (match(T_WHEN)) { whenStatement(); } else
    if (match(T_WHILE)) { whileStatement(); } else
    if (match(T_OPEN)) {
        beginScope();
        scope();
        endScope();
    }
    else { expressionStatement(); }
    return;
}

static void declaration () {
    if (match(T_DEFINE)) { definition(); } 
    else { statement(); }

    if (parser.panic) { rebase(); }
    return;
}

static void literal () {
    switch (parser.prev.type) {
        case T_FALSE: byteEmitter(OP_FALSE); break;
        case T_NONE: byteEmitter(OP_NONE); break;
        case T_TRUE: byteEmitter(OP_TRUE); break;
        default: return;
    }
    return;
}

static void numeral (bool assignable) {
    double value = strtod(parser.prev.start, NULL);
    valueEmitter(NUMERAL_VALUE(value));
    return;
}

static void string (bool assignable) {
    valueEmitter(OBJECT_VALUE(copyString(parser.prev.start + 1, parser.prev.length - 2)));
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

static void variable (bool assignable) {
    variableName(parser.prev, assignable);
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
        default: return;
    }
    return;
}


bool compile(const char* source, Sequence* sequence) {
    Compiler compiler;
    int line = -1;

    initScanner(source);
    initCompiler(&compiler);

    compilingSequence = sequence;

    parser.erroneous = false;
    parser.panic = false;

    stepThrough();

    while (!match(T_EOF)) {
        declaration();
    }
 
    closer();
    return !parser.erroneous;
}