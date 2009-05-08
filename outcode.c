
#include <stdio.h>
#include <ctype.h>

#include "output.h"
#include "utils.h"


static
void print_block (FILE *out, struct basicblock *block, int reversecond)
{
  element opel;
  struct operation *jumpop = NULL;
  int options = 0;

  if (reversecond) options |= OPTS_REVERSECOND;
  opel = list_head (block->operations);
  while (opel) {
    struct operation *op = element_getvalue (opel);
    if (!(op->status & OPERATION_DEFERRED)) {
      if (op->type == OP_INSTRUCTION) {
        if (op->info.iop.loc->insn->flags & (INSN_JUMP | INSN_BRANCH))
          jumpop = op;
      }
      if (op != jumpop)
        print_operation (out, op, block->identsize + 1, options);
    }
    opel = element_next (opel);
  }
  if (jumpop)
    print_operation (out, jumpop, block->identsize + 1, options);
}

static
void print_block_recursive (FILE *out, struct basicblock *block, int verbosity)
{
  struct basicedge *edge;
  int reversecond = FALSE, haselse = FALSE;
  element ref;

  block->mark1 = 1;

  if (block->haslabel) {
    fprintf (out, "\n");
    ident_line (out, block->identsize);
    fprintf (out, "label%d:\n", block->node.dfsnum);
  }

  if (block->loopst) {
    if (block->loopst->start == block) {
      ident_line (out, block->identsize);
      fprintf (out, "loop {\n");
    }
  }

  if (verbosity > 2) {
    ident_line (out, block->identsize + 1);
    fprintf (out, "/* Block %d ", block->node.dfsnum);
    if (block->type == BLOCK_SIMPLE) {
      fprintf (out, "Address 0x%08X ", block->info.simple.begin->address);
    }
    fprintf (out, "*/\n");
  }

  if (block->st) {
    struct basicedge *edge1, *edge2;

    edge1 = list_headvalue (block->outrefs);
    edge2 = list_tailvalue (block->outrefs);

    if (edge1->type != EDGE_IFEXIT &&
        edge2->type != EDGE_IFEXIT) {
      haselse = TRUE;
    } else {
      if (edge2->type == EDGE_IFEXIT)
        reversecond = TRUE;
    }
  }

  print_block (out, block, reversecond);


  if (reversecond)
    ref = list_tail (block->outrefs);
  else
    ref = list_head (block->outrefs);

  while (ref) {
    edge = element_getvalue (ref);

    if (edge->type == EDGE_FOLLOW) {
      if (block->st) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "{\n");
      }
      print_block_recursive (out, edge->to, verbosity);
      if (block->st) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "}\n");
      }
    } else if (edge->type != EDGE_IFEXIT) {
      int identsize = block->identsize + 1;
      if (block->st) identsize++;
      ident_line (out, identsize);
      switch (edge->type) {
      case EDGE_GOTO:     fprintf (out, "goto label%d;\n", edge->to->node.dfsnum); break;
      case EDGE_BREAK:    fprintf (out, "break;\n"); break;
      case EDGE_CONTINUE: fprintf (out, "continue;\n"); break;
      default:
        break;
      }
    }

    if (reversecond)
      ref = element_previous (ref);
    else
      ref = element_next (ref);

    if (ref && haselse) {
      ident_line (out, block->identsize + 1);
      fprintf (out, "else\n");
    }
  }


  if (block->st) {
    if (block->st->end && block->st->info.ifctrl.isoutermost) {
      if (block->st->hasendgoto) {
        ident_line (out, block->identsize + 1);
        fprintf (out, "goto label%d;\n", block->st->end->node.dfsnum);
      } else {
        print_block_recursive (out, block->st->end, verbosity);
      }
    }
  }

  if (block->loopst) {
    if (block->loopst->start == block) {
      ident_line (out, block->identsize);
      fprintf (out, "}\n");
      if (block->loopst->end) {
        if (block->loopst->hasendgoto) {
          ident_line (out, block->identsize);
          fprintf (out, "goto label%d;\n", block->loopst->end->node.dfsnum);
        } else {
          print_block_recursive (out, block->loopst->end, verbosity);
        }
      }
    }
  }
}

static
void print_subroutine (FILE *out, struct subroutine *sub, int verbosity)
{
  if (sub->import) { return; }

  fprintf (out, "/**\n * Subroutine at address 0x%08X\n", sub->begin->address);
  if (verbosity > 1 && !sub->haserror) {
    struct location *loc = sub->begin;
    for (loc = sub->begin; ; loc++) {
      fprintf (out, " * %s\n", allegrex_disassemble (loc->opc, loc->address, TRUE));
      if (loc == sub->end) break;
    }
  }
  fprintf (out, " */\n");
  print_subroutine_declaration (out, sub);
  fprintf (out, "\n{\n");

  if (sub->haserror) {
    struct location *loc;
    for (loc = sub->begin; ; loc++) {
      fprintf (out, "%s\n", allegrex_disassemble (loc->opc, loc->address, TRUE));
      if (loc == sub->end) break;
    }
  } else {
    element el;
    reset_marks (sub);

    el = list_head (sub->blocks);
    while (el) {
      struct basicblock *block = element_getvalue (el);
      if (!block->mark1)
        print_block_recursive (out, block, verbosity);
      el = element_next (el);
    }
  }
  fprintf (out, "}\n\n");
}

static
void print_source (FILE *out, struct code *c, char *headerfilename, int verbosity)
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
      if (func->pfunc) {
        fprintf (out, "extern ");
        print_subroutine_declaration (out, func->pfunc);
        fprintf (out, ";\n");
      }
    }
    fprintf (out, "\n");
  }

  el = list_head (c->subroutines);
  while (el) {
    struct subroutine *sub;
    sub = element_getvalue (el);

    print_subroutine (out, sub, verbosity);
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


int print_code (struct code *c, char *prxname, int verbosity)
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
  print_source (cout, c, buffer, verbosity);

  fclose (cout);
  fclose (hout);
  return 1;
}
