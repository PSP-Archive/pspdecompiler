
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
    int changed = FALSE;

    if (var->type != VARIABLE_UNK) {
      element opel;

      opel = list_head (var->def->operands);
      temp.type = VARIABLE_CONSTANT;
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
        } else if (var->def->type == OP_PHI) {
          opel = list_head (var->def->operands);
          while (opel) {
            uint32 intval = get_constant_value (element_getvalue (opel));
            if (opel == list_head (var->def->operands))
              temp.info = intval;
            else {
              if (temp.info != intval) {
                temp.type = VARIABLE_UNK;
                break;
              }
            }
            opel = element_next (opel);
          }
        } else if (var->def->type == OP_INSTRUCTION) {
          uint32 val1, val2;
          switch (var->def->insn) {
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

      if (temp.type != var->type)
        changed = TRUE;
      var->type = temp.type;
      var->info = temp.info;
    }

    if (changed) {
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
  }

  list_free (worklist);
}


static
void mark_variable (struct variable *var, enum variabletype type, int num)
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
        if (val->val.variable->type == VARIABLE_UNK) {
          mark_variable (val->val.variable, type, num);
        }
        phiel = element_next (phiel);
      }
      val = list_headvalue (use->results);
      if (val->val.variable->type == VARIABLE_UNK)
        mark_variable (val->val.variable, type, num);
    }
    useel = element_next (useel);
  }

  if (var->def->type == OP_PHI) {
    phiel = list_head (var->def->operands);
    while (phiel) {
      struct value *val = element_getvalue (phiel);
      if (val->val.variable->type == VARIABLE_UNK) {
        mark_variable (val->val.variable, type, num);
      }
      phiel = element_next (phiel);
    }
  }
}

static
void mark_variables (struct subroutine *sub)
{
  element varel;
  int count = 0;

  varel = list_head (sub->variables);
  while (varel) {
    struct variable *var = element_getvalue (varel);
    if (var->type == VARIABLE_UNK) {
      if (var->def->type == OP_START) {
        mark_variable (var, VARIABLE_ARGUMENT, var->name.val.intval);
      } else if (var->def->type == OP_CALL && var->name.val.intval != 2 &&
                 var->name.val.intval != 3) {
        var->type = VARIABLE_INVALID;
      } else {
        int istemp = FALSE;

        if (list_size (var->uses) <= 1) {
          struct operation *op = list_headvalue (var->uses);
          if (op) {
            if (op->type != OP_PHI)
              istemp = TRUE;
          } else {
            istemp = TRUE;
          }
        }

        if (var->def->type == OP_MOVE || var->def->type == OP_INSTRUCTION) {
          if (var->def->type == OP_INSTRUCTION) {
            if (var->def->begin->insn->flags & (INSN_LOAD | INSN_STORE | INSN_BRANCH))
              istemp = FALSE;
          }
        } else {
          istemp = FALSE;
        }

        if (istemp) {
          var->def->deferred = TRUE;
          var->type = VARIABLE_TEMP;
          var->info = 0;
        } else {
          mark_variable (var, VARIABLE_LOCAL, ++count);
        }
      }
    }
    varel = element_next (varel);
  }
}

void extract_variables (struct subroutine *sub)
{
  propagate_constants (sub);
  mark_variables (sub);
}
