#ifndef __CODE_H
#define __CODE_H

#include "prx.h"
#include "allegrex.h"
#include "alloc.h"
#include "lists.h"
#include "types.h"

#define REGNUM_GPR_BASE 0
#define REGNUM_GPR_END  31
#define REGNUM_LO       32
#define REGNUM_HI       33
#define NUMREGS         34

#define BRANCH_NORMAL   0
#define BRANCH_ALWAYS   1
#define BRANCH_NEVER    2

struct location {
  uint32 opc;
  uint32 address;

  const struct allegrex_instruction *insn;
  struct location *target;

  list references;
  int  branchtype;
  int  reachable;
  int  haserror;

  struct subroutine *sub;
  struct bblock *block;
};

struct codeswitch {
  struct prx_reloc *basereloc;
  list references;
  int count;
};

struct subroutine {
  struct prx_function *export;
  struct prx_function *import;
  struct location *location;
  struct location *end;
  int    haserror;
};

struct bblock {
  struct location *begin;
  struct location *end;
  list   outrefs, inrefs;
};

struct code {
  struct prx *file;

  uint32 baddr, numopc;
  struct location *base;
  struct location *end;

  list subroutines;
  list switches;

  listpool  lstpool;
  fixedpool switchpool;
  fixedpool subspool;
};

struct code *code_alloc (void);
void code_free (struct code *c);



#endif /* __CODE_H */
