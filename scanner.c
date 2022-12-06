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
static bool isHexadecimal (char c) { return (c >= '0' && c <= '9') || 
                                            (c >= 'a' && c <= 'f') ||
                                            (c >= 'A' && c <= 'F'); }

static bool isAlpha (char c) { return (c >= 'a' && c <= 'z') ||
                                      (c >= 'A' && c <= 'Z') ||
                                       c == '_';}

static TType checkWord(int s, int l, const char* rem, TType t) {
    if (scanner.current - scanner.start == s + l &&
        memcmp(scanner.start + s, rem, l) == 0) {
            return t;
        }
    
    return T_ID;
}

static TType iType() {
    switch (scanner.start[0]) {
        case 'a': return checkWord(1, 1, "s", T_AS);
        case 'e': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': return checkWord(2, 2, "um", T_ENUM);
                    case 'l': return checkWord(2, 2, "se", T_ELSE);
                }
            }
        case 'f': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkWord(2, 3, "lse", T_FALSE);
                    case 'o': return checkWord(2, 2, "rm", T_FORM);
                }
            }
        case 'g': return checkWord(1, 5, "lobal", T_GLOBAL);
        case 'i': return checkWord(1, 6, "nclude", T_INCLUDE);
        case 'l': return checkWord(1, 2, "og", T_LOG);
        case 'n': return checkWord(1, 3, "one", T_NONE);
        case 'N': return checkWord(1, 3, "ONE", T_NONE);
        case 'o': 
            if (scanner.current - scanner.start == 2) {
                switch (scanner.start[1]) { 
                    case 'p': return T_OP;
                    case 'r': return T_OR;
                }
            } else
            if (scanner.current - scanner.start > 2) {
                switch (scanner.start[1]) {
                    case 'b': 
                        if (scanner.current - scanner.start > 3) {
                            return checkWord(2, 4, "ject", T_OBJ);
                        } else 
                        if (scanner.start[2] == 'j') {
                            return T_OBJ;
                        }
                    case 'p': return checkWord(2, 7, "eration", T_OP);
                }
            }
        case 'p':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'r': return checkWord(3, 3, "ent", T_PARENT);
                                case 'i': return checkWord(3, 1, "r", T_PAIR);
                            }
                        }
                    case 'i': return checkWord(2, 3, "lot", T_PILOT);
                    case 'r': return checkWord(2, 5, "ivate", T_PRIVATE);
                    case 'u': return checkWord(2, 4, "blic", T_PUBLIC);
                }
            }
        case 'r': return checkWord(1, 5, "eturn", T_RETURN);
        case 's': return checkWord(1, 3, "elf", T_SELF);
        case 't': 
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkWord(2, 2, "is", T_THIS);
                    case 'r': return checkWord(2, 2, "ue", T_TRUE);
                }
            }
        case 'w':
            if (scanner.current - scanner.start > 3) {
                if (scanner.start[1] == 'h') {
                    switch (scanner.start[2]) {
                        case 'e': return checkWord(3, 1, "n", T_WHEN);
                        case 'i': return checkWord(3, 2, "le", T_WHILE);
                    }
                }
            }
    }
    return T_ID;
}

static Token identify() {
    while (isAlpha(peek()) || isDecimal(peek())) { read_c(); }
    return genToken(iType());
}

static Token numeral (char n) {
    if (n == '0') {
        switch(peek()) {
            case 'b': // binary
                read_c();
                scanner.start = scanner.current;
                while (isBinary(peek())) { read_c(); }
                return genToken(T_BINARY);
            case 'c': // octal
                read_c();
                scanner.start = scanner.current;
                while (isOctal(peek())) { read_c(); }
                return genToken(T_OCTAL);
            case 'x': // hexadecimal
                read_c();
                scanner.start = scanner.current;
                while (isHexadecimal(peek())) { read_c(); }
                return genToken(T_HEXADECIMAL);
        }
    }

    while(isDecimal(peek())) { read_c(); }

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

    if (ended()) return errToken("Unterminated string.");

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
    // consume blank spaces
    skipBlanks();

    scanner.start = scanner.current;
    if (ended()) return genToken(T_EOF);

    // read a char
    char c = read_c();

    // if it's _named or named but not _ 
    if ((c == '_' && isAlpha(peek())) ||
        (isAlpha(c) && c != '_')) return identify();

    // check for numbers
    if (isDecimal(c)) return numeral(c);

    // look for specific characters
    switch (c) {
        //case '': return genToken()
        case '(': return genToken(T_L_PAR);
        case ')': return genToken(T_R_PAR);
        case '[': return genToken(T_L_BRACK);
        case ']': return genToken(T_R_BRACK);
        case '{': return genToken(T_L_BRACE);
        case '}': return genToken(T_L_BRACE);
        case '.': return genToken(T_PERIOD);
        case ',': return genToken(T_COMMA);
        case ':': return genToken( match(':') ? T_MEMBER : T_PARAM_END );
        case '+': return genToken(T_PLUS);
        case '-': return genToken(T_MINUS);
        case '*': return genToken(T_STAR);
        case '/': return genToken(T_WHACK);
        case '_': return genToken(T_UNDER);
        case '?': return genToken(T_QUEST);
        case '$': return genToken(T_OPEN);
        case '^': return genToken(T_CLOSE);
        case '!': return genToken( match('=') ? T_INEQ : T_NOT );
        case '=': return genToken( match('=') ? T_EQEQ : T_EQ );
        case '<': return genToken( match('=') ? T_LTOE : T_LESSER );
        case '>': return genToken( match('=') ? T_GTOE : T_GREATER );
        case '"': return string();
    }

    return errToken("Unidentified character.");
}