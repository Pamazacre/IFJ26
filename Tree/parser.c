#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Token/token.h"
#include "../Tree/tree.h"

static Token cur;
static Token la; // token za cur
static int has_la = 0;

static void read_token(Token *t){
  int rc = get_token(t);
  if (rc == 1)
    exit(1); // lex chyba
  if (rc != 0)
    exit(99); // interni chyba
}

static void next(void){
  if (has_la){
    cur = la;
    has_la = 0;
  }
  else
    read_token(&cur);
}

// vrati token za cur
static Token peek(void){
  if (!has_la){
    read_token(&la);
    has_la = 1;
  }
  return la;
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

static Node *parse_expr(void);
static Node *parse_block(void);

// parsne typy
static DataType parse_type(void){
  DataType d;
  switch (cur.type){
  case TOKEN_TYPE_INT:
    d = TYPE_INT;
    break;
  case TOKEN_TYPE_DOUBLE:
    d = TYPE_DOUBLE;
    break;
  case TOKEN_TYPE_STRING:
    d = TYPE_STRING;
    break;
  case TOKEN_UNIT:
    d = TYPE_UNIT;
    break;
  default:
    error();
  }
  next();
  return d;
}

// parametry pro fce
static void parse_param(Node *f){
  if (cur.type != TOKEN_ID)
    error();
  char *name = cur.stringValue;
  next();
  expect(TOKEN_COLON);
  func_add_param(f, name, parse_type());
}

// parsne var, nebo ()
static Node *parse_var(void){
  Node *n;
  int line = cur.line;
  switch (cur.type){
  case TOKEN_INT:
    n = make_num(cur.intValue, line);
    break;
  case TOKEN_DOUBLE:
    n = make_double(cur.doubleValue, line);
    break;
  case TOKEN_STRING:
    n = make_string(cur.stringValue, line);
    break;
  case TOKEN_ID:
    n = make_var(cur.stringValue, line);
    break;
  case TOKEN_LPAR: // () = Unit
    next();
    expect(TOKEN_RPAR);
    return make_unit(line);
  default:
    error();
  }
  next();
  return n;
}

// Opus 5.5 "Potrebuju pomoc s porovnanim tokenu pro zjisteni jestli je operace binarni nebo ne"
static int is_binop(TokenType t){
  return t == TOKEN_PLUS || t == TOKEN_MINUS || t == TOKEN_MUL || t == TOKEN_DIV ||
         t == TOKEN_LT || t == TOKEN_LE || t == TOKEN_GT || t == TOKEN_GE ||
         t == TOKEN_EQ || t == TOKEN_NEQ;
}

// zatim BEZ PRIORIT
// TODO: PRIDAT PRIORITY OPERACI !!!
static Node *parse_expr(void){
  Node *left = parse_var();
  while (is_binop(cur.type)){
    TokenType op = cur.type;
    int line = cur.line;
    next();
    left = make_binop(left, parse_var(), op, line);
  }
  return left;
}

static Node *parse_call(void){
  Node *c = make_call(cur.stringValue, cur.line);
  next();
  expect(TOKEN_LPAR);
  if (cur.type != TOKEN_RPAR){
    call_add_arg(c, parse_var());
    while (cur.type == TOKEN_COMMA){
      next();
      if (cur.type == TOKEN_RPAR)
        break; // koncova carka
      call_add_arg(c, parse_var());
    }
  }
  expect(TOKEN_RPAR);
  return c;
}

static Node *parse_expr_or_call(void){
  if (cur.type == TOKEN_ID && peek().type == TOKEN_LPAR)
    return parse_call();
  return parse_expr();
}

static Node *parse_stmt(void){
  int line = cur.line;
  switch (cur.type){
  case TOKEN_VAL:
  case TOKEN_VAR:{
    int is_mutable = (cur.type == TOKEN_VAR);
    next();
    if (cur.type != TOKEN_ID)
      error();
    char *name = cur.stringValue;
    next();
    DataType declared = TYPE_UNKNOWN;
    if (cur.type == TOKEN_COLON){
      next();
      declared = parse_type();
    }
    expect(TOKEN_ASSIGN);
    return make_vardef(name, is_mutable, declared, parse_expr_or_call(), line);
  }
  case TOKEN_RETURN:{ // return
    next();
    Node *value = NULL;
    if (cur.type != TOKEN_EOL && cur.type != TOKEN_SEMICOLON &&
        cur.type != TOKEN_DEDENT && cur.type != TOKEN_EOF)
      value = parse_expr_or_call();
    return make_return(value, line);
  }
  case TOKEN_IF:{ // if cond then EOL blok else EOL blok
    next();
    Node *cond = parse_expr();
    expect(TOKEN_THEN);
    expect(TOKEN_EOL);
    Node *then_b = parse_block();
    expect(TOKEN_ELSE);
    expect(TOKEN_EOL);
    Node *else_b = parse_block();
    return make_if(cond, then_b, else_b, line);
  }
  case TOKEN_WHILE:{ // while cond do EOL blok
    next();
    Node *cond = parse_expr();
    expect(TOKEN_DO);
    expect(TOKEN_EOL);
    return make_while(cond, parse_block(), line);
  }
  case TOKEN_ID:
    if (peek().type == TOKEN_ASSIGN){ // id =
      char *name = cur.stringValue;
      next(); // id
      next(); // =
      return make_assign(name, parse_expr_or_call(), line);
    }
    return parse_expr_or_call(); // volani nebo vyraz
  default:
    return parse_expr(); // samotny vyraz, napr. ()
  }
}

static Node *parse_block(void){
  Node *blk = new_node(N_BLOCK, cur.line);
  expect(TOKEN_INDENT);
  while (1){
    Node *s = parse_stmt();
    block_add(blk, s);
    if (s->kind == N_IF || s->kind == N_WHILE){
      // vnoreny blok uz snedl EOL i DEDENT, oddelovac tu neni
    }
    else if (cur.type == TOKEN_EOL || cur.type == TOKEN_SEMICOLON)
      next();
    else if (cur.type != TOKEN_DEDENT)
      error(); // dva prikazy bez oddelovace
    if (cur.type == TOKEN_DEDENT){
      next();
      return blk;
    }
  }
}

static Node *parse_body(void)\{
  if (cur.type == TOKEN_EOL){
    next();
    return parse_block();
  }
  Node *n = parse_expr_or_call();
  if (cur.type != TOKEN_EOL && cur.type != TOKEN_EOF)
    error();
  return n;
}
// funkce i main funkce
static Node *parse_func(void){
  int is_main = 0;
  if (cur.type == TOKEN_AT_MAIN){ // @main muze byt i na samotnem radu
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
  if (cur.type != TOKEN_RPAR){ // aspon jeden parametr
    parse_param(f);
    while (cur.type == TOKEN_COMMA){
      next();
      if (cur.type == TOKEN_RPAR)
        break; // konc carka
      parse_param(f);
    }
  }
  expect(TOKEN_RPAR);

  if (cur.type == TOKEN_COLON){ // return type je nepovinny
    next();
    f->as.func.body = parse_body();
  }
  expect(TOKEN_ASSIGN);

  // TODO: telo funkce -> f->as.func.body = parse_body();
  return f;
}

//hlavicka souboru a prida funkce do arraye programu
Node *parse_program(void){
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
