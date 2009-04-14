
#include <string.h>

#include "code.h"
#include "utils.h"

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)

int decode_instructions (struct code *c)
{
  struct location *base;
  uint32 i, numopc, size, address;
  const uint8 *code;
  int slot = FALSE;

  address = c->file->programs->vaddr;
  size = c->file->modinfo->expvaddr - 4;
  code = c->file->programs->data;

  if ((size & 0x03) || (address & 0x03)) {
    error (__FILE__ ": size/address is not multiple of 4");
    return 0;
  }

  base = (struct location *) xmalloc ((numopc) * sizeof (struct location));
  memset (base, 0, (numopc) * sizeof (struct location));

  c->base = base;
  c->end = &base[numopc - 1];
  c->baddr = address;
  c->numopc = numopc;

  for (i = 0; i < numopc; i++) {
    struct location *loc = &base[i];
    uint32 tgt;


    loc->opc = code[i << 2];
    loc->opc |= code[(i << 2) + 1] << 8;
    loc->opc |= code[(i << 2) + 2] << 16;
    loc->opc |= code[(i << 2) + 3] << 24;
    loc->insn = allegrex_decode (loc->opc, FALSE);
    loc->address = address + (i << 2);

    if (loc->insn == NULL) {
      loc->error = ERROR_INVALID_OPCODE;
      continue;
    }

    if (loc->insn->flags & INSN_LINK) {
      loc->gpr_defined |= 1 << (31 - 1);
    }

    if (loc->insn->flags & INSN_WRITE_GPR_D) {
      if (RD (loc->opc) != 0)
        loc->gpr_defined |= 1 << (RD (loc->opc) - 1);
    }

    if (loc->insn->flags & INSN_WRITE_GPR_T) {
      if (RT (loc->opc) != 0)
        loc->gpr_defined |= 1 << (RT (loc->opc) - 1);
    }

    if (loc->insn->flags & INSN_READ_GPR_S) {
      if (RS (loc->opc) != 0)
        loc->gpr_used |= 1 << (RS (loc->opc) - 1);
    }

    if (loc->insn->flags & INSN_READ_GPR_T) {
      if (RT (loc->opc) != 0)
        loc->gpr_used |= 1 << (RT (loc->opc) - 1);
    }

    if (loc->insn->flags & (INSN_BRANCH | INSN_JUMP)) {
      if (slot) c->base[i - 1].error = ERROR_DELAY_SLOT;
      slot = TRUE;
    } else {
      slot = FALSE;
    }

    loc->branchtype = BRANCH_NORMAL;
    if (loc->insn->flags & INSN_BRANCH) {
      int normal = FALSE;

      if (loc->insn->flags & INSN_LINK)
        report (__FILE__ ": branch and link at 0x%08X\n%s\n", loc->address,
            allegrex_disassemble (loc->opc, loc->address, TRUE));

      tgt = loc->opc & 0xFFFF;
      if (tgt & 0x8000) { tgt |= ~0xFFFF;  }
      tgt += i + 1;
      if (tgt < numopc) {
        loc->target = &base[tgt];
      } else {
        loc->error = ERROR_TARGET_OUTSIDE_FILE;
      }

      if (loc->insn->flags & INSN_READ_GPR_S) {
        if (RS (loc->opc) != 0) normal = TRUE;
      }

      if (loc->insn->flags & INSN_READ_GPR_T) {
        if (RT (loc->opc) != 0) normal = TRUE;
      }

      if (INSN_PROCESSOR (loc->insn->flags) != INSN_ALLEGREX) {
        normal = TRUE;
      }

      if (!normal) {
        switch (loc->insn->insn) {
        case I_BEQ:
        case I_BEQL:
        case I_BGEZ:
        case I_BGEZAL:
        case I_BGEZL:
        case I_BLEZ:
        case I_BLEZL:
          loc->branchtype = BRANCH_ALWAYS;
          break;
        case I_BGTZ:
        case I_BGTZL:
        case I_BLTZ:
        case I_BLTZAL:
        case I_BLTZALL:
        case I_BLTZL:
        case I_BNE:
        case I_BNEL:
          report (__FILE__ ": branch never taken at 0x%08X\n", loc->address);
          loc->branchtype = BRANCH_NEVER;
          break;
        default:
          loc->branchtype = BRANCH_NORMAL;
        }
      }

    } else if ((loc->insn->flags & (INSN_JUMP | INSN_READ_GPR_S)) == INSN_JUMP) {
      uint32 target_addr = (loc->opc & 0x3FFFFFF) << 2;;
      target_addr |= ((loc->address) & 0xF0000000);
      tgt = (target_addr - address) >> 2;
      if (tgt < numopc) {
        loc->target = &base[tgt];
      } else {
        loc->error = ERROR_TARGET_OUTSIDE_FILE;
      }
    }
  }

  if (slot) {
    c->base[i - 1].error = ERROR_TARGET_OUTSIDE_FILE;
  }

  return 1;
}
