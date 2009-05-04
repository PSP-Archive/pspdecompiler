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

  list   wherecalled;
  list   variables;

  uint32 stacksize;
  int    numregargs;

  int    haserror;
  int    temp;
};


struct intpair {
  int first;
  int last;
};

struct basicblocknode {
  int dfsnum;
  struct intpair domdfs;
  struct basicblocknode *dominator;
  struct basicblocknode *parent;
  element block;
  list children;
  list domchildren;
  list frontier;
};

enum basicblocktype {
  BLOCK_START = 0,
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

  struct loopstructure *loopst;
  struct ifstructure *ifst;

  int    haslabel, identsize;

  int    mark1, mark2;
};

enum edgetype {
  EDGE_UNKNOWN = 0,
  EDGE_GOTO,
  EDGE_CONTINUE,
  EDGE_BREAK,
  EDGE_FOLLOW,
  EDGE_IFEXIT
};

struct basicedge {
  enum edgetype type;
  struct basicblock *from, *to;
  int fromnum, tonum;
};

#define REGISTER_LINK 31
#define REGISTER_LO   32
#define REGISTER_HI   33
#define NUM_REGISTERS 34

enum valuetype {
  VAL_CONSTANT = 0,
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
  VARIABLE_UNK = 0,
  VARIABLE_LOCAL,
  VARIABLE_ARGUMENT,
  VARIABLE_TEMP,
  VARIABLE_CONSTANT,
  VARIABLE_CONSTANTUNK,
  VARIABLE_INVALID
};

struct variable {
  struct value name;
  enum variabletype type;
  uint32 info;

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

  int  deferred;

  list results;
  list operands;
};

enum looptype {
  LOOP_WHILE,
  LOOP_REPEAT,
  LOOP_FOR
};

struct loopstructure {
  enum looptype type;
  struct basicblock *start;
  struct basicblock *end;
  int    hasendgoto;
  list  edges;
};

struct ifstructure {
  struct basicblock *end;
  int    outermost;
  int    hasendgoto;
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
  fixedpool loopspool;
  fixedpool ifspool;
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

int dom_isancestor (struct basicblocknode *ancestor, struct basicblocknode *node);
struct basicblocknode *dom_common (struct basicblocknode *n1, struct basicblocknode *n2);
void cfg_dominance (struct subroutine *sub, int reverse);
void cfg_frontier (struct subroutine *sub, int reverse);

void reset_marks (struct subroutine *sub);
void extract_structures (struct subroutine *sub);

void build_ssa (struct subroutine *sub);
void extract_variables (struct subroutine *sub);


#endif /* __CODE_H */
