/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * symtable.c -- TRP-izp: hash table s otvorenym adresovanim
 * xbeniaj00
 */

#include "symtable.h"
#include <stdint.h> /* pre hash kalkulaciu */
#include <stdlib.h>
#include <string.h>

/* pre future references array = arr */
typedef struct node {
  symbol_t sym; /* data symbolu */
  uint32_t
      hash; /* no error pri zvacsovani tabulky - predvypocitana hodnota hashu */
  char name[]; /* flexible arr - proti uniku pamate cez malloc - meno priamo do
                  memory bloku */
} node_t;

/* hash table implementacia if slot NULL znamena ze je prazzdny */
typedef struct {
  node_t **slots; /* dyn arr ukazovatelov */
  size_t cap;     /* celkova kapacita slotov */
  size_t count;
} table_t;

/* main struktura symtable */
struct symtable {
  table_t functions;  /* global hash table v ktorej su ulozene funkcie */
  table_t *scopes;    /* dyn arr tabuliek "table_t" - stack struktura */
  size_t scope_count; /* count tabuliek v scope range */
  size_t scope_cap;   /* kapacita stacku */
  unsigned next_uid;  /* global ID pre premenne ++ pri kazdej novej */
};

/* Implementacia hash tabulky */
