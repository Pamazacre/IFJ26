/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * codegen.h -- generator cieloveho kodu IFJcode26 z AST
 * Autor: xbeniaj00
 *
 *  - vstup je AST z parsera (tree.h), skontrolovany analyzerom (az bude
 *    zapojeny)
 *    typy si generator odvodi sam cez symtable (scopy, uid, navratove typy),
 *    dataType z AST nepouziva
 *    output ide az na konci naraz, pri chybe sa nevypise nic
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "tree.h"
#include <stdio.h>

/* vygeneruje cely program (N_PROGRAM) do out
 * 0 = ok, 99 = malloc alebo nepodporovany uzol (TODO rozsirenia)
 * 3 / 4 / 5 len ako poistka (nedefinovane meno, redefinicia, zly pocet
 * argumentov, chyba @main)
 * kym nie je zapojeny analyzer, inak to chyti uz on */
int codegen(Node *prog, FILE *out);

#endif /* CODEGEN_H */
