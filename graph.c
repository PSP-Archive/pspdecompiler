
#include "code.h"
#include "utils.h"

static
void step (struct subroutine *sub, struct basicblock *block)
{
  struct basicedge *edge;
  struct basicblock *next;
  element el;

  block->dfsnum = -1;

  el = list_head (block->outrefs);
  while (el) {
    edge = element_getvalue (el);
    next = edge->to;
    if (!next->dfsnum) {
      next->parent = block;
      step (sub, next);
    }
    el = element_next (el);
  }

  block->dfsnum = sub->dfscount--;
  list_inserthead (sub->blocks, block);
}

int cfg_dfs (struct subroutine *sub)
{
  struct basicblock *start;
  sub->dfscount = list_size (sub->blocks);
  start = list_headvalue (sub->blocks);
  list_reset (sub->blocks);

  step (sub, start);
  return (sub->dfscount == 0);
}

static
struct basicblock *intersect (struct basicblock *b1, struct basicblock *b2)
{
  while (b1 != b2) {
    while (b1->dfsnum > b2->dfsnum) {
      b1 = b1->dominator;
    }
    while (b2->dfsnum > b1->dfsnum) {
      b2 = b2->dominator;
    }
  }
  return b1;
}

static
void dominance (struct subroutine *sub)
{
  struct basicblock *start;
  int changed = TRUE;

  start = list_headvalue (sub->blocks);

  start->dominator = start;
  while (changed) {
    element el;

    changed = FALSE;
    el = list_head (sub->blocks);
    el = element_next (el);
    while (el) {
      struct basicblock *block, *dom = NULL;
      element ref;

      block = element_getvalue (el);
      ref = list_head (block->inrefs);
      while (ref) {
        struct basicedge *edge;
        struct basicblock *bref;

        edge = element_getvalue (ref);
        bref = edge->from;

        if (bref->dominator) {
          if (!dom) {
            dom = bref;
          } else {
            dom = intersect (dom, bref);
          }
        }

        ref = element_next (ref);
      }

      if (dom != block->dominator) {
        block->dominator = dom;
        changed = TRUE;
      }

      el = element_next (el);
    }
  }
}

void frontier (struct subroutine *sub)
{
  struct basicblock *block, *runner;
  struct basicedge *edge;
  element el, ref;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    if (list_size (block->inrefs) >= 2) {
      ref = list_head (block->inrefs);;
      while (ref) {
        edge = element_getvalue (ref);
        runner = edge->to;
        while (runner != block->dominator) {
          list_inserttail (runner->frontier, block);
          runner = runner->dominator;
        }
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }
}

void cfg_dominance (struct subroutine *sub)
{
  dominance (sub);
  frontier (sub);
}
