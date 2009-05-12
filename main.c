#include <stdio.h>
#include <string.h>

#include "code.h"
#include "prx.h"
#include "output.h"
#include "nids.h"
#include "hash.h"
#include "utils.h"

static
void print_help (char *prgname)
{
  report (
    "Usage:\n"
    "  %s [-g] [-n nidsfile] [-v] prxfile\n"
    "Where:\n"
    "  -g    output graphviz dot\n"
    "  -t    print depth first search number\n"
    "  -r    print the reverse depth first search number\n"
    "  -d    print the dominator\n"
    "  -x    print the reverse dominator\n"
    "  -f    print the frontier\n"
    "  -z    print the reverse frontier\n"
    "  -p    print phi functions\n"
    "  -q    print code into nodes\n"
    "  -s    print structures\n"
    "  -e    print edge types\n"
    "  -c    output code\n"
    "  -v    increase verbosity\n"
    "  -n    specify nids xml file\n"
    "  -i    print prx info\n",
    prgname
  );
}

int main (int argc, char **argv)
{
  char *prxfilename = NULL;
  char *nidsfilename = NULL;

  int i, j, verbosity = 0;
  int printgraph = FALSE;
  int printcode = FALSE;
  int printinfo = FALSE;
  int graphoptions = 0;

  struct nidstable *nids = NULL;
  struct prx *p = NULL;
  struct code *c;

  for (i = 1; i < argc; i++) {
    if (strcmp ("--help", argv[i]) == 0) {
      print_help (argv[0]);
      return 0;
    } else if (argv[i][0] == '-') {
      char *s = argv[i];
      for (j = 0; s[j]; j++) {
        switch (s[j]) {
        case 'v': verbosity++; break;
        case 'g': printgraph = TRUE; break;
        case 'c': printcode = TRUE; break;
        case 'i': printinfo = TRUE; break;
        case 't': graphoptions |= OUT_PRINT_DFS; break;
        case 'r': graphoptions |= OUT_PRINT_RDFS; break;
        case 'd': graphoptions |= OUT_PRINT_DOMINATOR; break;
        case 'x': graphoptions |= OUT_PRINT_RDOMINATOR; break;
        case 'f': graphoptions |= OUT_PRINT_FRONTIER; break;
        case 'z': graphoptions |= OUT_PRINT_RFRONTIER; break;
        case 'p': graphoptions |= OUT_PRINT_PHIS; break;
        case 'q': graphoptions |= OUT_PRINT_CODE; break;
        case 's': graphoptions |= OUT_PRINT_STRUCTURES; break;
        case 'e': graphoptions |= OUT_PRINT_EDGE_TYPES; break;
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

  if (!prxfilename) {
    print_help (argv[0]);
    return 0;
  }

  if (nidsfilename)
    nids = nids_load (nidsfilename);

  p = prx_load (prxfilename);
  if (!p)
    fatal (__FILE__ ": can't load prx `%s'", prxfilename);

  if (nids)
    prx_resolve_nids (p, nids);

  if (verbosity > 2 && nids && printinfo)
    nids_print (nids);

  if (verbosity > 0 && printinfo)
    prx_print (p, (verbosity > 1));

  c = code_analyse (p);
  if (!c)
    fatal (__FILE__ ": can't analyse code `%s'", prxfilename);


  if (printgraph)
    print_graph (c, prxfilename, graphoptions);

  if (printcode)
    print_code (c, prxfilename);

  code_free (c);

  prx_free (p);

  if (nids)
    nids_free (nids);

  return 0;
}
