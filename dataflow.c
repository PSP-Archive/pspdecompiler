
#include "code.h"
#include "utils.h"

static
int ssa_step (struct subroutine *sub, struct basicblock *block)
{
  return 1;
}

void build_ssa (struct subroutine *sub)
{
  struct variable *vars[VARIABLES_NUM];
  int i;

  for (i = 0; i < VARIABLES_NUM; i++) {
    vars[i] = fixedpool_alloc (sub->code->varspool);
    vars[i]->type = VARIABLE_TYPE_REGISTER;
    vars[i]->num = i;
  }

  ssa_step (sub, list_headvalue (sub->blocks));
}
