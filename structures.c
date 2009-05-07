
#include "code.h"
#include "utils.h"


static
void mark_backward (struct subroutine *sub, struct basicblock *block, int num, int end)
{
  element el, ref;
  int count = 1;

  el = block->node.blockel;
  while (el && count) {
    struct basicblock *block = element_getvalue (el);
    if (block->node.dfsnum < end) break;
    if (block->mark1 == num) {
      count--;
      ref = list_head (block->inrefs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        struct basicblock *next = edge->from;
        if (next->node.dfsnum >= end &&
            next->node.dfsnum < block->node.dfsnum) {
          if (next->mark1 != num) count++;
          next->mark1 = num;
        }
        ref = element_next (ref);
      }
    }

    el = element_previous (el);
  }
}

static
void mark_forward (struct subroutine *sub, struct basicblock *block,
                   struct loopstructure *loop, int num, int end)
{
  element el, ref;

  el = block->node.blockel;
  while (el) {
    struct basicblock *block = element_getvalue (el);
    if (block->node.dfsnum > end) break;
    if (block->mark2 == num) {
      block->loopst = loop;
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        struct basicblock *next = edge->to;
        if (next->node.dfsnum > block->node.dfsnum) {
          if (next->mark1 == num || dom_isancestor (&block->revnode, &next->revnode)) {
            next->mark2 = num;
            if (end < next->node.dfsnum)
              end = next->node.dfsnum;
          } else {
            if (!loop->end) loop->end = next;
            if (list_size (loop->end->inrefs) < list_size (next->inrefs))
              loop->end = next;
          }
        }
        ref = element_next (ref);
      }
    }

    el = element_next (el);
  }
}


static
void mark_loop (struct subroutine *sub, struct loopstructure *loop)
{
  element el;
  int num = ++sub->temp;
  int maxdfs = -1;

  el = list_head (loop->edges);
  while (el) {
    struct basicedge *edge = element_getvalue (el);
    struct basicblock *block = edge->from;

    if (maxdfs < block->node.dfsnum)
      maxdfs = block->node.dfsnum;

    if (block->mark1 != num) {
      block->mark1 = num;
      mark_backward (sub, block, num, loop->start->node.dfsnum);
    }
    el = element_next (el);
  }

  loop->start->mark2 = num;
  mark_forward (sub, loop->start, loop, num, maxdfs);

  if (loop->end) {
    el = list_head (loop->end->inrefs);
    while (el) {
      struct basicedge *edge = element_getvalue (el);
      struct basicblock *block = edge->from;
      if (block->loopst == loop)
        edge->type = EDGE_BREAK;
      el = element_next (el);
    }
  }
}

static
void extract_loops (struct subroutine *sub)
{
  struct basicblock *block;
  struct basicedge *edge;
  struct loopstructure *loop;
  element el, ref;

  el = list_head (sub->dfsblocks);
  while (el) {
    block = element_getvalue (el);

    loop = NULL;
    ref = list_head (block->inrefs);
    while (ref) {
      edge = element_getvalue (ref);
      if (edge->from->node.dfsnum >= block->node.dfsnum) {
        edge->type = EDGE_CONTINUE;
        if (!dom_isancestor (&block->node, &edge->from->node)) {
          error (__FILE__ ": graph of sub 0x%08X is not reducible (using goto)", sub->begin->address);
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        } else if (block->loopst == edge->from->loopst) {
          if (!loop) {
            loop = fixedpool_alloc (sub->code->loopspool);
            loop->start = block;
            loop->edges = list_alloc (sub->code->lstpool);
          }
          list_inserttail (loop->edges, edge);
        } else {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        }
      }
      ref = element_next (ref);
    }
    if (loop) mark_loop (sub, loop);
    el = element_next (el);
  }
}

