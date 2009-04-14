
#include <string.h>

#include "analyser.h"
#include "utils.h"


struct code* analyse_code (struct prx *p)
{
  struct code *c = code_alloc ();
  c->file = p;
  if (!decode_instructions (c)) {
    code_free (c);
    return NULL;
  }

  extract_switches (c);
  extract_subroutines (c);

  return c;
}
