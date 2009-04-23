
#include "code.h"
#include "utils.h"

static
int ssa_step (struct code *c, struct subroutine *sub, struct basicblock *block)
{
  return 1;
}

void build_ssa (struct code *c, struct subroutine *sub)
{
  struct variable *vars[VARIABLES_NUM];
  int i;

  for (i = 0; i < VARIABLES_NUM; i++) {
    vars[i] = fixedpool_alloc (c->varspool);
    vars[i]->type = VARIABLE_TYPE_REGISTER;
    vars[i]->num = i;
  }

  ssa_step (c, sub, list_headvalue (sub->blocks));
}
