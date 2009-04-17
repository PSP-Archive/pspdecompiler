
#include <string.h>

#include "code.h"
#include "utils.h"


static
void extract_blocks (struct code *c, struct subroutine *sub)
{
  struct location *begin, *end;
  struct basicblock *block;

  sub->blocks = list_alloc (c->lstpool);
  begin = sub->begin;

  while (1) {
    end = begin;
    block = fixedpool_alloc (c->blockspool);
    memset (block, 0, sizeof (struct basicblock));

    block->inrefs = list_alloc (c->lstpool);
    block->outrefs = list_alloc (c->lstpool);
    block->frontier = list_alloc (c->lstpool);
    list_inserttail (sub->blocks, block);
    if (!begin) break;

    for (; end != sub->end; end++) {
      if (end->references && (end != begin)) break;
      if (end->insn->flags & (INSN_JUMP | INSN_BRANCH)) {
        block->jumploc = end;
        end += 2; break;
      }
    }
    end--;

    block->begin = begin;
    block->end = end;

    do {
      begin->block = block;
    } while (begin++ != end);

    begin = NULL;
    while (end++ != sub->end) {
      if (end->reachable) {
        begin = end;
        break;
      }
    }
  }
}

static
void link_blocks (struct subroutine *sub)
{
  struct basicblock *block, *prev, *endblock;
  struct location *loc;
  element el, ref;

  endblock = list_tailvalue (sub->blocks);
  sub->endblock = endblock;

  el = list_head (sub->blocks);
  block = element_getvalue (el);

  while (1) {
    int linknext = FALSE;
    int linkend = FALSE;
    el = element_next (el);
    if (!el) break;

    prev = block;
    block = element_getvalue (el);
    if (block == endblock) {

    } else {
      if (block->begin->references) {
        ref = list_head (block->begin->references);
        while (ref) {
          loc = element_getvalue (ref);
          list_inserttail (block->inrefs, loc->block);
          list_inserttail (loc->block->outrefs, block);
          ref = element_next (ref);
        }
      }
    }

    if (prev->jumploc) {
      loc = prev->jumploc;
      if ((loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D))) {
        linknext = TRUE;
      } else {
        if (!loc->target) {
          if (loc->cswitch) {
            if (loc->cswitch->jumplocation != loc)
              linkend = TRUE;
          } else linkend = TRUE;
        } else {
          if ((loc->insn->flags & INSN_BRANCH) && !loc->branchalways)
            linknext = TRUE;
          if (loc->target->sub != loc->sub) linkend = TRUE;
        }
      }
    } else linknext = TRUE;

    if (linkend) {
      list_inserthead (prev->outrefs, endblock);
      list_inserthead (endblock->inrefs, prev);
    }

    if (linknext) {
      list_inserttail (prev->outrefs, block);
      list_inserttail (block->inrefs, prev);
    }
  }
}

int extract_cfg (struct code *c, struct subroutine *sub)
{
  extract_blocks (c, sub);
  link_blocks (sub);
  if (!cfg_dfs (sub)) {
    error (__FILE__ ": unreachable code at subroutine 0x%08X", sub->begin->address);
    return 0;
  }

  cfg_dominance (sub);

  return 1;
}
