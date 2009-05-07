

#include "code.h"
#include "utils.h"

static
uint32 get_constant_value (struct value *val)
{
  if (val->type == VAL_VARIABLE)
    return val->val.variable->info;
  else
    return val->val.intval;
}

static
void combine_constants (struct variable *out, struct value *val)
{
  uint32 constant;
  if (out->type == VARIABLE_UNK) return;

  if (val->type == VAL_REGISTER) {
    out->type = VARIABLE_UNK;
    return;
  }

  if (val->type == VAL_VARIABLE) {
    if (val->val.variable->type == VARIABLE_CONSTANTUNK)
      return;
    if (val->val.variable->type == VARIABLE_UNK) {
      out->type = VARIABLE_UNK;
      return;
    }
  }

  constant = get_constant_value (val);
  if (out->type == VARIABLE_CONSTANTUNK) {
    out->type = VARIABLE_CONSTANT;
    out->info = constant;
  } else {
    if (out->info != constant)
      out->type = VARIABLE_UNK;
  }
}

void propagate_constants (struct subroutine *sub)
{
  list worklist = list_alloc (sub->code->lstpool);
  element varel;

  varel = list_head (sub->variables);
  while (varel) {
    struct variable *var = element_getvalue (varel);
    var->type = VARIABLE_CONSTANTUNK;
    if (var->def->type == OP_ASM ||
        var->def->type == OP_CALL ||
        var->def->type == OP_START)
      var->type = VARIABLE_UNK;
    else
      list_inserttail (worklist, var);
    varel = element_next (varel);
  }

  while (list_size (worklist) != 0) {
    struct variable *var = list_removehead (worklist);
    struct variable temp;
    struct value *val;
    element opel;

    if (var->type == VARIABLE_UNK) continue;

    if (var->def->type == OP_PHI) {
      temp.type = VARIABLE_CONSTANTUNK;

      opel = list_head (var->def->operands);
      while (opel) {
        val = element_getvalue (opel);
        combine_constants (&temp, val);
        opel = element_next (opel);
      }
    } else {
      temp.type = VARIABLE_CONSTANT;

      opel = list_head (var->def->operands);
      while (opel) {
        val = element_getvalue (opel);
        if (val->type == VAL_CONSTANT) {
        } else if (val->type == VAL_VARIABLE) {
          if (val->val.variable->type == VARIABLE_UNK)
            temp.type = VARIABLE_UNK;
          else if (val->val.variable->type == VARIABLE_CONSTANTUNK &&
                   temp.type != VARIABLE_UNK)
            temp.type = VARIABLE_CONSTANTUNK;
        }
        opel = element_next (opel);
      }

      if (temp.type == VARIABLE_CONSTANT) {
        if (var->def->type == OP_MOVE) {
          val = list_headvalue (var->def->operands);
          temp.info = get_constant_value (val);
          var->def->deferred = TRUE;
        } else if (var->def->type == OP_INSTRUCTION) {
          uint32 val1, val2;
          switch (var->def->info.iop.insn) {
          case I_ADD:
          case I_ADDU:
            val1 = get_constant_value (list_headvalue (var->def->operands));
            val2 = get_constant_value (list_tailvalue (var->def->operands));
            temp.info = val1 + val2;
            var->def->deferred = TRUE;
            break;
          case I_OR:
            val1 = get_constant_value (list_headvalue (var->def->operands));
            val2 = get_constant_value (list_tailvalue (var->def->operands));
            temp.info = val1 | val2;
            var->def->deferred = TRUE;
            break;
          default:
            temp.type = VARIABLE_UNK;
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
          if (val->type == VAL_VARIABLE)
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


