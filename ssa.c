
#include "code.h"
#include "utils.h"

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))

#define IS_BIT_SET(flags, bit) ((1 << ((bit) & 31)) & ((flags)[(bit) >> 5]))
#define BIT_SET(flags, bit) ((flags)[(bit) >> 5]) |= 1 << ((bit) & 31)

#define END_REGMASK     0xFCFF000C
#define CALLIN_REGMASK  0x00000FF0
#define CALLOUT_REGMASK 0x0300FFFE

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
void append_value (struct subroutine *sub, list l, enum valuetype type, uint32 value)
{
  struct value *val;
  val = fixedpool_alloc (sub->code->valspool);
  val->type = type;
  val->val.intval = value;
  list_inserttail (l, val);
}

#define BLOCK_GPR_KILL() \
  if (!IS_BIT_SET (block->reg_kill, regno) && regno != 0) { \
    list_inserttail (defblocks[regno], block);              \
    BIT_SET (block->reg_kill, regno);                       \
  }

#define BLOCK_GPR_GEN() \
  if (!IS_BIT_SET (block->reg_kill, regno) && regno != 0 && \
      !IS_BIT_SET (block->reg_gen, regno)) {                \
    BIT_SET (block->reg_gen, regno);                        \
  }

#define ASM_GPR_KILL() \
  BLOCK_GPR_KILL ()                                       \
  if (!IS_BIT_SET (asm_kill, regno) && regno != 0) {      \
    BIT_SET (asm_kill, regno);                            \
    append_value (sub, op->results, VAL_REGISTER, regno); \
  }

#define ASM_GPR_GEN() \
  BLOCK_GPR_GEN ()                                         \
  if (!IS_BIT_SET (asm_kill, regno) && regno != 0 &&       \
      !IS_BIT_SET (asm_gen, regno)) {                      \
    BIT_SET (asm_gen, regno);                              \
    append_value (sub, op->operands, VAL_REGISTER, regno); \
  }

static
void extract_operations (struct subroutine *sub, list *defblocks)
{
  struct operation *op;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    block->operations = list_alloc (sub->code->lstpool);
    block->reg_gen[0] = block->reg_gen[1] = 0;
    block->reg_kill[0] = block->reg_kill[1] = 0;

    if (block->type == BLOCK_SIMPLE) {
      uint32 asm_gen[2], asm_kill[2];
      struct location *loc;
      int lastasm = FALSE;

      asm_gen[0] = asm_gen[1] = 0;
      asm_kill[0] = asm_kill[1] = 0;

      for (loc = block->info.simple.begin; ; loc++) {
        if (INSN_TYPE (loc->insn->flags) == INSN_ALLEGREX) {
          enum allegrex_insn insn;

          if (lastasm) list_inserttail (block->operations, op);
          lastasm = FALSE;

          op = alloc_operation (sub, block);
          op->type = OP_INSTRUCTION;
          op->begin = op->end = loc;

          if (loc->insn->flags & INSN_READ_GPR_S) {
            int regno = RS (loc->opc);
            BLOCK_GPR_GEN ()
            append_value (sub, op->operands, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            int regno = RT (loc->opc);
            BLOCK_GPR_GEN ()
            append_value (sub, op->operands, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            int regno = RD (loc->opc);
            BLOCK_GPR_GEN ()
            append_value (sub, op->operands, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_READ_LO) {
            int regno = REGISTER_LO;
            BLOCK_GPR_GEN ()
            append_value (sub, op->operands, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_READ_HI) {
            int regno = REGISTER_HI;
            BLOCK_GPR_GEN ()
            append_value (sub, op->operands, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & (INSN_LOAD | INSN_STORE)) {
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc));
          }

          switch (loc->insn->insn) {
          case I_ADDI:
            insn = I_ADD;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc));
            break;
          case I_ADDIU:
            insn = I_ADDU;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc));
            break;
          case I_ORI:
            insn = I_OR;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc));
            break;
          case I_XORI:
            insn = I_XOR;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc));
            break;
          case I_ANDI:
            insn = I_AND;
            append_value (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc));
            break;
          case I_LUI:
            op->type = OP_MOVE;
            append_value (sub, op->operands, VAL_CONSTANT, ((unsigned int) IMMU (loc->opc)) << 16);
            break;
          case I_SLTI:
            insn = I_SLT;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc));
            break;
          case I_SLTIU:
            insn = I_SLTU;
            append_value (sub, op->operands, VAL_CONSTANT, IMM (loc->opc));
            break;
          case I_EXT:
            insn = I_EXT;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
            append_value (sub, op->operands, VAL_CONSTANT, RD (loc->opc) + 1);
            break;
          case I_INS:
            insn = I_INS;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
            append_value (sub, op->operands, VAL_CONSTANT, RD (loc->opc) - SA (loc->opc) + 1);
            break;
          case I_ROTR:
            insn = I_ROTV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
            break;
          case I_SLL:
            insn = I_SLLV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
            break;
          case I_SRA:
            insn = I_SRAV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
            break;
          case I_SRL:
            insn = I_SRLV;
            append_value (sub, op->operands, VAL_CONSTANT, SA (loc->opc));
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

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            int regno = RT (loc->opc);
            BLOCK_GPR_KILL ()
            append_value (sub, op->results, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            int regno = RD (loc->opc);
            BLOCK_GPR_KILL ()
            append_value (sub, op->results, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            int regno = REGISTER_LO;
            BLOCK_GPR_KILL ()
            append_value (sub, op->results, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            int regno = REGISTER_HI;
            BLOCK_GPR_KILL ()
            append_value (sub, op->results, VAL_REGISTER, regno);
          }

          if (loc->insn->flags & INSN_LINK) {
            int regno = REGISTER_LINK;
            BLOCK_GPR_KILL ()
            append_value (sub, op->results, VAL_REGISTER, regno);
          }

          simplify_operation (sub, op);
          list_inserttail (block->operations, op);
        } else {
          if (!lastasm) {
            op = alloc_operation (sub, block);
            op->begin = op->end = loc;
            op->type = OP_ASM;
            asm_gen[0] = asm_gen[1] = 0;
            asm_kill[0] = asm_kill[1] = 0;
          } else {
            op->end = loc;
          }
          lastasm = TRUE;

          if (loc->insn->flags & INSN_READ_LO) {
            int regno = REGISTER_LO;
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_HI) {
            int regno = REGISTER_HI;
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            int regno = RD (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            int regno = RT (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_S) {
            int regno = RS (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            int regno = RT (loc->opc);
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            int regno = RD (loc->opc);
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            int regno = REGISTER_LO;
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            int regno = REGISTER_HI;
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_LINK) {
            int regno = REGISTER_LINK;
            ASM_GPR_KILL ()
          }
        }

        if (loc == block->info.simple.end) {
          if (lastasm) list_inserttail (block->operations, op);
          break;
        }
      }
    } else if (block->type == BLOCK_CALL) {
      int regno;
      op = alloc_operation (sub, block);
      op->type = OP_CALL;
      list_inserttail (block->operations, op);

      for (regno = 1; regno <= REGISTER_LINK; regno++) {
        if ((1 << regno) & CALLIN_REGMASK) {
          BLOCK_GPR_GEN ()
          append_value (sub, op->operands, VAL_REGISTER, regno);
        }
        if ((1 << regno) & CALLOUT_REGMASK) {
          BLOCK_GPR_KILL ()
          append_value (sub, op->results, VAL_REGISTER, regno);
        }
      }

      regno = REGISTER_LO;
      BLOCK_GPR_KILL ()
      append_value (sub, op->results, VAL_REGISTER, regno);

      regno = REGISTER_HI;
      BLOCK_GPR_KILL ()
      append_value (sub, op->results, VAL_REGISTER, regno);
    }

    el = element_next (el);
  }
}