static
void extract_ifs (struct subroutine *sub)
{
  struct basicblock *block;
  struct ifstructure *ifst;
  list unresolved;
  element el;

  unresolved = list_alloc (sub->code->lstpool);

  el = list_tail (sub->dfsblocks);
  while (el) {
    int hasswitch = FALSE;
    block = element_getvalue (el);

    if (block->type == BLOCK_SIMPLE) {
      if (block->info.simple.jumploc) {
        if (block->info.simple.jumploc->cswitch)
          hasswitch = TRUE;
      }
    }

    if (list_size (block->outrefs) == 2 && !hasswitch) {
      struct basicedge *edge1, *edge2;

      ifst = fixedpool_alloc (sub->code->ifspool);
      block->ifst = ifst;

      edge1 = list_headvalue (block->outrefs);
      edge2 = list_tailvalue (block->outrefs);
      if (edge1->type == EDGE_UNKNOWN || edge2->type == EDGE_UNKNOWN) {
        element domel;

        list_inserthead (unresolved, block);
        domel = list_head (block->node.domchildren);
        while (domel) {
          int incount = 0;
          struct basicblocknode *dom = element_getvalue (domel);
          struct basicblock *domblock = element_getvalue (dom->blockel);
          element ref;

          ref = list_head (domblock->inrefs);
          while (ref) {
            struct basicedge *edge = element_getvalue (ref);
            if (edge->from->node.dfsnum < domblock->node.dfsnum)
              incount++;
            ref = element_next (ref);
          }

          if (incount > 1) {
            block->ifst->outermost = TRUE;
            while (list_size (unresolved) != 0) {
              struct basicblock *ifblock = list_removehead (unresolved);
              ifblock->ifst->end = domblock;
            }
            break;
          }
          domel = element_next (domel);
        }
      }
    }

    el = element_previous (el);
  }

  list_free (unresolved);
}

static
void structure_search (struct basicblock *block, int identsize, struct ifstructure *ifst)
{
  struct basicedge *edge;
  struct basicblock *ifend = NULL;
  element ref;

  if (ifst)
    ifend = ifst->end;

  if (block->ifst)
    if (block->ifst->end)
      ifend = block->ifst->end;

  block->mark1 = 1;

  if (block->loopst) {
    if (block->loopst->start == block) {
      identsize++;
      if (block->loopst->end) {
        if (!block->loopst->end->mark1)
          structure_search (block->loopst->end, identsize - 1, ifst);
        else {
          block->loopst->end->haslabel = TRUE;
          block->loopst->hasendgoto = TRUE;
        }
      }
    }
  }

  if (block->ifst) {
    if (block->ifst->end && block->ifst->outermost) {
      if (!block->ifst->end->mark1)
        structure_search (block->ifst->end, identsize, ifst);
      else {
        block->ifst->end->haslabel = TRUE;
        block->ifst->hasendgoto = TRUE;
      }
    }
  }

  block->identsize = identsize;

  ref = list_head (block->outrefs);
  while (ref) {
    edge = element_getvalue (ref);
    if (edge->type == EDGE_UNKNOWN) {
      if (edge->to->loopst != block->loopst) {
        if (edge->to->loopst) {
          if (edge->to->loopst->start != edge->to) {
            edge->type = EDGE_GOTO;
            edge->to->haslabel = TRUE;
          }
        } else {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        }
      }
    }

    if (edge->type == EDGE_UNKNOWN) {
      if (edge->to == ifend) {
        edge->type = EDGE_IFEXIT;
      } else {
        if (edge->to->mark1) {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        } else {
          edge->type = EDGE_FOLLOW;
          if (block->ifst) {
            structure_search (edge->to, identsize + 1, block->ifst);
          } else {
            structure_search (edge->to, identsize, ifst);
          }
        }
      }
    }
    ref = element_next (ref);
  }
}

void reset_marks (struct subroutine *sub)
{
  element el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    block->mark1 = block->mark2 = 0;
    el = element_next (el);
  }
}

void extract_structures (struct subroutine *sub)
{
  element el;
  sub->temp = 0;
  reset_marks (sub);

  extract_loops (sub);
  extract_ifs (sub);

  reset_marks (sub);
  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    if (!block->mark1)
      structure_search (block, 0, NULL);
    el = element_next (el);
  }
}
