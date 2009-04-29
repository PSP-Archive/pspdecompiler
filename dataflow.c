
#include "code.h"
#include "utils.h"


static
void mark_variable (struct variable *var, enum variabletype type, int num)
{
  element useel, phiel;
  struct value *val;

  var->varnum = num;
  var->type = type;
  useel = list_head (var->uses);
  while (useel) {
    struct operation *use = element_getvalue (useel);
    if (use->type == OP_PHI) {
      phiel = list_head (use->operands);
      while (phiel) {
        struct value *val = element_getvalue (phiel);
        if (val->val.variable->varnum == 0) {
          mark_variable (val->val.variable, type, num);
        }
        phiel = element_next (phiel);
      }
      val = list_headvalue (use->results);
      if (val->val.variable->varnum == 0)
        mark_variable (val->val.variable, type, num);
    }
    useel = element_next (useel);
  }

  if (var->def->type == OP_PHI) {
    phiel = list_head (var->def->operands);
    while (phiel) {
      struct value *val = element_getvalue (phiel);
      if (val->val.variable->varnum == 0) {
        mark_variable (val->val.variable, type, num);
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
      if (var->def->type == OP_START) {
        mark_variable (var, VARIABLE_ARGUMENT, var->name.val.intval);
      } else if (var->def->type == OP_CALL && var->name.val.intval != 2 &&
                 var->name.val.intval != 3) {
        mark_variable (var, VARIABLE_INVALID, 1);
      } else {
        int istemp = FALSE;
        if (list_size (var->uses) <= 1) {
          struct operation *op = list_headvalue (var->uses);
          if (op) {
            if (op->type != OP_PHI) istemp = TRUE;
          } else {
            istemp = TRUE;
          }
        }

        if (istemp) {
          var->type = VARIABLE_TEMP;
          var->varnum = 1;
        } else {
          var->def->flushed = TRUE;
          mark_variable (var, VARIABLE_LOCAL,  ++count);
        }
      }
    }
    varel = element_next (varel);
  }
}
