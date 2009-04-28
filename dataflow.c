
#include "code.h"
#include "utils.h"


static
void mark_variable (struct variable *var, int num)
{
  element useel, phiel;
  struct value *val;

  var->varnum = num;
  useel = list_head (var->uses);
  while (useel) {
    struct operation *use = element_getvalue (useel);
    if (use->type == OP_PHI) {
      phiel = list_head (use->operands);
      while (phiel) {
        struct value *val = element_getvalue (phiel);
        if (val->val.variable->varnum == 0) {
          mark_variable (val->val.variable, num);
        }
        phiel = element_next (phiel);
      }
      val = list_headvalue (use->results);
      if (val->val.variable->varnum == 0)
        mark_variable (val->val.variable, num);
    }
    useel = element_next (useel);
  }

  if (var->def->type == OP_PHI) {
    phiel = list_head (var->def->operands);
    while (phiel) {
      struct value *val = element_getvalue (phiel);
      if (val->val.variable->varnum == 0) {
        mark_variable (val->val.variable, num);
      }
      phiel = element_next (phiel);
    }
  }
}

void extract_variables (struct subroutine *sub)
{
  element varel;
  int count = 0;

  varel = list_head (sub->variables);
  while (varel) {
    struct variable *var = element_getvalue (varel);
    if (var->varnum == 0) {
      mark_variable (var, ++count);
    }
    varel = element_next (varel);
  }
}
