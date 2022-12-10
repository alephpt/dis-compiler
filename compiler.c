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
    P_RETURN,
    P_EXECUTE,
    P_OR,
    P_AND,
    P_EQUALS,
    P_COMPARE,
    P_TERM,
    P_FACTOR,
    P_UNARY,
    P_CALL,
    P_PRIMARY
} Precedence;

typedef struct {
    PType prefix;
    PType infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int d;
} LocalT;

typedef struct {
    LocalT locals[UINT8_COUNT];
    int local_N;
    int local_D;
} Compiler;

Parser parser;
Compiler* current = NULL;
Sequence* compilingSequence;
static void expression();
static void declaration();
static void statement();
static ParseRule* getRule(TType type);
static void precedence(Precedence precede);
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
    [T_EXECUTE]          =             {NULL,          NULL,       P_EXECUTE},
    [T_LOG]              =             {NULL,          NULL,       P_NONE},
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
    [T_AND_OP]           =             {NULL,          binary,     P_NONE},
    [T_OR_OP]            =             {NULL,          binary,     P_NONE},
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
    compiler->local_N = 0;
    compiler->local_D = 0;
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
}

static void currentErr(const char* message) { err(&parser.current, message); }
static void prevErr(const char* message) { err(&parser.prev, message); }
static void byteEmitter(uint8_t byte) { writeSequence(currentSequence(), byte, parser.prev.line); }
static void emitBytes(uint8_t byte1, uint8_t byte2) { byteEmitter(byte1); byteEmitter(byte2); }

static uint8_t genValue(Value val) {
    int value = addValue(currentSequence(), val);

    if (value > UINT8_MAX) {
        prevErr("Too many values in one chunk.");
        return 0;
    }

    return (uint8_t)value;
}

static void valueEmitter(Value value) { emitBytes(OP_VALUE, genValue(value)); }

static void perception() {
    parser.prev = parser.current;

    for (;;) {
        parser.current = scanToken();
        
        if (parser.current.type != T_ERR) { break; }

        currentErr(parser.current.start);
    }
}

static bool check (TType type) { return parser.current.type == type; }

static bool match (TType type) {
    if (!check(type)) { return false; }
    
    perception();
    return true;
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

        perception();
    }
}

static void precedence(Precedence precede) {
    perception();

    PType prefix = getRule(parser.prev.type)->prefix;

    if (prefix == NULL) {
        prevErr("Expression expected.");
        return;
    }

    bool assignable = precede <= P_ASSIGN; 
    prefix(assignable);

    while (precede <= getRule(parser.current.type)->precedence) {
        perception();
        PType infix = getRule(parser.prev.type)->infix;
        infix(assignable);
    }

    if (assignable && match(T_ASSIGN)) {
        prevErr("Invalid assignment target.");
    }
}

static ParseRule* getRule (TType type) { return &rules[type]; }


static void expression () {
    precedence(P_ASSIGN);
    return;
}

static void consumption (TType t, const char* message) {
    if (parser.current.type == t) {
        perception();
        return;
    }

    currentErr(message);
    return;
}

static void segment () {
    while (!check(T_CLOSE) && !check(T_EOF)) {
        declaration();
    }

    consumption(T_CLOSE, "Expected closing '^' at end of segment.");
}

static void printStatement () {
    consumption(T_EXECUTE, "Expected '->' after 'log' statement.");
    expression();
    consumption(T_PERIOD, "Expected '.' after value.");
    byteEmitter(SIG_PRINT);
}

static void expressionStatement () {
    expression();
    consumption(T_PERIOD, "Expected '.' at the end of the expression.");
    byteEmitter(SIG_POP);
    return;
}

static void beginScope () {
    current->local_D++; 
    return;
}

static void endScope () {
    current->local_D--;

    while (current->local_N > 0 &&
           current->locals[current->local_N - 1].d > current->local_D) {
            byteEmitter(SIG_POP);
            current->local_N--;
    }

    return;
}

static void statement () {
    if (match(T_LOG)) { printStatement(); } 
    else if (match(T_OPEN)) {
        beginScope();
        segment();
        endScope();
    }
    else { expressionStatement(); }
    return;
}

static uint8_t identifier (Token* name) {
    printf("hit ident in parse");

    return genValue(OBJECT_VALUE(copyString(name->start, name->length)));
}

static void scopeLocalDefinition (Token name) {
    if (current->local_N == UINT8_COUNT) {
        prevErr("Local variable limit exceeded.");
        return;
    }

    LocalT* local = &current->locals[current->local_N++];
    local->name = name;
    local->d = -1;
}

static bool identifiersMatch(Token* name, Token* localName) {
    printf("idmatch - name: %.*s - lName: %.*s", name->start, name->length, localName->start, localName->length);
    if (localName->length != name->length) { return false; }
    return memcmp(name->start, localName->start, name->length) == 0;
}

static void declareDefinition () {
    if (current->local_D == 0) { return; }

    Token* name = &parser.prev;

    for (int i = current->local_N - 1; i >= 0; i--) {
        printf("declare - local.i: %d\n", i);

        LocalT* local = &current->locals[i];
        printf("declare - local.name: %.*s\n", &local->name.start, &local->name.length);


        if (local->d != -1 && local->d < current->local_D) {
            break;
        }

        if (identifiersMatch(name, &local->name)) {
            prevErr("Variable with that name already exists in the same scope.");
        }
    }

    scopeLocalDefinition(*name);
}

static uint8_t parseDefinition (const char* message) {
    consumption(T_ID, message);

    declareDefinition();
    if (current->local_D > 0) { return 0; }

    return identifier(&parser.prev);
}

static void initializeDefinition () {
    current->locals[current->local_N - 1].d = current->local_D;
}

static void define (uint8_t global) {
    if (current->local_D > 0) {
        initializeDefinition();
        return;
    }
    // TODO: implement check for global keyword and resort
    emitBytes(OP_GLOBAL, global);
}

static void definition () {
    uint8_t global = parseDefinition("Expected variable name.");

    if (match(T_ASSIGN)) {
        expression();
    } else {
        // TODO: Implement 'DEAD' type;
        byteEmitter(OP_NONE);
    }

    consumption(T_PERIOD, "Expected '.' after Variable declaration.");

    define(global);
    return;
}
 
static void declaration () {
    if (match(T_DEFINE)) {
        definition();
    } else {
        statement();
    }

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

uint8_t findLocality(Compiler* c, Token* name) {
    for (int i = c->local_N - 1; i >= 0; i--) {
        LocalT* local = &c->locals[i];

        if (identifiersMatch(name, &local->name)) { 
            if (local->d == -1) {
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

static void variable (bool assignable) {
    variableName(parser.prev, assignable);
}

static void grouping (bool assignable) {
    expression();
    consumption(T_R_PAR, "Expected ')' at end of expression.");
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

static void binary () {
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

static void returnEmitter() { byteEmitter(SIG_RETURN); return; }
static void closer() { returnEmitter(); return; }

bool compile(const char* source, Sequence* sequence) {
    Compiler compiler;
    int line = -1;

    initScanner(source);
    initCompiler(&compiler);

    compilingSequence = sequence;

    parser.erroneous = false;
    parser.panic = false;

    perception();

    while (!match(T_EOF)) {
        declaration();
    }
 
    closer();
    return !parser.erroneous;
}