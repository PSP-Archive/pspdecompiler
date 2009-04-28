
#include "code.h"
#include "utils.h"

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))

#define IS_REG_USED(flags, regno) ((1 << (regno)) & flags)
#define REG_USE(flags, regno) (flags) |= 1 << (regno)

static
struct operation *alloc_operation (struct subroutine *sub, struct basicblock *block)
{
  struct operation *op;
  op = fixedpool_alloc (sub->code->opspool);
  op->block = block;
  op->operands = list_alloc (sub->code->lstpool);
  op->results = list_alloc (sub->code->lstpool);
  return op;
}

static
struct variable *alloc_variable (struct basicblock *block)
{
  struct variable *var;
  var = fixedpool_alloc (block->sub->code->varspool);
  var->uses = list_alloc (block->sub->code->lstpool);
  return var;
}

static
void reset_operation (struct subroutine *sub, struct operation *op)
{
  struct value *val;

  while (list_size (op->results)) {
    val = list_removehead (op->results);
    fixedpool_free (sub->code->valspool, val);
  }

  while (list_size (op->operands)) {
    val = list_removehead (op->operands);
    fixedpool_free (sub->code->valspool, val);
  }
}

static
void simplify_reg_zero (list l)
{
  struct value *val;
  element el;
  el = list_head (l);
  while (el) {
    val = element_getvalue (el);
    if (val->type == VAL_REGISTER && val->val.intval == 0) {
      val->type = VAL_CONSTANT;
    }
    el = element_next (el);
  }
}

static
void simplify_operation (struct subroutine *sub, struct operation *op)
{
  struct value *val;

  if (op->type != OP_INSTRUCTION) return;

  if (list_size (op->results) == 1 && !(op->begin->insn->flags & (INSN_LOAD | INSN_JUMP))) {
    val = list_headvalue (op->results);
    if (val->val.intval == 0) {
      reset_operation (sub, op);
      op->type = OP_NOP;
      return;
    }
  }
  simplify_reg_zero (op->results);
  simplify_reg_zero (op->operands);

  switch (op->insn) {
  case I_ADDU:
  case I_ADD:
  case I_OR:
  case I_XOR:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removehead (op->operands);
      fixedpool_free (sub->code->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removetail (op->operands);
        fixedpool_free (sub->code->valspool, val);
        op->type = OP_MOVE;
      }
    }
    break;
  case I_AND:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (sub->code->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removehead (op->operands);
        fixedpool_free (sub->code->valspool, val);
        op->type = OP_MOVE;
      }
    }

    break;
  case I_BITREV:
  case I_SEB:
  case I_SEH:
  case I_WSBH:
  case I_WSBW:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      op->type = OP_MOVE;
    }
    break;
  case I_SUB:
  case I_SUBU:
  case I_SLLV:
  case I_SRLV:
  case I_SRAV:
  case I_ROTV:
    val = list_tailvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (sub->code->valspool, val);
      op->type = OP_MOVE;
    }
  case I_MOVN:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      reset_operation (sub, op);
      op->type = OP_NOP;
    }
    break;
  case I_MOVZ:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (sub->code->valspool, val);
      val = list_removetail (op->operands);
      fixedpool_free (sub->code->valspool, val);
      op->type = OP_MOVE;
    }
    break;
  default:
    break;
  }

  return;
}

static
void append_value (struct subroutine *sub, list l, enum valuetype type, uint32 value, int inserthead)
{
  struct value *val;
  val = fixedpool_alloc (sub->code->valspool);
  val->type = type;
  val->val.intval = value;
  if (inserthead)
    list_inserthead (l, val);
  else
    list_inserttail (l, val);
}

#define GLOBAL_GPR_DEF() \
  if (!IS_REG_USED (ggpr_defined, regno) && regno != 0) { \
    list_inserttail (wheredefined[regno], block);         \
  }                                                       \
  REG_USE (ggpr_defined, regno)

#define GPR_DEF() \
  if (!IS_REG_USED (gpr_defined, regno) && regno != 0) {           \
    list_inserttail (wheredefined[regno], block);                  \
    append_value (sub, op->results, VAL_REGISTER, regno, FALSE);   \
  }                                                                \
  REG_USE (gpr_defined, regno)

#define GPR_USE() \
  if (!IS_REG_USED (gpr_used, regno) && regno != 0 &&              \
      !IS_REG_USED (gpr_defined, regno)) {                         \
    append_value (sub, op->operands, VAL_REGISTER, regno, FALSE);  \
  }                                                                \
  REG_USE (gpr_used, regno)

#define GLOBAL_HILO_DEF() \
  if (!IS_REG_USED (ghilo_defined, regno)) {                    \
    list_inserttail (wheredefined[REGISTER_LO + regno], block); \
  }                                                             \
  REG_USE (ghilo_defined, regno)

