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


struct location {
  uint32 opc;
  uint32 address;

  const struct allegrex_instruction *insn;

  int    iscall;
  uint32 target_addr;
  struct location *target;
  struct location *next, *tnext;
  struct location *delayslot;

  list   references;
  list   externalrefs;

  int    isjoint;

  struct targetdeps *deptarget;
  struct sourcedeps *depsource, *depcall;

  struct location *regsource[2];
  list regtarget;

  struct extradeps *extrainfo;

  struct subroutine *sub;

};

struct extradeps {
  struct location *regsource[2];
  list regtarget[2];
};

struct sourcedeps {
  struct location *regs[NUMREGS];
};

struct targetdeps {
  list regs[NUMREGS];
};

struct codeswitch {
  struct prx_reloc *basereloc;
  int count;
};

struct subroutine {
  struct prx_function *function;
  struct location *location;
};

struct code {
  struct prx *file;

  uint32 baddr, numopc;
  struct location *base;
  struct location *extra;

  list subroutines;
  list switches;

  listpool  lstpool;
  fixedpool regsrcpool;
  fixedpool regtgtpool;
  fixedpool extrapool;
  fixedpool subspool;
  fixedpool switchpool;
};



#endif /* __CODE_H */
