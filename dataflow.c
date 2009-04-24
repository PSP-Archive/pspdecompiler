
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
struct operation *alloc_op (struct subroutine *sub)
{
  struct operation *op;
  op = fixedpool_alloc (sub->code->opspool);
  op->operands = list_alloc (sub->code->lstpool);
  op->results = list_alloc (sub->code->lstpool);
  return op;
}

static
struct value *alloc_val (struct subroutine *sub)
{
  struct value *val;
  val = fixedpool_alloc (sub->code->valspool);
  return val;
}

static
void simplify_operation (struct operation *op)
{
}

static
void extract_operations (struct subroutine *sub)
{
  struct operation *op;
  struct value *val;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    struct basicblock *block = element_getvalue (el);
    block->operations = list_alloc (sub->code->lstpool);

    if (block->type == BLOCK_SIMPLE) {
      uint32 gpr_used, gpr_defined;
      uint32 hilo_used, hilo_defined;
      struct location *loc;
      int lastasm = FALSE;

      for (loc = block->val.simple.begin; ; loc++) {
        if (0 /*&& INSN_TYPE (loc->insn->flags) == INSN_ALLEGREX*/) {
          enum allegrex_insn insn;

          if (lastasm)
            list_inserttail (block->operations, op);
          lastasm = FALSE;

          op = alloc_op (sub);
          op->type = OP_INSTRUCTION;
          op->begin = op->end = loc;

          switch (loc->insn->insn) {
          case I_ADDI:
            insn = I_ADD;
            val->type = VAL_CONSTANT;
            val->value = IMM (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_ADDIU:
            insn = I_ADDU;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMM (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_ORI:
            insn = I_OR;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMMU (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_XORI:
            insn = I_XOR;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMMU (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_ANDI:
            insn = I_AND;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMMU (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_LUI:
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMMU (loc->opc) << 16;
            list_inserttail (op->operands, val);
            break;
          case I_SLTI:
            insn = I_SLT;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMM (loc->opc) << 16;
            list_inserttail (op->operands, val);
            break;
          case I_SLTIU:
            insn = I_SLTU;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = IMM (loc->opc) << 16;
            list_inserttail (op->operands, val);
            break;
          case I_EXT:
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = RD (loc->opc) + 1;
            list_inserttail (op->operands, val);
            break;
          case I_INS:
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = RD (loc->opc) - SA (loc->opc) + 1;
            list_inserttail (op->operands, val);
            break;
          case I_ROTR:
            insn = I_ROTV;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_SLL:
            insn = I_SLLV;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_SRA:
            insn = I_SRAV;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
            break;
          case I_SRL:
            insn = I_SRLV;
            val = alloc_val (sub);
            val->type = VAL_CONSTANT;
            val->value = SA (loc->opc);
            list_inserttail (op->operands, val);
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

          if (loc->insn->flags & INSN_READ_HI) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_HI;
            list_inserthead (op->operands, val);
          }

          if (loc->insn->flags & INSN_READ_LO) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_LO;
            list_inserthead (op->operands, val);
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RD (loc->opc);
            list_inserthead (op->operands, val);
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RT (loc->opc);
            list_inserthead (op->operands, val);
          }

          if (loc->insn->flags & INSN_READ_GPR_S) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RS (loc->opc);
            list_inserthead (op->operands, val);
          }



          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RT (loc->opc);
            list_inserttail (op->results, val);
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RD (loc->opc);
            list_inserttail (op->results, val);
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_LO;
            list_inserttail (op->results, val);
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_HI;
            list_inserttail (op->results, val);
          }

          if (loc->insn->flags & INSN_LINK) {
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = 31;
            list_inserttail (op->results, val);
          }

          simplify_operation (op);
          list_inserttail (block->operations, op);
        } else {
          if (!lastasm) {
            op = alloc_op (sub);
            op->begin = op->end = loc;
            op->type = OP_ASM;
            hilo_used = hilo_defined = 0;
            gpr_used = gpr_defined = 0;
          } else {
            op->end = loc;
          }
          lastasm = TRUE;

          if ((loc->insn->flags & INSN_READ_HI) &&
              !IS_REG_USED (hilo_used, 1) &&
              !IS_REG_USED (hilo_defined, 1)) {
            REG_USE (hilo_used, 1);
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_HI;
            list_inserttail (op->operands, val);
          }

          if ((loc->insn->flags & INSN_READ_LO) &&
              !IS_REG_USED (hilo_used, 0) &&
              !IS_REG_USED (hilo_defined, 0)) {
            REG_USE (hilo_used, 0);
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_LO;
            list_inserttail (op->operands, val);
          }

          if ((loc->insn->flags & INSN_READ_GPR_D) &&
              !IS_REG_USED (gpr_used, RD (loc->opc)) &&
              !IS_REG_USED (gpr_defined, RD (loc->opc)) &&
              RD (loc->opc) != 0) {
            REG_USE (gpr_used, RD (loc->opc));
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RD (loc->opc);
            list_inserttail (op->operands, val);
          }

          if ((loc->insn->flags & INSN_READ_GPR_T) &&
              !IS_REG_USED (gpr_used, RT (loc->opc)) &&
              !IS_REG_USED (gpr_defined, RT (loc->opc)) &&
              RT (loc->opc) != 0) {
            REG_USE (gpr_used, RT (loc->opc));
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RT (loc->opc);
            list_inserttail (op->operands, val);
          }

          if ((loc->insn->flags & INSN_READ_GPR_S) &&
              !IS_REG_USED (gpr_used, RS (loc->opc)) &&
              !IS_REG_USED (gpr_defined, RS (loc->opc)) &&
              RS (loc->opc) != 0) {
            REG_USE (gpr_used, RS (loc->opc));
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RS (loc->opc);
            list_inserttail (op->operands, val);
          }

          if ((loc->insn->flags & INSN_WRITE_GPR_T) &&
              !IS_REG_USED (gpr_defined, RT (loc->opc)) &&
              RT (loc->opc) != 0) {
            REG_USE (gpr_defined, RT (loc->opc));
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RT (loc->opc);
            list_inserttail (op->results, val);
          }

          if ((loc->insn->flags & INSN_WRITE_GPR_D) &&
              !IS_REG_USED (gpr_defined, RD (loc->opc)) &&
              RD (loc->opc) != 0) {
            REG_USE (gpr_defined, RD (loc->opc));
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = RD (loc->opc);
            list_inserttail (op->results, val);
          }

          if ((loc->insn->flags & INSN_WRITE_LO) &&
              !IS_REG_USED (hilo_defined, 0)) {
            REG_USE (hilo_defined, 0);
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_LO;
            list_inserttail (op->results, val);
          }

          if ((loc->insn->flags & INSN_WRITE_HI) &&
              !IS_REG_USED (hilo_defined, 1)) {
            REG_USE (hilo_defined, 1);
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = REGISTER_HI;
            list_inserttail (op->results, val);
          }

          if ((loc->insn->flags & INSN_LINK) &&
              !IS_REG_USED (gpr_defined, 31)) {
            REG_USE (gpr_defined, 31);
            val = alloc_val (sub);
            val->type = VAL_REGISTER;
            val->value = 31;
            list_inserttail (op->results, val);
          }
        }

        if (loc == block->val.simple.end) {
          if (lastasm)
            list_inserttail (block->operations, op);
          break;
        }
      }
    } else if (block->type == BLOCK_CALL) {
      op = alloc_op (sub);
      op->type = OP_CALL;
      list_inserttail (block->operations, op);
    }

    el = element_next (el);
  }
}

void build_ssa (struct subroutine *sub)
{
  extract_operations (sub);
}
