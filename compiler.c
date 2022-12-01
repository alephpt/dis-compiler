#include <stdio.h>
#include <stdlib.h>
#include "header.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
    Token prev;
    Token current;
    bool erroneous;
    bool panic;
} Parser;

Parser parser;
Sequence* compilingSequence;

static Sequence* currentSequence() {
    return compilingSequence;
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

static void byteEmitter(uint8_t byte) {
    writeSequence(currentSequence(), byte, parser.prev.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    byteEmitter(byte1);
    byteEmitter(byte2);
}

static void perspective() {
    parser.prev = parser.current;

    for (;;) {
        parser.current = scanToken();
        
        if (parser.current.type != T_ERR) { break; }

        currentErr(parser.current.start);
    }
}

static void expression() {

}

static void consumption(TType t, const char* message) {
    if (parser.current.type = t) {
        perspective();
        return;
    }

    currentErr(message);
}

static void returnEmitter() {
    byteEmitter(SIG_RETURN);
}

static void closer() {
    returnEmitter();
}

bool compile(const char* source, Sequence* sequence) {
    int line = -1;
    initScanner(source);

    compilingSequence = sequence;

    parser.erroneous = false;
    parser.panic = false;

    perspective();
    expression();
    consumption(T_EOF, "Expected End of Expression.");
    closer();
    return !parser.erroneous;
}