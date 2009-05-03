
#include <stdio.h>
#include <ctype.h>

#include "output.h"
#include "utils.h"

void ident_line (FILE *out, int size)
{
  int i;
  for (i = 0; i < size; i++)
    fprintf (out, "  ");
}

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)

static
void print_condition (FILE *out, struct location *loc, int reverse)
{
  fprintf (out, "if (");
  if (reverse) fprintf (out, "!(");
  switch (loc->insn->insn) {
  case I_BNE:
  case I_BNEL:
    fprintf (out, "%s != %s", gpr_names[RS (loc->opc)], gpr_names[RT (loc->opc)]);
    break;
  case I_BEQ:
  case I_BEQL:
    fprintf (out, "%s == %s", gpr_names[RS (loc->opc)], gpr_names[RT (loc->opc)]);
    break;
  case I_BGEZ:
  case I_BGEZAL:
  case I_BGEZL:
    fprintf (out, "%s >= 0", gpr_names[RS (loc->opc)]);
    break;
  case I_BGTZ:
  case I_BGTZL:
    fprintf (out, "%s > 0", gpr_names[RS (loc->opc)]);
    break;
  case I_BLEZ:
  case I_BLEZL:
    fprintf (out, "%s <= 0", gpr_names[RS (loc->opc)]);
    break;
  case I_BLTZ:
  case I_BLTZAL:
  case I_BLTZALL:
  case I_BLTZL:
    fprintf (out, "%s < 0", gpr_names[RS (loc->opc)]);
    break;
  default:
    break;
  }
  if (reverse) fprintf (out, ")");
  fprintf (out, ")\n");
}


static
void print_block (FILE *out, struct basicblock *block)
{
  element opel;
  opel = list_head (block->operations);
  while (opel) {
    struct operation *op = element_getvalue (opel);
    print_operation (out, op, block->identsize + 1);
    opel = element_next (opel);
  }
}

static
void print_block_recursive (FILE *out, struct basicblock *block)
{
  struct basicedge *edge;
  int reversecond = FALSE, haselse = FALSE;
  element ref;

  block->mark1 = 1;
  block->sub->temp++;

  if (block->haslabel) {
    fprintf (out, "\n");
    ident_line (out, block->identsize);
    fprintf (out, "label%d:\n", block->node.dfsnum);
  }

  if (block->loopst) {
    if (block->loopst->start == block) {
      fprintf (out, "\n");
      ident_line (out, block->identsize);
      fprintf (out, "loop {\n");
    }
  }

  print_block (out, block);

  if (block->ifst) {
    struct basicedge *edge1, *edge2;

    edge1 = list_headvalue (block->outrefs);
    edge2 = list_tailvalue (block->outrefs);

    if (edge1->type == EDGE_FOLLOW &&
        edge2->type == EDGE_FOLLOW) {
      haselse = TRUE;
    } else {
      if (edge1->type == EDGE_FOLLOW)
        reversecond = TRUE;
    }

    fprintf (out, "\n");
    ident_line (out, block->identsize + 1);
    print_condition (out, block->info.simple.jumploc, reversecond);
  }

  ref = list_head (block->outrefs);
  while (ref) {
    edge = element_getvalue (ref);

    if (edge->type == EDGE_FOLLOW) {
      if (block->ifst) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "{\n");
      }
      print_block_recursive (out, edge->to);
      if (block->ifst) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "}\n");
      }
    } else if (edge->type != EDGE_IFEXIT) {
      int identsize = block->identsize + 1;
      if (block->ifst) {
        if (ref == list_head (block->outrefs))
          identsize++;
      }
      ident_line (out, identsize);
      switch (edge->type) {
      case EDGE_GOTO:     fprintf (out, "goto label%d;\n", edge->to->node.dfsnum); break;
      case EDGE_BREAK:    fprintf (out, "break;\n"); break;
      case EDGE_CONTINUE: fprintf (out, "continue;\n"); break;
      default:
        break;
      }
    }

    ref = element_next (ref);

    if (ref && haselse) {
      ident_line (out, block->identsize + 1);
      fprintf (out, "else\n");
    }
  }


  if (block->ifst) {
    if (block->ifst->end && block->ifst->outermost) {
      if (block->ifst->hasendgoto) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "goto label%d;\n", block->ifst->end->node.dfsnum);
      } else {
        print_block_recursive (out, block->ifst->end);
      }
    }
  }

  if (block->loopst) {
    if (block->loopst->start == block) {
      ident_line (out, block->identsize);
      fprintf (out, "}\n\n");
      if (block->loopst->end) {
        if (block->loopst->hasendgoto) {
          ident_line (out, block->identsize + 1);
          fprintf (out, "goto label%d;\n", block->loopst->end->node.dfsnum);
        } else {
          print_block_recursive (out, block->loopst->end);
        }
      }
    }
  }
}

static
void print_subroutine (FILE *out, struct code *c, struct subroutine *sub)
{
  if (sub->import) return;

  fprintf (out, "/**\n * Subroutine at address 0x%08X\n", sub->begin->address);
  fprintf (out, " */\n");
  fprintf (out, "void ");
  print_subroutine_name (out, sub);
  fprintf (out, " (void)\n{\n");

  if (sub->haserror) {
    struct location *loc;
    for (loc = sub->begin; ; loc++) {
      fprintf (out, "%s\n", allegrex_disassemble (loc->opc, loc->address, TRUE));
      if (loc == sub->end) break;
    }
  } else {
    element el;
    reset_marks (sub);

    sub->temp = 0;
    el = list_head (sub->blocks);
    while (sub->temp < list_size (sub->blocks)) {
      struct basicblock *block = element_getvalue (el);
      while (el && block->mark1) {
        el = element_next (el);
        block = element_getvalue (el);
      }
      if (!el) break;
      print_block_recursive (out, block);
    }
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
