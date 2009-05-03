
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
  block->node.domchildren = list_alloc (sub->code->lstpool);
  block->revnode.domchildren = list_alloc (sub->code->lstpool);
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
struct scope *scope_alloc (struct subroutine *sub, enum scopetype type, struct basicblock *start)
{
  struct scope *sc = fixedpool_alloc (sub->code->scopespool);
  sc->children = list_alloc (sub->code->lstpool);
  sc->type = type;
  sc->parent = start->sc;
  sc->breakparent = NULL;

  if (sc->parent) {
    if (sc->parent->type == SCOPE_LOOP)
      sc->breakparent = sc->parent;
    else
      sc->breakparent = sc->parent->breakparent;

    list_inserttail (sc->parent->children, sc);
  }

  sc->start = start;
  list_inserttail (sub->scopes, sc);
  return sc;
}

static
void mark_backward (struct subroutine *sub, struct basicblock *block, int num, int end)
{
  element el, ref;
  int count = 1;

  el = block->node.block;
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
void mark_forward (struct subroutine *sub, struct basicblock *block, struct scope *sc, int num, int end)
{
  element el, ref;
  struct basicblock *loopend = NULL;

  el = block->node.block;
  while (el) {
    struct basicblock *block = element_getvalue (el);
    if (block->node.dfsnum > end) break;
    if (block->mark2 == num) {
      block->sc = sc;
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        struct basicblock *next = edge->to;
        if (next->node.dfsnum > block->node.dfsnum) {
          if (next->mark1 == num || list_size (next->inrefs) == 1) {
            if (next->node.dfsnum > end)
              end = next->node.dfsnum;
            next->mark2 = num;
          }

          if (block->mark1 == num && next->mark1 != num) {
            if (!loopend) loopend = next;
            if (list_size (loopend->inrefs) < list_size (next->inrefs))
              loopend = next;
          }
        }
        ref = element_next (ref);
      }
    }

    el = element_next (el);
  }

  sc->info.loop.end = loopend;
}


static
void mark_loop (struct subroutine *sub, struct scope *sc)
{
  element el;
  int num = ++sub->temp;
  int maxdfs = -1;

  el = list_head (sc->info.loop.edges);
  while (el) {
    struct basicedge *edge = element_getvalue (el);
    struct basicblock *block = edge->from;

    if (maxdfs < block->node.dfsnum)
      maxdfs = block->node.dfsnum;

    if (block->mark1 != num) {
      block->mark1 = num;
      mark_backward (sub, block, num, sc->start->node.dfsnum);
    }
    el = element_next (el);
  }

  sc->start->mark2 = num;
  mark_forward (sub, sc->start, sc, num, maxdfs);
}

static
int extract_loops (struct subroutine *sub)
{
  struct basicblock *block;
  struct basicedge *edge;
  struct scope *sc;
  element el, ref;

  el = list_head (sub->dfsblocks);
  while (el) {
    block = element_getvalue (el);

    sc = NULL;
    ref = list_head (block->inrefs);
    while (ref) {
      edge = element_getvalue (ref);
      if (edge->from->node.dfsnum >= block->node.dfsnum) {
        if (!dom_isdominator (&block->node, &edge->from->node)) {
          error (__FILE__ ": graph of sub 0x%08X is not reducible", sub->begin->address);
          return FALSE;
        }
        if (block->sc == edge->from->sc) {
          if (!sc) sc = scope_alloc (sub, SCOPE_LOOP, block);
          if (!sc->info.loop.edges) sc->info.loop.edges = list_alloc (sub->code->lstpool);
          list_inserttail (sc->info.loop.edges, edge);
        } else {
          edge->to->haslabel = TRUE;
        }
      }
      ref = element_next (ref);
    }
    if (sc) mark_loop (sub, sc);
    el = element_next (el);
  }

  return TRUE;
}

static
void extract_ifs (struct subroutine *sub)
{
  struct basicblock *block;
  struct basicblocknode *node;
  struct scope *sc;
  element el, ref;

  el = list_head (sub->dfsblocks);
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
      struct basicblocknode *runner;
      struct basicblock *next, *end;

      node = &block->revnode;
      end = element_getvalue (node->dominator->block);
      if (end->node.dfsnum > block->node.dfsnum) {
        if (end->sc != block->sc) end = NULL;

        ref = list_head (block->outrefs);
        while (ref) {
          struct basicedge *edge = element_getvalue (ref);
          sc = NULL;
          if (block->node.dfsnum < edge->to->node.dfsnum) {
            next = edge->to;
            runner = &next->revnode;
            while (runner != node->dominator) {
              if ((block->sc->parent == next->sc && block->sc->type == SCOPE_IF) ||
                  next->sc == block->sc) {
                if (!sc) {
                  sc = scope_alloc (sub, SCOPE_IF, block);
                  sc->info.branch.edge = edge;
                  sc->info.branch.end = end;
                }
                next->sc = sc;
              } else break;
              runner = runner->dominator;
              next = element_getvalue (runner->block);
            }
          }
          ref = element_next (ref);
        }
      }
    }

    el = element_next (el);
  }
}

static
void scopes_dfs (struct scope *sc, int depth, int *dfsnum)
{
  element el;

  sc->depth = depth;
  el = list_head (sc->children);
  while (el) {
    scopes_dfs (element_getvalue (el), depth + 1, dfsnum);
    el = element_next (el);
  }
  sc->dfsnum = (*dfsnum)--;
}

void extract_scopes (struct subroutine *sub)
{
  struct scope *mainsc;
  int dfsnum;
  element el;

  sub->scopes = list_alloc (sub->code->lstpool);
  mainsc = scope_alloc (sub, SCOPE_MAIN, sub->startblock);

  sub->temp = 0;
  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    block->sc = mainsc;
    block->mark1 = block->mark2 = 0;
    el = element_next (el);
  }

  if (!extract_loops (sub)) {
    sub->haserror = TRUE;
    return;
  }

  extract_ifs (sub);

  dfsnum = list_size (sub->scopes);
  scopes_dfs (mainsc, 0, &dfsnum);
}
