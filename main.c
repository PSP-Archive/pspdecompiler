#include <stdio.h>

#include "prx.h"
#include "analyser.h"
#include "hash.h"

int main (int argc, char **argv)
{
  if (argc > 1) {
    struct prx *p = load_prx (argv[1]);
    struct elf_section *s;
    if (!p) {
      return 0;
    }
    print_prx (p);
    s = hashtable_search (p->secbyname, ".text", NULL);
    if (s) {
      struct code *c = analyse_code (s->data, s->size, s->addr);
      if (c) {
        /* print_code (c); */
        free_code (c);
      }
    }
    free_prx (p);
  }
  return 0;
}
