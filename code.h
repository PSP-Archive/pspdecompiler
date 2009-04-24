#ifndef __CODE_H
#define __CODE_H

#include "prx.h"
#include "allegrex.h"
#include "alloc.h"
#include "lists.h"
#include "types.h"

enum locationreachable {
  LOCATION_UNREACHABLE = 0,
  LOCATION_REACHABLE,
  LOCATION_DELAY_SLOT
};

enum locationerror {
  ERROR_NONE = 0,
  ERROR_INVALID_OPCODE,
  ERROR_DELAY_SLOT,
  ERROR_TARGET_OUTSIDE_FILE,
  ERROR_DELAY_SLOT_TARGET,
  ERROR_ILLEGAL_BRANCH
};

struct location {
  uint32 opc;
  uint32 address;

  const struct allegrex_instruction *insn;
  struct location *target;

  list references;
  int  branchalways;
  enum locationreachable reachable;
  enum locationerror  error;

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


struct basicblocknode {
  int dfsnum;
  struct basicblock *dominator;
  struct basicblock *parent;
  list children;
  list frontier;
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

  list   operations;
  struct subroutine *sub;

  struct basicblocknode node;
  struct basicblocknode revnode;

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

enum variabletype {
  VARIABLE_REGISTER,
  VARIABLE_STACK
};

#define REGISTER_LO   32
#define REGISTER_HI   33

struct variable {
  enum variabletype type;
  int    num;

  struct operation *def;
  list   uses;
};

enum valuetype {
  VAL_CONSTANT,
  VAL_REGISTER,
  VAL_STACKBASE,
  VAL_RETURNADDRESS,
  VAL_VOID
};

struct value {
  enum valuetype type;
  uint32        value;
};

enum operationtype {
  OP_CALL,
  OP_INSTRUCTION,
  OP_MOVE,
  OP_ASM,
  OP_NOP,
  OP_PHI
};

struct operation {
  enum operationtype type;
  enum allegrex_insn insn;
  struct location *begin;
  struct location *end;

  list results;
  list operands;
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
  fixedpool opspool;
  fixedpool valspool;
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

int cfg_dfs (struct subroutine *sub, int reverse);
struct basicblock *dom_intersect (struct basicblock *b1, struct basicblock *b2, int reverse);
void cfg_dominance (struct subroutine *sub, int reverse);
void cfg_frontier (struct subroutine *sub, int reverse);

void extract_loops (struct subroutine *sub);
void extract_ifs (struct subroutine *sub);

void build_ssa (struct subroutine *sub);


#endif /* __CODE_H */
