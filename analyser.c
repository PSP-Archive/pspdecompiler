
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "utils.h"

static
struct code *code_alloc (void)
{
  struct code *c;
  c = (struct code *) xmalloc (sizeof (struct code));
  memset (c, 0, sizeof (struct code));

  c->lstpool = listpool_create (8192, 4096);
  c->switchpool = fixedpool_create (sizeof (struct codeswitch), 64, 1);
  c->subspool = fixedpool_create (sizeof (struct subroutine), 1024, 1);
  c->blockspool = fixedpool_create (sizeof (struct basicblock), 4096, 1);
  c->edgespool = fixedpool_create (sizeof (struct basicedge), 8192, 1);
  c->looppool = fixedpool_create (sizeof (struct loopstruct), 256, 1);
  c->varspool = fixedpool_create (sizeof (struct variable), 4096, 1);

  return c;
}

struct code* code_analyse (struct prx *p)
{
  struct code *c = code_alloc ();
  c->file = p;

  if (!decode_instructions (c)) {
    code_free (c);
    return NULL;
  }

  extract_switches (c);
  extract_subroutines (c);

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

  if (c->blockspool)
    fixedpool_destroy (c->blockspool, NULL, NULL);
  c->blockspool = NULL;

  if (c->edgespool)
    fixedpool_destroy (c->edgespool, NULL, NULL);
  c->edgespool = NULL;

  if (c->looppool)
    fixedpool_destroy (c->looppool, NULL, NULL);
  c->looppool = NULL;

  if (c->varspool)
    fixedpool_destroy (c->varspool, NULL, NULL);
  c->varspool = NULL;

  free (c);
}

