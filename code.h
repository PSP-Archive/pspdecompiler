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
#define ERROR_DELAY_SLOT_TARGET    4
#define ERROR_ILLEGAL_BRANCH       5

struct location {
  uint32 opc;
  uint32 address;

  const struct allegrex_instruction *insn;
  struct location *target;

  list references;
  int  branchalways;
  int  reachable;
  int  error;

  struct subroutine *sub;
  struct basicblock *block;

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

  struct location *begin;
  struct location *end;

  struct basicblock *endblock;

  int    haserror;
  int    dfscount;
  list   blocks;
};

struct basicblock {
  struct location *begin;
  struct location *end;
  struct location *jumploc;

  struct basicblock *dominator;
  list   frontier;

  list   outrefs, inrefs;
  int    dfsnum;
};

struct code {
  struct prx *file;

  uint32 baddr, numopc;
  struct location *base;
  struct location *end;

  list subroutines;

  listpool  lstpool;
  fixedpool switchpool;
  fixedpool subspool;
  fixedpool blockspool;
};

struct code* code_analyse (struct prx *p);
void code_free (struct code *c);

int decode_instructions (struct code *c);
uint32 location_gpr_used (struct location *loc);
uint32 location_gpr_defined (struct location *loc);

void extract_switches (struct code *c);
void extract_subroutines (struct code *c);

int extract_cfg (struct code *c, struct subroutine *sub);
int cfg_dfs (struct subroutine *sub);
void cfg_dominance (struct subroutine *sub);

#endif /* __CODE_H */
