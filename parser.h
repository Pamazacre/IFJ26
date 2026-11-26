/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * parser.h -- rozhrani parseru, parse_program() vraci koren AST
 * Autor: xsenkyp00 (Pavel Šenkýř)
 */

#ifndef PARSER_H
#define PARSER_H

#include "tree.h"

// syntakticka analyza celeho programu ze stdin, vraci koren AST (N_PROGRAM),
// pri chybe ukonci program s kodem 1, 2 nebo 99
Node *parse_program(void);

#endif