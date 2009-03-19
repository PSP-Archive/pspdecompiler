#include <stdio.h>

#include "prx.h"
#include "analyser.h"
#include "nids.h"
#include "hash.h"

int main (int argc, char **argv)
{
  struct hashtable *nids = NULL;

  if (argc > 2) {
    nids = nids_load_xml (argv[2]);
  }


  if (argc > 1) {
    struct prx *p = prx_load (argv[1]);
    struct elf_section *s;
    if (!p) {
      return 0;
    }
    if (nids)
      prx_resolve_nids (p, nids);
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

  if (nids) {
    /* nids_print (nids); */
    nids_free (nids);
  }

  return 0;
}
