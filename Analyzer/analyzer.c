// xsmrckt00

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#include "analyzer.h"
#include "../Tree/tree.h"
#include "../symtable.h"


#define INTERNAL_ERROR 99

static int check_node(Node *n, symtable_t *st);

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

static data_type_t get_type(Node *n) {
  return from_tree(n->dataType);
}

static void set_type(Node *n, data_type_t t) {
  n->dataType = to_tree(t);
}

static data_type_t expReturn = TYPE_UNKNOWN;

static bool compatible(data_type_t expected, data_type_t actual) {
  if (expected == TYPE_ANY) {
    return true;
  }
  return expected == actual || (expected == TYPE_DOUBLE && actual == TYPE_INT);
}

int check_vardef(Node *n, symtable_t *st) {
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
  } else {
    fprintf(stderr, "Chyba (radek %d): Promenna nema jmeno\n", n->line);
    return 10;
  }
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
  } else if (lt == rt  && lt != TYPE_UNKNOWN) {
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
      // prostor pro rozsireni operatoru and or neg

    default:
      fprintf(stderr, "Chyba (radek %d): Neznamy operator\n", n->line);
      return INTERNAL_ERROR;
  }
  return 0;
}
/* projde blok a zavola nad kazdym nejvechrnejsim check node
 * ten mu vrati datatype ktery muze kontrolovat */

int check_block(Node *n, symtable_t *st) {
  if (!symtable_push_scope(st))
    return INTERNAL_ERROR;
  for (int i = 0; i < n->as.block.count; i++) {
    int err = check_node(n->as.block.stmts[i], st);
    if (err != 0) {
      symtable_pop_scope(st);
      return err;
    }
  }
  symtable_pop_scope(st);
  return 0;
}
int check_while(Node *n, symtable_t *st) {
  int err = 0;
  err = check_node(n->as.while_.cond, st);
  if (err != 0) {

    return err;
  }
  data_type_t condDT = get_type(n->as.while_.cond);
  if (condDT != TYPE_BOOLEAN) {
    return 6;
  }
  err = check_block(n->as.while_.body, st);
  if (err != 0) {
    return err;
  }
  return 0;
}
// zkontroluje podminku a blok then a pokud existuje else tak zkontroluje to
int check_if(Node *n, symtable_t *st) {
  int err = 0;

  err = check_node(n->as.if_.cond, st);
  if (err != 0) {
    return err;
  }
  if (get_type(n->as.if_.cond) != TYPE_BOOLEAN) {
    fprintf(stderr, "Chyba (radek %d): Podminka neni typu Boolean\n", n->line);
    return 6;
  }
  err = check_block(n->as.if_.then_b, st);
  if (err != 0) {
    return err;
  }
  if (n->as.if_.else_b != NULL) {
    err = check_block(n->as.if_.else_b, st);
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

int add_func(Node *n, symtable_t *st) {
  symbol_t *func = NULL;
  st_status_t status =
      symtable_insert_func(st, n->as.func.name, from_tree(n->as.func.ret), &func);
  if (status == ST_ERR_NO_SCOPE || status == ST_ERR_REDEF) {
    return 4;
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
int check_func(Node *n, symtable_t *st) {
  if (!symtable_push_scope(st)) {
    return INTERNAL_ERROR;
  }
  for (int i = 0; i < n->as.func.nparams; i++) {
    st_status_t statusVar = symtable_insert_var(
        st, n->as.func.params[i].name, from_tree(n->as.func.params[i].type), false, NULL);
    if (statusVar == ST_ERR_REDEF) {
      symtable_pop_scope(st);
      return 4;
    }
    if (statusVar != ST_OK) {
      symtable_pop_scope(st);
      return INTERNAL_ERROR;
    }
  }
  data_type_t prevReturn = expReturn;
  expReturn = from_tree(n->as.func.ret);
  int err = 0;

  for (int i = 0; i < n->as.func.body->as.block.count; i++) {
    err = check_node(n->as.func.body->as.block.stmts[i], st);
    if (err != 0) {
      break;
    }
  }

  expReturn = prevReturn;
  symtable_pop_scope(st);
  return err;
}
int check_program(Node *n, symtable_t *st) {

  int err;
  for (int i = 0; i < n->as.program.count; i++) {
    err = add_func(n->as.program.funcs[i], st);
    if (err != 0) {
      return err;
    }
  }
  for (int i = 0; i < n->as.program.count; i++) {
    err = check_func(n->as.program.funcs[i], st);
    if (err != 0) {
      return err;
    }
  }
  return 0;
}
/*
 *  Pouziti Ai pro dopsani funkce check_return model Gemini 4 Hydrogen Thinking
 *  promt:
* dopis mi funkci check_return z tohoto za pouziti staticke promene epxReturn

int check_return(Node *n, symtable_t *st) {
  if (expReturn !=n->as.ret.value->dataType) {
    fprintf(stderr,"Chyba (radek %d): Neschoda navratoveho typu");
  }
  return 0;
}
*/

int check_return(Node *n, symtable_t *st) {
  // 1. Prázdný příkaz "return;" (bez výrazu) -> vrací TYPE_UNIT
  if (n->as.ret.value == NULL) {
    if (expReturn != TYPE_UNIT && expReturn != TYPE_UNKNOWN) {
      fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
      return 6;
    }
    set_type(n, TYPE_UNIT);
    return 0;
  }

  // 2. Vyhodnotíme výraz za "return", aby se mu doplnil dataType
  int err = check_node(n->as.ret.value, st);
  if (err != 0) {
    return err;
  }

  // 3. Porovnáme typ výrazu s očekávaným návratovým typem funkce
  data_type_t valType = get_type(n->as.ret.value);
  if (expReturn != TYPE_UNKNOWN && !compatible(expReturn, valType)) {
    fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
    return 6;
  }

  set_type(n, valType);
  return 0;
}
// konec ai kodu
int check_call(Node *n, symtable_t *st) {
  int err = 0;
  symbol_t *symbol = symtable_find_func(st, n->as.call.name);
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
  set_type(n, symbol->func.ret_type);
  return 0;
}
int check_unop(Node *n, symtable_t *st) {
  int err = check_node(n->as.unop.operand, st);
  if (err != 0) {
    return err;
  }
  if (n->as.unop.op != TOKEN_MINUS) {
    fprintf(stderr, "Chyba (radek %d): Neznamy unarni operator\n", n->line);
    return 6;
  }
  data_type_t t = get_type(n->as.unop.operand);
  if (t != TYPE_INT && t != TYPE_DOUBLE) {
    fprintf(stderr, "Chyba (radek %d): Operand unarniho minus musi byt typu int nebo double\n",
            n->line);
    return 6;
  }
  set_type(n, t);
  return 0;
}

int check_node(Node *n, symtable_t *st) {

  switch (n->kind) {

    case N_ASSIGN:
      return check_assign(n, st);
    case N_VARDEF:
      return check_vardef(n, st);
    case N_VAR:
      return check_var(n, st);
    case N_IF:
      return check_if(n, st);
    case N_WHILE:
      return check_while(n, st);
    case N_PROGRAM:
      return check_program(n, st);
    case N_BLOCK:
      return check_block(n, st);
    case N_FUNC:
      return check_func(n, st);
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
      if (from_tree(n->as.program.funcs[i]->as.func.ret) != TYPE_UNIT) {
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

  symtable_free(st);
  return err;
}