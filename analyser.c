
#include <string.h>
#include <stdlib.h>

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
  struct location *base, *extra;
  uint32 i, extracount = 0;
  uint32 numopc = size >> 2;
  int slot = FALSE;

  if (size & 0x03 || address & 0x03) {
    error (__FILE__ ": size/address is not multiple of 4");
    return 0;
  }

  base = (struct location *) xmalloc ((numopc) * sizeof (struct location));
  memset (base, 0, (numopc) * sizeof (struct location));
  c->base = base;
  c->baddr = address;
  c->numopc = numopc;

  for (i = 0; i < numopc; i++) {
    uint32 tgt;

    base[i].opc = code[i << 2];
    base[i].opc |= code[(i << 2) + 1] << 8;
    base[i].opc |= code[(i << 2) + 2] << 16;
    base[i].opc |= code[(i << 2) + 3] << 24;
    base[i].insn = allegrex_decode (base[i].opc, FALSE);
    base[i].address = address + (i << 2);

    if ((i + 1) != numopc)
      base[i].next = &base[i + 1];

    if (base[i].insn == NULL) {
      error (__FILE__ ": invalid opcode 0x%08X at 0x%08X", base[i].opc, base[i].address);
      continue;
    }

    if (base[i].insn->flags & (INSN_BRANCH | INSN_JUMP)) {
      if (slot) error (__FILE__ ": branch/jump inside delay slot at 0x%08X", base[i].address);
      extracount++;
      slot = TRUE;
    } else {
      slot = FALSE;
    }

    if (base[i].insn->flags & INSN_BRANCH) {
      tgt = base[i].opc & 0xFFFF;
      if (tgt & 0x8000) { tgt |= ~0xFFFF;  }
      tgt += i + 1;
      if (tgt < numopc) {
        base[i].target = &base[tgt];
      } else {
        error (__FILE__ ": branch outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
      base[i].target_addr = (tgt << 2) + address;
    } else if ((base[i].insn->flags & (INSN_JUMP | INSN_READ_GPR_S)) == INSN_JUMP) {
      base[i].target_addr = (base[i].opc & 0x3FFFFFF) << 2;;
      base[i].target_addr |= ((base[i].address) & 0xF0000000);
      tgt = (base[i].target_addr - address) >> 2;
      if (tgt < numopc) {
        base[i].target = &base[tgt];
      } else {
        error (__FILE__ ": jump outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
    } else {
      base[i].target_addr = 0;
    }
  }

  extra = (struct location *) xmalloc ((extracount) * sizeof (struct location));
  memset (extra, 0, (extracount) * sizeof (struct location));
  c->extra = extra;
  extracount = 0;

  for (i = numopc; i > 0;) {
    i--;
    if (!base[i].insn) continue;
    if (base[i].insn->flags & (INSN_BRANCH | INSN_JUMP)) {
      if ((i + 1) != numopc) {
        memcpy (&extra[extracount], &base[i + 1], sizeof (struct location));
        extra[extracount].references = list_alloc (c->lstpool);
        base[i + 1].delayslot = &extra[extracount];

        if (base[i].target) {
          if (!base[i].target->references)
            base[i].target->references = list_alloc (c->lstpool);
          list_inserttail (base[i].target->references, &extra[extracount]);
        }

        if (base[i].insn->flags & INSN_LINK) {
          extra[extracount].iscall = TRUE;
          if ((i + 2) < numopc) {
            extra[extracount].next = &base[i + 2];
            if (!base[i + 2].references)
              base[i + 2].references = list_alloc (c->lstpool);
            list_inserttail (base[i + 2].references, &extra[extracount]);
          } else {
            error (__FILE__ ": call at the end of file");
            extra[extracount].next = NULL;
          }
        } else {
          extra[extracount].next = base[i].target;
        }

        base[i].tnext = &extra[extracount];
        list_inserttail (extra[extracount].references, &base[i]);

        if (base[i].insn->flags & INSN_BRANCHLIKELY) {
          if ((i + 2) < numopc) {
            base[i].next = &base[i + 2];
            if (!base[i + 2].references)
              base[i + 2].references = list_alloc (c->lstpool);
            list_inserttail (base[i + 2].references, &base[i]);
          } else {
            error (__FILE__ ": branch likely at the end of file");
            base[i].next = NULL;
          }
        } else if (base[i].insn->flags & INSN_BRANCH) {
          if (!base[i + 1].references)
            base[i + 1].references = list_alloc (c->lstpool);
          list_inserttail (base[i + 1].references, &base[i]);
        }
      } else {
        error (__FILE__ ": branch/jump at the end of file");
        base[i].target = NULL;
      }

      extracount++;
    } else {
      if ((i + 1) != numopc) {
        if (!base[i + 1].references)
          base[i + 1].references = list_alloc (c->lstpool);
        list_inserttail (base[i + 1].references, &base[i]);
      }
    }
  }

  return 1;
}

static
int analyse_relocs (struct code *c)
{
  uint32 i, relocnum;

  c->switchpool = fixedpool_create (sizeof (struct codeswitch), 32);
  c->switches = list_alloc (c->lstpool);
  relocnum = c->file->relocnum;

  i = prx_findreloc (c->file, c->baddr);
  for(;i < relocnum; i++) {
    uint32 j, opc;
    struct prx_reloc *rel = &c->file->relocs[i];

    opc = (rel->target - c->baddr) >> 2;
    if (opc >= c->numopc) continue;

    if (!c->base[opc].externalrefs)
      c->base[opc].externalrefs = list_alloc (c->lstpool);
    list_inserttail (c->base[opc].externalrefs, rel);

    if (rel->type == R_MIPS_32) {
      struct prx_reloc *frel;
      uint32 end, count = 0;

      j = prx_findrelocbyaddr (c->file, rel->vaddr);
      while (j < relocnum) {
        if (c->file->relocsbyaddr[j].vaddr != (rel->vaddr + (count << 2)))
          break;
        count++; j++;
      }

      j = end = prx_findreloc (c->file, rel->vaddr);

      for (;end < relocnum; end++) {
        frel = &c->file->relocs[end];
        if (frel->target != rel->vaddr) {
          if (count > ((frel->target - rel->vaddr) >> 2))
            count = (frel->target - rel->vaddr) >> 2;
          break;
        }
      }

      if (count == 1) continue;

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
  return 1;
}



static
void linkregs (struct sourcedeps *source, struct targetdeps *target,
               struct code *c, struct location *loc, int regno, int slot)
{
  struct location **src;
  list *tgt;

  if (!target->regs[regno]) {
    target->regs[regno] = list_alloc (c->lstpool);
    list_inserttail (target->regs[regno], loc);
  }

  if ((regno >= REGNUM_GPR_BASE) && (regno <= REGNUM_GPR_END)) {
    src = &loc->regsource[slot];
    tgt = &source->regs[regno]->regtarget;
  } else {
    if (!loc->extrainfo) {
      loc->extrainfo = fixedpool_alloc (c->extrapool);
      memset (loc->extrainfo, 0, sizeof (struct extradeps));
    }
    if (!source->regs[regno]->extrainfo) {
      source->regs[regno]->extrainfo = fixedpool_alloc (c->extrapool);
      memset (source->regs[regno]->extrainfo, 0, sizeof (struct extradeps));
    }

    src = &loc->extrainfo->regsource[slot];
    tgt = &source->regs[regno]->extrainfo->regtarget[slot];
  }

  if (!(*tgt)) {
    *tgt = list_alloc (c->lstpool);
  }

  list_inserttail (*tgt, loc);
  *src = source->regs[regno];
}

static
int analyse_register_dependencies (struct code *c)
{
  uint32 i;
  struct sourcedeps *source;
  struct targetdeps *target;
  list locs, sources, targets;

  c->regsrcpool = fixedpool_create (sizeof (struct sourcedeps), 256);
  c->regtgtpool = fixedpool_create (sizeof (struct targetdeps), 256);
  c->extrapool = fixedpool_create (sizeof (struct extradeps), 128);

  locs = list_alloc (c->lstpool);
  sources = list_alloc (c->lstpool);
  targets = list_alloc (c->lstpool);

  for (i = 0; i < c->numopc; i++) {
    if (!c->base[i].externalrefs) {
      if (!c->base[i].references) continue;
      if (list_size (c->base[i].references) == 1) {
        if (!((struct location *) list_headvalue (c->base[i].references))->iscall)
          continue;
      }
    }
    list_inserttail (locs, &c->base[i]);
    c->base[i].isjoint = 1;
  }

  while (list_size (locs) != 0) {
    struct location *loc;
    struct location *curr;

    loc = list_removehead (locs);

    if (loc->isjoint) {
      source = fixedpool_alloc (c->regsrcpool);
      target = fixedpool_alloc (c->regtgtpool);
      for (i = 0; i < NUMREGS; i++) {
        source->regs[i] = loc;
        target->regs[i] = NULL;
      }
    } else {
      source = list_removehead (sources);
      target = list_removehead (targets);
    }

    curr = loc;

    while (1) {
      if (curr->insn) {
        if (curr->insn->flags & INSN_READ_GPR_S) {
          if (RS (curr->opc) != 0)
            linkregs (source, target, c, curr, REGNUM_GPR_BASE + RS (curr->opc), 0);
        }

        if (curr->insn->flags & INSN_READ_GPR_T) {
          if (RT (curr->opc) != 0)
            linkregs (source, target, c, curr, REGNUM_GPR_BASE + RT (curr->opc), 1);
        }

        if (curr->insn->flags & INSN_READ_LO) {
          linkregs (source, target, c, curr, REGNUM_LO, 0);
        }

        if (curr->insn->flags & INSN_READ_HI) {
          linkregs (source, target, c, curr, REGNUM_HI, 1);
        }

        if (curr->insn->flags & INSN_WRITE_GPR_D) {
          if (RD (curr->opc) != 0)
            source->regs[REGNUM_GPR_BASE + RD (curr->opc)] = curr;
        }

        if (curr->insn->flags & INSN_WRITE_GPR_T) {
          if (RT (curr->opc) != 0)
            source->regs[REGNUM_GPR_BASE + RT (curr->opc)] = curr;
        }

        if (curr->insn->flags & INSN_WRITE_LO) {
          source->regs[REGNUM_LO] = curr;
        }

        if (curr->insn->flags & INSN_WRITE_HI) {
          source->regs[REGNUM_HI] = curr;
        }

        if (curr->insn->flags & INSN_LINK) {
          source->regs[REGNUM_GPR_BASE + 31] = curr;
        }

        if (curr->tnext) {
          struct sourcedeps *copy;
          list_inserthead (locs, curr->tnext);

          copy = fixedpool_alloc (c->regsrcpool);
          memcpy (copy, source, sizeof (struct sourcedeps));
          list_inserthead (sources, copy);
          list_inserthead (targets, target);
        }
      }

      if (curr->iscall) {
        curr->depcall = fixedpool_alloc (c->regsrcpool);
        memcpy (curr->depcall, source, sizeof (struct sourcedeps));
      }

      if (!curr->next) break;

      if (curr->next->isjoint) {
        curr->depsource = source;
        break;
      }

      curr = curr->next;
    }

    if (loc->isjoint)
      loc->deptarget = target;
  }

  list_free (locs);
  list_free (sources);
  list_free (targets);
  return 1;
}

static
int analyse_switches (struct code *c)
{
  return 1;
}

static
int analyse_subroutine (struct code *c, struct subroutine *sub)
{
  return 1;
}

static
int analyse_subroutines (struct code *c)
{
  uint32 i, j;
  struct prx_export *exp;
  element el;

  c->subspool = fixedpool_create (sizeof (struct subroutine), 128);
  c->subroutines = list_alloc (c->lstpool);

  for (i = 0; i < c->file->modinfo->numexports; i++) {
    exp = &c->file->modinfo->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct subroutine *sub;
      element el;
      uint32 tgt;

      tgt = (exp->funcs[j].vaddr - c->baddr) >> 2;
      if (exp->funcs[j].vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid exported function");
        continue;
      }

      sub = fixedpool_alloc (c->subspool);
      el = list_inserttail (c->subroutines, sub);
      sub->function = &exp->funcs[j];
      sub->location = &c->base[tgt];
      sub->location->sub = sub;
    }
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
  struct code *c;
  c = (struct code *) xmalloc (sizeof (struct code));
  memset (c, 0, sizeof (struct code));

  c->file = p;
  c->lstpool = listpool_create (1024, 1024);

  if (!decode_instructions (c, p->programs->data,
       p->modinfo->expvaddr - 4, p->programs->vaddr)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_relocs (c)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_register_dependencies (c)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_switches (c)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_subroutines (c)) {
    free_code (c);
    return NULL;
  }

  return c;
}


void free_code (struct code *c)
{
  if (c->base)
    free (c->base);
  c->base = NULL;

  if (c->extra)
    free (c->extra);
  c->extra = NULL;

  if (c->lstpool)
    listpool_destroy (c->lstpool);
  c->lstpool = NULL;

  if (c->regsrcpool)
    fixedpool_destroy (c->regsrcpool, NULL, NULL);
  c->regsrcpool = NULL;

  if (c->regtgtpool)
    fixedpool_destroy (c->regtgtpool, NULL, NULL);
  c->regtgtpool = NULL;

  if (c->extrapool)
    fixedpool_destroy (c->extrapool, NULL, NULL);
  c->extrapool = NULL;

  if (c->subspool)
    fixedpool_destroy (c->subspool, NULL, NULL);
  c->subspool = NULL
  ;
  if (c->switchpool)
    fixedpool_destroy (c->switchpool, NULL, NULL);

  free (c);
}

