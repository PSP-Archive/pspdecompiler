#include <stdio.h>

#include "prx.h"

int main (int argc, char **argv)
{
  if (argc > 1) {
    load_prx (argv[1]);
  }
  return 0;
}
