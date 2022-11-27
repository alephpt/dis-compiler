#ifndef dis_scanner_h
#define dis_scanner_h

typedef enum {
    // One Char Tokens
    L_PAR, R_PAR, L_BRACK, R_BRACK, L_BRACE, R_BRACE, 
    COMMA, LINE_END, SEMIC, WHACK, BWHACK, 
    QUEST, DOLLAR, HASH, NOT, BODY_END,

    // One or Two Char Tokens
    COLON, INDEX,
    STAR, POWER,
    REF, AND_OP, BODY_START, OR_OP,
    EQ, EQEQ, INEQ, EQ_PLUS, EQ_MINUS, 
    L_ASSIGN, R_ASSIGN, L_OUT, R_OUT,
    GREATER, LESSER, GREAT_EQ, LESS_EQ, 
    PLUS, PLUSPLUS, PLUS_EQ, MINUS, MINUSMINUS, MINUS_EQ,

    // Literals
    PARENT, GLOBAL,
    IDENTIFIER, STRING, NUMERAL,

    // Keywords
    WHEN, OR, ELSE, AS, WHILE,
    TRUE, FALSE, SELF, PILOT, PUBLIC, PRIVATE, NONE,
    OP, OBJ, FORM, ENUM, PAIR, LOG, RETURN, THIS, INCLUDE, DEFINE, 

    EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

initScanner(const char* source);

#endif