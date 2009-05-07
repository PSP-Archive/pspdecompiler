
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
  c->switchpool = fixedpool_create (sizeof (struct codeswitch), 64, TRUE);
  c->subspool = fixedpool_create (sizeof (struct subroutine), 1024, TRUE);
  c->blockspool = fixedpool_create (sizeof (struct basicblock), 4096, TRUE);
  c->edgespool = fixedpool_create (sizeof (struct basicedge), 8192, TRUE);
  c->ssavarspool = fixedpool_create (sizeof (struct ssavar), 4096, TRUE);
  c->opspool = fixedpool_create (sizeof (struct operation), 8192, TRUE);
  c->valspool = fixedpool_create (sizeof (struct value), 8192, TRUE);
  c->loopspool = fixedpool_create (sizeof (struct loopstructure), 256, TRUE);
  c->ifspool = fixedpool_create (sizeof (struct ifstructure), 1024, TRUE);

  return c;
}

struct code* code_analyse (struct prx *p)
{
  struct code *c = code_alloc ();
  struct subroutine *sub;
  element el;

  c->file = p;

  if (!decode_instructions (c)) {
    code_free (c);
    return NULL;
  }

  extract_switches (c);
  extract_subroutines (c);

  live_registers (c);

  el = list_head (c->subroutines);
  while (el) {
    sub = element_getvalue (el);
    if (!sub->import && !sub->haserror) {
      cfg_traverse (sub, FALSE);
      if (!sub->haserror) {
        sub->status |= SUBROUTINE_CFG_TRAVERSE;
        cfg_traverse (sub, TRUE);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_CFG_TRAVERSE_REV;
        fixup_call_arguments (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_FIXUP_CALL_ARGS;
        build_ssa (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_SSA;
      }
    }
    el = element_next (el);
  }

  live_registers_imports (c);

  el = list_head (c->subroutines);
  while (el) {
    sub = element_getvalue (el);
    if (!sub->import && !sub->haserror) {
      if (!(sub->status & SUBROUTINE_FIXUP_CALL_ARGS)) {
        fixup_call_arguments (sub);
        if (!sub->haserror) {
          sub->status |= SUBROUTINE_FIXUP_CALL_ARGS;
          build_ssa (sub);
        }
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_SSA;
        propagate_constants (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_CONSTANTS_EXTRACTED;
        extract_variables (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_VARIABLES_EXTRACTED;
        extract_structures (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUBROUTINE_STRUCTURES_EXTRACTED;
      }
    }
    el = element_next (el);
  }

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

  if (c->ssavarspool)
    fixedpool_destroy (c->ssavarspool, NULL, NULL);
  c->ssavarspool = NULL;

  if (c->opspool)
    fixedpool_destroy (c->opspool, NULL, NULL);
  c->opspool = NULL;

  if (c->valspool)
    fixedpool_destroy (c->valspool, NULL, NULL);
  c->valspool = NULL;

  if (c->loopspool)
    fixedpool_destroy (c->loopspool, NULL, NULL);
  c->loopspool = NULL;

  if (c->ifspool)
    fixedpool_destroy (c->ifspool, NULL, NULL);
  c->ifspool = NULL;

  free (c);
}

