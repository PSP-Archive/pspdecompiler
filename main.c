#include <stdio.h>

#include "code.h"
#include "prx.h"
#include "output.h"
#include "nids.h"
#include "hash.h"
#include "utils.h"

int main (int argc, char **argv)
{
  char *prxfilename = NULL;
  char *nidsfilename = NULL;

  int i, j, verbose = 0;
  int printgraph = FALSE;

  struct nidstable *nids = NULL;
  struct prx *p = NULL;
  struct code *c;

  for (i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      char *s = argv[i];
      for (j = 0; s[j]; j++) {
        switch (s[j]) {
        case 'v': verbose++; break;
        case 'g': printgraph = TRUE; break;
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
    fatal (__FILE__ ": can't load prx `%s'", prxfilename);

  if (nids)
    prx_resolve_nids (p, nids);

  if (verbose > 2 && nids)
    nids_print (nids);

  if (verbose > 1) prx_print (p, (verbose > 2));

  c = code_analyse (p);
  if (!c)
    fatal (__FILE__ ": can't analyse code `%s'", prxfilename);

  if (verbose > 0) {
    if (printgraph)
      print_graph (c, prxfilename);
    else
      print_code (c, prxfilename);
  }

  code_free (c);

  prx_free (p);

  if (nids)
    nids_free (nids);

  return 0;
}
