// xsmrckt00

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "analyzer.h"
#include "tree.h"
#include "symtable.h"

#include <string.h>

#define INTERNAL_ERROR 99
int check_func(Node *n);
static int check_node(Node *n, symtable_t *st);
int check_return(Node *n, symtable_t *st);

/* kvuli kompatibilite mezi symtable.c a tree.c jsem ji vytvoril nasledujici
 * pouzil jsem ai protoze jsem nechtel prepisovat rucne vsude kde mam neco s datype
 * claude Sonnet 5.5 Medium
 * pridej a pouprav cely soubor pro kompabilitu datatype mezi symtable a tree.c
 */
static data_type_t from_tree(DataType t) {
  switch (t) {
    case D_INT:
      return TYPE_INT;
    case D_FLOAT:
      return TYPE_DOUBLE;
    case D_STRING:
      return TYPE_STRING;
    case D_BOOL:
      return TYPE_BOOLEAN;
    case D_UNIT:
      return TYPE_UNIT;
    default:
      return TYPE_UNKNOWN;
  }
}
/* pouziti ai na pridani teto fukce a na zavolani na prislusnych mistech ve vardef, add_func, check_func
 * gemini hydrogen
 * promt pridej funkci is_invalid_type a zavolej na prislusnych mistech v vardef, add_func, check_func
 * priloha byl cely analyzer.c
 */
static bool is_invalid_type(DataType t) {
  return t != D_INT && t != D_FLOAT && t != D_STRING && t != D_BOOL && t != D_UNIT &&
         t != D_UNKNOWN;
}

static DataType to_tree(data_type_t t) {
  switch (t) {
    case TYPE_INT:
      return D_INT;
    case TYPE_DOUBLE:
      return D_FLOAT;
    case TYPE_STRING:
      return D_STRING;
    case TYPE_BOOLEAN:
      return D_BOOL;
    case TYPE_UNIT:
      return D_UNIT;
    default:
      return D_UNKNOWN;
  }
}
typedef enum {
  FUNC_UNCHECKED = 0,
  FUNC_CHECKING = 1,
  FUNC_DONE = 2,
} func_status_t;

static symtable_t *global_st = NULL;
static Node *current_prog = NULL;
static func_status_t *func_states = NULL;

int find_func_idx(char *name) {
  if (global_st == NULL) {
    return -INTERNAL_ERROR;
  }
  for (int i = 0; i < current_prog->as.program.count; i++) {
    if (strcmp(name, current_prog->as.program.funcs[i]->as.func.name) == 0) {
      return i;
    }
  }
  return -INTERNAL_ERROR;
}

static data_type_t get_type(Node *n) {
  return from_tree(n->dataType);
}

static void set_type(Node *n, data_type_t t) {
  n->dataType = to_tree(t);
}

static data_type_t expReturn = TYPE_UNKNOWN;
static bool retExplicit = false;

typedef enum { RES_VALUE, RES_RETURN, RES_MIXED } res_kind_t;

typedef struct {
  res_kind_t kind;
  data_type_t type;
  bool all_unit;
} result_t;

int check_stms(Node **stms, symtable_t *st, int count, result_t *res);
result_t res_return() {
  result_t res;
  res.type = TYPE_UNIT;
  res.kind = RES_RETURN;
  res.all_unit = true;
  return res;
}
result_t res_value(data_type_t type) {
  result_t res;
  res.type = type;
  res.kind = RES_VALUE;
  res.all_unit = false;
  return res;
}
result_t res_mixed(bool is_unit) {
  result_t res;
  res.type = TYPE_UNKNOWN;
  res.kind = RES_MIXED;
  res.all_unit = is_unit;
  return res;
}

static bool compatible(data_type_t expected, data_type_t actual) {
  if (expected == TYPE_ANY) {
    return true;
  }
  return expected == actual || (expected == TYPE_DOUBLE && actual == TYPE_INT);
}

