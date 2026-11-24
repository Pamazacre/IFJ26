// xsmrckt00

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//#include "parser.h"
#include "../Analyzer/analyzer.h"
#include "../Tree/tree.h"
#include "../symtable.h"

#include <string.h>

#define INTERNAL_ERROR 99

static int check_node(Node *n, symtable_t *st);
static DataType expReturn = TYPE_UNKNOWN;

int check_vardef(Node *n, symtable_t *st) {
  DataType finalType = n->as.vardef.declared;
  if (n->as.vardef.init != NULL) {
    int err = check_node(n->as.vardef.init, st);
    if (err) {
      fprintf(stderr, "Chyba (radek %d):\n, chybi typ u deklarace promenne \n",
              n->as.vardef.init->line);
      return err;
    }
    if (finalType == TYPE_UNKNOWN) {
      finalType = n->as.vardef.init->dataType;
    }
    if (finalType == TYPE_UNIT) {
      return 4;
    }
    if (finalType != n->as.vardef.init->dataType) {
      return 4;
    }
  } else {
    if (finalType == TYPE_UNKNOWN) {
      return 4;
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
  n->dataType = finalType;
  return 0;
}
int check_var(Node *n, symtable_t *st) {
  if (n->as.var.name != NULL) {
    symbol_t *symbol = symtable_find_var(st, n->as.var.name);
    if (symbol == NULL) {
      fprintf(stderr, "Chyba (radek %d): Nedefinovana promenna %s \n", n->line, n->as.var.name);
      return 3;
    } else {
      n->dataType = symbol->var.type;
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
      fprintf(stderr, "Chyba (radek %d): Nedefinovana promenna %s \n", n->line, n->as.var.name);
      return 3;
    }
    if (symbol->var.is_mutable == false) {
      fprintf(stderr, "Chyba (radek %d): Nelze zapisovat do konstanty %s \n", n->line,
              n->as.assign.name);
      return 7;
    }

    int err = check_node(n->as.assign.value, st);
    if (err) {
      return err;
    }
    fprintf(stderr,"DEBUG - prirazeni do '%s': typ promenne = %d, typ vyrazu = %d\n",
       n->as.assign.name, symbol->var.type, n->as.assign.value->dataType);
    if (symbol->var.type != n->as.assign.value->dataType) {
      fprintf(stderr, "Chyba (radek %d): Nesedi datype promenne %s \n", n->as.assign.value->line, n->as.assign.name);
      return 6;
    }
    return 0;
  }
  return INTERNAL_ERROR;
}
/* zkontrluje datove typy a podle nich a operatoru vrati nastavi datovy typ nadrazeneho node a vrati nulu,
 * v pripade chyby vraci prislusnou hodnotu chyby */
int check_binop(Node *n, symtable_t *st) {
  check_node(n->as.binop.l, st);
  check_node(n->as.binop.r, st);

  if (n->as.binop.l->dataType != n->as.binop.r->dataType || n->as.binop.l->dataType == TYPE_UNIT ||
      n->as.binop.l->dataType == TYPE_UNKNOWN) {
    fprintf(stderr, "Chyba (radek %d): Nesedi datove typy \n", n->line);
    return 6;
  }
  DataType curr = n->as.binop.l->dataType;
  switch (n->as.binop.op) {
    case TOKEN_PLUS:
    case TOKEN_MUL:
    case TOKEN_MINUS:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 10;
      }
      n->dataType = curr;
      break;
    case TOKEN_DIV:
      if (curr == TYPE_BOOLEAN || curr == TYPE_UNIT || curr == TYPE_STRING) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 10;
      }
      n->dataType = curr;
      break;
    case TOKEN_GE:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_LT:
      if (curr == TYPE_STRING) {
        fprintf(stderr, "Chyba (radek %d): Nelze provezt operaci %d s timto datovym typem \n",
                n->line, n->as.binop.op);
        return 10;
      }
      n->dataType = TYPE_BOOLEAN;
      break;
    case TOKEN_EQ:
      n->dataType = TYPE_BOOLEAN;
      break;
      // prostor pro rozsireni operatoru and or neg

    default:
      break;
  }
  return 0;
}
// projde blok a zkontroluje kazdy jednotivy node v bloku
int check_block(Node *n, symtable_t *st) {
  symtable_push_scope(st);
  for (int i = 0; i < n->as.block.count; i++) {
    int err = check_node(n->as.block.stmts[i], st);
    if (err != 0) {
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
  DataType condDT = n->as.while_.cond->dataType;
  if (condDT != TYPE_BOOLEAN ) {

    return 4;
  }
  err = check_block(n->as.while_.body, st);
  if (err != 0) {
    return err;
  }
  return 0;
}
// zkontoluje podminku a bloky then a pokud existuje else tak zkontroluje to
int check_if(Node *n, symtable_t *st) {
  int err = 0;

  err = check_node(n->as.if_.cond, st);
  if (err != 0) {
    return err;
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
  fprintf(stderr,"DEBUG return datatype: %d funkce: %s \n",n->as.func.ret,n->as.func.name);

  st_status_t status = symtable_insert_func(st, n->as.func.name, n->as.func.ret, &func);
  if (status == ST_ERR_NO_SCOPE || status == ST_ERR_REDEF) {
    return 4;
  }
  if (status == ST_ERR_MEMORY) {
    return INTERNAL_ERROR;
  }
  for (int i = 0; i < n->as.func.nparams; i++) {
    bool err = symtable_add_param(func, n->as.func.params[i].type);
    if (!err) {
      symtable_pop_scope(st);
      return INTERNAL_ERROR;
    }
  }
  return 0;
}
int check_func(Node *n, symtable_t *st){
  if (!symtable_push_scope(st)) {
    return INTERNAL_ERROR;
  }
  for (int i = 0; i < n->as.func.nparams; i++) {
    st_status_t statusVar =
        symtable_insert_var(st, n->as.func.params[i].name, n->as.func.params[i].type, false, NULL);
    if (statusVar == ST_ERR_REDEF) {
      symtable_pop_scope(st);
      return 4;
    }
    if (statusVar != ST_OK) {
      symtable_pop_scope(st);
      return INTERNAL_ERROR;
    }
  }
  DataType prevReturn = expReturn;
  expReturn = n->as.func.ret;
  fprintf(stderr,"DEBUG: expReturn: %d \n",expReturn);
  int err = check_node(n->as.func.body, st);

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
// Na začátek souboru analyzer.c (nad funkce):
//static DataType expReturn = TYPE_UNKNOWN;

int check_return(Node *n, symtable_t *st) {
  // 1. Prázdný příkaz "return;" (bez výrazu) -> vrací TYPE_UNIT
  if (n->as.ret.value == NULL) {
    if (expReturn != TYPE_UNIT && expReturn != TYPE_UNKNOWN) {
      fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
      return 6;
    }
    n->dataType = TYPE_UNIT;
    return 0;
  }

  // 2. Vyhodnotíme výraz za "return", aby se mu doplnil dataType
  int err = check_node(n->as.ret.value, st);
  if (err != 0) {
    return err;
  }

  // 3. Porovnáme typ výrazu s očekávaným návratovým typem funkce
  if (expReturn != TYPE_UNKNOWN && expReturn != n->as.ret.value->dataType) {
    fprintf(stderr, "Chyba (radek %d): Neschoda navratoveho typu\n", n->line);
    return 6;
  }

  n->dataType = n->as.ret.value->dataType;
  return 0;
}
// konec ai kodu
int check_call(Node *n, symtable_t *st) {
  symbol_t *symbol = symtable_find_func(st, n->as.call.name);
  if (symbol == NULL) {
    fprintf(stderr, "Chyba (radek %d): Volani nedefinovane funkce '%s'", n->line, n->as.call.name);
    return 3;
  }
  if (symbol->func.param_count != (uint32_t)n->as.call.nargs) {
    fprintf(stderr, "Chyba (radek %d): Neshodny pocet argumentu ve funkci %s", n->line,
            n->as.call.name);
    return 5;
  }

  for (int i = 0; i < n->as.call.nargs; i++) {
    check_node(n->as.call.args[i], st);

    if (n->as.call.args[i]->dataType != symbol->func.param_types[i]) {
      fprintf(stderr, "Chyba (radek %d): Neschodny datatype argumentu funkce %s", n->line,
              n->as.call.name);
      return 5;
    }
  }
  fprintf(stderr,"DEBUG navratova hodnota puvodni dataType %d novy %d\n",n->dataType, symbol->func.ret_type);
  n->dataType = symbol->func.ret_type;
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
    // case N_UNOP:
    //     return check_unop(n,st);
    case N_RETURN:
      return check_return(n, st);
    case N_UNIT:
      n->dataType = TYPE_UNIT;
      return 0;
    case N_NUM:
      n->dataType = TYPE_INT;
      return 0;
    case N_DOUBLE:
      n->dataType = TYPE_DOUBLE;
      return 0;
    case N_STRING:
      n->dataType = TYPE_STRING;
      return 0;
    default:
      return 0;
  }
}
/* projde vsechny funkce a podiva se zda existuje main
*/
int check_main(Node *n) {
  int mainCount = 0;
  for (int i = 0; i < n->as.program.count; i++) {
    if (n->as.program.funcs[i]->as.func.is_main == 1) {
      mainCount++;
      if (n->as.program.funcs[i]->as.func.nparams != 0) {
        fprintf(stderr, "Chyba (radek %d): V definici main je parametr",n->line);
        return 5;
      }
      if (n->as.program.funcs[i]->as.func.ret != TYPE_UNIT) {
        fprintf(stderr, "Chyba (radek %d): V return type main neni Unit",n->line);
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
  // Node *prog = parse_program();
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
    if (err ==0) {
      err = check_program(prog, st);
    }
  }
  else {
    symtable_free(st);
    err = check_node(prog, st);
  }

  symtable_free(st);
  return err;
}
