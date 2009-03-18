
#include <string.h>
#include <stdlib.h>

#include "analyser.h"
#include "utils.h"


struct code* analyse_code (const uint8 *code, uint32 size, uint32 address)
{
  struct code *c;
  struct location *loc;
  uint32 i, numopc = size >> 2;

  if (size & 0x03 || address & 0x03) {
    error (__FILE__ ": size/address is not multiple of 4");
    return NULL;
  }

  c = (struct code *) xmalloc (sizeof (struct code));
  loc = (struct location *) xmalloc (numopc * sizeof (struct location));
  memset (loc, 0, numopc * sizeof (struct location));
  c->loc = loc;
  c->address = address;
  c->numopc = numopc;

  for (i = 0; i < numopc; i++) {
    uint32 tgt;
    loc[i].opc = code[i << 2];
    loc[i].opc |= code[(i << 2) + 1] << 8;
    loc[i].opc |= code[(i << 2) + 2] << 16;
    loc[i].opc |= code[(i << 2) + 3] << 24;
    loc[i].itype = allegrex_insn_type (loc[i].opc);
    loc[i].address = address + (i << 2);

    switch (loc[i].itype) {
    case I_BEQ:
    case I_BNE:
    case I_BGEZ:
    case I_BGTZ:
    case I_BLEZ:
    case I_BLTZ:
      loc[i].jtype = JTYPE_BRANCH;
      break;

    case I_BEQL:
    case I_BNEL:
    case I_BGEZL:
    case I_BGTZL:
    case I_BLEZL:
    case I_BLTZL:
      loc[i].jtype = JTYPE_BRANCHLIKELY;
      break;

    case I_BGEZAL:
    case I_BLTZAL:
      loc[i].jtype = JTYPE_BRANCHANDLINK;
      break;

    case I_BLTZALL:
      loc[i].jtype = JTYPE_BRANCHANDLINKLIKELY;
      break;

    case I_J:
      loc[i].jtype = JTYPE_JUMP;
      break;

    case I_JAL:
      loc[i].jtype = JTYPE_JUMPANDLINK;
      break;

    case I_JALR:
      loc[i].jtype = JTYPE_JUMPANDLINKREGISTER;
      break;

    case I_JR:
      loc[i].jtype = JTYPE_JUMPREGISTER;
      break;

    default:
      loc[i].jtype = JTYPE_NONE;
      break;
    }

    switch (loc[i].jtype) {
    case JTYPE_BRANCH:
    case JTYPE_BRANCHANDLINK:
    case JTYPE_BRANCHANDLINKLIKELY:
    case JTYPE_BRANCHLIKELY:
      tgt = loc[i].opc & 0xFFFF;
      if (tgt & 0x8000) {
        tgt |= ~0xFFFF;
      }
      tgt += i + 1;
      if (tgt < numopc) {
        loc[i].target = &loc[tgt];
        if (loc[i].jtype == JTYPE_BRANCHANDLINK ||
            loc[i].jtype == JTYPE_BRANCHANDLINKLIKELY) {
          loc[i].nextref = loc[tgt].callrefs;
          loc[tgt].callrefs = &loc[i];
        } else {
          loc[i].nextref = loc[tgt].jumprefs;
          loc[tgt].jumprefs = &loc[i];
        }
      }
      loc[i].target_addr = (tgt << 2) + address;
      break;
    case JTYPE_JUMP:
    case JTYPE_JUMPANDLINK:
      loc[i].target_addr = (loc[i].opc & 0x3FFFFFF) << 2;;
      loc[i].target_addr |= ((loc[i].address) & 0xF0000000);
      tgt = (loc[i].target_addr - address) >> 2;
      if (tgt < numopc) {
        loc[i].target = &loc[tgt];
        if (loc[i].jtype == JTYPE_JUMPANDLINK) {
          loc[i].nextref = loc[tgt].callrefs;
          loc[tgt].callrefs = &loc[i];
        } else {
          loc[i].nextref = loc[tgt].jumprefs;
          loc[tgt].jumprefs = &loc[i];
        }
      }
      break;
    default:
      loc[i].target_addr = 0;
    }
  }

  for (i = 0; i < numopc; i++) {
    if (loc[i].jumprefs && loc[i].callrefs) {
      error (__FILE__ ": location 0x%08X referenced by calls and jumps", loc[i].address);
      free_code (c);
      return NULL;
    }
  }

  return c;
}

void free_code (struct code *c)
{
  if (c->loc)
    free (c->loc);
  c->loc = NULL;
  free (c);
}

void print_code (struct code *c)
{
  uint32 i;
  for (i = 0; i < c->numopc; i++) {
    if (c->loc[i].callrefs || c->loc[i].jumprefs) {
      report ("\n");
    }

    if (c->loc[i].callrefs) {
      struct location *l;
      report ("; Called from: ");
      for (l = c->loc[i].callrefs; l; l = l->nextref) {
        report ("0x%08X  ", l->address);
      }
      report ("\n");
    }
    if (c->loc[i].jumprefs) {
      struct location *l;
      report ("; Jumped from: ");
      for (l = c->loc[i].jumprefs; l; l = l->nextref) {
        report ("0x%08X  ", l->address);
      }
      report ("\n");
    }
    report ("%s", allegrex_disassemble (c->loc[i].opc, c->loc[i].address));
    report ("\n");
  }
}
