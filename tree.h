/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * tree.h --definice uzlu AST a constructors uzlu
 * Autor: xsenkyp00 (Pavel Šenkýř)
 */

#ifndef TREE_H
#define TREE_H

#include "token.h"

typedef enum {
  N_NUM,
  N_VAR,
  N_BINOP,
  N_ASSIGN,
  N_IF,
  N_WHILE,
  N_BLOCK,
  N_PROGRAM,
  N_FUNC,
  N_VARDEF,
  N_CALL,
  N_RETURN,
  N_UNIT,
  N_STRING,
  N_UNOP,
  N_DOUBLE,
  // nase rozsireni
  N_BOOL,
  N_ISTR
} NodeKind;

typedef enum { D_INT, D_FLOAT, D_STRING, D_BOOL, D_UNIT, D_UNKNOWN, D_UNDEFINED } DataType;

// Parametry pro funkce
typedef struct {
  char *name;
  DataType type;
} Param;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  int line;
  DataType dataType;

  union {
    struct {
      Node **funcs;
      int count;
    } program;
    struct {
      char *name;
      Param *params;
      int nparams;
      DataType ret;
      Node *body;
      int is_main;
    } func;
    struct {
      char *name;
      int is_mutable;
      DataType declared;
      Node *init;
    } vardef;
    struct {
      char *name;
      Node **args;
      int nargs;
    } call;
    struct {
      Node *value;
    } ret;
    struct {
      long long value;
    } num;
    struct {
      char *name;
    } var;
    struct {
      TokenType op;
      Node *l, *r;
    } binop;
    struct {
      char *name;
      Node *value;
    } assign;
    struct {
      Node *cond, *then_b, *else_b;
    } if_;
    struct {
      Node *cond, *body;
    } while_;
    struct {
      Node **stmts;
      int count;
    } block;
    struct {
      double value;
    } double_;
    struct {
      char *value;
    } string;
    struct {
      TokenType op;
      Node *operand;
    } unop;

    struct {
      int value; // 1 / 0, proste klasicky bool
    } bool_;

    struct {
      Node **parts; //
      int count;
    } istr;
  } as;
};

// alokuje novy prazdny uzel daneho druhu (calloc)
Node *new_node(NodeKind k, int line);

// pridani potomku do seznamu: prikaz do bloku, funkce do programu,
// parametr do funkce, argument do volani
void block_add(Node *blk, Node *stmt);
void program_add(Node *prog, Node *func);
void func_add_param(Node *f, char *name, DataType type);
void call_add_arg(Node *c, Node *arg);

// konstruktory uzlu AST, kazdy vytvori uzel a vyplni jeho data
Node *make_binop(Node *left, Node *right, TokenType op, int line);
Node *make_unop(TokenType op, Node *operand, int line);
Node *make_num(long long value, int line);
Node *make_double(double value, int line);
Node *make_string(char *value, int line);
Node *make_unit(int line);
Node *make_var(char *name, int line);
Node *make_assign(char *name, Node *value, int line);
Node *make_vardef(char *name, int is_mutable, DataType declared, Node *init, int line);
Node *make_call(char *name, int line);
Node *make_return(Node *value, int line);
Node *make_if(Node *cond, Node *then_b, Node *else_b, int line);
Node *make_while(Node *cond, Node *body, int line);
Node *make_program(int line);
Node *make_func(char *name, int is_main, int line);
Node *make_bool(int value, int line);
Node *make_istr(int line);

// prida cast (retezec nebo vyraz) do interpolovaneho retezce
void istr_add(Node *s, Node *part);

void print_node(Node *n, int depth);

#endif