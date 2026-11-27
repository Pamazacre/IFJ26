/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * symtable.h -- tabulka symbolov (ADT), varianta TRP-izp
 *
 * Autor: xbeniaj00 (Benjamin Joes Benian)
 *
 *  - ptr na symbol plati kym sa nepopne jeho scope!?! (funkcie az do free)
 *    takze co treba neskor (typ, uid) si treba skopirovat do AST
 *  - name a param_types nemenit zvonku, patria tabulke
 *  - error kody: nenajdene 3, redefinicia 4, zapis do val 7, malloc 99
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

/* typy jazyka, zatial len flat enum (bez rozsireni to staci) */
typedef enum {
  TYPE_UNKNOWN, /* este nevieme, napr. navratovy typ co sa odvodi neskor */
  TYPE_INT,
  TYPE_DOUBLE,
  TYPE_STRING,
  TYPE_UNIT,
  TYPE_BOOLEAN, /* len interne v podmienkach, bez BOOLTHEN nie je typ premennej
                 */
  TYPE_ANY      /* param print() a toStr(), inak nikde */
} data_type_t;

/* ci je to premenna alebo funkcia */
typedef enum { SYM_VAR, SYM_FUNC } sym_kind_t;

/* jeden zaznam v tabulke, pamat vlastni tabulka */
typedef struct {
  const char *name; /* identifikator, read only */
  sym_kind_t kind;
  struct { /* plati len pre SYM_VAR */
    data_type_t type;
    bool is_mutable; /* var = true, val a parametre = false -> zapis je error 7
                      */
    unsigned uid;    /* unikatne cez cely program, pre generator */
  } var;
  struct {                /* plati len pre SYM_FUNC */
    data_type_t ret_type; /* UNKNOWN kym sa neodvodi */
    bool ret_explicit;    /* bol typ napisany v hlavicke? */
    bool defined;         /* false = zatial len zavolana (forward zaznam) */
    bool is_builtin;      /* vlozena cez symtable_insert_builtins */
    bool is_main;         /* @main, nastavuje parser */
    size_t param_count;
    data_type_t *param_types; /* typy parametrov v poradi, read only */
  } func;
} symbol_t;

/* vysledok insertu */
typedef enum {
  ST_OK,
  ST_ERR_REDEF,    /* uz existuje v tom istom scope -> error 4 */
  ST_ERR_NO_SCOPE, /* insert premennej bez otvoreneho scope = bug v parseri */
  ST_ERR_MEMORY    /* malloc zlyhal -> error 99 */
} st_status_t;

/* samotna tabulka, struct je schovany v symtable.c (ADT) */
typedef struct symtable symtable_t;

/* nova prazdna tabulka, ziadne funkcie ani scope, NULL ked malloc zlyha */
symtable_t *symtable_create(void);

/* uvolni kompletne vsetko Null je solved tak isto ako coding... */
void symtable_free(symtable_t *st);

/* otvori novy scope (telo funkcie, if, while..), false => malloc fail */
bool symtable_push_scope(symtable_t *st);

/* zavrie najvnutornejsi scope a uvolni jeho symboly (ptr na ne uz neplatia)
 * ked nie je co poopnut tak nerobi nic */
void symtable_pop_scope(symtable_t *st);

/* vlozi premennu (alebo parameter) do najvnutornejsieho scope
 * ST_OK -> *out je novy symbol
 * ST_ERR_REDEF -> *out je ten co uz tam bol (ten isty scope!)
 * inak *out NULL, out moze byt NULL ak nas nezaujima */
st_status_t symtable_insert_var(symtable_t *st, const char *name,
                                data_type_t type, bool is_mutable,
                                symbol_t **out);

/* hlada premennu od vnutra von, NULL = nikde nie je (error 3) */
symbol_t *symtable_find_var(const symtable_t *st, const char *name);

/* definicia funkcie v globalnom scope
 * ret_type TYPE_UNKNOWN ked hlavicka nema typ (ret_explicit bude false)
 * zacina bez parametrov, pridavaju sa cez symtable_add_param v poradi
 *  ST_OK        nova funkcia ALEBO sa dokoncil forward zaznam z declare_func
 *               (ten uz je defined, jeho parametre sa resetli)
 *  ST_ERR_REDEF uz je definovana (aj builtin) -> error 4, *out = existujuca
 *  ST_ERR_MEMORY malloc, *out NULL
 * out moze byt NULL */
st_status_t symtable_insert_func(symtable_t *st, const char *name,
                                 data_type_t ret_type, symbol_t **out);

/* volanie funkcie ktora este nemusi byt definovana (volanie pred definiciou,
 * vzajomna rekurzia). ak existuje tak *out = ona, inak sa vytvori forward
 * zaznam s defined=false, ret UNKNOWN, bez parametrov
 * typy argumentov z volania si musi pamatat parser a skontrolovat ich az
 * ked bude funkcia definovana (alebo na konci cez foreach)
 * ST_OK alebo ST_ERR_MEMORY, out moze byt NULL */
st_status_t symtable_declare_func(symtable_t *st, const char *name,
                                  symbol_t **out);

/* najde funkciu (definovanu aj forward), NULL = nie je (error 3) */
symbol_t *symtable_find_func(const symtable_t *st, const char *name);

/* prida jeden typ parametra na koniec, false = malloc fail alebo to nie je
 * funkcia, symbol ostava nezmeneny */
bool symtable_add_param(symbol_t *func, data_type_t type);

/* vlozi vstavane fncie zo specifikacie sekcia (print, readLine, readInt,
 * readDouble, toStr, toInt, strLength, subStr, ord, chr) aj s typmi,
 * oznacene is_builtin + defined. volat raz hned po create!
 * false = malloc alebo uz tam nejaka bola */
bool symtable_insert_builtins(symtable_t *st);

/* zavola fn(symbol, ctx) pre kazdu funkciu, poradie je dane hashom (nie
 * abecedne). na kontrolu na konci parsovania: forward zaznam co sa nikdy
 * nedefinoval -> error 3, hladanie @main, generator......
 * fn nesmie nic vkladat do tabulky rehash by to rozbil) */
void symtable_foreach_func(symtable_t *st, void (*fn)(symbol_t *sym, void *ctx),
                           void *ctx);

#endif /* SYMTABLE_H */
