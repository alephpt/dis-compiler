#include <stdio.h>
#include <string.h>
#include "header.h"
#include "scanner.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner (const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static Token genToken (TType type) {
    Token t;

    t.type = type;
    t.start = scanner.start;
    t.length = (int)(scanner.current - scanner.start);
    t.line = scanner.line;

    return t;
} 

static Token errToken (const char* message) {
    Token t;

    t.type = T_ERR;
    t.start = message;
    t.length = (int)strlen(message);
    t.line = scanner.line;
    
    return t;
}

static bool ended () { return *scanner.current == '\0'; }
static char read_c () { scanner.current++; return scanner.current[-1]; }
static char peek () { return *scanner.current; }
static char peekNext () { return (ended() ? '\0' : scanner.current[1]); }

static bool isBinary (char c) { return c =='0' || c == '1'; }
static bool isOctal (char c) { return c >= '0' && c <= '7'; }
static bool isDecimal (char c) { return c >= '0' && c <= '9'; }
static bool isHexadecimal (char c) { return c >= '0' && c <= '9' || 
                                            c >= 'a' && c <= 'f' ||
                                            c >= 'A' && c <= 'F'; }

static Token numeral (char n) {
    if (n == '0') {
        switch(peek()) {
            case 'b': // binary
                while (isBinary(peek())) { read_c(); }
                return genToken(T_BINARY);
            case 'c': // octal
                while (isOctal(peek())) { read_c(); }
                return genToken(T_OCTAL);
            case 'x': // hexadecimal
                while (isHexadecimal(peek())) { read_c(); }
                return genToken(T_HEXADECIMAL)
        }
    }

    while(isDecimalal(peek())) { read_c(); }

    if(peek() == '.' && isDecimal(peekNext())) {
        read_c();

        while(isDecimal(peek())) { read_c(); }
    }

    return genToken(T_DECIMAL);
}

static Token string() {
    while (peek() != '"' && !ended()) {
        if (peekNext() == '\n') { 
            if (peek() == '\\') {
                read_c();
                scanner.line++; 
            } else {
                return errToken("Unterminated string.");
            }
        }
        read_c();
    }

    if (ended()) return errToken("Unterminated string.")

    read_c();
    return genToken(T_STRING);
}

static void skipBlanks () {
    for (;;) {
        char c = peek();
        
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                read_c();
                break;
            case '\n':
                scanner.line++;
                read_c();
                break;
            case '/':
                if (peekNext() == '/') {
                    // single line comment
                    while (peek() != '\n' && !ended()) { read_c(); }
                } else 
                if (peekNext() == '*') {
                    // block comment
                    while (peek() != '*' && peekNext() != '\\' && !ended()) { read_c(); }
                }
            default:
                return;
        }
    }
}

static bool match (char exp) {
    if (ended() || *scanner.current != exp) {
        return false;
    }
    scanner.current++;
    return true;
}

Token scanToken () {
    skipBlanks();
    scanner.start = scanner.current;

    if (ended()) return genToken(T_EOF);

    char c = read_c();

    if (isDecimal(c)) return numeral();

    switch (c) {
        //case '': return genToken()
        case '(': return genToken(T_L_PAR);
        case ')': return genToken(T_R_PAR);
        case '[': return genToken(T_L_BRACK);
        case ']': return genToken(T_R_BRACK);
        case '{': return genToken(T_L_BRACE);
        case '}': return genToken(T_L_BRACE);
        case '.': return genToken(T_LINE_END);
        case ',': return genToken(T_COMMA);
        case ':': return genToken(T_BODY_START);
        case '~': return genToken(T_BODY_END);
        case '+': return genToken(T_PLUS);
        case '-': return genToken(T_MINUS);
        case '*': return genToken(T_STAR);
        case '/': return genToken(T_WHACK);
        case '!': return genToken( match('=') ? T_INEQ : T_NOT );
        case '=': return genToken( match('=') ? T_EQEQ : T_EQ );
        case '<': return genToken( match('=') ? T_LTOE : T_LESSER );
        case '>': return genToken( match('=') ? T_GTOE : T_GREATER );
        case '"': return string();
    }

    return errToken("Unidentified character.");
}