int check_vardef(Node *n, symtable_t *st) {
  if (is_invalid_type(n->as.vardef.declared)) {
    fprintf(stderr, "Chyba (radek %d): Neznamy datovy typ promenne '%s'\n", n->line,
            n->as.vardef.name);
    return 3;
  }

  data_type_t finalType = from_tree(n->as.vardef.declared);
  if (n->as.vardef.init != NULL) {
    int err = check_node(n->as.vardef.init, st);
    if (err) {
      return err;
    }

    data_type_t initType = get_type(n->as.vardef.init);

    if (finalType == TYPE_UNKNOWN) {
      finalType = initType;
    }
    if (!compatible(finalType, initType)) {
      return 6;
    }
  } else {
    if (finalType == TYPE_UNKNOWN) {
      return 6;
    }
  }
  st_status_t status =
      symtable_insert_var(st, n->as.vardef.name, finalType, n->as.vardef.is_mutable, NULL);

  if (status == ST_ERR_REDEF) {
    fprintf(stderr, "Chyba (radek %d): Redefinice promenne '%s' \n", n->line, n->as.vardef.name);
    return 4;
  }
  if (status == ST_ERR_MEMORY || status == ST_ERR_NO_SCOPE) {
    return INTERNAL_ERROR;
  }
  set_type(n, finalType);
  return 0;
}
int check_var(Node *n, symtable_t *st) {
  if (n->as.var.name != NULL) {
    symbol_t *symbol = symtable_find_var(st, n->as.var.name);
    if (symbol == NULL) {
      fprintf(stderr, "Chyba (radek %d): Nedefinovana promenna %s \n", n->line, n->as.var.name);
      return 3;
    } else {
      set_type(n, symbol->var.type);
      return 0;
    }
  }
  return INTERNAL_ERROR;
}
int check_assign(Node *n, symtable_t *st) {
  if (n->as.assign.name != NULL) {
    symbol_t *symbol = symtable_find_var(st, n->as.assign.name);
    if (symbol == NULL) {
      fprintf(stderr, "Chyba (radek %d): Nedefinovana promenna %s \n", n->line, n->as.assign.name);
      return 7;
    }

    int err = check_node(n->as.assign.value, st);
    if (err) {
      return err;
    }
    if (symbol->var.is_mutable == false) {
      fprintf(stderr, "Chyba (radek %d): Nelze zapisovat do konstanty %s \n", n->line,
              n->as.assign.name);
      return 7;
    }
    if (!compatible(symbol->var.type, get_type(n->as.assign.value))) {
      fprintf(stderr, "Chyba (radek %d): Nesedi datype promenne %s \n", n->as.assign.value->line,
              n->as.assign.name);
      return 6;
    }
    return 0;
  }
  return INTERNAL_ERROR;
}
/* zkontrluje datove typy a podle nich a operatoru vrati nastavi datovy typ nadrazeneho node a vrati
 * nulu, v pripade chyby vraci prislusnou hodnotu chyby */
int check_binop(Node *n, symtable_t *st) {
  int err = 0;
  err = check_node(n->as.binop.l, st);
  if (err != 0) {
    return err;
  }
  err = check_node(n->as.binop.r, st);
  if (err != 0) {
    return err;
  }

  data_type_t lt = get_type(n->as.binop.l);
  data_type_t rt = get_type(n->as.binop.r);
  bool lNum = (lt == TYPE_INT || lt == TYPE_DOUBLE);
  bool rNum = (rt == TYPE_INT || rt == TYPE_DOUBLE);

  data_type_t curr;
  if (lNum && rNum) {
    curr = (lt == TYPE_DOUBLE || rt == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
  } else if (lt == rt && lt != TYPE_UNKNOWN) {
    curr = lt;
  } else {
    fprintf(stderr, "Chyba (radek %d): Nesedi datove typy \n", n->line);
    return 6;
  }

  switch (n->as.binop.op) {
    case TOKEN_PLUS:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 6;
      }
      set_type(n, curr);
      break;
    case TOKEN_MUL:
    case TOKEN_MINUS:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT || curr == TYPE_STRING) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 6;
      }
      set_type(n, curr);
      break;
    case TOKEN_DIV:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT || curr == TYPE_STRING) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 6;
      }
      set_type(n, curr);
      break;
    case TOKEN_GE:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_LT:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 6;
      }
      set_type(n, TYPE_BOOLEAN);
      break;
    case TOKEN_EQ:
    case TOKEN_NEQ:
      set_type(n, TYPE_BOOLEAN);
      break;
  case TOKEN_AND:
  case TOKEN_OR:
    if (curr != TYPE_BOOLEAN) {
      fprintf(stderr, "Chyba (radek %d): Operandy logickeho operatoru musi byt typu Boolean\n",
              n->line);
      return 6;
    }
    set_type(n, TYPE_BOOLEAN);
    break;


    default:
      fprintf(stderr, "Chyba (radek %d): Neznamy operator\n", n->line);
      return INTERNAL_ERROR;
  }
  return 0;
}
/* projde blok a zavola nad kazdym nejvechrnejsim check node
 * ten mu vrati datatype ktery muze kontrolovat */

