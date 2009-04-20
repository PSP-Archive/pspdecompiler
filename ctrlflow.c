
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
void link_blocks (struct code *c, struct subroutine *sub)
{
  struct basicblock *block, *prev, *endblock;
  struct basicedge *edge;
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
    if (block != endblock) {
      if (block->begin->references) {
        ref = list_head (block->begin->references);
        while (ref) {
          edge = fixedpool_alloc (c->edgespool);

          loc = element_getvalue (ref);
          edge->from = loc->block;
          edge->to = block;

          list_inserttail (block->inrefs, edge);
          list_inserttail (loc->block->outrefs, edge);
          ref = element_next (ref);
        }
      }
    }

    if (prev->jumploc) {
      loc = prev->jumploc;
      if ((loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D))) {
        linknext = TRUE;
        prev->hascall = TRUE;
        if (loc->target) {
          prev->calltarget = loc->target->sub;
        }
      } else {
        if (!loc->target) {
          if (loc->cswitch) {
            if (loc->cswitch->jumplocation != loc)
              linkend = TRUE;
          } else linkend = TRUE;
        } else {
          if ((loc->insn->flags & INSN_BRANCH) && !loc->branchalways)
            linknext = TRUE;
          if (loc->target->sub->begin == loc->target) {
            prev->hascall = TRUE;
            prev->calltarget = loc->target->sub;
            linkend = TRUE;
          }
        }
      }
    } else linknext = TRUE;

    if (linkend) {
      edge = fixedpool_alloc (c->edgespool);
      edge->from = prev;
      edge->to = endblock;

      list_inserthead (prev->outrefs, edge);
      list_inserthead (endblock->inrefs, edge);
    }

    if (linknext) {
      edge = fixedpool_alloc (c->edgespool);
      edge->from = prev;
      edge->to = block;

      list_inserttail (prev->outrefs, edge);
      list_inserttail (block->inrefs, edge);
    }
  }
}

int extract_cfg (struct code *c, struct subroutine *sub)
{
  extract_blocks (c, sub);
  link_blocks (c, sub);
  if (!cfg_dfs (sub)) {
    error (__FILE__ ": unreachable code at subroutine 0x%08X", sub->begin->address);
    return 0;
  }

  cfg_dominance (sub);

  return 1;
}
