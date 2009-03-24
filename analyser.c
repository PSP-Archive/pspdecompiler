
#include <string.h>
#include <stdlib.h>

#include "analyser.h"
#include "utils.h"

static
int analyse_jumps (struct code *c, const uint8 *code, uint32 size, uint32 address)
{
  struct location *base;
  uint32 i, numopc = size >> 2;

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

    switch (base[i].itype) {
    case I_BEQ:
    case I_BNE:
    case I_BGEZ:
    case I_BGTZ:
    case I_BLEZ:
    case I_BLTZ:
    case I_BC1F:
    case I_BC1T:
    case I_BVF:
    case I_BVT:
      base[i].jtype = JTYPE_BRANCH;
      break;

    case I_BEQL:
    case I_BNEL:
    case I_BGEZL:
    case I_BGTZL:
    case I_BLEZL:
    case I_BLTZL:
    case I_BC1FL:
    case I_BC1TL:
    case I_BVFL:
    case I_BVTL:
      base[i].jtype = JTYPE_BRANCHLIKELY;
      break;

    case I_BGEZAL:
    case I_BLTZAL:
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

    switch (base[i].jtype) {
    case JTYPE_BRANCH:
    case JTYPE_BRANCHANDLINK:
    case JTYPE_BRANCHANDLINKLIKELY:
    case JTYPE_BRANCHLIKELY:
      tgt = base[i].opc & 0xFFFF;
      if (tgt & 0x8000) {
        tgt |= ~0xFFFF;
      }
      tgt += i + 1;
      if (tgt < numopc) {
        if (base[i].jtype == JTYPE_BRANCHANDLINK ||
            base[i].jtype == JTYPE_BRANCHANDLINKLIKELY) {
          LLIST_ADD (c->pool, base[tgt].callrefs, &base[i]);
        } else {
          LLIST_ADD (c->pool, base[tgt].jumprefs, &base[i]);
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
        llist ref = llist_alloc (c->pool);
        ref->value = &base[i];
        base[i].target = &base[tgt];
        if (base[i].jtype == JTYPE_JUMPANDLINK) {
          ref->next = base[tgt].callrefs;
          base[tgt].callrefs = ref;
        } else {
          ref->next = base[tgt].jumprefs;
          base[tgt].jumprefs = ref;
        }
      } else {
        error (__FILE__ ": jump outside file\n%s", allegrex_disassemble (base[i].opc, base[i].address));
      }
      break;
    default:
      base[i].target_addr = 0;
    }
  }

  i = numopc - 1;
  do {
    if (base[i].callrefs) {
      LLIST_ADD (c->pool, c->subroutines, &base[i]);
    }
  } while (i-- != 0);

  return 1;
}

static
int analyse_relocs (struct code *c)
{
  return 1;
}

static
int analyse_registers (struct code *c)
{
  return 1;
}


struct code* analyse_code (struct prx *p)
{
  struct code *c;
  c = (struct code *) xmalloc (sizeof (struct code));
  c->image = p;
  c->pool = llist_create ();
  c->subroutines = NULL;

  if (!analyse_jumps (c, p->programs->data,
       p->modinfo->expvaddr - 4, p->programs->vaddr)) {
    free_code (c);
    return NULL;
  }

  if (!analyse_relocs (c)) {
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
  if (c->pool)
    llist_destroy (c->pool);
  c->pool = NULL;
  free (c);
}

void print_code (struct code *c)
{
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    if (c->base[i].callrefs || c->base[i].jumprefs) {
      report ("\n");
    }

    if (c->base[i].callrefs) {
      llist l;
      report ("; Called from: ");
      for (l = c->base[i].callrefs; l; l = l->next) {
        struct location *loc = (struct location *) l->value;
        report ("0x%08X  ", loc->address);
      }
      report ("\n");
    }
    if (c->base[i].jumprefs) {
      llist l;
      report ("; Jumped from: ");
      for (l = c->base[i].jumprefs; l; l = l->next) {
        struct location *loc = (struct location *) l->value;
        report ("0x%08X  ", loc->address);
      }
      report ("\n");
    }
    report ("%s", allegrex_disassemble (c->base[i].opc, c->base[i].address));
    report ("\n");
  }
}
