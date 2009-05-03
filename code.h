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

  list   scopes;
  list   variables;

  int    haserror;
  int    temp;
};


struct basicblocknode {
  int dfsnum;
  struct basicblocknode *dominator;
  struct basicblocknode *parent;
  element block;
  list children;
  list domchildren;
  list frontier;
};

enum basicblocktype {
  BLOCK_START,
  BLOCK_END,
  BLOCK_SIMPLE,
  BLOCK_CALL
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
  } info;

  uint32 reg_gen[2], reg_kill[2];
  uint32 reg_live_in[2], reg_live_out[2];
  list   operations;
  struct subroutine *sub;

  struct basicblocknode node;
  struct basicblocknode revnode;

  list   inrefs, outrefs;

  struct scope *sc;
  int    haslabel;
  int    mark1, mark2;
};

struct basicedge {
  struct basicblock *from, *to;
  int fromnum, tonum;
};

#define REGISTER_LINK 31
#define REGISTER_LO   32
#define REGISTER_HI   33
#define NUM_REGISTERS 34

enum valuetype {
  VAL_CONSTANT,
  VAL_REGISTER,
  VAL_VARIABLE
};

struct value {
  enum valuetype type;
  union {
    uint32 intval;
    struct variable *variable;
  } val;
};


enum variabletype {
  VARIABLE_LOCAL,
  VARIABLE_ARGUMENT,
  VARIABLE_TEMP,
  VARIABLE_INVALID
};

struct variable {
  struct value name;
  enum variabletype type;
  int varnum;

  struct operation *def;
  list   uses;
};

enum operationtype {
  OP_START,
  OP_END,
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
  struct basicblock *block;

  int  flushed;

  list results;
  list operands;
};

enum scopetype {
  SCOPE_MAIN,
  SCOPE_LOOP,
  SCOPE_IF
};

struct scope {
  enum scopetype type;
  struct scope *parent;
  struct scope *breakparent;

  struct basicblock *start;
  int   dfsnum, depth;
  list  children;

  union {
    struct {
      list  edges;
    } loop;
    struct {
      struct basicblock *end;
    } branch;
  } info;
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
  fixedpool opspool;
  fixedpool valspool;
  fixedpool scopespool;
};


struct code* code_analyse (struct prx *p);
void code_free (struct code *c);

int decode_instructions (struct code *c);
uint32 location_gpr_used (struct location *loc);
uint32 location_gpr_defined (struct location *loc);
int location_branch_may_swap (struct location *branch);

void extract_switches (struct code *c);
void extract_subroutines (struct code *c);

void extract_cfg (struct subroutine *sub);

int cfg_dfs (struct subroutine *sub, int reverse);

int dom_isdominator (struct basicblocknode *n1, struct basicblocknode *n2);
struct basicblocknode *dom_intersect (struct basicblocknode *n1, struct basicblocknode *n2);
void cfg_dominance (struct subroutine *sub, int reverse);
void cfg_frontier (struct subroutine *sub, int reverse);

void extract_scopes (struct subroutine *sub);

void build_ssa (struct subroutine *sub);
void extract_variables (struct subroutine *sub);


#endif /* __CODE_H */
