#include <stdio.h>

#include "prx.h"
#include "analyser.h"
#include "nids.h"
#include "hash.h"
#include "utils.h"

int main (int argc, char **argv)
{
  char *prxfilename = NULL;
  char *nidsfilename = NULL;

  int i, j, verbose = 0;

  struct nidstable *nids = NULL;
  struct prx *p = NULL;
  struct code *c;

  for (i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      char *s = argv[i];
      for (j = 0; s[j]; j++) {
        switch (s[j]) {
        case 'v': verbose++; break;
        case 'n':
          if (i == (argc - 1))
            fatal (__FILE__ ": missing nids file");

          nidsfilename = argv[++i];
          break;
        }
      }
    } else {
      prxfilename = argv[i];
    }
  }

  if (!prxfilename)
    fatal (__FILE__ ": missing prx file name");

  if (nidsfilename)
    nids = nids_load (nidsfilename);


  p = prx_load (prxfilename);
  if (!p)
    fatal (__FILE__ ": can't load prx");

  if (nids)
    prx_resolve_nids (p, nids);

  if (verbose > 2 && nids)
    nids_print (nids);

  if (verbose > 0) prx_print (p);

  c = analyse_code (p);
  if (!c)
    fatal (__FILE__ ": can't analyse code");

  if (verbose > 1) print_code (c);
  free_code (c);

  prx_free (p);

  if (nids)
    nids_free (nids);

  return 0;
}
