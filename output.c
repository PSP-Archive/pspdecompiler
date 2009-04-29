
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

void print_value (FILE *out, struct value *val, int printtemps)
{
  switch (val->type) {
  case VAL_CONSTANT: fprintf (out, "0x%08X", val->val.intval); break;
  case VAL_VARIABLE:
    if (val->val.variable->type == VARIABLE_TEMP) {
      if (printtemps) {
        fprintf (out, "(");
        print_operation (out, val->val.variable->def, FALSE);
        fprintf (out, ")");
      } else fprintf (out, "temp");
    } else if (val->val.variable->type == VARIABLE_INVALID) {
      fprintf (out, "invalid");
    } else {
      if (val->val.variable->type == VARIABLE_ARGUMENT)
        fprintf (out, "arg");
      else
        fprintf (out, "local");
      fprintf (out, "%d", val->val.variable->varnum);
    }
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
void print_binaryop (FILE *out, struct operation *op, const char *opsymbol, int printoutput)
{
  if (printoutput) {
    print_value (out, list_headvalue (op->results), FALSE);
    fprintf (out, " = ");
  }
  print_value (out, list_headvalue (op->operands), TRUE);
  fprintf (out, " %s ", opsymbol);
  print_value (out, list_tailvalue (op->operands), TRUE);
}

void print_complexop (FILE *out, struct operation *op, const char *opsymbol, int printoutput)
{
  element el;

  if (printoutput) {
    if (list_size (op->results) != 0) {
      if (op->type == OP_CALL) {
        el = list_head (op->results);
        el = element_next (el);
        print_value (out, element_getvalue (el), FALSE);
        fprintf (out, ", ");
        el = element_next (el);
        print_value (out, element_getvalue (el), FALSE);
        fprintf (out, " = ");
      } else {
        print_value (out, list_headvalue (op->results), FALSE);
        fprintf (out, " = ");
      }
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
    print_value (out, val, TRUE);
    el = element_next (el);
  }
  fprintf (out, ")");
}

static
void print_ext (FILE *out, struct operation *op, int printoutput)
{
  struct value *val1, *val2, *val3;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val3->val.intval);
  if (printoutput) {
    print_value (out, list_headvalue (op->results), FALSE);
    fprintf (out, " = ");
  }
  fprintf (out, "(");
  print_value (out, val1, TRUE);
  fprintf (out, " >> %d)", val2->val.intval);
  fprintf (out, " & 0x%08X", mask);
}

static
void print_ins (FILE *out, struct operation *op, int printoutput)
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
  if (printoutput) {
    print_value (out, list_headvalue (op->results), FALSE);
    fprintf (out, " = ");
  }
  fprintf (out, "(");
  print_value (out, val2, TRUE);
  fprintf (out, " & 0x%08X) | (", ~(mask << val3->val.intval));
  print_value (out, val1, TRUE);
  fprintf (out, " & 0x%08X)", mask);
}

static
void print_nor (FILE *out, struct operation *op, int printoutput)
{
  struct value *val1, *val2;
  int simple = 0;

  val1 = list_headvalue (op->operands);
  val2 = list_tailvalue (op->operands);

  if (val1->val.intval == 0 || val2->val.intval == 0) {
    simple = 1;
    if (val1->val.intval == 0) val1 = val2;
  }

  if (printoutput) {
    print_value (out, list_headvalue (op->results), FALSE);
    fprintf (out, " = ");
  }

  if (!simple) {
    fprintf (out, "!(");
    print_value (out, val1, TRUE);
    fprintf (out, " | ");
    print_value (out, val2, TRUE);
    fprintf (out, ")");
  } else {
    fprintf (out, "!");
    print_value (out, val1, TRUE);
  }
}
static
void print_movnz (FILE *out, struct operation *op, int ismovn, int printoutput)
{
  struct value *val1, *val2, *val3;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);
  result = list_headvalue (op->results);

  if (printoutput) {
    print_value (out, result, FALSE);
    fprintf (out, " = ");
  }

  if (ismovn)
    fprintf (out, "(");
  else
    fprintf (out, "!(");
  print_value (out, val2, TRUE);
  fprintf (out, ") ? ");
  print_value (out, val1, TRUE);
  fprintf (out, " : ");
  print_value (out, val3, TRUE);
}


