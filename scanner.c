#include <stdio.h>
#include <string.h>
#include "header.h"
#include "scanner.h"

// one file being scanned. an include suspends the current one and resumes it
// exactly where it left off once the nested source runs out
typedef struct {
    const char* start;
    const char* current;
    int line;
    const char* file;
} Source;

typedef struct {
    const char* start;
    const char* current;
    int line;
    const char* file;
    Source parents[INCLUDE_DEPTH_MAX];
    int depth;
} Scanner;

Scanner scanner;

void initScanner (const char* source, const char* file) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.file = file;
    scanner.depth = 0;
}

const char* scannerFile () { return scanner.file; }

bool pushSource (const char* source, const char* file) {
    if (scanner.depth == INCLUDE_DEPTH_MAX) { return false; }

    Source* parent = &scanner.parents[scanner.depth++];
    parent->start = scanner.start;
    parent->current = scanner.current;
    parent->line = scanner.line;
    parent->file = scanner.file;

    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.file = file;
    return true;
}

static void popSource () {
    Source* parent = &scanner.parents[--scanner.depth];
    scanner.start = parent->start;
    scanner.current = parent->current;
    scanner.line = parent->line;
    scanner.file = parent->file;
    return;
}

static Token genToken (TType type) {
    Token t;

    t.type = type;
    t.start = scanner.start;
    t.length = (int)(scanner.current - scanner.start);
    t.line = scanner.line;
    t.file = scanner.file;

    return t;
} 

