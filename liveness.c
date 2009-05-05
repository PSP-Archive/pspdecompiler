
#include "code.h"
#include "utils.h"

void live_registers (struct subroutine *sub)
{
  list worklist = list_alloc (sub->code->lstpool);
  struct basicblock *block;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    block->mark1 = 0;
    el = element_next (el);
  }

  list_inserthead (worklist, sub->endblock);

  while (list_size (worklist) != 0) {
    struct basicedge *edge;
    struct basicblock *bref;

    block = list_removehead (worklist);
    block->mark1 = 0;
    block->reg_live_out[0] = (block->reg_live_in[0] & ~(block->reg_kill[0])) | block->reg_gen[0];
    block->reg_live_out[1] = (block->reg_live_in[1] & ~(block->reg_kill[1])) | block->reg_gen[1];
    el = list_head (block->inrefs);
    while (el) {
      uint32 changed;

      edge = element_getvalue (el);
      bref = edge->from;

      changed = block->reg_live_out[0] & (~bref->reg_live_in[0]);
      bref->reg_live_in[0] |= block->reg_live_out[0];
      changed |= block->reg_live_out[1] & (~bref->reg_live_in[1]);
      bref->reg_live_in[1] |= block->reg_live_out[1];

      if (changed && !bref->mark1) {
        list_inserttail (worklist, bref);
        bref->mark1 = 1;
      }

      el = element_next (el);
    }
  }

  list_free (worklist);
}
