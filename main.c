#include <stdio.h>
#include "parser.h"

int main(void){
  Node *prog = parse_program(); // pri chybe skonci exit(1/2/99)
  (void)prog; // TODO: analyze(prog) a codegen(prog, stdout) az budou v repu
  return 0;
}
