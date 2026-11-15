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
