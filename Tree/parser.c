#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Token/token.h"
#include "../Tree/tree.h"

static Token cur;

// nacte dalsi token do cur
static void next(void){
  int rc = get_token(&cur);
  if (rc == 1)
    exit(1); // lex chyba
  if (rc != 0)
    exit(99); // interni chyba (malloc)
}

_Noreturn static void error(void){
  fprintf(stderr, "syntakticka chyba, radek %d\n", cur.line);
  exit(2);
}

// cur musi byt typu t, jinak chyba, pak posune na dalsi token
static void expect(TokenType t){
  if (cur.type != t)
    error();
  next();
}

// preskoci prazdne konce radku
static void skip_eols(void){
  while (cur.type == TOKEN_EOL)
    next();
}

// parsne typy
static DataType parse_type(void){
  DataType d;
  switch (cur.type){
  case TOKEN_TYPE_INT:
    d = D_INT;
    break;
  case TOKEN_TYPE_DOUBLE:
    d = D_FLOAT;
    break;
  case TOKEN_TYPE_STRING:
    d = D_STRING;
    break;
  case TOKEN_UNIT:
    d = D_UNIT;
    break;
  default:
    error();
  }
  next();
  return d;
}

static void parse_param(Node *f){
  if (cur.type != TOKEN_ID)
    error();
  char *name = cur.stringValue;
  next();
  expect(TOKEN_COLON);
  func_add_param(f, name, parse_type());
}

static Node *parse_func(void)
{
  int is_main = 0;
  if (cur.type == TOKEN_AT_MAIN)
  { // @main muze byt i na samotnem radu
    is_main = 1;
    next();
    skip_eols();
  }

  int line = cur.line;
  expect(TOKEN_DEF);
  if (cur.type != TOKEN_ID)
    error();
  Node *f = make_func(cur.stringValue, is_main, line);
  next();

  expect(TOKEN_LPAR);
  if (cur.type != TOKEN_RPAR)
  { // aspon jeden parametr
    parse_param(f);
    while (cur.type == TOKEN_COMMA)
    {
      next();
      if (cur.type == TOKEN_RPAR)
        break; // konc carka
      parse_param(f);
    }
  }
  expect(TOKEN_RPAR);

  if (cur.type == TOKEN_COLON)
  { // return type je nepovinny
    next();
    f->as.func.ret = parse_type();
  }
  expect(TOKEN_ASSIGN);

  // TODO: telo funkce -> f->as.func.body = parse_body();
  return f;
}

Node *parse_program(void)
{
  next(); // nacti prvni token
  skip_eols();
  Node *prog = make_program(cur.line);

  // import ifj26.*
  expect(TOKEN_IMPORT);
  if (cur.type != TOKEN_ID || strcmp(cur.stringValue, "ifj26") != 0)
    error();
  next();
  expect(TOKEN_DOT);
  expect(TOKEN_MUL);
  skip_eols();

  while (cur.type != TOKEN_EOF){
    program_add(prog, parse_func());
    skip_eols();
  }
  return prog;
}
