#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum{
  N_NUM,
  N_VAR,
  N_BINOP,
  N_ASSIGN,
  N_IF,
  N_WHILE,
  N_BLOCK
} NodeKind;

typedef enum{
  D_INT,
  D_FLOAT,
  D_STRING,
  D_BOOL,
  D_UNKNOWN
} DataType;

// Parametry pro funkce
typedef struct
{
  char *name;
  DataType type;
} Param;

typedef struct Node Node;
struct Node{
  NodeKind kind;
  int line;
  DataType dataType;
  union{
    struct{
      Node **funcs;
      int count;
    } program;
    struct{
      char *name;
      Param *params;
      int nparams;
      DataType ret;
      Node *body;
      int is_main;
    } func;
    struct{
      char *name;
      int is_mutable;
      DataType declared;
      Node *init;
    } vardef;
    struct{
      char *name;
      Node **args;
      int nargs;
    } call;
    struct{
      Node *value;
    } ret;
    struct{
      int value;
    } num;
    struct{
      char *name;
    } var;
    struct{
      char op;
      Node *l, *r;
    } binop;
    struct{
      char *name;
      Node *value;
    } assign;
    struct{
      Node *cond, *then_b, *else_b;
    } if_;
    struct{
      Node *cond, *body;
    } while_;
    struct{
      Node **stmts;
      int count;
    } block;
  } as;
};

Node *new_node(NodeKind k, int line){
  Node *n = calloc(1, sizeof *n);
  n->kind = k;
  n->line = line;
  return n;
}

void block_add(Node *blk, Node *stmt)
{
  blk->as.block.stmts = realloc(blk->as.block.stmts, (blk->as.block.count + 1) * sizeof(Node *));
  blk->as.block.stmts[blk->as.block.count++] = stmt;
}

Node *node_if(Node *cond, Node *then_b, Node *else_b, int line){
  Node *n = new_node(N_IF, line);
  n->as.if_.cond = cond;
  n->as.if_.then_b = then_b;
  n->as.if_.else_b = else_b;
  return n;
}

Node *make_while(Node *cond, Node *body, int line){
  Node *n = new_node(N_WHILE, line);
  n->as.while_.cond = cond;
  n->as.while_.body = body;
  return n;
}

/*
TODO:
DOPLNIT VSECHNY CONTRUCTORY PRO OSTATNI TYPY NODE
*/

void print_node(Node *n, int depth){
  printf("%*s", depth * 2, "");
  switch (n->kind)
  {
  case N_NUM:
    printf("NUM %d\n", n->as.num.value);
    break;
  case N_WHILE:
    printf("WHILE\n");
    print_node(n->as.while_.cond, depth + 1);
    print_node(n->as.while_.body, depth + 1);
    break;
  }
}