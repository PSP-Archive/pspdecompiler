#ifndef __CODE_H
#define __CODE_H

#include "prx.h"
#include "allegrex.h"
#include "alloc.h"
#include "lists.h"
#include "types.h"

/* Possible reachable status */
enum locationreachable {
  LOCATION_UNREACHABLE = 0,  /* Location is not reachable by any means */
  LOCATION_REACHABLE,        /* Location is reachable by some path */
  LOCATION_DELAY_SLOT        /* Location is a delay slot of a reachable branch/jump */
};

/* If an error was detected one a location */
enum locationerror {
  ERROR_NONE = 0,              /* No error */
  ERROR_INVALID_OPCODE,        /* Opcode is not recognized */
  ERROR_DELAY_SLOT,            /* Branch/jump inside a delay slot */
  ERROR_TARGET_OUTSIDE_FILE,   /* Branch/jump target outside the code */
  ERROR_ILLEGAL_BRANCH         /* Branch with a condition that can never occur, such as `bne  $0, $0, target' */
};

/* Represents a location in the code */
struct location {
  uint32 opc;                                /* The opcode (little-endian) */
  uint32 address;                            /* The virtual address of the location */

  const struct allegrex_instruction *insn;   /* The decoded instruction or null (illegal opcode) */
  struct location *target;                   /* A possible target of a branch/jump */

  list references;                           /* Number of references to this target inside the same subroutine */
  int  branchalways;                         /* True if this location is a branch that always occurs */
  enum locationreachable reachable;          /* Reachable status */
  enum locationerror  error;                 /* Error status */

  struct subroutine *sub;                    /* Owner subroutine */
  struct basicblock *block;                  /* Basic block mark (used when extracting basic blocks) */
  struct codeswitch *cswitch;                /* Code switch mark */
};

/* Represents a switch in the code */
struct codeswitch {
  struct prx_reloc *jumpreloc;
  struct prx_reloc *switchreloc;
  struct location  *location;       /* The location that loads the base address of the switch */
  struct location  *jumplocation;   /* The location of the jump instruction */
  list   references;                /* A list of possible target locations (without repeating) */
  int    count;                     /* How many possible targets this switch have */
  int    checked;                   /* Is this switch valid? */
};

/* Subroutine decompilation status */
#define SUBROUTINE_EXTRACTED          1
#define SUBROUTINE_CFG_EXTRACTED      2

/* A subroutine */
struct subroutine {
  struct code *code;                /* The owner code of this subroutine */
  struct prx_function *export;      /* Is this a function export? */
  struct prx_function *import;      /* Is this a function import? */

  struct location *begin;           /* Where the subroutine begins */
  struct location *end;             /* Where the subroutine ends */

  struct basicblock *startblock;    /* Points to the first basic block of this subroutine */
  struct basicblock *endblock;      /* Points to the last basic block of this subroutine */
  list   blocks;                    /* A list of the basic blocks of this subroutine */
  list   dfsblocks, revdfsblocks;   /* Blocks ordered in DFS and Reverse-DFS order */

  list   whereused;                 /* A list of basic blocks calling this subroutine */
  list   variables;

  uint32 stacksize;
  int    numregargs;

  int    haserror, status;          /* Subroutine decompilation status */
  int    temp;
};

/* Represents a pair of integers */
struct intpair {
  int first, last;
};


/* Abstract node in DFS and DOM trees (or reverse DFS and DOM trees) */
struct basicblocknode {
  int dfsnum;                        /* The Depth-First search number */
  struct intpair domdfsnum;          /* To determine ancestry information in the dominator tree */
  struct basicblocknode *dominator;  /* The dominator node */
  struct basicblocknode *parent;     /* The parent node (in the depth-first search) */
  element blockel;                   /* An element inside the list (dfsblocks or revdfsblocks) */
  list children;                     /* Children in the DFS tree */
  list domchildren;                  /* Children in the dominator tree */
  list frontier;                     /* The dominator frontier */
};

/* The type of the basic block */
enum basicblocktype {
  BLOCK_START = 0,      /* The first basic block in a subroutine */
  BLOCK_SIMPLE,         /* A simple block */
  BLOCK_CALL,           /* A block that represents a call */
  BLOCK_END             /* The last basic block */
};

/* The basic block */
struct basicblock {
  enum basicblocktype type;                /* The type of the basic block */
  element blockel;                         /* An element inside the list sub->blocks */
  union {
    struct {
      struct location *begin;              /* The start of the simple block */
      struct location *end;                /* The end of the simple block */
      struct location *jumploc;            /* The jump/branch location inside the block */
    } simple;
    struct {
      struct subroutine *calltarget;       /* The target of the call */
    } call;
  } info;

  uint32 reg_gen[2], reg_kill[2];
  uint32 reg_live_in[2], reg_live_out[2];
  list   operations;
  struct subroutine *sub;                  /* The owner subroutine */

  struct basicblocknode node;              /* Node info for DFS and DOM trees */
  struct basicblocknode revnode;           /* Node info for the reverse DFS and DOM trees */

  list   inrefs, outrefs;                  /* A list of in- and out-edges of this block */

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
  element fromel, toel;
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


/* Represents the entire PRX code */
struct code {
  struct prx *file;        /* The PRX file */

  uint32 baddr, numopc;    /* The code segment base address and number of opcodes */
  struct location *base;   /* The code segment start */
  struct location *end;    /* The code segment end */

  list subroutines;        /* The list of subroutines */

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
void make_graph (struct subroutine *sub, int reverse);

int dom_isancestor (struct basicblocknode *ancestor, struct basicblocknode *node);
struct basicblocknode *dom_common (struct basicblocknode *n1, struct basicblocknode *n2);

void reset_marks (struct subroutine *sub);
void extract_structures (struct subroutine *sub);

void build_ssa (struct subroutine *sub);
void extract_variables (struct subroutine *sub);


#endif /* __CODE_H */
