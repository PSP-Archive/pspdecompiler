
#include <string.h>
#include "analyser.h"
#include "utils.h"


struct code_position* analyse_code (uint8 *code, uint32 size)
{
  struct code_position *c;
  uint32 i, numopc = size >> 2;

  if (size & 0x03) {
    error (__FILE__ ": size is not multiple of 4");
    return NULL;
  }

  c = (struct code_position *) xmalloc (numopc * sizeof (struct code_position));
  memset (c, 0, numopc * sizeof (struct code_position));

  for (i = 0; i < numopc; i++) {
    c[i].opcode = code[i << 2];
    c[i].opcode |= code[(i << 2) + 1] << 8;
    c[i].opcode |= code[(i << 2) + 2] << 16;
    c[i].opcode |= code[(i << 2) + 3] << 24;



  }
  return c;
}
