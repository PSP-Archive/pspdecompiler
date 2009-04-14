#ifndef __CODE_H
#define __CODE_H

#include "prx.h"
#include "allegrex.h"
#include "alloc.h"
#include "lists.h"
#include "types.h"

#define ERROR_INVALID_OPCODE       1
#define ERROR_DELAY_SLOT           2
#define ERROR_TARGET_OUTSIDE_FILE  3
#define ERROR_ILLEGAL_JUMP         4

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
  int  error;
  int  switchtarget;

  uint32 gpr_used;
  uint32 gpr_defined;

  struct subroutine *sub;
  struct bblock *block;

  struct codeswitch *cswitch;
};

struct codeswitch {
  struct prx_reloc *jumpreloc;
  struct prx_reloc *switchreloc;
  struct location *location;
  struct location *jumplocation;
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
