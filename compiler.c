#include <stdio.h>
#include <stdlib.h>
#include "header.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

typedef void (*PType)();

typedef struct {
    Token prev;
    Token current;
    bool erroneous;
    bool panic;
} Parser;

typedef enum {
    P_NONE,
    P_ASSIGN,
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

Parser parser;
Sequence* compilingSequence;
static void expression();
static ParseRule* getRule(TType type);
static void precedence(Precedence precede);
static void numeral();
static void string();
static void grouping();
static void unary();
static void binary();
static void literal();

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
    [T_LOG]              =             {NULL,          NULL,       P_NONE},
    [T_MINUS]            =             {unary,         binary,     P_TERM},
    [T_PLUS]             =             {NULL,          binary,     P_TERM},
    [T_WHACK]            =             {NULL,          binary,     P_FACTOR},
    [T_STAR]             =             {NULL,          binary,     P_FACTOR},
    [T_MOD]              =             {NULL,          NULL,       P_NONE},
    [T_POWER]            =             {NULL,          NULL,       P_NONE},
    [T_PLUSPLUS]         =             {NULL,          NULL,       P_NONE},
    [T_MINUSMINUS]       =             {NULL,          NULL,       P_NONE},
    [T_PLUS_EQ]          =             {NULL,          binary,       P_NONE},
    [T_MINUS_EQ]         =             {NULL,          binary,       P_NONE},
    [T_EQ_PLUS]          =             {NULL,          binary,       P_NONE},
    [T_EQ_MINUS]         =             {NULL,          binary,       P_NONE},
    [T_AND_OP]           =             {NULL,          binary,       P_NONE},
    [T_OR_OP]            =             {NULL,          binary,       P_NONE},
    [T_GREATER]          =             {NULL,          binary,     P_COMPARE},
    [T_LESSER]           =             {NULL,          binary,     P_COMPARE},
    [T_GTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_LTOE]             =             {NULL,          binary,     P_COMPARE},
    [T_EQEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_INEQ]             =             {NULL,          binary,     P_EQUALS},
    [T_NOT]              =             {unary,         NULL,       P_NONE},
    [T_L_ASSIGN]         =             {NULL,          NULL,       P_NONE},
    [T_R_ASSIGN]         =             {NULL,          NULL,       P_NONE},
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
    [T_ID]               =             {NULL,          NULL,       P_NONE},
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

static void precedence(Precedence precede) {
    perception();

    PType prefix = getRule(parser.prev.type)->prefix;

    if (prefix == NULL) {
        prevErr("Expression expected.");
        return;
    }

    prefix();

    while (precede <= getRule(parser.current.type)->precedence) {
        perception();
        PType infix = getRule(parser.prev.type)->infix;
        infix();
    }
}

static ParseRule* getRule(TType type) { return &rules[type]; }

static void expression() {
    precedence(P_ASSIGN);
    return;
}

static void consumption (TType t, const char* message) {
    if (parser.current.type = t) {
        perception();
        return;
    }

    currentErr(message);
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

static void numeral () {
    double value = strtod(parser.prev.start, NULL);
    valueEmitter(NUMERAL_VALUE(value));
    return;
}

static void string () {
    valueEmitter(OBJECT_VALUE(copyString(parser.prev.start + 1, parser.prev.length - 2)));
    return;
}

static void grouping () {
    expression();
    consumption(T_R_PAR, "Expected ')' at end of expression.");
}

static void unary () {
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
    int line = -1;
    initScanner(source);

    compilingSequence = sequence;

    parser.erroneous = false;
    parser.panic = false;

    perception();
    expression();
    consumption(T_EOF, "Expected End of Expression.");
    closer();
    return !parser.erroneous;
}