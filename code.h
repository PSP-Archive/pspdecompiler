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

#define LOCATION_REACHABLE         1
#define LOCATION_DELAY_SLOT        2

#define VARIABLES_NUM             32
#define VARIABLE_TYPE_REGISTER     1
#define VARIABLE_TYPE_STACK        2

#define VARLOC_NORMAL         1
#define VARLOC_CALL           2
#define VARLOC_SUBINPUT       3
#define VARLOC_SUBOUTPUT      4
#define VARLOC_PHI            5



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
  struct location  *location;
  struct location  *jumplocation;
  list   references;
  int    count;
  int    checked;
};

struct subroutine {
  struct code *code;
  struct prx_function *export;
  struct prx_function *import;

  struct location *begin;
  struct location *end;

  struct basicblock *startblock;
  struct basicblock *endblock;
  list   blocks, dfsblocks, revdfsblocks;

  list   loops;
  list   variables;

  int    haserror;
  int    dfscount;
};


enum basicblocktype {
  BLOCK_START,
  BLOCK_END,
  BLOCK_SIMPLE,
  BLOCK_CALL,
  BLOCK_SWITCH
};

struct basicblock {
  enum basicblocktype type;
  union {
    struct {
      struct location *begin;
      struct location *end;
      struct location *jumploc;
    } simple;
    struct {
      struct subroutine *calltarget;
    } call;
    struct {
      struct codeswitch *cswitch;
      int    switchnum;
    } sw;
  } val;

  struct subroutine *sub;

  int    dfsnum;
  struct basicblock *dominator;
  struct basicblock *parent;
  list   frontier;

  int    revdfsnum;
  struct basicblock *revdominator;
  struct basicblock *revparent;
  list   revfrontier;

  list   inrefs, outrefs;

  struct loopstruct *loop;
  int    mark1, mark2;
};

struct ifstruct {
  struct basicblock *begin;
  struct basicblock *end;
};

struct loopstruct {
  struct basicblock *start;
  struct loopstruct *parent;
  int   maxdfsnum;
  list  edges;
};

struct variable {
  int    type;
  int    num;

  list   uses;
  list   phiargs;
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
  fixedpool varspool;
  fixedpool ifspool;
  fixedpool loopspool;
};


struct code* code_analyse (struct prx *p);
void code_free (struct code *c);

int decode_instructions (struct code *c);
uint32 location_gpr_used (struct location *loc);
uint32 location_gpr_defined (struct location *loc);

void extract_switches (struct code *c);
void extract_subroutines (struct code *c);

void extract_cfg (struct subroutine *sub);

int cfg_dfs (struct subroutine *sub);
int cfg_revdfs (struct subroutine *sub);
struct basicblock *dom_intersect (struct basicblock *b1, struct basicblock *b2);
void cfg_dominance (struct subroutine *sub);
struct basicblock *dom_revintersect (struct basicblock *b1, struct basicblock *b2);
void cfg_revdominance (struct subroutine *sub);
void extract_loops (struct subroutine *sub);
void extract_ifs (struct subroutine *sub);

void build_ssa (struct subroutine *sub);


#endif /* __CODE_H */
