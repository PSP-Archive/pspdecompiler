
#include "code.h"
#include "utils.h"

static
void dfs_step (struct subroutine *sub, struct basicblock *block)
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
      dfs_step (sub, next);
    }
    el = element_next (el);
  }

  block->dfsnum = sub->dfscount--;
  list_inserthead (sub->dfsblocks, block);
}

int cfg_dfs (struct subroutine *sub)
{
  struct basicblock *start;
  sub->dfscount = list_size (sub->blocks);
  start = list_headvalue (sub->blocks);

  dfs_step (sub, start);
  return (sub->dfscount == 0);
}

static
void revdfs_step (struct subroutine *sub, struct basicblock *block)
{
  struct basicedge *edge;
  struct basicblock *next;
  element el;

  block->revdfsnum = -1;

  el = list_head (block->inrefs);
  while (el) {
    edge = element_getvalue (el);
    next = edge->from;
    if (!next->revdfsnum) {
      next->revparent = block;
      revdfs_step (sub, next);
    }
    el = element_next (el);
  }

  block->revdfsnum = sub->dfscount--;
  list_inserthead (sub->revdfsblocks, block);
}

int cfg_revdfs (struct subroutine *sub)
{
  struct basicblock *start;
  sub->dfscount = list_size (sub->blocks);
  start = sub->endblock;

  revdfs_step (sub, start);
  return (sub->dfscount == 0);
}

struct basicblock *dom_intersect (struct basicblock *b1, struct basicblock *b2)
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

  start = list_headvalue (sub->dfsblocks);

  start->dominator = start;
  while (changed) {
    element el;

    changed = FALSE;
    el = list_head (sub->dfsblocks);
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
            dom = dom_intersect (dom, bref);
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

  el = list_head (sub->dfsblocks);
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

struct basicblock *dom_revintersect (struct basicblock *b1, struct basicblock *b2)
{
  while (b1 != b2) {
    while (b1->revdfsnum > b2->revdfsnum) {
      b1 = b1->revdominator;
    }
    while (b2->revdfsnum > b1->revdfsnum) {
      b2 = b2->revdominator;
    }
  }
  return b1;
}

void cfg_revdominance (struct subroutine *sub)
{
  struct basicblock *start;
  int changed = TRUE;

  start = list_headvalue (sub->revdfsblocks);

  start->revdominator = start;
  while (changed) {
    element el;

    changed = FALSE;
    el = list_head (sub->revdfsblocks);
    el = element_next (el);
    while (el) {
      struct basicblock *block, *revdom = NULL;
      element ref;

      block = element_getvalue (el);
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicedge *edge;
        struct basicblock *bref;

        edge = element_getvalue (ref);
        bref = edge->to;

        if (bref->revdominator) {
          if (!revdom) {
            revdom = bref;
          } else {
            revdom = dom_revintersect (revdom, bref);
          }
        }

        ref = element_next (ref);
      }

      if (revdom != block->revdominator) {
        block->revdominator = revdom;
        changed = TRUE;
      }

      el = element_next (el);
    }
  }
}

static
void mark_forward (struct loopstruct *loop, struct basicblock *block)
{
  element el;

  block->mark2 = 0;
  block->mark1 = loop->start->dfsnum;
  el = list_head (block->outrefs);
  while (el) {
    struct basicedge *edge;
    edge = element_getvalue (el);
    if (edge->to->dfsnum <= loop->maxdfsnum &&
        edge->to->dfsnum > block->dfsnum &&
        edge->to->mark1 != loop->start->dfsnum) {
      mark_forward (loop, edge->to);
    }
    el = element_next (el);
  }
}

static
void mark_backward (struct subroutine *sub, struct loopstruct *loop, struct basicblock *block)
{
  element el;

  block->mark2 = 1;
  if (block->loop && block->loop != loop && block->loop != loop->parent ) {
    error (__FILE__ ": cross loops at node %d (current loop = %d, parent loop = %d, another loop = %d) subroutine 0x%08X",
        block->dfsnum, loop->start->dfsnum, loop->parent->start->dfsnum, block->loop->start->dfsnum, sub->begin->address);
    sub->haserror = TRUE;
  }
  block->loop = loop;

  el = list_head (block->inrefs);
  while (el) {
    struct basicedge *edge;
    edge = element_getvalue (el);
    if (edge->from->mark1 == loop->start->dfsnum &&
        edge->from->dfsnum < block->dfsnum &&
        edge->from->mark2 == 0) {
      mark_backward (sub, loop, edge->from);
    }
    el = element_next (el);
  }
}

static
void mark_blocks (struct subroutine *sub, struct loopstruct *loop)
{
  element el;
  struct basicedge *edge;

  loop->parent = loop->start->loop;
  mark_forward (loop, loop->start);
  el = list_head (loop->edges);
  while (el) {
    edge = element_getvalue (el);
    mark_backward (sub, loop, edge->from);
    el = element_next (el);
  }
}

void extract_loops (struct code *c, struct subroutine *sub)
{
  element el, ref;

  sub->loops = list_alloc (c->lstpool);

  el = list_head (sub->dfsblocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    struct loopstruct *loop = NULL;
    ref = list_head (block->inrefs);
    while (ref) {
      struct basicedge *edge;
      edge = element_getvalue (ref);
      if (edge->from->dfsnum >= block->dfsnum) {
        if (!loop) {
          loop = fixedpool_alloc (c->looppool);
          loop->edges = list_alloc (c->lstpool);
        }
        if (loop->maxdfsnum < edge->from->dfsnum)
          loop->maxdfsnum = edge->from->dfsnum;
        list_inserttail (loop->edges, edge);
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
