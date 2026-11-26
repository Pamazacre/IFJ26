#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Node *new_node(NodeKind k, int line) {
  Node *n = calloc(1, sizeof *n);
  n->kind = k;
  n->line = line;
  return n;
}

// pomocna funkce na pridavani do arraye
static void add_to_array(Node ***arr, int *count, Node *val) {
  *arr = realloc(*arr, (*count + 1) * sizeof(Node *));
  (*arr)[(*count)++] = val;
}

void block_add(Node *blk, Node *stmt) {
  add_to_array(&blk->as.block.stmts, &blk->as.block.count, stmt);
}

Node *make_binop(Node *left, Node *right, TokenType op, int line) {
  Node *n = new_node(N_BINOP, line);
  n->as.binop.l = left;
  n->as.binop.r = right;
  n->as.binop.op = op;
  return n;
}

Node *make_return(Node *value, int line) {
  Node *n = new_node(N_RETURN, line);
  n->as.ret.value = value;
  return n;
}

Node *make_var(char *name, int line) {
  Node *n = new_node(N_VAR, line);
  n->as.var.name = name;
  return n;
}

Node *make_num(long long value, int line) {
  Node *n = new_node(N_NUM, line);
  n->as.num.value = value;
  return n;
}

Node *make_assign(char *name, Node *value, int line) {
  Node *n = new_node(N_ASSIGN, line);
  n->as.assign.name = name;
  n->as.assign.value = value;
  return n;
}

Node *make_if(Node *cond, Node *then_b, Node *else_b, int line) {
  Node *n = new_node(N_IF, line);
  n->as.if_.cond = cond;
  n->as.if_.then_b = then_b;
  n->as.if_.else_b = else_b;
  return n;
}

Node *make_while(Node *cond, Node *body, int line) {
  Node *n = new_node(N_WHILE, line);
  n->as.while_.cond = cond;
  n->as.while_.body = body;
  return n;
}

Node *make_double(double value, int line) {
  Node *n = new_node(N_DOUBLE, line);
  n->as.double_.value = value;
  return n;
}

Node *make_string(char *value, int line) {
  Node *n = new_node(N_STRING, line);
  n->as.string.value = value;
  return n;
}

Node *make_unop(TokenType op, Node *operand, int line) {
  Node *n = new_node(N_UNOP, line);
  n->as.unop.op = op;
  n->as.unop.operand = operand;
  return n;
}

Node *make_unit(int line) {
  Node *n = new_node(N_UNIT, line);
  return n;
}

Node *make_program(int line) { return new_node(N_PROGRAM, line); }

Node *make_func(char *name, int is_main, int line) {
  Node *n = new_node(N_FUNC, line);
  n->as.func.name = name;
  n->as.func.is_main = is_main;
  n->as.func.ret = D_UNKNOWN;
  return n;
}

Node *make_vardef(char *name, int is_mutable, DataType declared, Node *init,
                  int line) {
  Node *n = new_node(N_VARDEF, line);
  n->as.vardef.name = name;
  n->as.vardef.is_mutable = is_mutable;
  n->as.vardef.declared = declared;
  n->as.vardef.init = init;
  return n;
}

Node *make_call(char *name, int line) {
  Node *n = new_node(N_CALL, line);
  n->as.call.name = name;
  return n;
}

void program_add(Node *prog, Node *func) {
  add_to_array(&prog->as.program.funcs, &prog->as.program.count, func);
}

void call_add_arg(Node *c, Node *arg) {
  add_to_array(&c->as.call.args, &c->as.call.nargs, arg);
}

void func_add_param(Node *f, char *name, DataType type) {
  f->as.func.params =
      realloc(f->as.func.params, (f->as.func.nparams + 1) * sizeof(Param));
  f->as.func.params[f->as.func.nparams++] = (Param){name, type};
}

Node *make_bool(int value, int line) {
  Node *n = new_node(N_BOOL, line);
  n->as.bool_.value = value;
  return n;
}

Node *make_istr(int line) { return new_node(N_ISTR, line); }

void istr_add(Node *s, Node *part) {
  add_to_array(&s->as.istr.parts, &s->as.istr.count, part);
}

void print_node(Node *n, int depth) {
  printf("%*s", depth * 2, "");
  switch (n->kind) {
  case N_NUM:
    printf("NUM %lld\n", n->as.num.value);
    break;
  case N_WHILE:
    printf("WHILE\n");
    print_node(n->as.while_.cond, depth + 1);
    print_node(n->as.while_.body, depth + 1);
    break;
  default:
    break;
  }
}