int check_block(Node *n, symtable_t *st, result_t *res) {
  if (!symtable_push_scope(st))
    return INTERNAL_ERROR;
  int err = check_stms(n->as.block.stmts, st, n->as.block.count, res);
  symtable_pop_scope(st);
  return err;
}
int check_while(Node *n, symtable_t *st, result_t *res) {
  int err = 0;
  err = check_node(n->as.while_.cond, st);
  if (err != 0) {
    return err;
  }
  data_type_t condDT = get_type(n->as.while_.cond);
  if (condDT != TYPE_BOOLEAN) {
    return 6;
  }
  err = check_block(n->as.while_.body, st, res);
  if (err != 0) {
    return err;
  }
  return 0;
}

bool branch_is_unit(result_t r) {
  if (r.kind == RES_RETURN)
    return true;
  if (r.kind == RES_VALUE)
    return r.type == TYPE_UNIT;
  return r.all_unit;
}
/* zkontroluje podminku a blok then a pokud existuje else tak zkontroluje to
 *  take zkouma zda maji stejny return type */
int check_if(Node *n, symtable_t *st, result_t *res) {
  int err = 0;
  result_t thenRes, elseRes = res_value(TYPE_UNIT);

  err = check_node(n->as.if_.cond, st);
  if (err != 0) {
    return err;
  }
  if (get_type(n->as.if_.cond) != TYPE_BOOLEAN) {
    fprintf(stderr, "Chyba (radek %d): Podminka neni typu Boolean\n", n->line);
    return 6;
  }
  err = check_block(n->as.if_.then_b, st, &thenRes);
  if (err != 0) {
    return err;
  }
  if (n->as.if_.else_b != NULL && n->as.if_.else_b->kind == N_BLOCK) {
    err = check_block(n->as.if_.else_b, st, &elseRes);
    if (err != 0) {
      return err;
    }
  } else if (n->as.if_.else_b == NULL) {
    elseRes = res_value(TYPE_UNIT);
  } else {
    err = check_if(n->as.if_.else_b, st, &elseRes);
    if (err != 0) {
      return err;
    }
  }
  // pokud jsou return vrati return pokud jsou jine vrati mixed
  if (thenRes.kind == RES_RETURN && elseRes.kind == RES_RETURN) {
    *res = res_return();
  } else {
    *res = res_mixed(branch_is_unit(thenRes) && branch_is_unit(elseRes));
  }

  return 0;
}

int check_stmt(Node *n, symtable_t *st, result_t *res) {
  int err;
  switch (n->kind) {
    case N_RETURN:
      err = check_return(n, st);
      if (err != 0) {
        return err;
      }
      *res = res_return();
      return 0;

    case N_IF:
      return check_if(n, st, res);

    case N_BLOCK:
      return check_block(n, st, res);

    case N_VARDEF:
    case N_ASSIGN:
    case N_WHILE:
      err = check_node(n, st);
      if (err != 0) {
        return err;
      }
      *res = res_value(TYPE_UNIT);
      return 0;

    default:
      err = check_node(n, st);
      if (err != 0) {
        return err;
      }
      *res = res_value(get_type(n));
      return 0;
  }
}

int check_stms(Node **stms, symtable_t *st, int count, result_t *res) {
  int err = 0;
  *res = res_value(TYPE_UNIT);
  for (int i = 0; i < count; i++) {
    err = check_stmt(stms[i], st, res);
    if (err != 0) {
      return err;
    }
  }
  return err;
}
int ensure_checked(int idx) {
  if (func_states == NULL) {
    return INTERNAL_ERROR;
  }
  if (func_states[idx] == FUNC_DONE) {
    return 0;
  }
  if (func_states[idx] == FUNC_CHECKING) {
    fprintf(stderr, "Chyba (radek %d): Rekurzivni volani funkce bez navratoveho typu\n",
            current_prog->as.program.funcs[idx]->line);
    return 6;
  }
  func_states[idx] = FUNC_CHECKING;
  int err = check_func(current_prog->as.program.funcs[idx]);
  if (err != 0) {
    return err;
  }
  func_states[idx] = FUNC_DONE;
  return 0;
}

