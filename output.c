
#include "output.h"
#include "utils.h"

void print_code (struct code *c)
{
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    report ("%s", allegrex_disassemble (c->base[i].opc, c->base[i].address));
    report ("\n");
  }
}

