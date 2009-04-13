
#include <string.h>

#include "analyser.h"
#include "utils.h"

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define FT(op) ((op >> 16) & 0x1F)
#define FS(op) ((op >> 11) & 0x1F)
#define FD(op) ((op >> 6) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))

static
int decode_instructions (struct code *c, const uint8 *code, uint32 size, uint32 address)
{
  struct location *base;
  uint32 i, numopc = size >> 2;
  int slot = FALSE;

  if (size & 0x03 || address & 0x03) {
    error (__FILE__ ": size/address is not multiple of 4");
    return 0;
  }

  base = (struct location *) xmalloc ((numopc) * sizeof (struct location));
  memset (base, 0, (numopc) * sizeof (struct location));

  c->base = base;
  c->end = &base[numopc];
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
      error (__FILE__ ": invalid opcode 0x%08X at 0x%08X", loc->opc, loc->address);
      continue;
    }

    if (loc->insn->flags & (INSN_BRANCH | INSN_JUMP)) {
      if (slot) error (__FILE__ ": branch/jump inside delay slot at 0x%08X", loc->address);
      slot = TRUE;
    } else {
      slot = FALSE;
    }

    if (loc->insn->flags & INSN_BRANCH) {
      int normal = FALSE;

      if (loc->insn->flags & INSN_LINK)
        report (__FILE__ ": branch and link at 0x%08X\n", loc->address);

      tgt = loc->opc & 0xFFFF;
      if (tgt & 0x8000) { tgt |= ~0xFFFF;  }
      tgt += i + 1;
      if (tgt < numopc) {
        loc->target = &base[tgt];
      } else {
        error (__FILE__ ": branch outside file\n%s", allegrex_disassemble (loc->opc, loc->address, TRUE));
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

      loc->branchtype = BRANCH_NORMAL;
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
        error (__FILE__ ": jump outside file\n%s", allegrex_disassemble (loc->opc, loc->address, TRUE));
      }
    }

    if (loc->target) {
      if (!loc->target->references)
        loc->target->references = list_alloc (c->lstpool);
      list_inserttail (loc->target->references, &base[i]);
    }
  }
  if (slot) {
    error (__FILE__ ": delay slot at the end of file");
  }

  return 1;
}

static
void new_subroutine (struct code *c, struct location *loc, struct prx_function *imp, struct prx_function *exp)
{
  struct subroutine *sub = loc->sub;
  if (!sub) {
    sub = fixedpool_alloc (c->subspool);
    memset (sub, 0, sizeof (struct subroutine));
    sub->location = loc;
    loc->sub = sub;
  }
  if (imp) sub->import = imp;
  if (exp) sub->export = exp;

  if (sub->import && sub->export) {
    error (__FILE__ ": location 0x%08X is both import and export", loc->address);
  }
}


static
int analyse_relocs (struct code *c)
{
  uint32 i, j, tgt;

  c->switches = list_alloc (c->lstpool);
  i = prx_findreloc (c->file, c->baddr);
  for(;i < c->file->relocnum; i++) {
    struct location *loc;
    struct prx_reloc *rel = &c->file->relocs[i];

    tgt = (rel->target - c->baddr) >> 2;
    if (tgt >= c->numopc) continue;
    if (rel->target & 0x03) {
      error (__FILE__ ": .text reloc not word aligned 0x%08X", rel->target);
      continue;
    }

    loc = &c->base[tgt];

    if (rel->type == R_MIPSX_JAL26) {
      new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_26) {
      struct location *calledfrom;
      uint32 ctgt;

      if (rel->vaddr < c->baddr) continue;
      ctgt = (rel->vaddr - c->baddr) >> 2;
      if (ctgt >= c->numopc) continue;

      calledfrom = &c->base[ctgt];
      if (calledfrom->insn->insn == I_JAL) {
        new_subroutine (c, loc, NULL, NULL);
      }
    } else if (rel->type == R_MIPS_32) {
      struct prx_reloc *frel;
      uint32 end, count = 0;

      j = prx_findrelocbyaddr (c->file, rel->vaddr);
      while (j < c->file->relocnum) {
        if (c->file->relocsbyaddr[j].vaddr != (rel->vaddr + (count << 2)))
          break;
        count++; j++;
      }

      j = end = prx_findreloc (c->file, rel->vaddr);

      if (c->file->relocs[j].target == rel->vaddr) {
        for (;end < c->file->relocnum; end++) {
          frel = &c->file->relocs[end];
          if (frel->target != rel->vaddr) {
            if (count > ((frel->target - rel->vaddr) >> 2))
              count = (frel->target - rel->vaddr) >> 2;
            break;
          }
        }

        if (count > 1) {
          for (;j < end; j++) {
            frel = &c->file->relocs[j];
            if (frel->type == R_MIPS_LO16) {
              struct codeswitch *cs;
              cs = fixedpool_alloc (c->switchpool);
              memset (cs, 0, sizeof (struct codeswitch));

              cs->basereloc = frel;
              cs->count = count;
              report ("switch at 0x%08X to 0x%08X count = %d\n", rel->vaddr, rel->target, count);
              list_inserttail (c->switches, cs);
            }
          }
        }
      }
    }
  }
  return 1;
}

