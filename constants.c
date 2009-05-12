/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */


#include "code.h"
#include "utils.h"

static
uint32 get_constant_value (struct value *val)
{
  if (val->type == VAL_SSAVAR)
    return val->val.variable->info;
  else
    return val->val.intval;
}

static
void combine_constants (struct ssavar *out, struct value *val)
{
  uint32 constant;
  if (out->type == SSAVAR_UNK) return;

  if (val->type == VAL_REGISTER) {
    out->type = SSAVAR_UNK;
    return;
  }

  if (val->type == VAL_SSAVAR) {
    if (val->val.variable->type == SSAVAR_CONSTANTUNK)
      return;
    if (val->val.variable->type == SSAVAR_UNK) {
      out->type = SSAVAR_UNK;
      return;
    }
  }

  constant = get_constant_value (val);
  if (out->type == SSAVAR_CONSTANTUNK) {
    out->type = SSAVAR_CONSTANT;
    out->info = constant;
  } else {
    if (out->info != constant)
      out->type = SSAVAR_UNK;
  }
}

void propagate_constants (struct subroutine *sub)
{
  list worklist = list_alloc (sub->code->lstpool);
  element varel;

  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    var->type = SSAVAR_CONSTANTUNK;
    if (var->def->type == OP_ASM ||
        var->def->type == OP_CALL ||
        var->def->type == OP_START ||
        !(IS_BIT_SET (regmask_localvars, var->name.val.intval)))
      var->type = SSAVAR_UNK;
    else
      list_inserttail (worklist, var);
    varel = element_next (varel);
  }

  while (list_size (worklist) != 0) {
    struct ssavar *var = list_removehead (worklist);
    struct ssavar temp;
    struct value *val;
    element opel;

    if (var->type == SSAVAR_UNK) continue;

    if (var->def->type == OP_PHI) {
      temp.type = SSAVAR_CONSTANTUNK;

      opel = list_head (var->def->operands);
      while (opel) {
        val = element_getvalue (opel);
        combine_constants (&temp, val);
        opel = element_next (opel);
      }
    } else {
      temp.type = SSAVAR_CONSTANT;

      opel = list_head (var->def->operands);
      while (opel) {
        val = element_getvalue (opel);
        if (val->type == VAL_CONSTANT) {
        } else if (val->type == VAL_SSAVAR) {
          if (val->val.variable->type == SSAVAR_UNK)
            temp.type = SSAVAR_UNK;
          else if (val->val.variable->type == SSAVAR_CONSTANTUNK &&
                   temp.type != SSAVAR_UNK)
            temp.type = SSAVAR_CONSTANTUNK;
        }
        opel = element_next (opel);
      }

      if (temp.type == SSAVAR_CONSTANT) {
        if (var->def->type == OP_MOVE) {
          val = list_headvalue (var->def->operands);
          temp.info = get_constant_value (val);
          var->def->status |= OP_STAT_DEFERRED;
        } else if (var->def->type == OP_INSTRUCTION) {
          uint32 val1, val2;
          switch (var->def->info.iop.insn) {
          case I_ADD:
          case I_ADDU:
            val1 = get_constant_value (list_headvalue (var->def->operands));
            val2 = get_constant_value (list_tailvalue (var->def->operands));
            temp.info = val1 + val2;
            var->def->status |= OP_STAT_DEFERRED;
            break;
          case I_OR:
            val1 = get_constant_value (list_headvalue (var->def->operands));
            val2 = get_constant_value (list_tailvalue (var->def->operands));
            temp.info = val1 | val2;
            var->def->status |= OP_STAT_DEFERRED;
            break;
          default:
            temp.type = SSAVAR_UNK;
            break;
          }
        }
      }

    }

    if (temp.type != var->type) {
      element useel;
      useel = list_head (var->uses);
      while (useel) {
        struct operation *use = element_getvalue (useel);
        varel = list_head (use->results);
        while (varel) {
          val = element_getvalue (varel);
          if (val->type == VAL_SSAVAR)
            list_inserttail (worklist, val->val.variable);
          varel = element_next (varel);
        }
        useel = element_next (useel);
      }
    }
    var->type = temp.type;
    var->info = temp.info;

  }

  list_free (worklist);
}


