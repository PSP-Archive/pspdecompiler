
#include "code.h"
#include "utils.h"

static
struct basicblock *alloc_block (struct subroutine *sub)
{
  struct basicblock *block;
  block = fixedpool_alloc (sub->code->blockspool);

  block->inrefs = list_alloc (sub->code->lstpool);
  block->outrefs = list_alloc (sub->code->lstpool);
  block->node.children = list_alloc (sub->code->lstpool);
  block->revnode.children = list_alloc (sub->code->lstpool);
  block->node.frontier = list_alloc (sub->code->lstpool);
  block->revnode.frontier = list_alloc (sub->code->lstpool);
  block->sub = sub;
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

  block = alloc_block (sub);
  list_inserttail (sub->blocks, block);
  block->type = BLOCK_START;
  sub->startblock = block;

  begin = sub->begin;

  while (1) {
    block = alloc_block (sub);
    list_inserttail (sub->blocks, block);

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

  list_inserttail (from->outrefs, edge);
  list_inserttail (to->inrefs, edge);
}

static
element make_link_and_insert (struct basicblock *from, struct basicblock *to, element el)
{
  struct basicblock *block = alloc_block (from->sub);
  element inserted = element_alloc (from->sub->code->lstpool, block);

  element_insertbefore (el, inserted);
  make_link (from, block);
  make_link (block, to);
  return inserted;
}


static
void link_blocks (struct subroutine *sub)
{
  struct basicblock *block, *next;
  struct basicblock *target;
  struct location *loc;
  element el, inserted;

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
          struct basicblock *slot = alloc_block (sub);
          inserted = element_alloc (sub->code->lstpool, slot);
          element_insertbefore (el, inserted);

          slot->type = BLOCK_SIMPLE;
          slot->info.simple.begin = &block->info.simple.end[1];
          slot->info.simple.end = slot->info.simple.begin;
          make_link (block, slot);
          block = slot;
        }

