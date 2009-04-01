
#include <string.h>
#include <stdlib.h>

#include "analyser.h"
#include "utils.h"

static
int analyse_jumps (struct code *c, const uint8 *code, uint32 size, uint32 address)
{
  struct location *base;
  uint32 i, numopc = size >> 2;
  int dslot = FALSE, skip = FALSE;

  if (size & 0x03 || address & 0x03) {
    error (__FILE__ ": size/address is not multiple of 4");
    return 0;
  }

  base = (struct location *) xmalloc (numopc * sizeof (struct location));
  memset (base, 0, numopc * sizeof (struct location));
  c->base = base;
  c->baddr = address;
  c->numopc = numopc;

  for (i = 0; i < numopc; i++) {
    uint32 tgt;
    base[i].opc = code[i << 2];
    base[i].opc |= code[(i << 2) + 1] << 8;
    base[i].opc |= code[(i << 2) + 2] << 16;
    base[i].opc |= code[(i << 2) + 3] << 24;
    base[i].itype = allegrex_insn_type (base[i].opc);
    base[i].address = address + (i << 2);

    if (!skip || dslot)
      base[i].notskipped = 1;

    switch (base[i].itype) {
    case I_BEQ: case I_BNE: case I_BGEZ: case I_BGTZ: case I_BLEZ:
    case I_BLTZ: case I_BC1F: case I_BC1T: case I_BVF: case I_BVT:
      base[i].jtype = JTYPE_BRANCH;
      break;
    case I_BEQL: case I_BNEL: case I_BGEZL: case I_BGTZL: case I_BLEZL:
    case I_BLTZL: case I_BC1FL: case I_BC1TL: case I_BVFL: case I_BVTL:
      base[i].jtype = JTYPE_BRANCHLIKELY;
      break;
    case I_BGEZAL: case I_BLTZAL:
      base[i].jtype = JTYPE_BRANCHANDLINK;
      break;
    case I_BLTZALL:
      base[i].jtype = JTYPE_BRANCHANDLINKLIKELY;
      break;
    case I_J:
      base[i].jtype = JTYPE_JUMP;
      break;
    case I_JAL:
      base[i].jtype = JTYPE_JUMPANDLINK;
      break;
    case I_JALR:
      base[i].jtype = JTYPE_JUMPANDLINKREGISTER;
      break;
    case I_JR:
      base[i].jtype = JTYPE_JUMPREGISTER;
      break;
    default:
      base[i].jtype = JTYPE_NONE;
      break;
    }

    if (base[i].jtype != JTYPE_NONE) {
      if (dslot) error (__FILE__ ": jump inside a delay slot at 0x%08X", base[i].address);
      else {
        if (base[i].jtype == JTYPE_JUMP || base[i].jtype == JTYPE_JUMPREGISTER)
          skip = TRUE;
        else
          skip = FALSE;
      }
      dslot = TRUE;
    } else {
      if (!dslot) skip = FALSE;
      dslot = FALSE;
    }

    switch (base[i].jtype) {
    case JTYPE_BRANCH:
    case JTYPE_BRANCHANDLINK:
    case JTYPE_BRANCHANDLINKLIKELY:
    case JTYPE_BRANCHLIKELY:
      tgt = base[i].opc & 0xFFFF;
      if (tgt & 0x8000) { tgt |= ~0xFFFF;  }
      tgt += i + 1;
      if (tgt < numopc) {
        if (base[i].jtype == JTYPE_BRANCHANDLINK ||
            base[i].jtype == JTYPE_BRANCHANDLINKLIKELY) {
          base[tgt].callcount++;
        } else {
          base[tgt].jumpcount++;
        }
      } else {
        error (__FILE__ ": branch outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
      base[i].target_addr = (tgt << 2) + address;
      break;
    case JTYPE_JUMP:
    case JTYPE_JUMPANDLINK:
      base[i].target_addr = (base[i].opc & 0x3FFFFFF) << 2;;
      base[i].target_addr |= ((base[i].address) & 0xF0000000);
      tgt = (base[i].target_addr - address) >> 2;
      if (tgt < numopc) {
        if (base[i].jtype == JTYPE_JUMPANDLINK)
          base[tgt].callcount++;
        else
          base[tgt].jumpcount++;
      } else {
        error (__FILE__ ": jump outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
      break;
    default:
      base[i].target_addr = 0;
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
int analyse_subroutines (struct code *c)
{
  uint32 i, j;
  struct prx_export *exp;

  c->subs_pool = pool_create (sizeof (struct subroutine), 64, 8);
  c->subroutines = list_create (c->subs_pool);

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

      el = list_inserttail (c->subroutines, NULL);
      sub = element_addendum (el);
      sub->function = &exp->funcs[j];
      sub->loc = &c->base[tgt];
    }
  }

  return 1;
}




struct code* analyse_code (struct prx *p)
{
  struct code *c;
  c = (struct code *) xmalloc (sizeof (struct code));
  c->file = p;

  if (!analyse_jumps (c, p->programs->data,
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
