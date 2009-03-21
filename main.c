#include <stdio.h>

#include "prx.h"
#include "analyser.h"
#include "nids.h"
#include "hash.h"

int main (int argc, char **argv)
{
  struct nidstable *nids = NULL;
  struct prx *p = NULL;

  if (argc > 2) {
    nids = nids_load (argv[2]);
  }


  if (argc > 1) {
    struct code *c;
    p = prx_load (argv[1]);
    if (!p) {
      return 0;
    }

    if (nids)
      prx_resolve_nids (p, nids);
    prx_print (p);

    c = analyse_code (p->programs->data, p->modinfo->expvaddr - 4, p->programs->vaddr);
    if (c) {
      print_code (c);
      free_code (c);
    }
    prx_free (p);
  }

  if (nids) {
    /* nids_print (nids); */
    nids_free (nids);
  }

  return 0;
}
