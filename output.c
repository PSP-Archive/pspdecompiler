
#include <stdio.h>
#include <string.h>

#include "output.h"
#include "allegrex.h"
#include "utils.h"

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

void print_value (FILE *out, struct value *val)
{
  switch (val->type) {
  case VAL_CONSTANT: fprintf (out, "0x%08X", val->val.intval); break;
  case VAL_VARIABLE:
    print_value (out, &val->val.variable->name);
    break;
  case VAL_REGISTER:
    if (val->val.intval == REGISTER_HI)      fprintf (out, "hi");
    else if (val->val.intval == REGISTER_LO) fprintf (out, "lo");
    else fprintf (out, "%s", gpr_names[val->val.intval]);
    break;
  default:
    fprintf (out, "UNK");
  }
}


static
void print_asm (FILE *out, struct operation *op, int identsize)
{
  struct location *loc;
  element el;

  ident_line (out, identsize);
  fprintf (out, "__asm__ (\n");
  for (loc = op->begin; ; loc++) {
    ident_line (out, identsize);
    fprintf (out, "  \"%s\"\n", allegrex_disassemble (loc->opc, loc->address, FALSE));
    if (loc == op->end) break;
  }
  ident_line (out, identsize);
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

  ident_line (out, identsize);
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
  ident_line (out, identsize);
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

void print_complexop (FILE *out, struct operation *op, const char *opsymbol)
{
  element el;

  if (list_size (op->results) != 0) {
    if (op->type == OP_CALL) {
      el = list_head (op->results);
      el = element_next (el);
      print_value (out, element_getvalue (el));
      fprintf (out, ", ");
      el = element_next (el);
      print_value (out, element_getvalue (el));
      fprintf (out, " = ");
    } else {
      print_value (out, list_headvalue (op->results));
      fprintf (out, " = ");
    }
  }

  fprintf (out, "%s (", opsymbol);
  el = list_head (op->operands);
  while (el) {
    struct value *val;
    val = element_getvalue (el);
    if (val->type == VAL_VARIABLE) {
      if (val->val.variable->type == VARIABLE_INVALID) break;
    }
    if (el != list_head (op->operands))
      fprintf (out, ", ");
    print_value (out, val);
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
  fprintf (out, " = ");

  fprintf (out, "(");
  print_value (out, val1);
  fprintf (out, " >> %d)", val2->val.intval);
  fprintf (out, " & 0x%08X", mask);
}

static
void print_ins (FILE *out, struct operation *op)
{
  struct value *val1, *val2, *val3, *val4;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el); el = element_next (el);
  val4 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val4->val.intval);
  print_value (out, list_headvalue (op->results));
  fprintf (out, " = ");

  fprintf (out, "(");
  print_value (out, val2);
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
  val2 = list_tailvalue (op->operands);

  if (val1->val.intval == 0 || val2->val.intval == 0) {
    simple = 1;
    if (val1->val.intval == 0) val1 = val2;
  }

  print_value (out, list_headvalue (op->results));
  fprintf (out, " = ");

  if (!simple) {
    fprintf (out, "!(");
    print_value (out, val1);
    fprintf (out, " | ");
    print_value (out, val2);
    fprintf (out, ")");
  } else {
    fprintf (out, "!");
    print_value (out, val1);
  }
}
static
void print_movnz (FILE *out, struct operation *op, int ismovn)
{
  struct value *val1, *val2, *val3;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);
  result = list_headvalue (op->results);

  print_value (out, result);
  fprintf (out, " = ");

  if (ismovn)
    fprintf (out, "(");
  else
    fprintf (out, "!(");
  print_value (out, val2);
  fprintf (out, ") ? ");
  print_value (out, val1);
  fprintf (out, " : ");
  print_value (out, val3);
}


void print_operation (FILE *out, struct operation *op, int identsize)
{
  if (op->type == OP_ASM) {
    print_asm (out, op, identsize);
    return;
  }

  ident_line (out, identsize);
  if (op->type == OP_INSTRUCTION) {
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
  } else if (op->type == OP_MOVE) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
    print_value (out, list_headvalue (op->operands));
  } else if (op->type == OP_NOP) {
    fprintf (out, "nop ()");
  } else if (op->type == OP_PHI) {
    print_complexop (out, op, "PHI");
  } else if (op->type == OP_CALL) {
    print_complexop (out, op, "CALL");
  } else if (op->type == OP_START) {
    print_complexop (out, op, "START");
  }

  fprintf (out, ";\n");
}



