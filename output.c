
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
void print_value (FILE *out, struct value *val)
{
  switch (val->type) {
  case VAL_CONSTANT: fprintf (out, "0x%08X", val->value); break;
  case VAL_VARIABLE:
    print_value (out, &val->variable->name);
    fprintf (out, "_%d", val->variable->count);
    break;
  case VAL_REGISTER:
    if (val->value == REGISTER_HI)      fprintf (out, "hi");
    else if (val->value == REGISTER_LO) fprintf (out, "lo");
    else fprintf (out, "%s", gpr_names[val->value]);
    break;
  default:
    fprintf (out, "UNK");
  }
}

static
void ident_line (FILE *out, int size)
{
  int i;
  for (i = 0; i < size; i++)
    fprintf (out, "  ");
}

static
void print_asm (FILE *out, int ident, struct operation *op)
{
  struct location *loc;
  element el;

  ident_line (out, ident);
  fprintf (out, "__asm__ (\n");
  for (loc = op->begin; ; loc++) {
    ident_line (out, ident);
    fprintf (out, "  \"%s\"\n", allegrex_disassemble (loc->opc, loc->address, FALSE));
    if (loc == op->end) break;
  }
  ident_line (out, ident);
  fprintf (out, "  : ");

  el = list_head (op->results);
  while (el) {
    struct value *val = element_getvalue (el);
    if (el != list_head (op->results))
      fprintf (out, ", ");
    fprintf (out, "\"=r\"(");
    print_value (out, val);
    fprintf (out, ")");
    el = element_next (el);
  }
  fprintf (out, "\n");

  ident_line (out, ident);
  fprintf (out, "  : ");
  el = list_head (op->results);
  while (el) {
    struct value *val = element_getvalue (el);
    if (el != list_head (op->results))
      fprintf (out, ", ");
    fprintf (out, "\"r\"(");
    print_value (out, val);
    fprintf (out, ")");
    el = element_next (el);
  }

  fprintf (out, "\n");
  ident_line (out, ident);
  fprintf (out, ");\n");
}

static
void print_binaryop (FILE *out, struct operation *op, const char *opsymbol)
{
  print_value (out, list_headvalue (op->results));
  fprintf (out, " = ");
  print_value (out, list_headvalue (op->operands));
  fprintf (out, " %s ", opsymbol);
  print_value (out, list_tailvalue (op->operands));
}

static
void print_compound (FILE *out, struct operation *op, const char *opsymbol)
{
  element el;

  if (list_size (op->results) != 0) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
  }
  fprintf (out, "%s (", opsymbol);
  el = list_head (op->operands);
  while (el) {
    if (el != list_head (op->operands))
      fprintf (out, ", ");
    print_value (out, element_getvalue (el));
    el = element_next (el);
  }
  fprintf (out, ")");
}

static
void print_ext (FILE *out, struct operation *op)
{
  struct value *val1, *val2, *val3;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val3->value);
  print_value (out, list_headvalue (op->results));
  fprintf (out, " = (");
  print_value (out, val1);
  fprintf (out, " >> %d)", val2->value);
  fprintf (out, " & 0x%08X", mask);
}

static
void print_ins (FILE *out, struct operation *op)
{
  struct value *val1, *val3, *val4;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el); el = element_next (el);
  val3 = element_getvalue (el); el = element_next (el);
  val4 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val4->value);
  print_value (out, list_headvalue (op->results));

  fprintf (out, " = (");
  print_value (out, list_headvalue (op->results));
  fprintf (out, " & 0x%08X) | (", ~(mask << val3->value));
  print_value (out, val1);
  fprintf (out, " & 0x%08X)", mask);
}

static
void print_nor (FILE *out, struct operation *op)
{
  struct value *val1, *val2;
  int simple = 0;

  val1 = list_headvalue (op->operands);
  val2 = list_headvalue (op->operands);

  if (val1->value == 0 || val2->value == 0) {
    simple = 1;
    if (val1->value == 0) val1 = val2;
  }

  print_value (out, list_headvalue (op->results));
  if (!simple) {
    fprintf (out, " = !(");
    print_value (out, val1);
    fprintf (out, " | ");
    print_value (out, val2);
    fprintf (out, ")");
  } else {
    fprintf (out, " = !");
    print_value (out, val1);
  }
}
static
void print_movnz (FILE *out, struct operation *op, int ismovn)
{
  struct value *val1, *val2;
  struct value *result;

  val1 = list_headvalue (op->operands);
  val2 = element_getvalue (element_next (list_head (op->operands)));
  result = list_headvalue (op->results);

  print_value (out, result);
  if (ismovn)
    fprintf (out, " = (");
  else
    fprintf (out, " = !(");
  print_value (out, val2);
  fprintf (out, ") ? ");
  print_value (out, val1);
  fprintf (out, " : ");
  print_value (out, result);
}


