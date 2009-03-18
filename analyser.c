
#include <string.h>
#include "analyser.h"
#include "utils.h"


struct code_position* analyse_code (uint8 *code, uint32 size)
{
  struct code_position *c;
  uint32 i, numopc = size >> 2;

  if (size & 0x03) {
    error (__FILE__ ": size is not multiple of 4");
    return NULL;
  }

  c = (struct code_position *) xmalloc (numopc * sizeof (struct code_position));
  memset (c, 0, numopc * sizeof (struct code_position));

  for (i = 0; i < numopc; i++) {
    uint32 offset;
    int mode;
    c[i].opc = code[i << 2];
    c[i].opc |= code[(i << 2) + 1] << 8;
    c[i].opc |= code[(i << 2) + 2] << 16;
    c[i].opc |= code[(i << 2) + 3] << 24;
    c[i].itype = allegrex_get_itype (c[i].opc);

    switch (c[i].itype) {
    case I_BEQ:
    case I_BNE:
    case I_BGEZ:
    case I_BGTZ:
    case I_BLEZ:
    case I_BLTZ:
      c[i].jumptype = JTYPE_BRANCH;
      mode = 1;
      break;

    case I_BEQL:
    case I_BNEL:
    case I_BGEZL:
    case I_BGTZL:
    case I_BLEZL:
    case I_BLTZL:
      c[i].jumptype = JTYPE_BRANCHLIKELY;
      mode = 1;
      break;

    case I_BGEZAL:
    case I_BLTZAL:
      c[i].jumptype = JTYPE_BRANCHANDLINK;
      mode = 1;
      break;

    case I_BLTZALL:
      c[i].jumptype = JTYPE_BRANCHANDLINKLIKELY;
      mode = 1;
      break;

    case I_J:
      c[i].jumptype = JTYPE_JUMP;
      mode = 2;
      break;

    case I_JAL:
      c[i].jumptype = JTYPE_JUMPANDLINK;
      mode = 2;
      break;

    case I_JALR:
      c[i].jumptype = JTYPE_JUMPANDLINKREGISTER;
      break;

    case I_JR:
      c[i].jumptype = JTYPE_JUMPREGISTER;
      break;

    default:
      c[i].jumptype = JTYPE_NONE;
      break;
    }

    switch (mode) {
    case 1:
      offset = c[i].opc & 0xFFFF;
      if (offset & 0x8000) {
        offset |= ~0xFFFF;
      }
      offset += i + 1;
      if (offset < numopc) {
        c[i].target = &c[offset];
        c[i].next = c[offset].jumprefs;
        c[offset].jumprefs = &c[i];
      }
      break;
    }
  }
  return c;
}