static
void mark_reachable (struct code *c, struct location *loc)
{
  do {
    loc->reachable = TRUE;
    if (!(loc->insn)) continue;
    if (loc->insn->flags & (INSN_JUMP | INSN_BRANCH)) {

    }
  } while (loc++ != c->end);
}


static
int analyse_subroutine (struct code *c, struct subroutine *sub)
{
  struct location *loc;
  loc = sub->location;
  do {
    if (!loc->insn) {
      error (__FILE__ ": subroutine 0x%08X with unknown allegrex opcode 0x%08X", sub->location->address, loc->opc);
      sub->haserror = 1;
      return 1;
    }

    if (loc->target) {
      if (loc->target->sub != loc->sub) {
        if (loc->target->sub->location != loc->target) {
          error (__FILE__ ": jump (0x%08X) outside subroutine 0x%08X at 0x%08X",
              loc->target->address, sub->location->address, loc->address);
          sub->haserror = 1;
          loc->target->sub->haserror = 1;
          return 1;
        }
        if (!(loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D))) {
          report (__FILE__ ": jumped call at 0x%08X\n", loc->address);
        }
      }
    }
  } while (loc++ != sub->end);
  return 1;
}


static
int analyse_subroutines (struct code *c)
{
  uint32 i, j, tgt;
  struct subroutine *prevsub = NULL;
  element el;

  c->switches = list_alloc (c->lstpool);
  c->subroutines = list_alloc (c->lstpool);

  for (i = 0; i < c->file->modinfo->numexports; i++) {
    struct prx_export *exp;

    exp = &c->file->modinfo->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct location *loc;

      tgt = (exp->funcs[j].vaddr - c->baddr) >> 2;
      if (exp->funcs[j].vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid exported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, NULL, &exp->funcs[j]);
    }
  }

  for (i = 0; i < c->file->modinfo->numimports; i++) {
    struct prx_import *imp;

    imp = &c->file->modinfo->imports[i];
    for (j = 0; j < imp->nfuncs; j++) {
      struct location *loc;

      tgt = (imp->funcs[j].vaddr - c->baddr) >> 2;
      if (imp->funcs[j].vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid imported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, &imp->funcs[j], NULL);
    }
  }

  if (!c->base->sub) {
    error (__FILE__ ": creating artificial subroutine at address 0x%08X", c->baddr);
    new_subroutine (c, c->base, NULL, NULL);
  }

  for (i = 0; i < c->numopc; i++) {
    if (c->base[i].sub) {
      list_inserttail (c->subroutines, c->base[i].sub);
      if (prevsub) {
        prevsub->end = &c->base[i - 1];
      }
      prevsub = c->base[i].sub;
    } else {
      c->base[i].sub = prevsub;
    }
  }
  if (prevsub) {
    prevsub->end = &c->base[i - 1];
  }

  el = list_head (c->subroutines);
  while (el) {
    if (!analyse_subroutine (c, element_getvalue (el)))
      return 0;

    el = element_next (el);
  }

  return 1;
}


struct code* analyse_code (struct prx *p)
{
  struct code *c = code_alloc ();
  c->file = p;
  if (!decode_instructions (c, p->programs->data,
       p->modinfo->expvaddr - 4, p->programs->vaddr)) {
    code_free (c);
    return NULL;
  }

  if (!analyse_relocs (c)) {
    code_free (c);
    return NULL;
  }

  if (!analyse_subroutines (c)) {
    code_free (c);
    return NULL;
  }

  return c;
}
