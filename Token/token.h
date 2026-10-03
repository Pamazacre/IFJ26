#ifndef TOKEN_H
#define TOKEN_H


typedef enum {
    // odsazení, konec řádků atd.
    TOKEN_EOF,
    TOKEN_EOL,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    // tokeny s hodnotou
    TOKEN_ID,
    TOKEN_INT,
    TOKEN_DOUBLE,
    TOKEN_STRING,

    // klíčová slova
    TOKEN_DEF,
    TOKEN_DO,
    TOKEN_DOUBLE,
    TOKEN_ELSE,
    TOKEN_IF,
    TOKEN_IMPORT,
    TOKEN_INT,
    TOKEN_RETURN,
    TOKEN_STRING,
    TOKEN_THEN,
    TOKEN_UNIT,
    TOKEN_VAL,
    TOKEN_VAR,
    TOKEN_WHILE,

    // rezervovaná slova z rozšíření
    TOKEN_RESERVED,

    // anotace
    TOKEN_AT_MAIN,// @main

    // operátory
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,

    // oddělovače
    TOKEN_LPAR,
    TOKEN_RPAR,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_DOT,

} TokenType;


typedef struct { // sem dát všechny možné prvky, které jakýkoliv token potřebuje, pak jen vybírám
    TokenType type;
    long long intValue;
    double doubleValue;
    char *stringValue;
    int line; //chybové hlášky
}Token;







#endif