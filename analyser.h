#ifndef __ANALYSER_H
#define __ANALYSER_H

#include "types.h"
#include "allegrex.h"

enum jump_type {
  JTYPE_NONE,
  JTYPE_BRANCH,
  JTYPE_BRANCHLIKELY,
  JTYPE_BRANCHANDLINK,
  JTYPE_BRANCHANDLINKLIKELY,
  JTYPE_JUMP,
  JTYPE_JUMPANDLINK,
  JTYPE_JUMPREGISTER,
  JTYPE_JUMPANDLINKREGISTER
};

struct location {
  uint32 opc;
  uint32 address;

  enum insn_type itype;

  uint32 target_addr;
  enum jump_type jtype;
  struct location *target;
  struct location *jumprefs;
  struct location *callrefs;

  struct location *nextref;
};

struct code {
  uint32 address, numopc;
  struct location *loc;
};

struct code* analyse_code (const uint8 *code, uint32 size, uint32 address);
void free_code (struct code *c);
void print_code (struct code *c);

#endif /* __ANALYSER_H */
