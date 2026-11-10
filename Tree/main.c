#include <stdio.h>
#include "parser.h"

int main(void)
{
  Node *prog = parse_program();

  // debug vystuup
  printf("OK, pocet funkci: %d\n", prog->as.program.count);
  for (int i = 0; i < prog->as.program.count; i++)
  {
    Node *f = prog->as.program.funcs[i];
    printf("  %s  params=%d  main=%d  ret=%d\n", f->as.func.name, f->as.func.nparams, f->as.func.is_main, f->as.func.ret);
  }
  return 0;
}