int add_func(Node *n, symtable_t *st) {
  if (is_invalid_type(n->as.func.ret)) {
    fprintf(stderr, "Chyba (radek %d): Neznamy navratovy typ funkce '%s'\n", n->line,
            n->as.func.name);
    return 3;
  }
  for (int i = 0; i < n->as.func.nparams; i++) {
    if (is_invalid_type(n->as.func.params[i].type) || n->as.func.params[i].type == D_UNKNOWN) {
      fprintf(stderr, "Chyba (radek %d): Neznamy typ parametru '%s'\n", n->line,
              n->as.func.params[i].name);
      return 3;
    }
  }

  symbol_t *func = NULL;
  st_status_t status = symtable_insert_func(st, n->as.func.name, from_tree(n->as.func.ret), &func);

  if (status == ST_ERR_REDEF) {
    return 4;
  }
  if (status == ST_ERR_NO_SCOPE) {
    return INTERNAL_ERROR;
  }
  if (status == ST_ERR_MEMORY) {
    return INTERNAL_ERROR;
  }
  for (int i = 0; i < n->as.func.nparams; i++) {
    bool ok = symtable_add_param(func, from_tree(n->as.func.params[i].type));
    if (!ok) {
      return INTERNAL_ERROR;
    }
  }
  return 0;
}
int check_body_result(Node *n, result_t *res) {
  data_type_t final_type = expReturn;

  if (!retExplicit) {
    if (res->kind == RES_VALUE && res->type != TYPE_UNKNOWN) {
      final_type = res->type;
    } else if (res->kind == RES_MIXED && res->all_unit) {
      final_type = TYPE_UNIT;
    } else {
      fprintf(stderr, "Chyba (radek %d): Nelze odvodit navratovy typ funkce\n", n->line);
      return 6;
    }

    n->as.func.ret = to_tree(final_type);
    symbol_t *sym = symtable_find_func(global_st, n->as.func.name);
    if (sym == NULL) {
      return INTERNAL_ERROR;
    }
    sym->func.ret_type = final_type;

  } else if (expReturn == TYPE_UNIT) {
    final_type = TYPE_UNIT;

  } else {
    bool ok =
        (res->kind == RES_RETURN) || (res->kind == RES_VALUE && compatible(expReturn, res->type));
    if (!ok) {
      fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
      return 6;
    }
  }

  if (n->as.func.is_main && final_type != TYPE_UNIT) {
    fprintf(stderr, "Chyba (radek %d): funkce main musi vracet unit\n", n->line);
    return 6;
  }

  return 0;
}
int check_func(Node *n) {
  symtable_t *local_st = symtable_create();
  if (local_st == NULL) {
    return INTERNAL_ERROR;
  }
  if (!symtable_push_scope(local_st)) {
    symtable_free(local_st);
    return INTERNAL_ERROR;
  }

  if (is_invalid_type(n->as.func.ret)) {
    fprintf(stderr, "Chyba (radek %d): Neznamy navratovy typ funkce '%s'\n", n->line,
            n->as.func.name);
    symtable_pop_scope(local_st);
    symtable_free(local_st);
    return 3;
  }

  for (int i = 0; i < n->as.func.nparams; i++) {
    if (is_invalid_type(n->as.func.params[i].type) || n->as.func.params[i].type == D_UNKNOWN) {
      fprintf(stderr, "Chyba (radek %d): Neznamy typ parametru '%s'\n", n->line,
              n->as.func.params[i].name);
      symtable_pop_scope(local_st);
      symtable_free(local_st);
      return 3;
    }
    st_status_t statusVar = symtable_insert_var(local_st, n->as.func.params[i].name,
                                                from_tree(n->as.func.params[i].type), false, NULL);
    if (statusVar == ST_ERR_REDEF) {
      symtable_pop_scope(local_st);
      symtable_free(local_st);
      return 4;
    }
    if (statusVar != ST_OK) {
      symtable_pop_scope(local_st);
      symtable_free(local_st);
      return INTERNAL_ERROR;
    }
  }
  data_type_t prevReturn = expReturn;
  expReturn = from_tree(n->as.func.ret);
  bool prevRetExplicit = retExplicit;
  retExplicit = (n->as.func.ret != D_UNKNOWN);
  result_t bodyRes;

  int err = 0;
  if (n->as.func.body == NULL) {
    err = INTERNAL_ERROR;
  } else if (n->as.func.body->kind == N_BLOCK) {
    err = check_stms(n->as.func.body->as.block.stmts, local_st, n->as.func.body->as.block.count,
                     &bodyRes);
  } else {
    err = check_stmt(n->as.func.body, local_st, &bodyRes);
  }
  if (err != 0) {
    expReturn = prevReturn;
    retExplicit = prevRetExplicit;
    symtable_pop_scope(local_st);
    symtable_free(local_st);
    return err;
  }
  err = check_body_result(n, &bodyRes);
  expReturn = prevReturn;
  retExplicit = prevRetExplicit;
  symtable_pop_scope(local_st);
  symtable_free(local_st);
  return err;
}
int check_program(Node *n, symtable_t *st) {
  int err;
  if (n->as.program.count > 0) {
    func_states = calloc((size_t)n->as.program.count, sizeof(func_status_t));
    if (func_states == NULL) {
      return INTERNAL_ERROR;
    }
  }
  for (int i = 0; i < n->as.program.count; i++) {
    err = add_func(n->as.program.funcs[i], st);
    if (err != 0) {
      free(func_states);
      func_states = NULL;
      return err;
    }
  }
  for (int i = 0; i < n->as.program.count; i++) {
    err = ensure_checked(i);
    if (err != 0) {
      free(func_states);
      func_states = NULL;
      return err;
    }
  }
  free(func_states);
  func_states = NULL;
  return 0;
}

