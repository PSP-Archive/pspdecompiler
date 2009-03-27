#ifndef __ANALYSER_H
#define __ANALYSER_H

#include "prx.h"
#include "allegrex.h"
#include "types.h"

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

  int refcount;
  int ext_refcount;
  enum insn_type itype;

  uint32 target_addr;
  enum jump_type jtype;
  struct location *target;
};

struct code {
  struct prx *file;

  uint32 baddr, numopc;
  struct location *base;
};

struct code* analyse_code (struct prx *p);
void free_code (struct code *c);
void print_code (struct code *c);

#endif /* __ANALYSER_H */
