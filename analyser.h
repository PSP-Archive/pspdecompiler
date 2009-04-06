#ifndef __ANALYSER_H
#define __ANALYSER_H

#include "prx.h"
#include "allegrex.h"
#include "lists.h"
#include "types.h"

#define REGNUM_GPR_BASE 0
#define REGNUM_GPR_END  31
#define REGNUM_LO       32
#define REGNUM_HI       33
#define NUMREGS         34

#define REF_CALL        0
#define REF_JUMP        1

struct location {
  uint32 opc;
  uint32 address;

  struct prx_reloc *extref;
  const struct allegrex_instruction *insn;

  int mark;

  int skipped, delayslot;
  int callcount, jumpcount;

  uint32 target_addr;
  struct location *target;

  list reg_sources;
  list reg_targets;

  list references;
  list targets;

  struct register_info *where_used;
  struct register_info *where_defined;
};

struct reference {
  uint32 info;
};

struct register_info {
  struct location *regs[NUMREGS];
};


struct subroutine {
  struct prx_function *function;
};

struct code {
  struct prx *file;

  uint32 baddr, lastaddr, numopc;
  struct location *base;

  list subroutines;

  list branches;
  list joints;

  list_pool subs_pool;
  list_pool references_pool;
  list_pool reginfo_pool;
};

struct code* analyse_code (struct prx *p);
void free_code (struct code *c);
void print_code (struct code *c);

#endif /* __ANALYSER_H */
