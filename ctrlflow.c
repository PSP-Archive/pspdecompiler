
#include "code.h"
#include "utils.h"


static
void extract_blocks (struct code *c, struct subroutine *sub)
{
  struct location *begin, *next;
  struct basicblock *block;

  sub->blocks = list_alloc (c->lstpool);
  begin = sub->begin;

  while (1) {
    block = fixedpool_alloc (c->blockspool);

    block->inrefs = list_alloc (c->lstpool);
    block->outrefs = list_alloc (c->lstpool);
    block->frontier = list_alloc (c->lstpool);
    list_inserttail (sub->blocks, block);

    if (!begin) break;
    next = begin;

    block->begin = begin;
    block->end = next;

    for (; next != sub->end; next++) {
      if (next->references && (next != begin)) {
        next--;
        break;
      }

      if (next->insn->flags & (INSN_JUMP | INSN_BRANCH)) {
        block->jumploc = next;
        block->end = next;
        if (!(next->insn->flags & INSN_BRANCHLIKELY))
          block->end++;
        break;
      }

      block->end = next;
    }

    do {
      begin->block = block;
    } while (begin++ != block->end);

    begin = NULL;
    while (next++ != sub->end) {
      if (next->reachable == LOCATION_REACHABLE) {
        begin = next;
        break;
      }
    }
  }
}

static
struct basicedge *make_link (struct code *c, struct basicblock *from, struct basicblock *to)
{
  struct basicedge *edge;
  edge = fixedpool_alloc (c->edgespool);
  edge->from = from;
  edge->to = to;

  list_inserthead (from->outrefs, edge);
  list_inserthead (to->inrefs, edge);
  return edge;
}

static
void link_blocks (struct code *c, struct subroutine *sub)
{
  struct basicblock *block, *next, *endblock;
  struct basicedge *edge;
  struct location *loc;
  element el;

  endblock = list_tailvalue (sub->blocks);
  sub->endblock = endblock;

  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);
    if (block == endblock) break;
    el = element_next (el);
    next = element_getvalue (el);


    if (block->jumploc) {
      loc = block->jumploc;
      if (loc->insn->flags & INSN_BRANCH) {
        make_link (c, block, next);
        if (loc->insn->flags & INSN_BRANCHLIKELY) {
          struct basicblock *slot = fixedpool_alloc (c->blockspool);
          element_insertbefore (el, element_alloc (c->lstpool, slot));
          slot->inrefs = list_alloc (c->lstpool);
          slot->outrefs = list_alloc (c->lstpool);
          slot->frontier = list_alloc (c->lstpool);
          slot->begin = &block->end[1];
          slot->end = slot->begin;

          make_link (c, block, slot);
          block = slot;
        }
        if (loc->insn->flags & INSN_LINK) {
          edge = make_link (c, block, next);
          edge->hascall = TRUE;
          edge->calltarget = loc->target->sub;
        } else if (loc->target->sub->begin == loc->target) {
          edge = make_link (c, block, endblock);
          edge->hascall = TRUE;
          edge->calltarget = loc->target->sub;
        } else {
          make_link (c, block, loc->target->block);
        }
      } else {
        if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
          edge = make_link (c, block, next);
          edge->hascall = TRUE;
          if (loc->target)
            edge->calltarget = loc->target->sub;
        } else {
          if (loc->target) {
            if (loc->target->sub->begin == loc->target) {
              edge = make_link (c, block, endblock);
              edge->hascall = TRUE;
              edge->calltarget = loc->target->sub;
            } else {
              make_link (c, block, loc->target->block);
            }
          } else {
            if (loc->cswitch) {
              element ref;
              int count = 0;

              if (loc->cswitch->jumplocation == loc) {
                ref = list_head (loc->cswitch->references);
                while (ref) {
                  struct location *target = element_getvalue (ref);
                  edge = make_link (c, block, target->block);
                  edge->cswitch = loc->cswitch;
                  edge->switchnum = count++;
                  ref = element_next (ref);
                }
              }
            } else
              make_link (c, block, endblock);
          }
        }
      }
    } else {
      make_link (c, block, next);
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