#define HILO_DEF() \
  if (!IS_REG_USED (hilo_defined, regno)) {                                    \
    append_value (sub, op->results, VAL_REGISTER, REGISTER_LO + regno, FALSE); \
    list_inserttail (wheredefined[REGISTER_LO + regno], block);                \
  }                                                                            \
  REG_USE (hilo_defined, regno)

#define HILO_USE() \
  if (!IS_REG_USED (hilo_used, regno) &&                                        \
      !IS_REG_USED (hilo_defined, regno)) {                                     \
    append_value (sub, op->operands, VAL_REGISTER, REGISTER_LO + regno, FALSE); \
  }                                                                             \
  REG_USE (hilo_used, regno)

static
void extract_operations (struct subroutine *sub, list *wheredefined)
{
  struct operation *op;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    block->operations = list_alloc (sub->code->lstpool);

    if (block->type == BLOCK_SIMPLE) {
      uint32 ggpr_defined = 0, ghilo_defined = 0;
      uint32 gpr_used, gpr_defined;
      uint32 hilo_used, hilo_defined;
      struct location *loc;
      int lastasm = FALSE;

      for (loc = block->info.simple.begin; ; loc++) {
        if (INSN_TYPE (loc->insn->flags) == INSN_ALLEGREX) {
          enum allegrex_insn insn;

          if (lastasm)
            list_inserttail (block->operations, op);
          lastasm = FALSE;

          op = alloc_operation (sub, block);
          op->type = OP_INSTRUCTION;
          op->begin = op->end = loc;

          if (loc->insn->flags & (INSN_LOAD | INSN_STORE))
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);

          switch (loc->insn->insn) {
          case I_ADDI:
            insn = I_ADD;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_ADDIU:
            insn = I_ADDU;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_ORI:
            insn = I_OR;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_XORI:
            insn = I_XOR;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_ANDI:
            insn = I_AND;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_LUI:
            op->type = OP_MOVE;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc) << 16, FALSE);
            break;
          case I_SLTI:
            insn = I_SLT;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_SLTIU:
            insn = I_SLTU;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_EXT:
            insn = I_EXT;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            append_value (sub, op->operands, VAL_CONSTANT, RD (loc->opc) + 1, FALSE);
            break;
          case I_INS:
            insn = I_INS;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            append_value (sub, op->operands, VAL_CONSTANT, RD (loc->opc) - SA (loc->opc) + 1, FALSE);
            break;
          case I_ROTR:
            insn = I_ROTV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            break;
          case I_SLL:
            insn = I_SLLV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            break;
          case I_SRA:
            insn = I_SRAV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            break;
          case I_SRL:
            insn = I_SRLV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            break;
          case I_BEQL:
            insn = I_BEQ;
            break;
          case I_BGEZL:
            insn = I_BGEZ;
            break;
          case I_BGTZL:
            insn = I_BGTZ;
            break;
          case I_BLEZL:
            insn = I_BLEZ;
            break;
          case I_BLTZL:
            insn = I_BLTZ;
            break;
          case I_BLTZALL:
            insn = I_BLTZAL;
            break;
          case I_BNEL:
            insn = I_BNE;
            break;
          default:
            insn = loc->insn->insn;
          }
          op->insn = insn;

          if (loc->insn->flags & INSN_READ_HI) {
            append_value (sub, op->operands, VAL_REGISTER, REGISTER_HI, TRUE);
          }

          if (loc->insn->flags & INSN_READ_LO) {
            append_value (sub, op->operands, VAL_REGISTER, REGISTER_LO, TRUE);
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            int regno = RD (loc->opc);
            append_value (sub, op->operands, VAL_REGISTER, regno, TRUE);
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            int regno = RT (loc->opc);
            append_value (sub, op->operands, VAL_REGISTER, regno, TRUE);
          }

          if (loc->insn->flags & INSN_READ_GPR_S) {
            int regno = RS (loc->opc);
            append_value (sub, op->operands, VAL_REGISTER, regno, TRUE);
          }

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            int regno = RT (loc->opc);
            GLOBAL_GPR_DEF ();
            append_value (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            int regno = RD (loc->opc);
            GLOBAL_GPR_DEF ();
            append_value (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            int regno = 0;
            GLOBAL_HILO_DEF ();
            append_value (sub, op->results, VAL_REGISTER, REGISTER_LO, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            int regno = 1;
            GLOBAL_HILO_DEF ();
            append_value (sub, op->results, VAL_REGISTER, REGISTER_HI, FALSE);
          }

          if (loc->insn->flags & INSN_LINK) {
            int regno = REGISTER_LINK;
            GLOBAL_GPR_DEF ();
            append_value (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          simplify_operation (sub, op);
          list_inserttail (block->operations, op);
        } else {
          if (!lastasm) {
            op = alloc_operation (sub, block);
            op->begin = op->end = loc;
            op->type = OP_ASM;
            hilo_used = hilo_defined = 0;
            gpr_used = gpr_defined = 0;
          } else {
            op->end = loc;
          }
          lastasm = TRUE;

          if (loc->insn->flags & INSN_READ_LO) {
            int regno = 0;
            HILO_USE ();
          }

          if (loc->insn->flags & INSN_READ_HI) {
            int regno = 1;
            HILO_USE ();
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            int regno = RD (loc->opc);
            GPR_USE ();
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            int regno = RT (loc->opc);
            GPR_USE ();
          }

          if (loc->insn->flags & INSN_READ_GPR_S) {
            int regno = RS (loc->opc);
            GPR_USE ();
          }

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            int regno = RT (loc->opc);
            GPR_DEF ();
            GLOBAL_GPR_DEF ();
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            int regno = RD (loc->opc);
            GPR_DEF ();
            GLOBAL_GPR_DEF ();
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            int regno = 0;
            HILO_DEF ();
            GLOBAL_HILO_DEF ();
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            int regno = 1;
            HILO_DEF ();
            GLOBAL_HILO_DEF ();
          }

          if (loc->insn->flags & INSN_LINK) {
            int regno = REGISTER_LINK;
            GPR_DEF ();
            GLOBAL_GPR_DEF ();
          }
        }

        if (loc == block->info.simple.end) {
          if (lastasm)
            list_inserttail (block->operations, op);
          break;
        }
      }
    } else if (block->type == BLOCK_CALL) {
      op = alloc_operation (sub, block);
      op->type = OP_CALL;
      list_inserttail (block->operations, op);
    }

    el = element_next (el);
  }
}


