
#include <stdio.h>
#include <ctype.h>

#include "output.h"
#include "utils.h"

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
void print_complexop (FILE *out, struct operation *op, const char *opsymbol)
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

  mask = 0xFFFFFFFF >> (32 - val3->val.intval);
  print_value (out, list_headvalue (op->results));
  fprintf (out, " = (");
  print_value (out, val1);
  fprintf (out, " >> %d)", val2->val.intval);
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

  mask = 0xFFFFFFFF >> (32 - val4->val.intval);
  print_value (out, list_headvalue (op->results));

  fprintf (out, " = (");
  print_value (out, list_headvalue (op->results));
  fprintf (out, " & 0x%08X) | (", ~(mask << val3->val.intval));
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

  if (val1->val.intval == 0 || val2->val.intval == 0) {
    simple = 1;
    if (val1->val.intval == 0) val1 = val2;
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
  case I_MIN:  print_complexop (out, op, "MIN"); break;
  case I_MAX:  print_complexop (out, op, "MAX"); break;
  case I_BITREV: print_complexop (out, op, "BITREV"); break;
  case I_CLZ:  print_complexop (out, op, "CLZ"); break;
  case I_CLO:  print_complexop (out, op, "CLO"); break;
  case I_NOR:  print_nor (out, op);             break;
  case I_MOVN: print_movnz (out, op, TRUE);     break;
  case I_MOVZ: print_movnz (out, op, FALSE);    break;
  case I_LW:   print_complexop (out, op, "LW");   break;
  case I_LB:   print_complexop (out, op, "LB");   break;
  case I_LBU:  print_complexop (out, op, "LBU");  break;
  case I_LH:   print_complexop (out, op, "LH");   break;
  case I_LHU:  print_complexop (out, op, "LHU");  break;
  case I_LL:   print_complexop (out, op, "LL");   break;
  case I_LWL:  print_complexop (out, op, "LWL");  break;
  case I_LWR:  print_complexop (out, op, "LWR");  break;
  case I_SW:   print_complexop (out, op, "SW");   break;
  case I_SH:   print_complexop (out, op, "SH");   break;
  case I_SB:   print_complexop (out, op, "SB");   break;
  case I_SC:   print_complexop (out, op, "SC");   break;
  case I_SWL:  print_complexop (out, op, "SWL");  break;
  case I_SWR:  print_complexop (out, op, "SWR");  break;
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
      print_complexop (out, op, "PHI");
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