void print_operation (FILE *out, struct operation *op, int printoutput)
{
  if (op->type == OP_ASM) {
  } else if (op->type == OP_INSTRUCTION) {
    switch (op->insn) {
    case I_ADD:  print_binaryop (out, op, "+", printoutput);   break;
    case I_ADDU: print_binaryop (out, op, "+", printoutput);   break;
    case I_SUB:  print_binaryop (out, op, "-", printoutput);   break;
    case I_SUBU: print_binaryop (out, op, "-", printoutput);   break;
    case I_XOR:  print_binaryop (out, op, "^", printoutput);   break;
    case I_AND:  print_binaryop (out, op, "&", printoutput);   break;
    case I_OR:   print_binaryop (out, op, "|", printoutput);   break;
    case I_SRAV: print_binaryop (out, op, ">>", printoutput);  break;
    case I_SRLV: print_binaryop (out, op, ">>", printoutput);  break;
    case I_SLLV: print_binaryop (out, op, "<<", printoutput);  break;
    case I_INS:  print_ins (out, op, printoutput);             break;
    case I_EXT:  print_ext (out, op, printoutput);             break;
    case I_MIN:  print_complexop (out, op, "MIN", printoutput); break;
    case I_MAX:  print_complexop (out, op, "MAX", printoutput); break;
    case I_BITREV: print_complexop (out, op, "BITREV", printoutput); break;
    case I_CLZ:  print_complexop (out, op, "CLZ", printoutput); break;
    case I_CLO:  print_complexop (out, op, "CLO", printoutput); break;
    case I_NOR:  print_nor (out, op, printoutput);             break;
    case I_MOVN: print_movnz (out, op, TRUE, printoutput);     break;
    case I_MOVZ: print_movnz (out, op, FALSE, printoutput);    break;
    case I_LW:   print_complexop (out, op, "LW", printoutput);   break;
    case I_LB:   print_complexop (out, op, "LB", printoutput);   break;
    case I_LBU:  print_complexop (out, op, "LBU", printoutput);  break;
    case I_LH:   print_complexop (out, op, "LH", printoutput);   break;
    case I_LHU:  print_complexop (out, op, "LHU", printoutput);  break;
    case I_LL:   print_complexop (out, op, "LL", printoutput);   break;
    case I_LWL:  print_complexop (out, op, "LWL", printoutput);  break;
    case I_LWR:  print_complexop (out, op, "LWR", printoutput);  break;
    case I_SW:   print_complexop (out, op, "SW", printoutput);   break;
    case I_SH:   print_complexop (out, op, "SH", printoutput);   break;
    case I_SB:   print_complexop (out, op, "SB", printoutput);   break;
    case I_SC:   print_complexop (out, op, "SC", printoutput);   break;
    case I_SWL:  print_complexop (out, op, "SWL", printoutput);  break;
    case I_SWR:  print_complexop (out, op, "SWR", printoutput);  break;
    default:
      break;
    }
  } else if (op->type == OP_MOVE) {
    if (printoutput) {
      print_value (out, list_headvalue (op->results), FALSE);
      fprintf (out, " = ");
    }
    print_value (out, list_headvalue (op->operands), TRUE);
  } else if (op->type == OP_NOP) {
    fprintf (out, "nop ()");
  } else if (op->type == OP_PHI) {
    print_complexop (out, op, "PHI", printoutput);
  } else if (op->type == OP_CALL) {
    print_complexop (out, op, "CALL", printoutput);
  } else if (op->type == OP_START) {
    print_complexop (out, op, "START", printoutput);
  }

  if (printoutput)
    fprintf (out, ";\n");
}



