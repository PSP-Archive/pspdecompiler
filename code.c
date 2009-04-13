
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "utils.h"


struct code *code_alloc (void)
{
  struct code *c;
  c = (struct code *) xmalloc (sizeof (struct code));
  memset (c, 0, sizeof (struct code));

  c->lstpool = listpool_create (1024, 1024);
  c->switchpool = fixedpool_create (sizeof (struct codeswitch), 32);
  c->subspool = fixedpool_create (sizeof (struct subroutine), 128);

  return c;
}

void code_free (struct code *c)
{
  if (c->base)
    free (c->base);
  c->base = NULL;

  if (c->lstpool)
    listpool_destroy (c->lstpool);
  c->lstpool = NULL;

  if (c->subspool)
    fixedpool_destroy (c->subspool, NULL, NULL);
  c->subspool = NULL;

  if (c->switchpool)
    fixedpool_destroy (c->switchpool, NULL, NULL);
  c->switchpool = NULL;

  free (c);
}

