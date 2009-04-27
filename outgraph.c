
#include <stdio.h>

#include "output.h"
#include "utils.h"

static
void print_subroutine_graph (FILE *out, struct code *c, struct subroutine *sub)
{
  struct basicblock *block;
  element el, ref;

  fprintf (out, "digraph sub_%05X {\n", sub->begin->address);
  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);

    fprintf (out, "    %3d ", block->node.dfsnum);
    fprintf (out, "[label=\"(%d) ", block->node.dfsnum);
    switch (block->type) {
    case BLOCK_START: fprintf (out, "Start");   break;
    case BLOCK_END: fprintf (out, "End");       break;
    case BLOCK_CALL: fprintf (out, "Call");     break;
    case BLOCK_SWITCH: fprintf (out, "Switch"); break;
    case BLOCK_SIMPLE: fprintf (out, "0x%08X", block->val.simple.begin->address);
    }
    ref = list_head (block->operations);
    while (ref) {
      element lel;
      struct operation *op = element_getvalue (ref);
      fprintf (out, "\\l<");
      lel = list_head (op->results);
      while (lel) {
        print_value (out, element_getvalue (lel));
        fprintf (out, " ");
        lel = element_next (lel);
      }
      fprintf (out, "> = ");
      if (op->type == OP_PHI) {
        fprintf (out, "PHI");
      }
      fprintf (out, "<");
      lel = list_head (op->operands);
      while (lel) {
        print_value (out, element_getvalue (lel));
        fprintf (out, " ");
        lel = element_next (lel);
      }
      fprintf (out, ">");
      ref = element_next (ref);
    }
    fprintf (out, "\"];\n");


    if (block->revnode.dominator && list_size (block->outrefs) > 1) {
      fprintf (out, "    %3d -> %3d [color=green];\n", block->node.dfsnum, block->revnode.dominator->node.dfsnum);
    }

    /*
    if (list_size (block->frontier) != 0) {
      fprintf (out, "    %3d -> { ", block->dfsnum);
      ref = list_head (block->frontier);
      while (ref) {
        struct basicblock *refblock = element_getvalue (ref);
        fprintf (out, "%3d ", refblock->dfsnum);
        ref = element_next (ref);
      }
      fprintf (out, " } [color=green];\n");
    }
    */

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


int print_graph (struct code *c, char *prxname)
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
        print_subroutine_graph (fp, c, sub);
        fclose (fp);
      }
    } else {
      if (sub->haserror) report ("Skipping subroutine at 0x%08X\n", sub->begin->address);
    }
    el = element_next (el);
  }

  return ret;
}