static
void ssa_place_phis (struct subroutine *sub, list *wheredefined)
{
  struct basicblock *block, *bref;
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
    list worklist = wheredefined[i];
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
        bref = element_getvalue (ref);
        if (bref->mark2 != i) {
          struct operation *op;

          bref->mark2 = i;
          op = alloc_operation (sub, bref);
          op->type = OP_PHI;
          append_value (sub, op->results, VAL_REGISTER, i, FALSE);
          for (j = 0; j < list_size (bref->inrefs); j++)
            append_value (sub, op->operands, VAL_REGISTER, i, FALSE);
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
  int i, pushcount[NUM_REGISTERS];

  for (i = 1; i < NUM_REGISTERS; i++)
    pushcount[i] = 0;

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
        list_inserthead (vars[val->val.intval], var);
        list_inserttail (block->sub->variables, var);
        pushcount[val->val.intval]++;
        val->val.variable = var;
      }
      rel = element_next (rel);
    }

    el = element_next (el);
  }

  el = list_head (block->outrefs);
  while (el) {
    struct basicblock *ref = element_getvalue (el);
    ref->mark1 = 0;
    el = element_next (el);
  }

  el = list_head (block->outrefs);
  while (el) {
    struct basicblock *ref = element_getvalue (el);
    element phiel, refs, opel;
    int j;

    ref->mark1++;
    i = j = 0;
    refs = list_head (ref->inrefs);
    while (refs) {
      if (element_getvalue (refs) == block) {
        if (++j == ref->mark1) {
          break;
        }
      }
      i++;
      refs = element_next (refs);
    }

    phiel = list_head (ref->operations);
    while (phiel) {
      struct operation *op;
      struct value *val;

      op = element_getvalue (phiel);
      if (op->type != OP_PHI) break;

      opel = list_head (op->operands);
      for (j = 0; j < i; j++) {
        opel = element_next (opel);
      }
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
    struct basicblock *child = element_getvalue (el);
    ssa_search (child, vars);
    el = element_next (el);
  }

  for (i = 1; i < NUM_REGISTERS; i++) {
    while (pushcount[i]--) {
      list_removehead (vars[i]);
    }
  }

}

static
void ssa_create_vars (struct subroutine *sub, list *vars)
{
  struct operation *op;
  int i;

  op = alloc_operation (sub, sub->startblock);
  op->type = OP_START;

  for (i = 1; i < NUM_REGISTERS; i++) {
    append_value (sub, op->results, VAL_REGISTER, i, FALSE);
  }

  list_inserttail (sub->startblock->operations, op);

  op = alloc_operation (sub, sub->endblock);
  op->type = OP_END;
  for (i = 1; i < NUM_REGISTERS; i++) {
    append_value (sub, op->operands, VAL_REGISTER, i, FALSE);
  }

  list_inserttail (sub->endblock->operations, op);

  ssa_search (sub->startblock, vars);
}

void build_ssa (struct subroutine *sub)
{
  list reglist[NUM_REGISTERS];
  int i;

  for (i = 0; i < NUM_REGISTERS; i++) {
    reglist[i] = list_alloc (sub->code->lstpool);
  }

  sub->variables = list_alloc (sub->code->lstpool);

  extract_operations (sub, reglist);
  ssa_place_phis (sub, reglist);
  ssa_create_vars (sub, reglist);

  for (i = 0; i < NUM_REGISTERS; i++) {
    list_free (reglist[i]);
  }
}