static
void live_registers (struct subroutine *sub)
{
  list worklist = list_alloc (sub->code->lstpool);
  struct basicblock *block;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    block->mark1 = 0;
    el = element_next (el);
  }

  list_inserthead (worklist, sub->endblock);

  while (list_size (worklist) != 0) {
    struct basicedge *edge;
    struct basicblock *bref;

    block = list_removehead (worklist);
    block->mark1 = 0;
    block->reg_live_out[0] = (block->reg_live_in[0] & ~(block->reg_kill[0])) | block->reg_gen[0];
    block->reg_live_out[1] = (block->reg_live_in[1] & ~(block->reg_kill[1])) | block->reg_gen[1];
    el = list_head (block->inrefs);
    while (el) {
      uint32 changed;

      edge = element_getvalue (el);
      bref = edge->from;

      changed = block->reg_live_out[0] & (~bref->reg_live_in[0]);
      bref->reg_live_in[0] |= block->reg_live_out[0];
      changed |= block->reg_live_out[1] & (~bref->reg_live_in[1]);
      bref->reg_live_in[1] |= block->reg_live_out[1];

      if (changed && !bref->mark1) {
        list_inserttail (worklist, bref);
        bref->mark1 = 1;
      }

      el = element_next (el);
    }
  }

  list_free (worklist);
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
          op = alloc_operation (sub, bref);
          op->type = OP_PHI;
          append_value (sub, op->results, VAL_REGISTER, i);
          for (j = list_size (bref->inrefs); j > 0; j--)
            append_value (sub, op->operands, VAL_REGISTER, i);
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
  struct operation *op;
  int i;

  reglist[0] = NULL;
  for (i = 1; i < NUM_REGISTERS; i++) {
    reglist[i] = list_alloc (sub->code->lstpool);
  }

  sub->variables = list_alloc (sub->code->lstpool);

  extract_operations (sub, reglist);

  op = alloc_operation (sub, sub->startblock);
  op->type = OP_START;

  for (i = 1; i < NUM_REGISTERS; i++) {
    BIT_SET (sub->startblock->reg_kill, i);
    append_value (sub, op->results, VAL_REGISTER, i);
  }

  list_inserttail (sub->startblock->operations, op);

  op = alloc_operation (sub, sub->endblock);
  op->type = OP_END;

  for (i = 1; i <= REGISTER_LINK; i++) {
    if (END_REGMASK & (1 << i)) {
      BIT_SET (sub->endblock->reg_gen, i);
      append_value (sub, op->operands, VAL_REGISTER, i);
    }
  }

  list_inserttail (sub->endblock->operations, op);

  live_registers (sub);
  ssa_place_phis (sub, reglist);
  ssa_search (sub->startblock, reglist);

  for (i = 1; i < NUM_REGISTERS; i++) {
    list_free (reglist[i]);
  }
}


