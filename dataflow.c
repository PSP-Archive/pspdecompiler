/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

const uint32 regmask_localvars[NUM_REGMASK] = { 0x43FFFFFE, 0x00000000 };

static
void mark_ssavar (struct ssavar *var, enum ssavartype type, int num)
{
  element useel, phiel;
  struct value *val;

  var->info = num;
  var->type = type;
  useel = list_head (var->uses);
  while (useel) {
    struct operation *use = element_getvalue (useel);
    if (use->type == OP_PHI) {
      phiel = list_head (use->operands);
      while (phiel) {
        struct value *val = element_getvalue (phiel);
        if (val->val.variable->type == SSAVAR_UNK) {
          mark_ssavar (val->val.variable, type, num);
        }
        phiel = element_next (phiel);
      }
      val = list_headvalue (use->results);
      if (val->val.variable->type == SSAVAR_UNK)
        mark_ssavar (val->val.variable, type, num);
    }
    useel = element_next (useel);
  }

  if (var->def->type == OP_PHI) {
    phiel = list_head (var->def->operands);
    while (phiel) {
      struct value *val = element_getvalue (phiel);
      if (val->val.variable->type == SSAVAR_UNK) {
        mark_ssavar (val->val.variable, type, num);
      }
      phiel = element_next (phiel);
    }
  }
}

void extract_variables (struct subroutine *sub)
{
  element varel;
  int count = 0;

  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    if (var->type == SSAVAR_UNK) {
      if (IS_BIT_SET (regmask_localvars, var->name.val.intval)) {
        if (var->def->type == OP_START) {
          mark_ssavar (var, SSAVAR_ARGUMENT, var->name.val.intval);
        } else if (var->def->type == OP_CALL && var->name.val.intval != REGISTER_GPR_V0 &&
                   var->name.val.intval != REGISTER_GPR_V1) {
          mark_ssavar (var, SSAVAR_INVALID, 0);
        } else {
          int istemp = FALSE;

          if (list_size (var->uses) <= 1 &&
              !(var->status & VAR_STAT_PHIARG)) {
            istemp = FALSE;
          }

          if (var->def->type == OP_MOVE || var->def->type == OP_INSTRUCTION) {
            if (var->def->type == OP_INSTRUCTION) {
              if (var->def->info.iop.loc->insn->flags & (INSN_LOAD | INSN_STORE | INSN_BRANCH))
                istemp = FALSE;
            }
          } else {
            istemp = FALSE;
          }

          if (istemp) {
            var->def->status |= OP_STAT_DEFERRED;
            var->type = SSAVAR_TEMP;
            var->info = 0;
          } else {
            mark_ssavar (var, SSAVAR_LOCAL, ++count);
          }
        }
      } else {
        mark_ssavar (var, SSAVAR_ARGUMENT, var->name.val.intval);
      }
    }
    varel = element_next (varel);
  }
}