int check_return(Node *n, symtable_t *st) {
  if (n->as.ret.value == NULL) {
    if (expReturn != TYPE_UNIT) {
      fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
      return 6;
    }
    set_type(n, TYPE_UNIT);
    return 0;
  }
  int err = check_node(n->as.ret.value, st);
  if (err != 0) {
    return err;
  }
  data_type_t val_type = get_type(n->as.ret.value);

  if (retExplicit == false) {
    fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
    return 6;
  }
  if (expReturn == TYPE_UNIT) {
    set_type(n, TYPE_UNIT);
    return 0;
  }

  if (get_type(n->as.ret.value) == TYPE_UNKNOWN) {
    fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
    return 6;
  }
  if (!compatible(expReturn, get_type(n->as.ret.value))) {
    fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
    return 6;
  }

  set_type(n, val_type);
  return 0;
}

int check_call(Node *n, symtable_t *st) {
  int err = 0;
  symbol_t *symbol = symtable_find_func(global_st, n->as.call.name);
  if (symbol == NULL) {
    fprintf(stderr, "Chyba (radek %d): Volani nedefinovane funkce %s\n", n->line, n->as.call.name);
    return 3;
  }
  if (symbol->func.param_count != (uint32_t)n->as.call.nargs) {
    fprintf(stderr, "Chyba (radek %d): Neshodny pocet argumentu ve funkci %s\n", n->line,
            n->as.call.name);
    return 5;
  }

  for (int i = 0; i < n->as.call.nargs; i++) {
    err = check_node(n->as.call.args[i], st);
    if (err != 0) {
      return err;
    }
    if (!compatible(symbol->func.param_types[i], get_type(n->as.call.args[i]))) {
      fprintf(stderr, "Chyba (radek %d): Neschodny datatype argumentu funkce %s\n", n->line,
              n->as.call.name);
      return 5;
    }
  }
  if (symbol->func.ret_type == TYPE_UNKNOWN) {
    int idx = find_func_idx(n->as.call.name);
    if (idx >= 0) {
      err = ensure_checked(idx);
      if (err != 0) {
        return err;
      }
    }
  }
  set_type(n, symbol->func.ret_type);
  return 0;
}
int check_unop(Node *n, symtable_t *st) {
  int err = check_node(n->as.unop.operand, st);
  if (err != 0) {
    return err;
  }
  data_type_t t = get_type(n->as.unop.operand);
  if (n->as.unop.op == TOKEN_MINUS) {
    if (t != TYPE_INT && t != TYPE_DOUBLE) {
      fprintf(stderr, "Chyba (radek %d): Operand unarniho minus musi byt typu int nebo double\n",
              n->line);
      return 6;
    }
    set_type(n, t);
    return 0;
  }
  if (n->as.unop.op == TOKEN_NOT) {
    if (t != TYPE_BOOLEAN) {
      fprintf(stderr, "Chyba (radek %d): Operand negace musi byt typu Boolean\n", n->line);
      return 6;
    }
    set_type(n, TYPE_BOOLEAN);
    return 0;
  }
  fprintf(stderr, "Chyba (radek %d): Neznamy unarni operator\n", n->line);
  return 6;
}

