/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * symtable.c -- TRP-izp: hash table s otvorenym adresovanim
 * Autor: xbeniaj00 (Benjamin Joes Benian)
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

/* moja hash fcia - nie FNV ani djb2, kombinacia rotate + xor + nasobenie
 * kazdy byte sa najprv zrotuje h o 7 dolava (aby sa bity posuvali po slove)
 * potom xor s bytom * pozicia (pozicia preto aby "ab" a "ba" nedali to iste,
 * bez toho by sa pri cistom xor anagramy bili) a nakoniec * neparna konst
 * na konci este avalanche (xor shift) lebo nas zaujimaju len spodne bity
 * (index = hash & mask) tak nech do nich pretecie aj to co je hore.
 * konstanty - seed je golden ratio, 0x85EBCA6B je z murmur3 finalizera,
 * 0x2C1B3C6D je len nahodne neparne cislo, musi byt neparne!"! inak sa
 * stratia bity pri nasobeny */
static uint32_t hash_str(const char *s) {
  uint32_t h = 0x9E3779B9U; /* seed */
  uint32_t pos = 1;
  for (; *s != '\0'; s++, pos++) {
    h = (h << 7) | (h >> 25);               /* rotl 7 */
    h ^= (uint32_t)(unsigned char)*s * pos; /* byte vazeny poziciou */
    h *= 0x2C1B3C6DU;
  }
  h ^= h >> 16; /* avalanche - horne bity dole */
  h *= 0x85EBCA6BU;
  h ^= h >> 13;
  return h; /* prazdny string -> vrati len "zamiesany" seed, tiez ok */
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

/* team funkcie */

symtable_t *symtable_create(void) {
  /* calloc -> vsetko 0 / NULL = prazdne tabulky, prazdny stack */
  return calloc(1, sizeof(symtable_t));
}

void symtable_free(symtable_t *st) {
  if (st == NULL)
    return;
  while (st->scope_count > 0) /* ak niekto zabudol popnut scopy */
    symtable_pop_scope(st);
  table_free(&st->functions);
  free(st->scopes);
  free(st);
}

bool symtable_push_scope(symtable_t *st) {
  if (st->scope_count == st->scope_cap) { /* stack plny -> zvacsit */
    size_t cap = st->scope_cap ? st->scope_cap * 2 : 8;
    table_t *scopes = realloc(st->scopes, cap * sizeof *scopes);
    if (scopes == NULL)
      return false; /* realloc fail = stare pole ostava platne */
    st->scopes = scopes;
    st->scope_cap = cap;
  }
  table_t empty = {NULL, 0, 0}; /* nova tabulka bez slotov, alokuje sa az
                                   pri prvom inserte */
  st->scopes[st->scope_count++] = empty;
  return true;
}

void symtable_pop_scope(symtable_t *st) {
  if (st->scope_count == 0)
    return; /* nic na popnutie, radsej ticho nez crash */
  st->scope_count--;
  table_free(&st->scopes[st->scope_count]); /* tu umieraju ptr na symboly! */
}
st_status_t symtable_insert_var(symtable_t *st, const char *name,
                                data_type_t type, bool is_mutable,
                                symbol_t **out) {
  if (out != NULL)
    *out = NULL;
  if (st->scope_count == 0)
    return ST_ERR_NO_SCOPE; /* premenna mimo?? to je chyba parsera */

  table_t *top = &st->scopes[st->scope_count - 1]; /* len najvnutornejsi */
  node_t *existing = table_find(top, name);
  if (existing != NULL) { /* uz je v tomto bloku -> redefinicia err 4 */
    if (out != NULL)
      *out = &existing->sym;
    return ST_ERR_REDEF;
  }

  node_t *n = node_new(name, SYM_VAR);
  if (n == NULL)
    return ST_ERR_MEMORY;
  n->sym.var.type = type;
  n->sym.var.is_mutable = is_mutable;
  if (!table_insert(top, n)) {
    node_free(n); /* nepodarilo sa zvacsit tabulku, node zahodime */
    return ST_ERR_MEMORY;
  }
  n->sym.var.uid = st->next_uid++; /* az ked je naozaj vlozeny, aby sa
                                      uid "neminulo" pri chybe */

  if (out != NULL)
    *out = &n->sym;
  return ST_OK;
}

symbol_t *symtable_find_var(const symtable_t *st, const char *name) {
  /* od vrchu stacku dole -> najvnutornejsia definicia vyhrava (shadowing) */
  for (size_t i = st->scope_count; i > 0; i--) {
    node_t *n = table_find(&st->scopes[i - 1], name);
    if (n != NULL)
      return &n->sym;
  }
  return NULL; /* nikde -> error 3 */
}

/* helper: vytvori node funkcie a hodi ho do globalnej tabulky
 * pouziva insert_func aj declare_func, NULL pri chybe alokacie */
static node_t *func_new(symtable_t *st, const char *name, data_type_t ret_type,
                        bool defined) {
  node_t *n = node_new(name, SYM_FUNC);
  if (n == NULL)
    return NULL;
  n->sym.func.ret_type = ret_type;
  n->sym.func.ret_explicit = (ret_type != TYPE_UNKNOWN); /* UNKNOWN = nebol
                                                            v hlavicke */
  n->sym.func.defined = defined;
  if (!table_insert(&st->functions, n)) {
    node_free(n);
    return NULL;
  }
  return n;
}

st_status_t symtable_insert_func(symtable_t *st, const char *name,
                                 data_type_t ret_type, symbol_t **out) {
  if (out != NULL)
    *out = NULL;

  node_t *existing = table_find(&st->functions, name);
  if (existing != NULL) {
    if (out != NULL)
      *out = &existing->sym;
    if (existing->sym.func.defined)
      return ST_ERR_REDEF; /* uz definovana (aj builtin) -> error 4 */
    /* bola len zavolana skor nez definovana (forward zaznam z declare_func)
     * takze ju teraz "dokoncime" na mieste, ptr na nu ostava platny
     * parametre zahodime, dava ich az definicia cez add_param */
    existing->sym.func.defined = true;
    existing->sym.func.ret_type = ret_type;
    existing->sym.func.ret_explicit = (ret_type != TYPE_UNKNOWN);
    free(existing->sym.func.param_types);
    existing->sym.func.param_types = NULL;
    existing->sym.func.param_count = 0;
    return ST_OK;
  }

  node_t *n = func_new(st, name, ret_type, true);
  if (n == NULL)
    return ST_ERR_MEMORY;
  if (out != NULL)
    *out = &n->sym;
  return ST_OK;
}

st_status_t symtable_declare_func(symtable_t *st, const char *name,
                                  symbol_t **out) {
  if (out != NULL)
    *out = NULL;

  node_t *n = table_find(&st->functions, name);
  if (n == NULL) { /* este o nej nevieme -> forward zaznam, defined=false */
    n = func_new(st, name, TYPE_UNKNOWN, false);
    if (n == NULL)
      return ST_ERR_MEMORY;
  }
  /* ak uz existuje (definovana alebo nie) tak len vratime tu istu */
  if (out != NULL)
    *out = &n->sym;
  return ST_OK;
}

symbol_t *symtable_find_func(const symtable_t *st, const char *name) {
  node_t *n = table_find(&st->functions, name);
  return n != NULL ? &n->sym : NULL;
}

bool symtable_add_param(symbol_t *func, data_type_t type) {
  if (func == NULL || func->kind != SYM_FUNC)
    return false; /* niekto poslal premennu, nech to nespadne potichu */
  size_t count = func->func.param_count;
  data_type_t *types =
      realloc(func->func.param_types, (count + 1) * sizeof *types);
  if (types == NULL)
    return false; /* povodne pole ostava, nic sa nezmenilo */
  types[count] = type;
  func->func.param_types = types;
  func->func.param_count = count + 1;
  return true;
}

bool symtable_insert_builtins(symtable_t *st) {
  /* vstavane fcie zo specifikacie (sekcia 4), Any = TYPE_ANY
   * poradie poli v structe je kvoli padingu (clang-tidy sa stazoval) */
  static const struct {
    const char *name;
    size_t param_count;
    data_type_t ret;
    data_type_t params[3]; /* max 3 (subStr), zvysok UNKNOWN = nepouzite */
  } builtins[] = {
      {"print", 1, TYPE_UNIT, {TYPE_ANY}},
      {"readLine", 0, TYPE_STRING, {TYPE_UNKNOWN}},
      {"readInt", 0, TYPE_INT, {TYPE_UNKNOWN}},
      {"readDouble", 0, TYPE_DOUBLE, {TYPE_UNKNOWN}},
      {"toStr", 1, TYPE_STRING, {TYPE_ANY}},
      {"toInt", 1, TYPE_INT, {TYPE_DOUBLE}},
      {"strLength", 1, TYPE_INT, {TYPE_STRING}},
      {"subStr", 3, TYPE_STRING, {TYPE_STRING, TYPE_INT, TYPE_INT}},
      {"ord", 2, TYPE_INT, {TYPE_STRING, TYPE_INT}},
      {"chr", 1, TYPE_STRING, {TYPE_INT}},
      {"readBoolean", 0, TYPE_BOOLEAN, {TYPE_UNKNOWN}},
  };

  for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++) {
    symbol_t *f = NULL;
    if (symtable_insert_func(st, builtins[i].name, builtins[i].ret, &f) !=
        ST_OK)
      return false; /* bud malloc fail alebo uz tam su (volane 2x?) */
    f->func.is_builtin = true;
    for (size_t p = 0; p < builtins[i].param_count; p++)
      if (!symtable_add_param(f, builtins[i].params[p]))
        return false;
  }
  return true;
}

void symtable_foreach_func(symtable_t *st, void (*fn)(symbol_t *sym, void *ctx),
                           void *ctx) {
  /* prejde vsetky sloty, poradie je "nahodne" (podla hashu), nie abecedne */
  for (size_t i = 0; i < st->functions.cap; i++)
    if (st->functions.slots[i] != NULL)
      fn(&st->functions.slots[i]->sym, ctx);
}

// EOF-koniec symtable.c konecne