        if (loc->insn->flags & INSN_LINK) {
          inserted = make_link_and_insert (block, loc[2].block, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          target->info.call.calltarget = loc->target->sub;
        } else if (loc->target->sub->begin == loc->target) {
          inserted = make_link_and_insert (block, sub->endblock, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          target->info.call.calltarget = loc->target->sub;
        } else {
          make_link (block, loc->target->block);
        }

      } else {
        if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
          inserted = make_link_and_insert (block, next, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          if (loc->target)
            target->info.call.calltarget = loc->target->sub;
        } else {
          if (loc->target) {
            if (loc->target->sub->begin == loc->target) {
              inserted = make_link_and_insert (block, sub->endblock, el);
              target = element_getvalue (inserted);
              target->type = BLOCK_CALL;
              target->info.call.calltarget = loc->target->sub;
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
struct scope *scope_alloc (struct subroutine *sub, struct scope *parent, enum scopetype type, struct basicblock *start)
{
  struct scope *sc = fixedpool_alloc (sub->code->scopespool);
  sc->edges = list_alloc (sub->code->lstpool);
  sc->children = list_alloc (sub->code->lstpool);
  sc->type = type;
  sc->parent = parent;
  sc->breakparent = NULL;

  if (parent) {
    if (parent->type == SCOPE_LOOP)
      sc->breakparent = parent;
    else
      sc->breakparent = parent->breakparent;

    list_inserttail (parent->children, sc);
  }

  sc->start = start;
  list_inserttail (sub->scopes, sc);
  return sc;
}

static
int mark_forward (struct basicblock *block, int maxdfsnum, struct scope *currscope)
{
  struct basicedge *edge;
  struct scope *sc;
  element el, ref;
  int mark;

  mark = ++block->sub->temp;
  sc = block->sc;
  if (currscope) block->sc = currscope;
  block->mark1 = mark;
  block->mark2 = 0;

  el = block->node.block;
  while (el) {
    block = element_getvalue (el);
    if (block->node.dfsnum > maxdfsnum) break;
    if (block->mark1 == mark) {
      ref = list_head (block->outrefs);
      while (ref) {
        edge = element_getvalue (ref);
        if (edge->to->node.dfsnum > block->node.dfsnum &&
            edge->to->node.dfsnum <= maxdfsnum &&
            edge->to->sc == sc) {
          if (currscope) edge->to->sc = currscope;
          edge->to->mark1 = mark;
          edge->to->mark2 = 0;
        }
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }

  return mark;
}

static
void mark_branch (struct scope *sc)
{
  struct basicedge *edge;
  int maxdfsnum;

  edge = list_headvalue (sc->edges);
  maxdfsnum = sc->start->revnode.dominator->dfsnum;
  mark_forward (edge->to, maxdfsnum, sc);
}

static
void mark_backward (struct basicblock *block, int mark, struct scope *currscope)
{
  struct basicedge *edge;
  element el, ref;

  block->sc = currscope;
  block->mark2 = 1;

  el = block->node.block;
  while (el) {
    block = element_getvalue (el);
    if (block->mark2 && block->mark1 == mark) {
      ref = list_head (block->inrefs);
      while (ref) {
        edge = element_getvalue (ref);
        if (edge->from->node.dfsnum < block->node.dfsnum &&
            edge->from->mark1 == mark &&
            !edge->from->mark2) {
          edge->from->sc = currscope;
          edge->from->mark2 = 1;
        }
        ref = element_next (ref);
      }
    }
    el = element_previous (el);
  }
}

static
void mark_loop (struct scope *sc)
{
  struct basicedge *edge;
  element ref;
  int mark;

  mark = mark_forward (sc->start, sc->maxdfsnum, NULL);
  ref = list_head (sc->edges);
  while (ref) {
    edge = element_getvalue (ref);
    mark_backward (edge->from, mark, sc);
    ref = element_next (ref);
  }
}


static
void dfs_scopes (struct scope *sc, int depth)
{
  element el;
  el = list_head (sc->children);
  while (el) {
    struct scope *next = element_getvalue (el);
    dfs_scopes (next, depth + 1);
    el = element_next (el);
  }
  sc->dfsnum = sc->start->sub->temp--;
  sc->depth = depth;
}

void extract_scopes (struct subroutine *sub)
{
  struct basicblock *block;
  struct basicedge *edge;
  struct scope *mainsc, *sc;
  element el, ref;

  sub->scopes = list_alloc (sub->code->lstpool);
  mainsc = scope_alloc (sub, NULL, SCOPE_MAIN, sub->startblock);

  sub->temp = 0;
  el = list_head (sub->dfsblocks);
  while (el) {
    block = element_getvalue (el);
    block->sc = mainsc;
    block->mark1 = 0;
    el = element_next (el);
  }

  el = list_head (sub->dfsblocks);
  while (el) {
    block = element_getvalue (el);

    sc = NULL;
    ref = list_head (block->inrefs);
    while (ref) {
      edge = element_getvalue (ref);
      if (edge->from->node.dfsnum >= block->node.dfsnum) {
        if (edge->from->sc == block->sc) {
          edge->type = EDGE_LOOP;
          if (!sc) {
            sc = scope_alloc (sub, block->sc, SCOPE_LOOP, block);
            sc->maxdfsnum = edge->from->node.dfsnum;
          }
          if (sc->maxdfsnum < edge->from->node.dfsnum)
            sc->maxdfsnum = edge->from->node.dfsnum;
          list_inserttail (sc->edges, edge);
        } else {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        }
      }
      ref = element_next (ref);
    }

    if (sc) mark_loop (sc);

    sc = NULL;
    if (list_size (block->outrefs) == 2) {
      edge = list_headvalue (block->outrefs);
      if (edge->to->node.dfsnum > block->node.dfsnum) {
        if (edge->to->sc == block->sc) {
          sc = scope_alloc (sub, block->sc, SCOPE_IFELSE, block);
          edge->type = EDGE_NORMAL;
          list_inserttail (sc->edges, edge);
          mark_branch (sc);
        } else {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        }
      }

      edge = list_tailvalue (block->outrefs);
      if (edge->to->node.dfsnum > block->node.dfsnum) {
        if (edge->to->sc == block->sc) {
          sc = scope_alloc (sub, block->sc, SCOPE_IFTHEN, block);
          edge->type = EDGE_NORMAL;
          list_inserttail (sc->edges, edge);
          mark_branch (sc);
        } else {
          edge->type = EDGE_GOTO;
          edge->to->haslabel = TRUE;
        }
      }

    } else if (list_size (block->outrefs) == 1) {
      edge = list_headvalue (block->outrefs);
      if (edge->type == EDGE_INVALID)
        edge->type = EDGE_NORMAL;
    }

    el = element_next (el);
  }

  sub->temp = list_size (sub->scopes);
  dfs_scopes (mainsc, 0);
}


int scope_isancestor (struct scope *node, struct scope *ancestor)
{
  int count = 0;
  while (node) {
    if (node == ancestor)
      return count;
    node = node->parent;
    count++;
  }
  return -1;
}
