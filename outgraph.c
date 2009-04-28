
#include <stdio.h>

#include "output.h"
#include "utils.h"

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
    if (options & OUT_PRINT_DFS)  fprintf (out, "(%d) ", block->node.dfsnum);
    if (options & OUT_PRINT_RDFS) fprintf (out, "(%d) ", block->revnode.dfsnum);

    switch (block->type) {
    case BLOCK_START:  fprintf (out, "Start");   break;
    case BLOCK_END:    fprintf (out, "End");       break;
    case BLOCK_CALL:   fprintf (out, "Call");     break;
    case BLOCK_SWITCH: fprintf (out, "Switch"); break;
    case BLOCK_SIMPLE: fprintf (out, "0x%08X-0x%08X",
        block->info.simple.begin->address, block->info.simple.end->address);
    }
    fprintf (out, "\\l");

    if (options & OUT_PRINT_PHIS) {
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
              print_value (out, val);
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
              print_value (out, val);
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
    if (block->type == BLOCK_SIMPLE && (options & OUT_PRINT_CODE)) {
      struct location *loc;
      for (loc = block->info.simple.begin; ; loc++) {
        fprintf (out, "%s\\l", allegrex_disassemble (loc->opc, loc->address, FALSE));
        if (loc == block->info.simple.end) break;
      }
    }
    fprintf (out, "\"];\n");


    if (options & OUT_PRINT_DOMINATOR) {
      if (block->node.dominator && list_size (block->inrefs) > 1) {
        fprintf (out, "    %3d -> %3d [color=green];\n", block->node.dfsnum, block->node.dominator->node.dfsnum);
      }
    }

    if (options & OUT_PRINT_RDOMINATOR) {
      if (block->revnode.dominator && list_size (block->outrefs) > 1) {
        fprintf (out, "    %3d -> %3d [color=yellow];\n", block->node.dfsnum, block->revnode.dominator->node.dfsnum);
      }
    }

    if (list_size (block->node.frontier) != 0 && (options & OUT_PRINT_FRONTIER)) {
      fprintf (out, "    %3d -> { ", block->node.dfsnum);
      ref = list_head (block->node.frontier);
      while (ref) {
        struct basicblock *refblock = element_getvalue (ref);
        fprintf (out, "%3d ", refblock->node.dfsnum);
        ref = element_next (ref);
      }
      fprintf (out, " } [color=orange];\n");
    }

    if (list_size (block->revnode.frontier) != 0 && (options & OUT_PRINT_RFRONTIER)) {
      fprintf (out, "    %3d -> { ", block->node.dfsnum);
      ref = list_head (block->revnode.frontier);
      while (ref) {
        struct basicblock *refblock = element_getvalue (ref);
        fprintf (out, "%3d ", refblock->node.dfsnum);
        ref = element_next (ref);
      }
      fprintf (out, " } [color=blue];\n");
    }

    if (list_size (block->outrefs) != 0) {
      ref = list_head (block->outrefs);
      while (ref) {
        struct basicblock *refblock;
        refblock = element_getvalue (ref);
        fprintf (out, "    %3d -> %3d ", block->node.dfsnum, refblock->node.dfsnum);
        if (ref != list_head (block->outrefs))
          fprintf (out, "[arrowtail=dot]");

        if (refblock->node.parent == block) {
          fprintf (out, "[style=bold]");
        } else if (block->node.dfsnum >= refblock->node.dfsnum) {
          fprintf (out, "[color=red]");
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
  char buffer[64];
  char basename[32];
  element el;
  FILE *fp;
  int ret = 1;

  get_base_name (prxname, basename, sizeof (basename));

  el = list_head (c->subroutines);
  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (!sub->haserror && !sub->import) {
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