static Token errToken (const char* message) {
    Token t;

    t.type = T_ERR;
    t.start = message;
    t.length = (int)strlen(message);
    t.line = scanner.line;
    t.file = scanner.file;

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

// 'u8' 'u16' 'u32' 'u64' 'i8' 'i16' 'i32' 'i64' 'f32' 'f64' - form field widths
static TType checkWidth (int len) {
    const char* w = scanner.start;

    if (len == 2) {
        return (w[1] == '8' && (w[0] == 'u' || w[0] == 'i')) ? T_WIDTH : T_ID;
    }

    if (len != 3) { return T_ID; }

    if (memcmp(w + 1, "16", 2) != 0 &&
        memcmp(w + 1, "32", 2) != 0 &&
        memcmp(w + 1, "64", 2) != 0) { return T_ID; }

    if (w[0] == 'u' || w[0] == 'i') { return T_WIDTH; }

    // floats carry no 16 bit width
    return (w[0] == 'f' && w[1] != '1') ? T_WIDTH : T_ID;
}

// every path must resolve to a keyword or fall back to T_ID - never drop through
static TType iType() {
    int len = (int)(scanner.current - scanner.start);

    switch (scanner.start[0]) {
        case 'a': return checkWord(1, 1, "s", T_AS);
        case 'd':
            if (len == 3) { return checkWord(1, 2, "ef", T_DEFINE); }
            return checkWord(1, 5, "efine", T_DEFINE);
        case 'e':
            if (len < 4) { return T_ID; }
            switch (scanner.start[1]) {
                case 'n': return checkWord(2, 2, "um", T_ENUM);
                case 'l': return checkWord(2, 2, "se", T_ELSE);
                default: return T_ID;
            }
        case 'f':
            if (len == 3) { return checkWidth(len); }
            switch (scanner.start[1]) {
                case 'a': return checkWord(2, 3, "lse", T_FALSE);
                case 'o': return checkWord(2, 2, "rm", T_FORM);
                default: return T_ID;
            }
        case 'g': return checkWord(1, 5, "lobal", T_GLOBAL);
        case 'i':
            if (len < 4) { return checkWidth(len); }
            return checkWord(1, 6, "nclude", T_INCLUDE);
        case 'l': return checkWord(1, 2, "og", T_LOG);
        case 'n': return checkWord(1, 3, "one", T_NONE);
        case 'N': return checkWord(1, 3, "ONE", T_NONE);
        case 'o':
            if (len == 2) {
                switch (scanner.start[1]) {
                    case 'p': return T_OP;
                    case 'r': return T_OR;
                    default: return T_ID;
                }
            }
            switch (scanner.start[1]) {
                case 'b':
                    if (len == 3) { return checkWord(2, 1, "j", T_OBJ); }
                    return checkWord(2, 4, "ject", T_OBJ);
                case 'p': return checkWord(2, 7, "eration", T_OP);
                default: return T_ID;
            }
        case 'p':
            if (len < 4) { return T_ID; }
            switch (scanner.start[1]) {
                case 'a':
                    switch (scanner.start[2]) {
                        case 'r': return checkWord(3, 3, "ent", T_PARENT);
                        case 'i': return checkWord(3, 1, "r", T_PAIR);
                        default: return T_ID;
                    }
                case 'i': return checkWord(2, 3, "lot", T_PILOT);
                case 'r': return checkWord(2, 5, "ivate", T_PRIVATE);
                case 'u': return checkWord(2, 4, "blic", T_PUBLIC);
                default: return T_ID;
            }
        case 'r': return checkWord(1, 5, "eturn", T_RETURN);
        case 's': return checkWord(1, 3, "elf", T_SELF);
        case 't':
            if (len < 4) { return T_ID; }
            switch (scanner.start[1]) {
                case 'h': return checkWord(2, 2, "is", T_THIS);
                case 'r': return checkWord(2, 2, "ue", T_TRUE);
                default: return T_ID;
            }
        case 'u': return checkWidth(len);
        case 'w':
            if (len < 4) { return T_ID; }
            switch (scanner.start[1]) {
                case 'h':
                    switch (scanner.start[2]) {
                        case 'e': return checkWord(3, 1, "n", T_WHEN);
                        case 'i': return checkWord(3, 2, "le", T_WHILE);
                        default: return T_ID;
                    }
                case 'r': return checkWord(2, 3, "ite", T_WRITE);
                default: return T_ID;
            }
        default: return T_ID;
    }
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

static Token string(bool singleQ) {
    char comp;
    if (singleQ) {
        comp = '\'';
    } else {
        comp = '"';
    }
    while (peek() != comp && !ended()) {
        // an escaped character never closes the string - the escape itself is
        // resolved later, at compile time
        if (peek() == '\\' && peekNext() != '\0') {
            if (peekNext() == '\n') { scanner.line++; }
            read_c();
        } else
        if (peek() == '\n') {
            return errToken("Unterminated string.");
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
                    break;
                } else
                if (peekNext() == '*') {
                    // block comment
                    read_c();
                    read_c();

                    while (!ended() && !(peek() == '*' && peekNext() == '/')) {
                        if (peek() == '\n') { scanner.line++; }
                        read_c();
                    }

                    if (!ended()) { read_c(); read_c(); }
                    break;
                }
                return;
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

    // running out of an included file just returns us to the one that included
    // it - only the root source can produce end of file
    while (*scanner.current == '\0' && scanner.depth > 0) {
        popSource();
        skipBlanks();
        scanner.start = scanner.current;
    }

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
        case '}': return genToken(T_R_BRACE);
        case '.': return genToken(T_PERIOD);
        case ',': return genToken(T_COMMA);
        case ':': return genToken( match(':') ? T_MEMBER : T_PARAM_END );
        case '+': return genToken( match('+') ? T_INCREMENT : match('=') ? T_PLUS_EQ : T_PLUS);
        // '->' must be tested first - it is the call operator, not a minus
        case '-': return genToken( match('>') ? T_EXECUTE : match('-') ? T_DECREMENT : match('=') ? T_MINUS_EQ : T_MINUS);
        case '*': return genToken(T_STAR);
        case '/': return genToken(T_WHACK);
        case '%': return genToken(T_MOD);
        case '_': return genToken(T_UNDER);
        case '?': return genToken(T_QUEST);
        case '$': return genToken(T_OPEN);
        case '^': return genToken(T_CLOSE);
        case '!': return genToken( match('=') ? T_INEQ : T_NOT );
        case '=': return genToken( match('=') ? T_EQEQ : T_EQ );
        case '<': return genToken( match('=') ? T_LTOE : match('-') ? T_ASSIGN : T_LESSER );
        case '>': return genToken( match('=') ? T_GTOE : T_GREATER );
        case '@': return genToken(T_DEREF);
        case '&': return genToken( match('&') ? T_AND_OP : T_REF );
        case '|': return genToken( match('|') ? T_OR_OP : T_BITWISE );
        case '\'': return string(true);
        case '"': return string(false);
    }

    return errToken("Unidentified character.");
}