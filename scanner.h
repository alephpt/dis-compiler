#ifndef dis_scanner_h
#define dis_scanner_h

typedef enum {
    // Functional Operands
    T_L_PAR,            // (                        - opening order of operations
    T_R_PAR,            // )                        - closing order of operations
    T_L_BRACK,          // [                        - opening indexing
    T_R_BRACK,          // ]                        - closing indexing
    T_L_BRACE,          // {                        - opening form body
    T_R_BRACE,          // }                        - closing form body
    T_COMMA,            // ,                        - seperated sequence of items
    T_REF,              // &                        - address
    T_DEREF,            // @                        - access variable at address
    T_HASH,             // #                        - used to define includes and preprocessor operations
    T_BODY_START,       // :                        - opens body of function
    T_UNDER,            // _                        - blank, default, any
    T_LINE_END,         // .                        - ends statement
    T_BODY_END,         // ~                        - closes body
    T_RETURN,           // "^" || "return" 
    T_LOG,              // "log"                    - print

    // Arithmetic Operators
    T_PLUS,             // +                        - arithmetic add
    T_MINUS,            // -                        - arithmetic subtract
    T_STAR,             // *                        - arithmetic multiply
    T_WHACK,            // /                        - arithmetic divided
    T_MOD,              // %                        - modulos
    T_POWER,            // **                       - arithmetic power
    T_PLUSPLUS,         // ++                       - arithmetic + 1
    T_MINUSMINUS,       // --                       - arithmetic - 1
    T_PLUS_EQ,          // +=                       - add right to left and store in left
    T_MINUS_EQ,         // -=                       - subtract right from left
    T_EQ_PLUS,          // =+                       - add left to right, and store in right
    T_EQ_MINUS,         // =-                       - subtract left from right, and store in right

    // Logical Operators
    T_AND_OP,           // &&                       - logical and
    T_OR_OP,            // ||                       - logical or
    T_GREATER,          // >                        - logical comparison or vector
    T_LESSER,           // <                        - logical comparison or vector
    T_GTOE,             // >=                       - logical comparison ( right side fixed )
    T_EOGT,             // =>                       - logical comparison ( left side fixed )
    T_LTOE,             // <=                       - logical comparison ( right side fixed )
    T_EOLT,             // =<                       - logical comparison ( left side fixed )
    T_EQEQ,             // ==                       - logical equality
    T_INEQ,             // !=                       - logical inequality
    T_NOT,              // !                        - negation

    // Operational Operands
    T_L_ASSIGN,         // <-                       - insert / apply right into left
    T_R_ASSIGN,         // ->                       - insert / apply left to right
    T_L_OUT,            // <<                       - pipe right into left ?
    T_R_OUT,            // >>                       - pipe left into right ?
    T_COMMENT,          // //                       - comment
    T_DOLLAR,           // $                        - type cast / result
    T_BWHACK,           // \                        - escape
    T_BITWISE,          // |                        - binary bitwise
    T_QUEST,            // ?                        - ternary operator
    T_INDEX,            // ::                       - allows access to member to index
    
    // Declarative Keywords
    T_DEFINE,           // "def" || "define"        - declare variable 
    T_INCLUDE,          // "include"                - dependency inclusion 
    T_PILOT,            // "pilot"                  - used to define type with unknown members/functions 
    T_PARENT,           // "parent"                 - used to access parent level scope
    T_GLOBAL,           // "global"                 - used to declare global scoping   
    T_SELF,             // "self"                   - used to refer to parent scope
    T_THIS,             // "this"                   - reference to a member of local scope
    T_PUBLIC,           // "public"                 - allows object members to be public
    T_PRIVATE,          // "private"                - declares object members to be private 
    T_OP,               // "op" || "operation"
    T_OBJ,              // "obj" || "object"
    T_ENUM,             // "enum" || "enumeration"  
    T_FORM,             // "form"                   - template, can be used as a dictionary, or used with pilots
    T_PAIR,             // "pair"                   - single kv pair
    T_STRING,           // type of char[]  
    T_BINARY,           // type of number         
    T_DECIMAL,          // type of number
    T_OCTAL,            // type of number
    T_HEXADECIMAL,      // type of number
    T_ID,               // type identifier          - variable name
    T_AS,               // "as"                     - for loop
    T_WHILE,            // "while"
    T_WHEN,             // "when"                   - 'if'
    T_OR,               // "or"                     - 'else if'
    T_ELSE,             // "else"                   - final 'else'
    T_NONE,             // NONE
    T_TRUE,             // "true"
    T_FALSE,            // "false"

    // Extra Operands
    T_EOF,              // End Of File
    T_ERR,              // Token Error
    T_SEMIC,            // ;                        - unused
    T_EQ                // =                        - unused
} TType;

typedef struct {
    TType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif