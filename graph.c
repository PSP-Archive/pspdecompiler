
#include "code.h"
#include "utils.h"

static
void cfg_dfs_step (struct subroutine *sub, struct basicblock *block)
{
  struct basicblock *next;
  element el;

  el = list_head (block->outrefs);
  while (el) {
    next = element_getvalue (el);
    if (!next->dfsnum)
      cfg_dfs_step (sub, next);
    el = element_next (el);
  }

  block->dfsnum = sub->dfscount--;
}

void cfg_dfs (struct subroutine *sub)
{
  sub->dfscount = list_size (sub->blocks);
  cfg_dfs_step (sub, list_headvalue (sub->blocks));
}
