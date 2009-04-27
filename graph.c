
#include "code.h"
#include "utils.h"

static
void dfs_step (struct subroutine *sub, struct basicblock *block, int reverse)
{
  struct basicblock *next;
  struct basicblocknode *node, *nextnode;
  list refs, out;
  element el;

  if (reverse) {
    node = &block->revnode;
    refs = block->inrefs;
    out = sub->revdfsblocks;
  } else {
    node = &block->node;
    refs = block->outrefs;
    out = sub->dfsblocks;
  }
  node->dfsnum = -1;

  el = list_head (refs);
  while (el) {
    next = element_getvalue (el);
    nextnode = (reverse) ? &next->revnode : &next->node;
    if (!nextnode->dfsnum) {
      nextnode->parent = block;
      list_inserttail (node->children, next);
      dfs_step (sub, next, reverse);
    }
    el = element_next (el);
  }

  node->dfsnum = sub->dfscount--;
  list_inserthead (out, block);
}

int cfg_dfs (struct subroutine *sub, int reverse)
{
  struct basicblock *start;
  sub->dfscount = list_size (sub->blocks);
  start = reverse ? sub->endblock : sub->startblock;

  dfs_step (sub, start, reverse);
  return (sub->dfscount == 0);
}

struct basicblock *dom_intersect (struct basicblock *b1, struct basicblock *b2, int reverse)
{
  struct basicblocknode *n1, *n2;
  n1 = (reverse) ? &b1->revnode : &b1->node;
  n2 = (reverse) ? &b2->revnode : &b2->node;
  while (b1 != b2) {
    while (n1->dfsnum > n2->dfsnum) {
      b1 = n1->dominator;
      n1 = (reverse) ? &b1->revnode : &b1->node;
    }
    while (n2->dfsnum > n1->dfsnum) {
      b2 = n2->dominator;
      n2 = (reverse) ? &b2->revnode : &b2->node;
    }
  }
  return b1;
}

void cfg_dominance (struct subroutine *sub, int reverse)
{
  struct basicblock *start;
  list blocks, refs;
  int changed = TRUE;

  if (reverse) {
    blocks = sub->revdfsblocks;
    start = sub->endblock;
    start->revnode.dominator = start;

  } else {
    blocks = sub->dfsblocks;
    start = sub->startblock;
    start->node.dominator = start;
  }

  while (changed) {
    element el;

    changed = FALSE;
    el = list_head (blocks);
    el = element_next (el);
    while (el) {
      struct basicblock *block, *dom = NULL;
      struct basicblocknode *node;
      element ref;

      block = element_getvalue (el);
      refs = (reverse) ? block->outrefs : block->inrefs;
      ref = list_head (refs);
      while (ref) {
        struct basicblock *bref, *brefdom;

        bref = element_getvalue (ref);
        brefdom = (reverse) ? bref->revnode.dominator : bref->node.dominator;

        if (brefdom) {
          if (!dom) {
            dom = bref;
          } else {
            dom = dom_intersect (dom, bref, reverse);
          }
        }

        ref = element_next (ref);
      }

      node = (reverse) ? &block->revnode : &block->node;
      if (dom != node->dominator) {
        node->dominator = dom;
        changed = TRUE;
      }

      el = element_next (el);
    }
  }
}

void cfg_frontier (struct subroutine *sub, int reverse)
{
  struct basicblock *block, *runner;
  struct basicblocknode *blocknode, *runnernode;
  element el, ref;
  list refs;

  el = (reverse) ? list_head (sub->revdfsblocks) : list_head (sub->dfsblocks);

  while (el) {
    block = element_getvalue (el);
    if (reverse) {
      refs = block->outrefs;
      blocknode = &block->revnode;
    } else {
      refs = block->inrefs;
      blocknode = &block->node;
    }
    if (list_size (refs) >= 2) {
      ref = list_head (refs);
      while (ref) {
        runner = element_getvalue (ref);
        while (runner != blocknode->dominator) {
          runnernode = (reverse) ? &runner->revnode : &runner->node;
          list_inserttail (runnernode->frontier, block);
          runner = runnernode->dominator;
        }
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }
}

static
void mark_forward (int num, int maxnum, struct basicblock *block)
{
  element el;

  block->mark2 = 0;
  block->mark1 = num;
  el = list_head (block->outrefs);
  while (el) {
    struct basicblock *to;
    to = element_getvalue (el);
    if (to->node.dfsnum <= maxnum &&
        to->node.dfsnum > block->node.dfsnum &&
        to->mark1 != num) {
      mark_forward (num, maxnum, to);
    }
    el = element_next (el);
  }
}

static
void mark_backward (struct subroutine *sub, struct loopstruct *loop, struct basicblock *block)
{
  element el;

  block->mark2 = 1;
  if (block->loop && block->loop != loop && block->loop != loop->parent) {
    error (__FILE__ ": cross loops at node %d subroutine 0x%08X",
        block->node.dfsnum, sub->begin->address);
    sub->haserror = TRUE;
  }
  block->loop = loop;

  el = list_head (block->inrefs);
  while (el) {
    struct basicblock *from;
    from = element_getvalue (el);
    if (from->mark1 == loop->start->node.dfsnum &&
        from->node.dfsnum < block->node.dfsnum &&
        from->mark2 == 0) {
      mark_backward (sub, loop, from);
    }
    el = element_next (el);
  }
}

static
void mark_blocks (struct subroutine *sub, struct loopstruct *loop)
{
  element el;
  struct basicblock *from;

  loop->parent = loop->start->loop;
  mark_forward (loop->start->node.dfsnum, loop->maxdfsnum, loop->start);
  el = list_head (loop->edges);
  while (el) {
    from = element_getvalue (el);
    mark_backward (sub, loop, from);
    el = element_next (el);
  }
}

void extract_loops (struct subroutine *sub)
{
  element el, ref;

  sub->loops = list_alloc (sub->code->lstpool);

  el = list_head (sub->dfsblocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    struct loopstruct *loop = NULL;
    ref = list_head (block->inrefs);
    while (ref) {
      struct basicblock *from;
      from = element_getvalue (ref);
      if (from->node.dfsnum >= block->node.dfsnum) {
        if (!loop) {
          loop = fixedpool_alloc (sub->code->loopspool);
          loop->edges = list_alloc (sub->code->lstpool);
        }
        if (loop->maxdfsnum < from->node.dfsnum)
          loop->maxdfsnum = from->node.dfsnum;
        list_inserttail (loop->edges, from);
      }
      ref = element_next (ref);
    }
    if (loop) {
      loop->start = block;
      mark_blocks (sub, loop);
      list_inserttail (sub->loops, loop);
    }
    el = element_next (el);
  }
}

void extract_ifs (struct subroutine *sub)
{

}
