/*
* Implementace překladače imperativního jazyka IFJ26
*
* token.h - Konecny automat, definovani typu tokenu
* Autor: xnemecm00 (Matyas Nemecek)
*/


#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
  // odsazení, konec řádků atd.
  TOKEN_EOF,
  TOKEN_EOL,    // end of line
  TOKEN_INDENT, // Odsazení - tabulátor/mezery
  TOKEN_DEDENT, // Indent, ale v rámci víceřádkových textových řetězců

  TOKEN_STRING, // hodnota typu string - char znackaAuta[] = {"BMW"} <- tohle je
                // hodnota stringu

  TOKEN_TYPE_INT,    // název typu -> "int"
  TOKEN_TYPE_DOUBLE, // název typu -> "double"
  TOKEN_TYPE_STRING, // název typu -> "string"

  // klíčová slova
  TOKEN_DEF,    // KLíčové slovo: def
  TOKEN_DO,     // KLíčové slovo: do
  TOKEN_DOUBLE, // KLíčové slovo: double - > toto je hodnota typu double
  TOKEN_ELSE,   // KLíčové slovo: else
  TOKEN_IF,     // KLíčové slovo: if
  TOKEN_IMPORT, // KLíčové slovo: import
  TOKEN_INT,    // KLíčové slovo: int -> opět hodnota typu int
  TOKEN_RETURN, // KLíčové slovo: return
  TOKEN_ID,     // KLíčové slovo: ID -> identifikátor -> NÁZEV jakéhokoliv
                // identifikátoru = char znackaAuta <- název identifikátoru
  TOKEN_THEN,   // KLíčové slovo: then
  TOKEN_UNIT,   // KLíčové slovo: unit
  TOKEN_VAL,    // KLíčové slovo: val
  TOKEN_VAR,    // KLíčové slovo: var
  TOKEN_WHILE,  // KLíčové slovo: while

  // rezervovaná slova z rozšíření -> zatím jen zakázat jejich používání jako
  // identifkátory
  TOKEN_RESERVED,

  // anotace
  TOKEN_AT_MAIN, // @main

  // operátory
  TOKEN_PLUS,   // KLíčové slovo: +
  TOKEN_MINUS,  // KLíčové slovo: -
  TOKEN_MUL,    // KLíčové slovo: *
  TOKEN_DIV,    // KLíčové slovo: /
  TOKEN_ASSIGN, // KLíčové slovo: =
  TOKEN_EQ,     // Klíčové slovo: ==
  TOKEN_NEQ,    // Klíčové slovo: !=
  TOKEN_LT,     // KLíčové slovo: <
  TOKEN_LE,     // Klíčové slovo: <=
  TOKEN_GT,     // Klíčové slovo: >
  TOKEN_GE,     // KLíčové slovo: >=

  // oddělovače
  TOKEN_LPAR,      // KLíčové slovo: (
  TOKEN_RPAR,      // KLíčové slovo: )
  TOKEN_COMMA,     // KLíčové slovo: "
  TOKEN_COLON,     // KLíčové slovo: :
  TOKEN_SEMICOLON, // KLíčové slovo: ;
  TOKEN_DOT,       // KLíčové slovo: .

  // interpovolavne retezce - rozsireni pro INTERPOLATEDS
  TOKEN_ISTR_START,
  TOKEN_ISTR_END,
  TOKEN_ISTR_EXPR_START,
  TOKEN_ISTR_EXPR_END,

  // pravdivostní hodnoty - rozsireni pro boolean
  TOKEN_TYPE_BOOLEAN,
  TOKEN_TRUE,    // Klíčová hodnota bool - true
  TOKEN_FALSE,   // Klíčová hodnota bool - false
  TOKEN_NOT,     // Klíčová hodnota bool - NOT
  TOKEN_AND,     // Klíčová hodnota bool - AND
  TOKEN_OR,      // Klíčová hodnota bool - OR

} TokenType;

typedef struct { // sem dát všechny možné prvky, které jakýkoliv token
                 // potřebuje, pak jen vybírám
  TokenType type;
  long long intValue;
  double doubleValue;
  char *stringValue;
  int line; // chybové hlášky
} Token;

int get_token(Token *t);

#endif
