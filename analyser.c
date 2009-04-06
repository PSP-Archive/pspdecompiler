
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
  struct location *base;
  uint32 i, numopc = size >> 2;
  int dslot = FALSE, skip = FALSE;

  if (size & 0x03 || address & 0x03) {
    error (__FILE__ ": size/address is not multiple of 4");
    return 0;
  }

  base = (struct location *) xmalloc ((numopc) * sizeof (struct location));
  memset (base, 0, (numopc) * sizeof (struct location));
  c->base = base;
  c->baddr = address;
  c->numopc = numopc;
  c->lastaddr = address + size - 4;

  for (i = 0; i < numopc; i++) {
    uint32 tgt;
    base[i].opc = code[i << 2];
    base[i].opc |= code[(i << 2) + 1] << 8;
    base[i].opc |= code[(i << 2) + 2] << 16;
    base[i].opc |= code[(i << 2) + 3] << 24;
    base[i].insn = allegrex_decode (base[i].opc);
    base[i].address = address + (i << 2);

    base[i].reg_sources = list_create (c->references_pool);
    base[i].reg_targets = list_create (c->references_pool);
    base[i].references = list_create (c->references_pool);

    if (base[i].insn == NULL) {
      error (__FILE__ ": invalid opcode 0x%08X at 0x%08X", base[i].opc, base[i].address);
      return 0;
    }

    base[i].skipped = skip && !dslot;
    base[i].delayslot = dslot;

    if (base[i].insn->flags & INSN_BRANCH) {
      tgt = base[i].opc & 0xFFFF;
      if (tgt & 0x8000) { tgt |= ~0xFFFF;  }
      tgt += i + 1;
      if (tgt < numopc) {
        base[i].target = &base[tgt];
        if (base[i].insn->flags & INSN_LINK) {
          base[tgt].callcount++;
        } else {
          base[tgt].jumpcount++;
        }
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
        if (base[i].insn->flags & INSN_LINK)
          base[tgt].callcount++;
        else
          base[tgt].jumpcount++;
      } else {
        error (__FILE__ ": jump outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
    } else {
      base[i].target_addr = 0;
    }

    if (base[i].insn->flags & (INSN_JUMP | INSN_BRANCH)) {
      if (dslot) error (__FILE__ ": jump or branch inside a delay slot at 0x%08X", base[i].address);
      else {
        if ((base[i].insn->flags & (INSN_JUMP | INSN_LINK)) == INSN_JUMP)
          skip = TRUE;
        else
          skip = FALSE;
      }
      dslot = TRUE;
    } else {
      if (!dslot) skip = FALSE;
      dslot = FALSE;
    }
  }

  if (dslot) {
    error (__FILE__ ": a delay slot at the end of code");
  }

  return 1;
}

static
int analyse_relocs (struct code *c)
{
  uint32 i, opc;
  for (i = 0; i < c->file->relocnum; i++) {
    struct prx_reloc *rel = &c->file->relocs[i];
    if (rel->target < c->baddr) continue;

    opc = (rel->target - c->baddr) >> 2;
    if (opc >= c->numopc) continue;

    c->base[opc].extref = rel;
  }
  return 1;
}

static
void linkregs (struct register_info *sources, struct register_info *targets, struct location *loc, int regno)
{
  element el;

  if (!targets->regs[regno])
    targets->regs[regno] = loc;

  el = list_inserthead (sources->regs[regno]->reg_targets, loc);
  ((struct reference *) element_addendum (el))->info = regno;

  el = list_inserthead (loc->reg_sources, sources->regs[regno]);
  ((struct reference *) element_addendum (el))->info = regno;
}

static
int analyse_subroutine (struct code *c, struct location *loc, struct subroutine *sub)
{
  int i;
  struct register_info *sources, *targets;
  element branch, joint;

  branch = list_inserttail (c->branches, NULL);
  joint = list_inserttail (c->joints, loc);

  sources = element_addendum (joint);
  targets = element_addendum (branch);

  for (i = 0; i < NUMREGS; i++) {
    sources->regs[i] = loc;
    targets->regs[i] = NULL;
  }

  do {
    struct location *jumploc = NULL;
    loc = element_value (joint);
    loc->where_used = targets;

    while (loc) {
      struct location *nloc = NULL;
      int flags = loc->insn->flags;

      loc->mark = 1;
      if (loc->address != c->lastaddr) {
        nloc = loc + 1;
        if (nloc->skipped) {
          nloc = jumploc;
        }
      } else {
      }


      if (flags & INSN_READ_GPR_S) {
        linkregs (sources, targets, loc, REGNUM_GPR_BASE + RS (loc->opc));
      }

      if (flags & INSN_READ_GPR_T) {
        linkregs (sources, targets, loc, REGNUM_GPR_BASE + RT (loc->opc));
      }

      if (flags & INSN_READ_LO) {
        linkregs (sources, targets, loc, REGNUM_LO);
      }

      if (flags & INSN_READ_HI) {
        linkregs (sources, targets, loc, REGNUM_HI);
      }

      if (flags & INSN_WRITE_GPR_D) {
        sources->regs[REGNUM_GPR_BASE + RD (loc->opc)] = loc;
      }

      if (flags & INSN_WRITE_GPR_T) {
        sources->regs[REGNUM_GPR_BASE + RT (loc->opc)] = loc;
      }

      if (flags & INSN_WRITE_LO) {
        sources->regs[REGNUM_LO] = loc;
      }

      if (flags & INSN_WRITE_HI) {
        sources->regs[REGNUM_HI] = loc;
      }

      if (flags & INSN_LINK) {

      } else {
        if (flags & INSN_BRANCH) {
          loc->where_defined = sources;
          branch = list_inserthead (c->branches, loc);
          sources = element_addendum (branch);
          memcpy (sources, loc->where_defined, sizeof (struct register_info));
          if (loc->target) {
            list_inserthead (loc->target->references, loc);
            if (!loc->target->mark)
              list_inserthead (c->joints, loc->target);
          }
        } else if (flags & INSN_JUMP) {
        }
      }

      if (nloc->callcount) {
        error (__FILE__ ": call inside a subroutine");
        return 0;
      }

      loc = nloc;
    }
    joint = element_next (joint);
  } while (joint);

  return 1;
}

static
int analyse_subroutines (struct code *c)
{
  uint32 i, j;
  struct prx_export *exp;
  element el;

  c->subs_pool = pool_create (sizeof (struct subroutine), 64, 8);
  c->subroutines = list_create (c->subs_pool);

  c->branches = list_create (c->reginfo_pool);
  c->joints = list_create (c->reginfo_pool);


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

      el = list_inserttail (c->subroutines, &c->base[tgt]);
      sub = element_addendum (el);
      sub->function = &exp->funcs[j];
    }
  }

  el = list_head (c->subroutines);
  while (el) {
    if (!analyse_subroutine (c, element_value (el), element_addendum (el)))
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
  c->references_pool = pool_create (sizeof (struct reference), 4096, 4096);
  c->reginfo_pool = pool_create (sizeof (struct register_info), 1024, 128);

  if (!decode_instructions (c, p->programs->data,
       p->modinfo->expvaddr - 4, p->programs->vaddr)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_relocs (c)) {
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
  if (c->subs_pool)
    pool_destroy (c->subs_pool);
  c->subs_pool = NULL;
  if (c->references_pool)
    pool_destroy (c->references_pool);
  c->references_pool = NULL;
  if (c->reginfo_pool)
    pool_destroy (c->reginfo_pool);
  c->reginfo_pool = NULL;
  free (c);
}

void print_code (struct code *c)
{
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    report ("%s", allegrex_disassemble (c->base[i].opc, c->base[i].address));
    report ("\n");
  }
}