int check_node(Node *n, symtable_t *st) {
  result_t ignored;
  switch (n->kind) {
    case N_ASSIGN:
      return check_assign(n, st);
    case N_VARDEF:
      return check_vardef(n, st);
    case N_VAR:
      return check_var(n, st);
    case N_IF:
      return check_if(n, st, &ignored);
    case N_WHILE:
      return check_while(n, st, &ignored);
    case N_PROGRAM:
      return check_program(n, st);
    case N_BLOCK:
      return check_block(n, st, &ignored);
    case N_FUNC:
      return check_func(n);
    case N_BINOP:
      return check_binop(n, st);
    case N_CALL:
      return check_call(n, st);
    case N_UNOP:
      return check_unop(n, st);
    case N_RETURN:
      return check_return(n, st);
    case N_UNIT:
      set_type(n, TYPE_UNIT);
      return 0;
    case N_NUM:
      set_type(n, TYPE_INT);
      return 0;
    case N_DOUBLE:
      set_type(n, TYPE_DOUBLE);
      return 0;
    case N_STRING:
      set_type(n, TYPE_STRING);
      return 0;
    case N_BOOL:
      set_type(n, TYPE_BOOLEAN);
      return 0;
    default:
      return 0;
  }
}
/* projde vsechny funkce a podiva se zda existuje main
 * v pripade vice main vraci 4
 * v pripade zadneho main vraci 3
 * v pripade parametru v main vraci 5
 * v pripade ze return type neni unit vraci 6
 */
int check_main(Node *n) {
  int mainCount = 0;
  for (int i = 0; i < n->as.program.count; i++) {
    if (n->as.program.funcs[i]->as.func.is_main == 1) {
      mainCount++;
      if (n->as.program.funcs[i]->as.func.nparams != 0) {
        fprintf(stderr, "Chyba (radek %d): V definici main je parametr\n", n->line);
        return 5;
      }
      if (from_tree(n->as.program.funcs[i]->as.func.ret) != TYPE_UNIT &&
          from_tree(n->as.program.funcs[i]->as.func.ret) != TYPE_UNKNOWN) {
        fprintf(stderr, "Chyba (radek %d): V return type main neni Unit\n", n->line);
        return 6;
      }
    }
  }
  if (mainCount > 1) {
    fprintf(stderr, "Chyba (radek %d): V souboru je definovano vice funkci main\n", n->line);
    return 4;
  }
  if (mainCount == 0) {
    fprintf(stderr, "Chyba (radek %d): V souboru neni definovana funkce main\n", n->line);
    return 3;
  }
  return 0;
}

int analyze(Node *prog) {
  symtable_t *st = symtable_create();
  if (st == NULL) {
    fprintf(stderr, "Chyba vytvareni symtable\n");
    return INTERNAL_ERROR;
    // chyba alokace
  }

  global_st = st;
  current_prog = prog;
  if (!symtable_insert_builtins(st)) {
    symtable_free(st);
    fprintf(stderr, "Chyba vlozeni vestavnych funkci\n");
    return INTERNAL_ERROR;
  }
  int err = 0;


  if (prog->kind == N_PROGRAM) {
    err = check_main(prog);
    if (err == 0) {
      err = check_program(prog, st);
    }
  } else {
    err = check_node(prog, st);
  }
  current_prog = NULL;
  global_st = NULL;
  symtable_free(st);
  return err;
}