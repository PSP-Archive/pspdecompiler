
#include "code.h"
#include "utils.h"


static
void mark_backward (struct subroutine *sub, struct basicblock *start,
                    struct basicblock *block, int num, int *count)
{
  element el, ref;
  int remaining = 1;

  block->mark1 = num;
  el = block->node.blockel;
  while (el && remaining) {
    struct basicblock *block = element_getvalue (el);
    if (block == start) break;
    if (block->mark1 == num) {
      remaining--;
      ref = list_head (block->inrefs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        struct basicblock *next = edge->from;
        ref = element_next (ref);

        if (next->node.dfsnum >= block->node.dfsnum)
          if (!dom_isancestor (&block->node, &next->node) ||
              !dom_isancestor (&block->revnode, &next->revnode))
            continue;
        if (next->mark1 != num) {
          remaining++;
          (*count)++;
        }
        next->mark1 = num;
      }
    }

    el = element_previous (el);
  }
}

static
void mark_forward (struct subroutine *sub, struct basicblock *start,
                   struct ctrlstruct *loop, int num, int count)
{
  element el, ref;

  el = start->node.blockel;
  while (el && count) {
    struct basicblock *block = element_getvalue (el);
    if (block->mark1 == num) {
      block->loopst = loop; count--;
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        struct basicblock *next = edge->to;
        if (next->mark1 != num) {
          edge->type = EDGE_GOTO;
          next->haslabel = TRUE;
          if (!loop->end) loop->end = next;
          if (list_size (loop->end->inrefs) < list_size (next->inrefs))
            loop->end = next;
        }
        ref = element_next (ref);
      }
    }

    el = element_next (el);
  }
}


static
void mark_loop (struct subroutine *sub, struct ctrlstruct *loop)
{
  element el;
  int num = ++sub->temp;
  int count = 0;

  el = list_head (loop->info.loopctrl.edges);
  while (el) {
    struct basicedge *edge = element_getvalue (el);
    struct basicblock *block = edge->from;

    if (block->mark1 != num) {
      mark_backward (sub, loop->start, block, num, &count);
    }
    el = element_next (el);
  }

  mark_forward (sub, loop->start, loop, num, count);

  if (loop->end) {
    loop->end->haslabel = FALSE;
    el = list_head (loop->end->inrefs);
    while (el) {
      struct basicedge *edge = element_getvalue (el);
      struct basicblock *block = edge->from;
      if (block->loopst == loop) edge->type = EDGE_BREAK;
      el = element_next (el);
    }
  }
}

static
void extract_loops (struct subroutine *sub)
{
  struct basicblock *block;
  struct basicedge *edge;
  struct ctrlstruct *loop;
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
            loop = fixedpool_alloc (sub->code->ctrlspool);
            loop->start = block;
            loop->type = CONTROL_LOOP;
            loop->info.loopctrl.edges = list_alloc (sub->code->lstpool);
          }
          list_inserttail (loop->info.loopctrl.edges, edge);
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
void extract_branches (struct subroutine *sub)
{
  struct basicblock *block;
  struct ctrlstruct *st;
  list unresolvedifs;
  element el;

  unresolvedifs = list_alloc (sub->code->lstpool);

  el = list_tail (sub->dfsblocks);
  while (el) {
    int hasswitch = FALSE;
    block = element_getvalue (el);

    if (block->type == BLOCK_SIMPLE)
      if (block->info.simple.jumploc)
        if (block->info.simple.jumploc->cswitch)
          hasswitch = TRUE;

    if (hasswitch) {
      st = fixedpool_alloc (sub->code->ctrlspool);
      st->type = CONTROL_SWITCH;
      block->st = st;

    } else if (list_size (block->outrefs) == 2) {
      struct basicedge *edge1, *edge2;

      st = fixedpool_alloc (sub->code->ctrlspool);
      st->type = CONTROL_IF;
      block->st = st;

      edge1 = list_headvalue (block->outrefs);
      edge2 = list_tailvalue (block->outrefs);
      if (edge1->type == EDGE_UNKNOWN && edge2->type == EDGE_UNKNOWN) {
        element domel;

        list_inserthead (unresolvedifs, block);
        domel = list_head (block->node.domchildren);
        while (domel) {
          struct basicblocknode *dom = element_getvalue (domel);
          struct basicblock *domblock = element_getvalue (dom->blockel);

          if (list_size (domblock->inrefs) > 1) {
            block->st->info.ifctrl.isoutermost = TRUE;
            while (list_size (unresolvedifs) != 0) {
              struct basicblock *ifblock = list_removehead (unresolvedifs);
              ifblock->st->end = domblock;
            }
            break;
          }
          domel = element_next (domel);
        }
      } else if (edge1->type == EDGE_UNKNOWN || edge2->type == EDGE_UNKNOWN) {
        if (edge1->type == EDGE_UNKNOWN) {
          edge1->type = EDGE_IFEXIT;
          st->end = edge1->to;
        } else {
          edge2->type = EDGE_IFEXIT;
          st->end = edge2->to;
        }

      }
    }

    el = element_previous (el);
  }

  list_free (unresolvedifs);
}

static
void structure_search (struct basicblock *block, int identsize, struct ctrlstruct *ifst)
{
  struct basicedge *edge;
  struct basicblock *ifend = NULL;
  element ref;

  if (ifst)
    ifend = ifst->end;

  if (block->st)
    if (block->st->end)
      ifend = block->st->end;

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

  if (block->st) {
    if (block->st->end && block->st->info.ifctrl.isoutermost) {
      if (!block->st->end->mark1)
        structure_search (block->st->end, identsize, ifst);
      else {
        block->st->end->haslabel = TRUE;
        block->st->hasendgoto = TRUE;
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
          if (block->st) {
            structure_search (edge->to, identsize + 1, block->st);
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
  extract_branches (sub);

  reset_marks (sub);
  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    if (!block->mark1)
      structure_search (block, 0, NULL);
    el = element_next (el);
  }
}
