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

#define VARDEF_TYPE_LOCATION       1
#define VARDEF_TYPE_CALL           2
#define VARDEF_TYPE_SUBINPUT       3
#define VARDEF_TYPE_SUBOUTPUT      4
#define VARDEF_TYPE_PHI            5



struct location {
  uint32 opc;
  uint32 address;

  const struct allegrex_instruction *insn;
  struct location *target;

  list references;
  int  branchalways;
  int  reachable;
  int  error;

  list   usedvars;
  list   definedvars;

  struct subroutine *sub;
  struct basicblock *block;
  struct codeswitch *cswitch;
};

struct codeswitch {
  struct prx_reloc *jumpreloc;
  struct prx_reloc *switchreloc;
  struct location *location;
  struct location *jumplocation;
  list   references;
  int    count;
  int    checked;
};

struct subroutine {
  struct prx_function *export;
  struct prx_function *import;

  struct location *begin;
  struct location *end;

  struct basicblock *endblock;
  list   blocks, dfsblocks, revdfsblocks;

  list   loops;
  list   variables;

  int    haserror;
  int    dfscount;
};

struct vardef {
  int    type;
  union {
    struct basicedge *edge;
    struct subroutine *sub;
    struct location *loc;
    list   phiargs;
  } value;
};

struct varuse {
  int    type;
  union {
    struct basicedge *edge;
    struct subroutine *sub;
    struct location *loc;
    struct vardef *def;
  } value;
};

struct variable {
  int    type;
  int    num;

  struct vardef def;
  list   uses;
};

struct basicblock {
  struct location *begin;
  struct location *end;

  struct location *jumploc;

  struct basicblock *dominator;
  struct basicblock *parent;
  list   frontier;

  struct basicblock *revdominator;
  struct basicblock *revparent;

  list   outrefs, inrefs;
  int    dfsnum, revdfsnum;

  struct loopstruct *loop;
  int    mark1, mark2;
};

struct basicedge {
  struct basicblock *from, *to;

  struct subroutine *calltarget;
  int    hascall;

  struct codeswitch *cswitch;
  int    switchnum;
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
  fixedpool edgespool;
  fixedpool varspool;
  fixedpool ifpool;
  fixedpool looppool;
};


struct code* code_analyse (struct prx *p);
void code_free (struct code *c);

int decode_instructions (struct code *c);
uint32 location_gpr_used (struct location *loc);
uint32 location_gpr_defined (struct location *loc);

void extract_switches (struct code *c);
void extract_subroutines (struct code *c);

void extract_cfg (struct code *c, struct subroutine *sub);

int cfg_dfs (struct subroutine *sub);
int cfg_revdfs (struct subroutine *sub);
struct basicblock *dom_intersect (struct basicblock *b1, struct basicblock *b2);
void cfg_dominance (struct subroutine *sub);
struct basicblock *dom_revintersect (struct basicblock *b1, struct basicblock *b2);
void cfg_revdominance (struct subroutine *sub);
void extract_loops (struct code *c, struct subroutine *sub);


void build_ssa (struct code *c, struct subroutine *sub);


#endif /* __CODE_H */
