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

  list   references;
  list   externalrefs;

  int    isjoint;

  struct regdep_target *deptarget;
  struct regdep_source *depsource, *depcall;

  struct location *regsource[2];
  list regtarget;

  struct extraregs_info *extrainfo;

};

struct extraregs_info {
  struct location *regsource[2];
  list regtarget[2];
};

struct regdep_source {
  struct location *dependency[NUMREGS];
};

struct regdep_target {
  list dependency[NUMREGS];
};


struct subroutine {
  struct prx_function *function;
};

struct code {
  struct prx *file;

  uint32 baddr, numopc;
  struct location *base;
  struct location *extra;

  listpool  lstpool;
  fixedpool regsrcpool;
  fixedpool regtgtpool;
  fixedpool extrapool;
};



#endif /* __CODE_H */
