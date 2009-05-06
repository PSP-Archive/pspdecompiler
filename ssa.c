
#include "code.h"
#include "utils.h"

static
struct variable *alloc_variable (struct basicblock *block)
{
  struct variable *var;
  var = fixedpool_alloc (block->sub->code->varspool);
  var->uses = list_alloc (block->sub->code->lstpool);
  return var;
}

static
void ssa_place_phis (struct subroutine *sub, list *defblocks)
{
  struct basicblock *block, *bref;
  struct basicblocknode *brefnode;
  element el, ref;
  int i, j;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    block->mark1 = 0;
    block->mark2 = 0;
    el = element_next (el);
  }

  for (i = 1; i < NUM_REGISTERS; i++) {
    list worklist = defblocks[i];
    el = list_head (worklist);
    while (el) {
      block = element_getvalue (el);
      block->mark1 = i;
      el = element_next (el);
    }

    while (list_size (worklist) != 0) {
      block = list_removehead (worklist);
      ref = list_head (block->node.frontier);
      while (ref) {
        brefnode = element_getvalue (ref);
        bref = element_getvalue (brefnode->blockel);
        if (bref->mark2 != i && IS_BIT_SET (bref->reg_live_out, i)) {
          struct operation *op;

          bref->mark2 = i;
          op = operation_alloc (bref);
          op->type = OP_PHI;
          value_append (sub, op->results, VAL_REGISTER, i);
          for (j = list_size (bref->inrefs); j > 0; j--)
            value_append (sub, op->operands, VAL_REGISTER, i);
          list_inserthead (bref->operations, op);

          if (bref->mark1 != i) {
            bref->mark1 = i;
            list_inserttail (worklist, bref);
          }
        }
        ref = element_next (ref);
      }
    }
  }
}

static
void ssa_search (struct basicblock *block, list *vars)
{
  element el;
  int i, pushed[NUM_REGISTERS];

  for (i = 1; i < NUM_REGISTERS; i++)
    pushed[i] = FALSE;

  el = list_head (block->operations);
  while (el) {
    struct operation *op;
    struct variable *var;
    struct value *val;
    element opel, rel;

    op = element_getvalue (el);

    if (op->type != OP_PHI) {
      opel = list_head (op->operands);
      while (opel) {
        val = element_getvalue (opel);
        if (val->type == VAL_REGISTER) {
          var = list_headvalue (vars[val->val.intval]);
          val->type = VAL_VARIABLE;
          val->val.variable = var;
          list_inserttail (var->uses, op);
        }
        opel = element_next (opel);
      }
    }

    rel = list_head (op->results);
    while (rel) {
      val = element_getvalue (rel);
      if (val->type == VAL_REGISTER) {
        val->type = VAL_VARIABLE;
        var = alloc_variable (block);
        var->name.type = VAL_REGISTER;
        var->name.val.intval = val->val.intval;
        var->def = op;
        list_inserttail (block->sub->variables, var);
        if (!pushed[val->val.intval]) {
          pushed[val->val.intval] = TRUE;
          list_inserthead (vars[val->val.intval], var);
        } else {
          element_setvalue (list_head (vars[val->val.intval]), var);
        }
        val->val.variable = var;
      }
      rel = element_next (rel);
    }

    el = element_next (el);
  }

  el = list_head (block->outrefs);
  while (el) {
    struct basicedge *edge;
    struct basicblock *ref;
    element phiel, opel;

    edge = element_getvalue (el);
    ref = edge->to;

    phiel = list_head (ref->operations);
    while (phiel) {
      struct operation *op;
      struct value *val;

      op = element_getvalue (phiel);
      if (op->type != OP_PHI) break;

      opel = list_head (op->operands);
      for (i = edge->tonum; i > 0; i--)
        opel = element_next (opel);

      val = element_getvalue (opel);
      val->type = VAL_VARIABLE;
      val->val.variable = list_headvalue (vars[val->val.intval]);
      list_inserttail (val->val.variable->uses, op);
      phiel = element_next (phiel);
    }
    el = element_next (el);
  }

  el = list_head (block->node.children);
  while (el) {
    struct basicblocknode *childnode;
    struct basicblock *child;

    childnode = element_getvalue (el);
    child = element_getvalue (childnode->blockel);
    ssa_search (child, vars);
    el = element_next (el);
  }

  for (i = 1; i < NUM_REGISTERS; i++)
    if (pushed[i]) list_removehead (vars[i]);
}

void build_ssa (struct subroutine *sub)
{
  list reglist[NUM_REGISTERS];
  element blockel;
  int i;

  reglist[0] = NULL;
  for (i = 1; i < NUM_REGISTERS; i++) {
    reglist[i] = list_alloc (sub->code->lstpool);
  }

  sub->variables = list_alloc (sub->code->lstpool);

  blockel = list_head (sub->blocks);
  while (blockel) {
    struct basicblock *block = element_getvalue (blockel);
    for (i = 0; i < NUM_REGISTERS; i++) {
      if (IS_BIT_SET (block->reg_kill, i))
        list_inserttail (reglist[i], block);
    }
    blockel = element_next (blockel);
  }

  ssa_place_phis (sub, reglist);
  ssa_search (sub->startblock, reglist);

  for (i = 1; i < NUM_REGISTERS; i++) {
    list_free (reglist[i]);
  }
}


