/*
 * Implementace překladače imperativního jazyka IFJ26
 *
 * main.c -- vstupni bod, spoji parser, analyzu a generovani kodu
 * Autor: xsenkyp00 (Pavel Šenkýř), xsmrckt00 (Tomáš Smrčka),
 *        xbeniaj00 (Benjamin Joes Benian)
 */

#include <stdio.h>
#include "parser.h"
#include "analyzer.h"
#include "codegen.h"

#include <stdlib.h>

int main(void){
  Node *prog = parse_program(); // pri chybe skonci exit(1/2/99)
  int err = analyze(prog);
  if (err != 0)
    return err;
  return codegen(prog, stdout);
}
