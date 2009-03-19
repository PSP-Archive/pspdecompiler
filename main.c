#include <stdio.h>

#include "prx.h"
#include "analyser.h"
#include "hash.h"

int main (int argc, char **argv)
{
  if (argc > 1) {
    struct prx *p = prx_load (argv[1]);
    struct elf_section *s;
    if (!p) {
      return 0;
    }
    prx_print (p);
    s = hashtable_search (p->secbyname, ".text", NULL);
    if (s) {
      struct code *c = analyse_code (s->data, s->size, s->addr);
      if (c) {
        /* print_code (c); */
        free_code (c);
      }
    }
    prx_free (p);
  }
  return 0;
}