static
void print_instruction (FILE *out, int ident, struct operation *op)
{
  ident_line (out, ident);
  switch (op->insn) {
  case I_ADD:  print_binaryop (out, op, "+");   break;
  case I_ADDU: print_binaryop (out, op, "+");   break;
  case I_SUB:  print_binaryop (out, op, "-");   break;
  case I_SUBU: print_binaryop (out, op, "-");   break;
  case I_XOR:  print_binaryop (out, op, "^");   break;
  case I_AND:  print_binaryop (out, op, "&");   break;
  case I_OR:   print_binaryop (out, op, "|");   break;
  case I_SRAV: print_binaryop (out, op, ">>");  break;
  case I_SRLV: print_binaryop (out, op, ">>");  break;
  case I_SLLV: print_binaryop (out, op, "<<");  break;
  case I_INS:  print_ins (out, op);             break;
  case I_EXT:  print_ext (out, op);             break;
  case I_MIN:  print_compound (out, op, "MIN"); break;
  case I_MAX:  print_compound (out, op, "MAX"); break;
  case I_BITREV: print_compound (out, op, "BITREV"); break;
  case I_CLZ:  print_compound (out, op, "CLZ"); break;
  case I_CLO:  print_compound (out, op, "CLO"); break;
  case I_NOR:  print_nor (out, op);             break;
  case I_MOVN: print_movnz (out, op, TRUE);     break;
  case I_MOVZ: print_movnz (out, op, FALSE);    break;
  case I_LW:   print_compound (out, op, "LW");   break;
  case I_LB:   print_compound (out, op, "LB");   break;
  case I_LBU:  print_compound (out, op, "LBU");  break;
  case I_LH:   print_compound (out, op, "LH");   break;
  case I_LHU:  print_compound (out, op, "LHU");  break;
  case I_LL:   print_compound (out, op, "LL");   break;
  case I_LWL:  print_compound (out, op, "LWL");  break;
  case I_LWR:  print_compound (out, op, "LWR");  break;
  case I_SW:   print_compound (out, op, "SW");   break;
  case I_SH:   print_compound (out, op, "SH");   break;
  case I_SB:   print_compound (out, op, "SB");   break;
  case I_SC:   print_compound (out, op, "SC");   break;
  case I_SWL:  print_compound (out, op, "SWL");  break;
  case I_SWR:  print_compound (out, op, "SWR");  break;
  default:
    break;
  }
  fprintf (out, ";\n");
}

static
void print_block (FILE *out, int ident, struct basicblock *block)
{
  element el;

  if (block->type != BLOCK_SIMPLE) return;
  el = list_head (block->operations);
  while (el) {
    struct operation *op = element_getvalue (el);

    if (op->type == OP_ASM) {
      print_asm (out, ident, op);
    } else if (op->type == OP_INSTRUCTION) {
      print_instruction (out, ident, op);
    } else if (op->type == OP_MOVE) {
      ident_line (out, ident);
      print_value (out, list_headvalue (op->results));
      fprintf (out, " = ");
      print_value (out, list_headvalue (op->operands));
      fprintf (out, ";\n");
    } else if (op->type == OP_NOP) {
      ident_line (out, ident);
      fprintf (out, "nop ();\n");
    } else if (op->type == OP_PHI) {
      ident_line (out, ident);
      print_compound (out, op, "PHI");
      fprintf (out, ";\n");
    }

    el = element_next (el);
  }
}

static
void print_subroutine (FILE *out, struct code *c, struct subroutine *sub)
{
  element el;
  int ident;

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
    el = list_head (sub->dfsblocks);
    while (el) {
      struct basicblock *block = element_getvalue (el);
      ident = 1;
      print_block (out, ident, block);
      fprintf (out, "\n");
      el = element_next (el);
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
