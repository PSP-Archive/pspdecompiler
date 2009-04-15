
#include <string.h>

#include "code.h"
#include "utils.h"


static
void extract_blocks (struct code *c, struct subroutine *sub)
{
  struct location *begin, *end;
  struct basicblock *block;

  sub->blocks = list_alloc (c->lstpool);
  begin = sub->location;

  while (1) {
    end = begin;
    block = fixedpool_alloc (c->blockspool);
    memset (block, 0, sizeof (struct basicblock));

    block->inrefs = list_alloc (c->lstpool);
    block->outrefs = list_alloc (c->lstpool);
    list_inserttail (sub->blocks, block);
    if (!begin) break;

    while (end++ != sub->end) {
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
  struct basicblock *block, *prev, *tail;
  struct location *loc;
  element el, ref;

  tail = list_tailvalue (sub->blocks);
  el = list_head (sub->blocks);
  block = element_getvalue (el);

  while (1) {
    int linknext = FALSE;
    int linktail = FALSE;
    el = element_next (el);
    if (!el) break;

    prev = block;
    block = element_getvalue (el);
    if (block == tail) {

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
          if (loc->target->cswitch) {
            if (loc->target->cswitch->location != loc->target)
              linktail = TRUE;
          } else linktail = TRUE;
        } else {
          if ((loc->insn->flags & INSN_BRANCH) && !loc->branchalways)
            linknext = TRUE;
          if (loc->target->sub != loc->sub) linktail = TRUE;
        }
      }
    } else linknext = TRUE;

    if (linktail) {
      list_inserthead (prev->outrefs, tail);
      list_inserthead (tail->inrefs, prev);
    }

    if (linknext) {
      list_inserttail (prev->outrefs, block);
      list_inserttail (block->inrefs, prev);
    }
  }
}

static
void print_cfg (struct subroutine *sub)
{
  element el, ref;
  report ("digraph sub_%05X {\n", sub->location->address);
  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);

    report ("    %3d [label=\"", block->dfsnum);
    if (block->begin) {
      report ("0x%05X-0x%05X", block->begin->address, block->end->address);
    } else {
      report ("End");
    }
    report ("\"];\n");

    if (list_size (block->outrefs) != 0) {
      report ("    %3d -> { ", block->dfsnum);
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicblock *refblock = element_getvalue (ref);
        report ("%3d ", refblock->dfsnum);
        ref = element_next (ref);
      }
      report (" };\n");
    }
    el = element_next (el);
  }
  report ("}\n");
}

int extract_cfg (struct code *c, struct subroutine *sub)
{
  extract_blocks (c, sub);
  link_blocks (sub);
  cfg_dfs (sub);
  print_cfg (sub);
  return 0;
}
