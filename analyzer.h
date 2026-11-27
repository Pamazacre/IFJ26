/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * analyzer.h -- rozhrani semanticke analyzy
 * Autor: xsmrckt00 (Tomáš Smrčka)
 */

#ifndef IFJ26_ANALYZER_H
#define IFJ26_ANALYZER_H
#include "tree.h"
/*
 * Provede semantickou analyzu jako argument predat Node obsahujici program */
int analyze(Node *prog);
#endif
