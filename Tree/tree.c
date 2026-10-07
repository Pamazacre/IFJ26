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
typedef struct{
  char *name;
  DataType type;
} Param;

typedef struct Node Node;
struct Node{
  NodeKind kind;
  int line;
  DataType dataType;

  union{
    struct{ Node **funcs; int count;
    } program;
    struct{ char *name; Param *params; int nparams; DataType ret; Node *body; int is_main;
    } func;
    struct{ char *name; int is_mutable; DataType declared; Node *init;
    } vardef;
    struct{ char *name; Node **args; int nargs;
    } call;
    struct{ Node *value;
    } ret; // parse done
    struct{ int value;
    } num; // parse done
    struct{ char *name;
    } var; // parse done
    struct{ char op; Node *l, *r;
    } binop; // parse done
    struct{ char *name; Node *value;
    } assign; // parse done
    struct{ Node *cond, *then_b, *else_b;
    } if_; // parse done
    struct{ Node *cond, *body;
    } while_; // parse done
    struct{ Node **stmts; int count;
    } block; // parse done
  } as;
};

Node *new_node(NodeKind k, int line){
  Node *n = calloc(1, sizeof *n);
  n->kind = k;
  n->line = line;
  return n;
}

void block_add(Node *blk, Node *stmt){
  blk->as.block.stmts = realloc(blk->as.block.stmts, (blk->as.block.count + 1) * sizeof(Node *));
  blk->as.block.stmts[blk->as.block.count++] = stmt;
}

Node *parse_binop(Node *left, Node *right, char op, int line){
  Node *n = new_node(N_BINOP, line);
  n->as.binop.l = left;
  n->as.binop.r = right;
  n->as.binop.op = op;
  return n;
}

Node *parse_return(Node *value, int line){
  Node *n = new_node(N_VAR, line);
  n->as.ret.value = value;
  return n;
}

Node *parse_var(char *name, int line){
  Node *n = new_node(N_VAR, line);
  n->as.var.name = name;
  return n;
}

Node *parse_num(int value, int line){
  Node *n = new_node(N_NUM, line);
  n->as.num.value = value;
  return n;
}

Node *parse_assign(char *name, Node *value, int line){
  Node *n = new_node(N_ASSIGN, line);
  n->as.assign.name = name;
  n->as.assign.value = value;
  return n;
}

Node *parse_if(Node *cond, Node *then_b, Node *else_b, int line){
  Node *n = new_node(N_IF, line);
  n->as.if_.cond = cond;
  n->as.if_.then_b = then_b;
  n->as.if_.else_b = else_b;
  return n;
}

Node *parse_while(Node *cond, Node *body, int line){
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
  switch (n->kind){
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