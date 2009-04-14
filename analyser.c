
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

    if (loc->target && (loc->branchtype != BRANCH_NEVER)) {
      if (!loc->target->references)
        loc->target->references = list_alloc (c->lstpool);
      list_inserttail (loc->target->references, loc);
    }
  }

  if (slot) {
    c->base[i - 1].error = ERROR_TARGET_OUTSIDE_FILE;
  }

  return 1;
}

static
int analyse_switch (struct code *c, struct codeswitch *cs)
{
  struct location *loc = cs->location;
  element el;
  uint32 gpr;

  if (!loc->insn) return 0;

  if (loc->insn->insn == I_LW) {
    gpr = loc->gpr_defined;
    while (1) {
      if (loc++ == c->end) return 0;
      if (!loc->insn) return 0;
      if (loc->gpr_used & gpr) {
        if (loc->insn->insn != I_JR)
          return 0;
        break;
      }
      if (loc->insn->flags & (INSN_JUMP | INSN_BRANCH)) return 0;
    }
  } else if (loc->insn->insn == I_ADDIU) {
    int count = 0;
    gpr = loc->gpr_defined;
    while (1) {
      if (loc++ == c->end) return 0;
      if (!loc->insn) return 0;
      if (loc->gpr_used & gpr) {
        if (count == 0) {
          if (loc->insn->insn != I_ADDU) return 0;
        } else if (count == 1) {
          if (loc->insn->insn != I_LW) return 0;
        } else {
          if (loc->insn->insn != I_JR)
            return 0;
          break;
        }
        count++;
        gpr = loc->gpr_defined;
      }
      if (loc->insn->flags & (INSN_JUMP | INSN_BRANCH)) return 0;
    }
  } else return 0;

  cs->jumplocation = loc;
  cs->jumplocation->cswitch = cs;

  el = list_head (cs->references);
  while (el) {
    struct location *target = element_getvalue (el);

    target->switchtarget = TRUE;
    if (!target->references)
      target->references = list_alloc (c->lstpool);
    list_inserttail (target->references, cs->jumplocation);

    el = element_next (el);
  }
  return 1;
}

static
void analyse_switches (struct code *c)
{
  struct prx_reloc *aux;
  uint32 base, end, count = 0;
  uint32 i, j, tgt;

  c->switches = list_alloc (c->lstpool);

  for (i = 0; i < c->file->relocnum; i++) {
    struct prx_reloc *rel = &c->file->relocsbyaddr[i];
    count = 0;
    do {
      if (rel->type != R_MIPS_32) break;
      tgt = (rel[count].target - c->baddr) >> 2;
      if (tgt >= c->numopc) break;
      if (rel[count].target & 0x03) break;

      count++;
    } while ((i + count) < c->file->relocnum);

    if (count == 0) continue;

    base = end = prx_findreloc (c->file, rel->vaddr);
    if (base >= c->file->relocnum) continue;
    if (c->file->relocs[base].target != rel->vaddr) continue;

    for (; end < c->file->relocnum; end++) {
      aux = &c->file->relocs[end];
      if (aux->target != rel->vaddr) {
        if (aux->target & 0x03) {
          error (__FILE__ ": relocation target not word aligned 0x%08X", aux->target);
          count = 0;
        } else if (count > ((aux->target - rel->vaddr) >> 2))
          count = (aux->target - rel->vaddr) >> 2;
        break;
      }
    }

    if (count <= 1) continue;

    for (;base < end; base++) {
      aux = &c->file->relocs[base];
      tgt = (aux->vaddr - c->baddr) >> 2;
      if (tgt >= c->numopc) continue;
      if (aux->vaddr & 0x03) {
        error (__FILE__ ": relocation vaddr not word aligned 0x%08X", aux->vaddr);
        continue;
      }

      if (aux->type == R_MIPS_LO16) {
        struct codeswitch *cs;

        cs = fixedpool_alloc (c->switchpool);
        memset (cs, 0, sizeof (struct codeswitch));

        cs->jumpreloc = aux;
        cs->switchreloc = rel;
        cs->location = &c->base[tgt];
        cs->count = count;
        cs->references = list_alloc (c->lstpool);
        for (j = 0; j < count; j++) {
          tgt = (rel[j].target - c->baddr) >> 2;
          list_inserttail (cs->references, &c->base[tgt]);
        }

        if (analyse_switch (c, cs)) {
          list_inserttail (c->switches, cs);
        } else {
          fixedpool_free (c->switchpool, cs);
        }
      }
    }
  }
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
    sub->haserror = TRUE;
    error (__FILE__ ": location 0x%08X is both import and export", loc->address);
  }
}

