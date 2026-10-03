#ifndef TOKEN_H
#define TOKEN_H


typedef enum{ //jednotlivé druhy tokenů - int, double, EOF, identifikátory atd.
    tokenId,
    tokenInt,
    tokenAssign,
    tokenEof
}TokenType;






#endif