
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "output.h"
#include "allegrex.h"
#include "utils.h"

static
void get_base_name (char *filename, char *basename, size_t len)
{
  char *temp;

  temp = strrchr (filename, '/');
  if (temp) filename = &temp[1];

  strncpy (basename, filename, len - 1);
  basename[len - 1] = '\0';
  temp = strchr (basename, '.');
  if (temp) *temp = '\0';
}


static
void print_header (FILE *out, struct code *c, char *headerfilename)
{
  uint32 i, j;
  char buffer[256];
  int pos = 0;

  while (pos < sizeof (buffer) - 1) {
    char c = headerfilename[pos];
    if (!c) break;
    if (c == '.') c = '_';
    else c = toupper (c);
    buffer[pos++] =  c;
  }
  buffer[pos] = '\0';

  fprintf (out, "#ifndef __%s\n", buffer);
  fprintf (out, "#define __%s\n\n", buffer);

  for (i = 0; i < c->file->modinfo->numexports; i++) {
    struct prx_export *exp = &c->file->modinfo->exports[i];

    fprintf (out, "/*\n * Exports from library: %s\n */\n", exp->name);
    for (j = 0; j < exp->nfuncs; j++) {
      struct prx_function *func = &exp->funcs[j];
      if (func->name) {
        fprintf (out, "void %s (void);\n",func->name);
      } else {
        fprintf (out, "void %s_%08X (void);\n", exp->name, func->nid);
      }
    }
    fprintf (out, "\n");
  }

  fprintf (out, "#endif /* __%s */\n", buffer);
}

static
void print_subroutine_name (FILE *out, struct subroutine *sub)
{
  if (sub->export) {
    if (sub->export->name) {
      fprintf (out, "%s", sub->export->name);
    } else {
      fprintf (out, "nid_%08X", sub->export->nid);
    }
  } else {
    fprintf (out, "sub_%05X", sub->begin->address);
  }
}

static
void print_block (FILE *out, struct subroutine *sub, struct basicblock *block)
{

}

static
void print_subroutine (FILE *out, struct code *c, struct subroutine *sub)
{
  struct location *loc;
  int unreach = FALSE;

  fprintf (out, "/**\n * Subroutine at address 0x%08X\n", sub->begin->address);
  fprintf (out, " */\n");
  fprintf (out, "void ");
  print_subroutine_name (out, sub);
  fprintf (out, " (void)\n{\n");

  for (loc = sub->begin; ; loc++) {
    element el;
    if (loc->reachable) {
      unreach = FALSE;
      if (loc->references) {
        fprintf (out, "\n;  Refs:   ");
        el = list_head (loc->references);
        while (el) {
          fprintf (out, "0x%08X ", ((struct location *) element_getvalue (el))->address);
          el = element_next (el);
        }
        fprintf (out, "\n");
      }
      fprintf (out, "  %s ", allegrex_disassemble (loc->opc, loc->address, TRUE));
      if (loc->target) {
        if (loc->target->sub != loc->sub) {
          fprintf (out, "(");
          print_subroutine_name (out, loc->target->sub);
          fprintf (out, ") ");
        }
      }
      if (loc->cswitch) {
        if (loc->cswitch->jumplocation == loc) {
          fprintf (out, " (switch locations: ");
          el = list_head (loc->cswitch->references);
          while (el) {
            fprintf (out, "0x%08X ", ((struct location *) element_getvalue (el))->address);
            el = element_next (el);
          }
          fprintf (out, ") ");
        }
      }
      fprintf (out, "\n");
    } else {

      if (!unreach) {
        fprintf (out, "\n/*  Unreachable code */\n");
      }
      fprintf (out, "  %s\n", allegrex_disassemble (loc->opc, loc->address, TRUE));
      unreach = TRUE;
    }
    if (loc == sub->end) break;
  }
  fprintf (out, "}\n\n");
}

static
void print_source (FILE *out, struct code *c, char *headerfilename)
{
  uint32 i, j;
  element el;

  fprintf (out, "#include <pspsdk.h>\n");
  fprintf (out, "#include \"%s\"\n\n", headerfilename);

  for (i = 0; i < c->file->modinfo->numimports; i++) {
    struct prx_import *imp = &c->file->modinfo->imports[i];

    fprintf (out, "/*\n * Imports from library: %s\n */\n", imp->name);
    for (j = 0; j < imp->nfuncs; j++) {
      struct prx_function *func = &imp->funcs[j];
      fprintf (out, "extern ");
      if (func->name) {
        fprintf (out, "void %s (void);\n",func->name);
      } else {
        fprintf (out, "void %s_%08X (void);\n", imp->name, func->nid);
      }
    }
    fprintf (out, "\n");
  }

  el = list_head (c->subroutines);
  while (el) {
    struct subroutine *sub;
    sub = element_getvalue (el);

    print_subroutine (out, c, sub);
    el = element_next (el);
  }

}

int print_code (struct code *c, char *prxname)
{
  char buffer[64];
  char basename[32];
  FILE *cout, *hout;


  get_base_name (prxname, basename, sizeof (basename));
  sprintf (buffer, "%s.c", basename);

  cout = fopen (buffer, "w");
  if (!cout) {
    xerror (__FILE__ ": can't open file for writing `%s'", buffer);
    return 0;
  }

  sprintf (buffer, "%s.h", basename);
  hout = fopen (buffer, "w");
  if (!hout) {
    xerror (__FILE__ ": can't open file for writing `%s'", buffer);
    return 0;
  }


  print_header (hout, c, buffer);
  print_source (cout, c, buffer);

  fclose (cout);
  fclose (hout);
  return 1;
}



static
void print_subroutine_graph (FILE *out, struct code *c, struct subroutine *sub)
{
  struct basicedge *edge;
  struct basicblock *block;
  element el, ref;

  fprintf (out, "digraph sub_%05X {\n", sub->begin->address);
  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);

    fprintf (out, "    %3d ", block->dfsnum);
    fprintf (out, "[label=\"%d - %d\"];\n", block->dfsnum, block->loop ? block->loop->start->dfsnum : 0);
    /*if (block->begin) {
      struct location *loc;
      fprintf (out, "[label=\"0x%08X:\\l", block->begin->address);
      for (loc = block->begin; ; loc++) {
        fprintf (out, "%s\\l", allegrex_disassemble (loc->opc, loc->address, 0));
        if (loc == block->end) break;
      }
      fprintf (out, "\"]");
    }
    fprintf (out, ";\n");*/


    if (block->revdominator && list_size (block->outrefs) > 1) {
      fprintf (out, "    %3d -> %3d [color=green];\n", block->dfsnum, block->revdominator->dfsnum);
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
        edge = element_getvalue (ref);
        refblock = edge->to;
        fprintf (out, "    %3d -> %3d ", block->dfsnum, refblock->dfsnum);
        if (edge->hascall) {
          fprintf (out, "[label=\"Call ");
          if (edge->calltarget) {
            fprintf (out, "0x%08X", edge->calltarget->begin->address);
          }
          fprintf (out, "\"]");
        }
        if (ref != list_head (block->outrefs))
          fprintf (out, "[arrowtail=dot]");

        if (refblock->parent == block) {
          fprintf (out, "[style=bold]");
        } else if (block->dfsnum >= refblock->dfsnum) {
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