static
void analyse_relocs (struct code *c)
{
  uint32 i, tgt;

  i = prx_findreloc (c->file, c->baddr);
  for (; i < c->file->relocnum; i++) {
    struct location *loc;
    struct prx_reloc *rel = &c->file->relocs[i];

    tgt = (rel->target - c->baddr) >> 2;
    if (tgt >= c->numopc) continue;

    if (rel->target & 0x03) {
      error (__FILE__ ": relocation not word aligned 0x%08X", rel->target);
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
      if (!loc->switchtarget)
        new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_HI16 || rel->type == R_MIPSX_HI16) {
      /* TODO */
      if (!loc->switchtarget)
        new_subroutine (c, loc, NULL, NULL);
    }
  }
}

static
void mark_reachable (struct code *c, struct location *loc)
{
  while (loc) {
    if (loc->reachable == 1) break;
    loc->reachable = 1;

    if (!loc->insn) return;

    if (loc->insn->flags & INSN_JUMP) {
      if (loc != c->end) loc[1].reachable = 2;

      if (loc->cswitch) {
        element el;
        el = list_head (loc->cswitch->references);
        while (el) {
          mark_reachable (c, element_getvalue (el));
          el = element_next (el);
        }
        return;
      }

      if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
        if (loc->target)
          mark_reachable (c, loc->target);

        if ((loc->address + 8) > c->end->address)
          return;
        loc += 2;
      } else
        loc = loc->target;
    } else if (loc->insn->flags & INSN_BRANCH) {
      if (loc != c->end) {
        if (!(loc->insn->flags & INSN_BRANCHLIKELY) || (loc->branchtype != BRANCH_NEVER))
          loc[1].reachable = 2;
      }

      if (loc->branchtype == BRANCH_ALWAYS) {
        if (loc->insn->flags & INSN_LINK) {
          mark_reachable (c, loc->target);
          if ((loc->address + 8) > c->end->address)
            return;
          loc += 2;
        } else
          loc = loc->target;
      } else {
        if (loc->branchtype == BRANCH_NORMAL) {
          mark_reachable (c, loc->target);
        }

        if ((loc->address + 8) > c->end->address)
          return;
        loc += 2;
      }
    } else {
      if (loc++ == c->end) return;
    }
  };
}

static
void find_hidden_subroutines (struct code *c)
{
  struct subroutine *cursub;
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    struct location *loc = &c->base[i];
    if (loc->sub)
      mark_reachable (c, loc);
  }

  for (i = 0; i < c->numopc; i++) {
    struct location *loc = &c->base[i];
    if (loc->sub) {
      cursub = loc->sub;
    }

    if (loc->target && (loc->branchtype != BRANCH_NEVER)) {
      struct location *aux = loc->target;
      if (!loc->target->sub) {
        do {
          if (aux->sub) {
            if (aux->sub != cursub) {
              if (!loc->reachable) {
                report (__FILE__ ": hidden subroutine at 0x%08X discovered because of 0x%08X (cursub = 0x%08X, sub = 0x%08X)\n",
                    loc->target->address, loc->address, cursub->location->address, aux->sub->location->address);
                new_subroutine (c, loc->target, NULL, NULL);
                mark_reachable (c, loc->target);
              } else {
                loc->error = ERROR_ILLEGAL_JUMP;
                loc->target->error = ERROR_ILLEGAL_JUMP;
                error (__FILE__ ": jump to a subroutine internal location at 0x%08X\n", loc->address);
              }
            }
            break;
          }
        } while (aux-- != c->base);
      }
    }
  }
}


static
void analyse_subroutine (struct code *c, struct subroutine *sub)
{
  struct location *loc;
  loc = sub->location;
  do {
    if (!loc->reachable) continue;

    if (loc->error) {
      switch (loc->error) {
      case ERROR_INVALID_OPCODE:
        error (__FILE__ ": invalid opcode 0x%08X at 0x%08X", loc->opc, loc->address);
        break;
      case ERROR_TARGET_OUTSIDE_FILE:
        error (__FILE__ ": branch/jump outside file at 0x%08X\n", loc->address);
        break;
      case ERROR_DELAY_SLOT:
        error (__FILE__ ": branch/jump inside delay slot at 0x%08X", loc->address);
        break;
      }

      sub->haserror = 1;
      return;
    }

    if (loc->target) {
      if (loc->target->sub != loc->sub) {
        if (!(loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D))) {
          report (__FILE__ ": jumped call at 0x%08X\n", loc->address);
        }
      }
    }
  } while (loc++ != sub->end);
}


static
void analyse_subroutines (struct code *c)
{
  uint32 i, j, tgt;
  struct subroutine *prevsub = NULL;
  element el;

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

  find_hidden_subroutines (c);

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
    analyse_subroutine (c, element_getvalue (el));
    el = element_next (el);
  }
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

  analyse_switches (c);
  analyse_relocs (c);
  analyse_subroutines (c);

  return c;
}
