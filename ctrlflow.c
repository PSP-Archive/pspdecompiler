
#include "code.h"
#include "utils.h"

static
struct basicblock *alloc_block (struct subroutine *sub)
{
  struct basicblock *block;
  block = fixedpool_alloc (sub->code->blockspool);

  block->inrefs = list_alloc (sub->code->lstpool);
  block->outrefs = list_alloc (sub->code->lstpool);
  block->frontier = list_alloc (sub->code->lstpool);
  block->sub = sub;
  return block;
}

static
void extract_blocks (struct subroutine *sub)
{
  struct location *begin, *next;
  struct basicblock *block;

  sub->blocks = list_alloc (sub->code->lstpool);
  sub->revdfsblocks = list_alloc (sub->code->lstpool);
  sub->dfsblocks = list_alloc (sub->code->lstpool);

  block = alloc_block (sub);
  list_inserttail (sub->blocks, block);
  block->type = BLOCK_START;

  begin = sub->begin;

  while (1) {
    block = alloc_block (sub);
    list_inserttail (sub->blocks, block);

    if (!begin) break;
    next = begin;

    block->type = BLOCK_SIMPLE;
    block->val.simple.begin = begin;
    block->val.simple.end = next;

    for (; next != sub->end; next++) {
      if (next->references && (next != begin)) {
        next--;
        break;
      }

      if (next->insn->flags & (INSN_JUMP | INSN_BRANCH)) {
        block->val.simple.jumploc = next;
        block->val.simple.end = next;
        if (!(next->insn->flags & INSN_BRANCHLIKELY))
          block->val.simple.end++;
        break;
      }

      block->val.simple.end = next;
    }

    do {
      begin->block = block;
    } while (begin++ != next);

    begin = NULL;
    while (next++ != sub->end) {
      if (next->reachable == LOCATION_REACHABLE) {
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
  list_inserttail (from->outrefs, to);
  list_inserttail (to->inrefs, from);
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


    if (block->val.simple.jumploc) {
      loc = block->val.simple.jumploc;
      if (loc->insn->flags & INSN_BRANCH) {
        make_link (block, next);

        if (loc->insn->flags & INSN_BRANCHLIKELY) {
          struct basicblock *slot = alloc_block (sub);
          inserted = element_alloc (sub->code->lstpool, slot);
          element_insertbefore (el, inserted);

          slot->type = BLOCK_SIMPLE;
          slot->val.simple.begin = &block->val.simple.end[1];
          slot->val.simple.end = slot->val.simple.begin;
          make_link (block, slot);
          block = slot;
        }

        if (loc->insn->flags & INSN_LINK) {
          inserted = make_link_and_insert (block, next, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          target->val.call.calltarget = loc->target->sub;
        } else if (loc->target->sub->begin == loc->target) {
          inserted = make_link_and_insert (block, sub->endblock, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          target->val.call.calltarget = loc->target->sub;
        } else {
          make_link (block, loc->target->block);
        }

      } else {
        if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
          inserted = make_link_and_insert (block, next, el);
          target = element_getvalue (inserted);
          target->type = BLOCK_CALL;
          if (loc->target)
            target->val.call.calltarget = loc->target->sub;
        } else {
          if (loc->target) {
            if (loc->target->sub->begin == loc->target) {
              inserted = make_link_and_insert (block, sub->endblock, el);
              target = element_getvalue (inserted);
              target->type = BLOCK_CALL;
              target->val.call.calltarget = loc->target->sub;
            } else {
              make_link (block, loc->target->block);
            }
          } else {
            if (loc->cswitch) {
              element ref;
              int count = 0;

              if (loc->cswitch->jumplocation == loc) {
                ref = list_head (loc->cswitch->references);
                while (ref) {
                  struct location *switchtarget = element_getvalue (ref);
                  inserted = make_link_and_insert (block, switchtarget->block, el);
                  target = element_getvalue (inserted);
                  target->type = BLOCK_SWITCH;
                  target->val.sw.switchnum = count++;
                  target->val.sw.cswitch = loc->cswitch;
                  ref = element_next (ref);
                }
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
  if (!cfg_dfs (sub)) {
    error (__FILE__ ": unreachable code at subroutine 0x%08X", sub->begin->address);
    sub->haserror = TRUE;
    return;
  }
  if (!cfg_revdfs (sub)) {
    error (__FILE__ ": infinite loop at subroutine 0x%08X", sub->begin->address);
    sub->haserror = TRUE;
    return;
  }

  cfg_dominance (sub);
  cfg_revdominance (sub);
}
