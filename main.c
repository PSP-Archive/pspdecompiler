#include <stdio.h>

#include "prx.h"

int main (int argc, char **argv)
{
  if (argc > 1) {
    struct prx *p = load_prx (argv[1]);
    print_prx (p);
    free_prx (p);
  }
  return 0;
}
