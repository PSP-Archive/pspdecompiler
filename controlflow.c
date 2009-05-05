
#include "code.h"
#include "utils.h"

static
struct basicblock *alloc_block (struct subroutine *sub, int insert)
{
  struct basicblock *block;
  block = fixedpool_alloc (sub->code->blockspool);

  block->inrefs = list_alloc (sub->code->lstpool);
  block->outrefs = list_alloc (sub->code->lstpool);
  block->node.children = list_alloc (sub->code->lstpool);
  block->revnode.children = list_alloc (sub->code->lstpool);
  block->node.domchildren = list_alloc (sub->code->lstpool);
  block->revnode.domchildren = list_alloc (sub->code->lstpool);
  block->node.frontier = list_alloc (sub->code->lstpool);
  block->revnode.frontier = list_alloc (sub->code->lstpool);
  block->sub = sub;
  if (insert) {
    block->blockel = list_inserttail (sub->blocks, block);
  } else {
    block->blockel = element_alloc (sub->code->lstpool, block);
  }

  return block;
}

static
void extract_blocks (struct subroutine *sub)
{
  struct location *begin, *next;
  struct basicblock *block;
  int prevlikely = FALSE;

  sub->blocks = list_alloc (sub->code->lstpool);
  sub->revdfsblocks = list_alloc (sub->code->lstpool);
  sub->dfsblocks = list_alloc (sub->code->lstpool);

  block = alloc_block (sub, TRUE);
  block->type = BLOCK_START;
  sub->startblock = block;

  begin = sub->begin;

  while (1) {
    block = alloc_block (sub, TRUE);

    if (!begin) break;
    next = begin;

    block->type = BLOCK_SIMPLE;
    block->info.simple.begin = begin;

    for (; next != sub->end; next++) {
      if (prevlikely) {
        prevlikely = FALSE;
        break;
      }

      if (next->references && (next != begin)) {
        next--;
        break;
      }

      if (next->insn->flags & (INSN_JUMP | INSN_BRANCH)) {
        block->info.simple.jumploc = next;
        if (next->insn->flags & INSN_BRANCHLIKELY)
          prevlikely = TRUE;
        if (!(next->insn->flags & INSN_BRANCHLIKELY) &&
            !next[1].references && location_branch_may_swap (next)) {
          next++;
        }
        break;
      }
    }
    block->info.simple.end = next;

    do {
      begin->block = block;
    } while (begin++ != next);

    begin = NULL;
    while (next++ != sub->end) {
      if (next->reachable == LOCATION_DELAY_SLOT) {
        if (!prevlikely) {
          begin = next;
          break;
        }
      } else if (next->reachable == LOCATION_REACHABLE) {
        if (next != &block->info.simple.end[1])
          prevlikely = FALSE;
        begin = next;
        break;
      }
    }
  }
  block->type = BLOCK_END;
  sub->endblock = block;
}


static
void make_link (struct basicblock *from, struct basicblock *to)
{
  struct basicedge *edge = fixedpool_alloc (from->sub->code->edgespool);

  edge->from = from;
  edge->fromnum = list_size (from->outrefs);
  edge->to = to;
  edge->tonum = list_size (to->inrefs);

  edge->fromel = list_inserttail (from->outrefs, edge);
  edge->toel = list_inserttail (to->inrefs, edge);
}

static
struct basicblock *make_link_and_insert (struct basicblock *from, struct basicblock *to, element el)
{
  struct basicblock *block = alloc_block (from->sub, FALSE);
  element_insertbefore (el, block->blockel);
  make_link (from, block);
  make_link (block, to);
  return block;
}

static
void make_call (struct basicblock *block, struct location *loc)
{
  block->type = BLOCK_CALL;
  if (loc->target) {
    block->info.call.calltarget = loc->target->sub;
    list_inserttail (loc->target->sub->whereused, block);
  }
}


static
void link_blocks (struct subroutine *sub)
{
  struct basicblock *block, *next;
  struct basicblock *target;
  struct location *loc;
  element el;

  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);
    if (block->type == BLOCK_END) break;
    if (block->type == BLOCK_START) {
      el = element_next (el);
      make_link (block, element_getvalue (el));
      continue;
    }

    el = element_next (el);
    next = element_getvalue (el);


    if (block->info.simple.jumploc) {
      loc = block->info.simple.jumploc;
      if (loc->insn->flags & INSN_BRANCH) {
        if (!loc->branchalways) {
          if (loc->insn->flags & INSN_BRANCHLIKELY) {
            make_link (block, loc[2].block);
          } else {
            make_link (block, next);
          }
        }

        if (loc == block->info.simple.end) {
          struct basicblock *slot = alloc_block (sub, FALSE);
          element_insertbefore (el, slot->blockel);

          slot->type = BLOCK_SIMPLE;
          slot->info.simple.begin = &block->info.simple.end[1];
          slot->info.simple.end = slot->info.simple.begin;
          make_link (block, slot);
          block = slot;
        }

        if (loc->insn->flags & INSN_LINK) {
          target = make_link_and_insert (block, loc[2].block, el);
          make_call (target, loc);
        } else if (loc->target->sub->begin == loc->target) {
          target = make_link_and_insert (block, sub->endblock, el);
          make_call (target, loc);
        } else {
          make_link (block, loc->target->block);
        }

      } else {
        if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
          target = make_link_and_insert (block, next, el);
          make_call (target, loc);
        } else {
          if (loc->target) {
            if (loc->target->sub->begin == loc->target) {
              target = make_link_and_insert (block, sub->endblock, el);
              make_call (target, loc);
            } else {
              make_link (block, loc->target->block);
            }
          } else {
            element ref;
            if (loc->cswitch && loc->cswitch->jumplocation == loc) {
              ref = list_head (loc->cswitch->references);
              while (ref) {
                struct location *switchtarget = element_getvalue (ref);
                make_link (block, switchtarget->block);
                ref = element_next (ref);
              }
            } else
              make_link (block, sub->endblock);
          }
        }
      }
    } else {
      make_link (block, next);
    }
  }
}

void extract_cfg (struct subroutine *sub)
{
  extract_blocks (sub);
  link_blocks (sub);
  if (!cfg_dfs (sub, 0)) {
    error (__FILE__ ": unreachable code at subroutine 0x%08X", sub->begin->address);
    sub->haserror = TRUE;
    return;
  }
  if (!cfg_dfs (sub, 1)) {
    error (__FILE__ ": infinite loop at subroutine 0x%08X", sub->begin->address);
    sub->haserror = TRUE;
    return;
  }

  cfg_dominance (sub, 0);
  cfg_frontier (sub, 0);

  cfg_dominance (sub, 1);
  cfg_frontier (sub, 1);
}


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
          if (next->mark1 == num) next->mark2 = num;
          else {
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
        } else if (block->loopst == edge->from->loopst ||
            dom_isancestor (&block->revnode, &edge->from->revnode)) {
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
