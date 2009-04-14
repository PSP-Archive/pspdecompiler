
#include "code.h"
#include "utils.h"


static
void extract_blocks (struct code *c, struct subroutine *sub)
{
  struct location *begin, *end;
  struct basicblock *block;

  sub->blocks = list_alloc (c->lstpool);
  begin = sub->location;

  while (begin) {
    end = begin;
    block = fixedpool_alloc (c->blockspool);
    memset (block, 0, sizeof (struct basicblock));

    block->inrefs = list_alloc (c->lstpool);
    block->outrefs = list_alloc (c->lstpool);
    list_inserttail (sub->blocks, block);

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
  struct basicblock *block, *prev = NULL;
  element el;

  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);

    el = element_next (el);
    prev = block;
  }
}

int extract_cfg (struct code *c, struct subroutine *sub)
{
  extract_blocks (c, sub);
  link_blocks (sub);
  return 0;
}
