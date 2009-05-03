
#include <stdio.h>

#include "output.h"
#include "utils.h"

static
void print_structures (FILE *out, struct basicblock *block)
{
  if (block->loopst) {
    fprintf (out, "LOOP start %d", block->loopst->start->node.dfsnum);
    if (block->loopst->end)
      fprintf (out, " end %d", block->loopst->end->node.dfsnum);
    fprintf (out, "\\l");
  }

  if (block->ifst) {
    fprintf (out, "IF");
    if (block->ifst->end)
      fprintf (out, " end %d", block->ifst->end->node.dfsnum);
    fprintf (out, "\\l");
  }
}

static
void print_block_code (FILE *out, struct basicblock *block)
{
  if (block->type == BLOCK_SIMPLE) {
    struct location *loc;
    for (loc = block->info.simple.begin; ; loc++) {
      fprintf (out, "%s\\l", allegrex_disassemble (loc->opc, loc->address, FALSE));
      if (loc == block->info.simple.end) break;
    }
  }
}

static
void print_block_phis (FILE *out, struct basicblock *block)
{
  element opsel, argel;
  opsel = list_head (block->operations);
  while (opsel) {
    struct operation *op = element_getvalue (opsel);
    int count1 = 0, count2 = 0;

    if (op->type != OP_START && op->type != OP_END) {
      argel = list_head (op->results);
      while (argel) {
        struct value *val = element_getvalue (argel);
        if (val->type != VAL_CONSTANT) {
          if (count1++ == 0) fprintf (out, "<");
          print_value (out, val, FALSE);
          fprintf (out, " ");
        }
        argel = element_next (argel);
      }
      if (count1 > 0) fprintf (out, "> = ");

      if (op->type == OP_PHI) {
        fprintf (out, "PHI");
      }
      argel = list_head (op->operands);
      while (argel) {
        struct value *val = element_getvalue (argel);
        if (val->type != VAL_CONSTANT) {
          if (count2++ == 0) fprintf (out, "<");
          print_value (out, val, FALSE);
          fprintf (out, " ");
        }
        argel = element_next (argel);
      }
      if (count2 > 0) fprintf (out, ">");
      if (count1 || count2) fprintf (out, "\\l");
    }
    opsel = element_next (opsel);
  }
}

static
void print_dominator (FILE *out, struct basicblock *block, int reverse, const char *color)
{
  struct basicblock *dominator;

  if (reverse) {
    if (list_size (block->outrefs) <= 1) return;
    dominator = element_getvalue (block->revnode.dominator->block);
  } else {
    if (list_size (block->inrefs) <= 1) return;
    dominator = element_getvalue (block->node.dominator->block);
  }
  fprintf (out, "    %3d -> %3d [color=%s];\n",
      block->node.dfsnum, dominator->node.dfsnum, color);
}

static
void print_frontier (FILE *out, struct basicblock *block, list frontier, const char *color)
{
  element ref;
  if (list_size (frontier) == 0) return;

  fprintf (out, "    %3d -> { ", block->node.dfsnum);
  ref = list_head (frontier);
  while (ref) {
    struct basicblocknode *refnode;
    struct basicblock *refblock;

    refnode = element_getvalue (ref);
    refblock = element_getvalue (refnode->block);

    fprintf (out, "%3d ", refblock->node.dfsnum);
    ref = element_next (ref);
  }
  fprintf (out, " } [color=%s];\n", color);
}

static
void print_subroutine_graph (FILE *out, struct code *c, struct subroutine *sub, int options)
{
  struct basicblock *block;
  element el, ref;

  fprintf (out, "digraph ");
  print_subroutine_name (out, sub);
  fprintf (out, " {\n    rankdir=LR;\n");

  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);

    fprintf (out, "    %3d ", block->node.dfsnum);
    fprintf (out, "[label=\"");

    if (options & OUT_PRINT_STRUCTURES)
      print_structures (out, block);

    if (options & OUT_PRINT_DFS)
      fprintf (out, "(%d) ", block->node.dfsnum);

    if (options & OUT_PRINT_RDFS)
      fprintf (out, "(%d) ", block->revnode.dfsnum);

    switch (block->type) {
    case BLOCK_START:  fprintf (out, "Start");   break;
    case BLOCK_END:    fprintf (out, "End");       break;
    case BLOCK_CALL:   fprintf (out, "Call");     break;
    case BLOCK_SIMPLE: fprintf (out, "0x%08X-0x%08X",
        block->info.simple.begin->address, block->info.simple.end->address);
    }
    if (block->haslabel) fprintf (out, "(*)");
    fprintf (out, "\\l");

    if (options & OUT_PRINT_PHIS)
      print_block_phis (out, block);

    if (options & OUT_PRINT_CODE)
      print_block_code (out, block);

    fprintf (out, "\"];\n");


    if (options & OUT_PRINT_DOMINATOR)
      print_dominator (out, block, FALSE, "green");

    if (options & OUT_PRINT_RDOMINATOR)
      print_dominator (out, block, TRUE, "yellow");

    if (options & OUT_PRINT_FRONTIER)
      print_frontier (out, block, block->node.frontier, "orange");

    if (options & OUT_PRINT_RFRONTIER)
      print_frontier (out, block, block->revnode.frontier, "blue");


    if (list_size (block->outrefs) != 0) {
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicedge *edge;
        struct basicblock *refblock;
        edge = element_getvalue (ref);
        refblock = edge->to;
        fprintf (out, "    %3d -> %3d ", block->node.dfsnum, refblock->node.dfsnum);
        if (ref != list_head (block->outrefs))
          fprintf (out, "[arrowtail=dot]");

        if (element_getvalue (refblock->node.parent->block) == block) {
          fprintf (out, "[style=bold]");
        } else if (block->node.dfsnum >= refblock->node.dfsnum) {
          fprintf (out, "[color=red]");
        }
        if (options & OUT_PRINT_EDGE_TYPES) {
          fprintf (out, "[label=\"");
          switch (edge->type) {
          case EDGE_UNKNOWN:  fprintf (out, "UNK");      break;
          case EDGE_CONTINUE: fprintf (out, "CONTINUE"); break;
          case EDGE_BREAK:    fprintf (out, "BREAK");    break;
          case EDGE_FOLLOW:   fprintf (out, "FOLLOW");   break;
          case EDGE_GOTO:     fprintf (out, "GOTO");     break;
          case EDGE_IFEXIT:   fprintf (out, "IFEXIT");   break;
          }
          fprintf (out, "\"]");
        }
        fprintf (out, " ;\n");
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }
  fprintf (out, "}\n");
}


int print_graph (struct code *c, char *prxname, int options)
{
  char buffer[128];
  char basename[32];
  element el;
  FILE *fp;
  int ret = 1;

  get_base_name (prxname, basename, sizeof (basename));

  el = list_head (c->subroutines);
  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (!sub->haserror && !sub->import) {
      if (sub->export) {
        if (sub->export->name) {
          sprintf (buffer, "%s_%-.64s.dot", basename, sub->export->name);
        } else
          sprintf (buffer, "%s_nid_%08X.dot", basename, sub->export->nid);
      } else
        sprintf (buffer, "%s_%08X.dot", basename, sub->begin->address);
      fp = fopen (buffer, "w");
      if (!fp) {
        xerror (__FILE__ ": can't open file for writing `%s'", buffer);
        ret = 0;
      } else {
        print_subroutine_graph (fp, c, sub, options);
        fclose (fp);
      }
    } else {
      if (sub->haserror) report ("Skipping subroutine at 0x%08X\n", sub->begin->address);
    }
    el = element_next (el);
  }

  return ret;
}
