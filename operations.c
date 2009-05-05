
#include "code.h"
#include "utils.h"

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))

#define END_REGMASK     0xFCFF000C
#define CALLIN_REGMASK  0x00000FF0
#define CALLOUT_REGMASK 0x0300FFFE

#define IS_BIT_SET(flags, bit) ((1 << ((bit) & 31)) & ((flags)[(bit) >> 5]))
#define BIT_SET(flags, bit) ((flags)[(bit) >> 5]) |= 1 << ((bit) & 31)

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
struct operation *alloc_operation (struct basicblock *block)
{
  struct operation *op;
  struct code *c = op->block->sub->code;

  op = fixedpool_alloc (c->opspool);
  op->block = block;
  op->operands = list_alloc (c->lstpool);
  op->results = list_alloc (c->lstpool);
  return op;
}

static
void reset_operation (struct operation *op)
{
  struct value *val;
  struct code *c = op->block->sub->code;

  while (list_size (op->results)) {
    val = list_removehead (op->results);
    fixedpool_free (c->valspool, val);
  }

  while (list_size (op->operands)) {
    val = list_removehead (op->operands);
    fixedpool_free (c->valspool, val);
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
void simplify_operation (struct operation *op)
{
  struct value *val;
  struct code *c = op->block->sub->code;

  if (op->type != OP_INSTRUCTION) return;

  if (list_size (op->results) == 1 && !(op->begin->insn->flags & (INSN_LOAD | INSN_JUMP))) {
    val = list_headvalue (op->results);
    if (val->val.intval == 0) {
      reset_operation (op);
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
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removetail (op->operands);
        fixedpool_free (c->valspool, val);
        op->type = OP_MOVE;
      }
    }
    break;
  case I_AND:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removehead (op->operands);
        fixedpool_free (c->valspool, val);
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
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    }
  case I_MOVN:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      reset_operation (op);
      op->type = OP_NOP;
    }
    break;
  case I_MOVZ:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
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

          op = alloc_operation (block);
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

          simplify_operation (op);
          list_inserttail (block->operations, op);
        } else {
          if (!lastasm) {
            op = alloc_operation (block);
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
      op = alloc_operation (block);
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
