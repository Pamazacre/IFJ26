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
/* nie je to FNV ani djb2 ale inspiroval som sa nimi, je to
 * kombinacia rotate + xor + nasobenie, hladal som optimalnu funkciu pre
 * hashovanie stringov
 * https://www.reddit.com/r/C_Programming/comments/i1oj5w/hash_functions/
 * kazdy byte sa najprv zrotuje h o 7 dolava (aby sa bity posuvali po slove)
 * potom xor s bytom * pozicia (pozicia preto aby "ab" a "ba" nedali to iste,
 * bez toho by sa pri cistom xor anagramy bili) a nakoniec * neparna konst
 * (index = hash & mask) tak nech do nich pretecie aj to co je hore.
 * konstanty - seed je golden ratio, 0x85EBCA6B je z murmur3 finalizera,
 * 0x2C1B3C6D je len nahodne neparne cislo, musi byt neparne inak se
 * stratia bity pri nasobeny */
static uint32_t hash_str(const char *s) {
  uint32_t h = 0x9E3779B9U; /* seed */
  uint32_t pos = 1;
  for (; *s != '\0'; s++, pos++) {
    h = (h << 7) | (h >> 25);               /* rotl 7 */
    h ^= (uint32_t)(unsigned char)*s * pos; /* byte vazeny poziciou */
    h *= 0x2C1B3C6DU;
  }
  h ^= h >> 16; /* horne bity dole */
  h *= 0x85EBCA6BU;
  h ^= h >> 13;
  return h; /* prazdny string -> vrati len "zamiesany" seed tiez ok */
}
/* ---Ai prompt-- model: gemini
 * How can I check whether my hash func works well with strings
 * im concerned about similar variable names producing
 * too many collisions. What should I look at? */

/* hladanie kluca v jednej tabulke, NULL ked tam nie je
 * ideme od home slotu hash a mask a linearne dalej kym nenarazime na
 * NULL slot -> vtedy vieme ze tam kluc urcite snad neexistuje
 * TODO// check
 * po jednom takze v "retazci" nemozu byt dieri)
 * nekonecny cyklus nehrozi lebo tabulka je max z polovice plna */
static node_t *table_find(const table_t *t, const char *key) {
  if (t->count == 0)
    return NULL; /* prazdna tabulka moze mat cap 0 a slots NULL, pozor */
  size_t mask = t->cap - 1;
  size_t i = hash_str(key) & mask;
  while (t->slots[i] != NULL) {
    if (strcmp(t->slots[i]->name, key) == 0)
      return t->slots[i];
    i = (i + 1) & mask; /* dalsi slot, wrap okolo konca */
  }
  return NULL;
}

/* vlozi n do prveho volneho slotu od jeho home slotu
 * predpoklad: kluc tam este nie je a volny slot existuje inak sa loopne
 * -> volat len z table_insert ktory to garantuje */
static void table_place(table_t *t, node_t *n) {
  size_t mask = t->cap - 1;
  size_t i = n->hash & mask; /* hash mame ulozeny, nepocita sa znova */
  while (t->slots[i] != NULL)
    i = (i + 1) & mask;
  t->slots[i] = n;
}
/* --Ai prompt-- model: claude sonnet 5.5 - low
 * help me understand insertion of elements into a hashtable, whatif 2 diff
 * elements have the same index, idk how to handle that, what are some examples
 * to help me understand */
/* insert noveho unique  kluca
 * ked by sme prekrocili load 1/2 tak najprv zdvojnasobime pole slotov
 * a vsetky stare nody rehashneme, preto si node pamata hash
 * TODO//
 * idx = (idx + 1) & mask;  - solution na nasu otazku
 */

static bool table_insert(table_t *t, node_t *n) {
  if ((t->count + 1) * 2 > t->cap) {
    size_t cap = t->cap ? t->cap * 2 : 8;
    node_t **slots = calloc(cap, sizeof(node_t *)); /* calloc = vsetko NULL */
    if (slots == NULL)
      return false;
    table_t grown = {slots, cap, t->count};
    for (size_t i = 0; i < t->cap; i++)
      if (t->slots[i] != NULL)
        table_place(&grown, t->slots[i]); /* nody sa nekopiruju, len ptr */
    free(t->slots); /* stare pole slotov, nody ziju dalej */
    *t = grown;
  }
  table_place(t, n);
  t->count++;
  return true;
}

/* uvolni node, pri funkcii aj jej pole param typov */
static void node_free(node_t *n) {
  if (n->sym.kind == SYM_FUNC)
    free(n->sym.func.param_types);
  free(n); /* meno je vo vnutri node-u takze sa uvolni s nim */
}

/* uvolni cele obsah tabulky + pole slotov, necha ju prazdnu (cap 0) */
static void table_free(table_t *t) {
  for (size_t i = 0; i < t->cap; i++)
    if (t->slots[i] != NULL)
      node_free(t->slots[i]);
  free(t->slots);
  t->slots = NULL;
  t->cap = 0;
  t->count = 0;
}

/* alokuje novy node nulled ako nulovany aj s kopiou mena v jednom bloku
 * NULL ked calloc zlyha */
static node_t *node_new(const char *name, sym_kind_t kind) {
  size_t len = strlen(name) + 1; /* +1 kvoli '\0' */
  node_t *n = calloc(1, sizeof *n + len);
  if (n == NULL)
    return NULL;
  memcpy(n->name, name, len);
  n->sym.name = n->name; /* public ptr ukazuje na nasu kopiu */
  n->sym.kind = kind;
  n->hash = hash_str(name);
  return n;
}
