
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

void ident_line (FILE *out, int size)
{
  int i;
  for (i = 0; i < size; i++)
    fprintf (out, "  ");
}


void print_subroutine_name (FILE *out, struct subroutine *sub)
{
  if (sub->export) {
    if (sub->export->name) {
      fprintf (out, "%s", sub->export->name);
    } else {
      fprintf (out, "%s_%08X", sub->export->libname, sub->export->nid);
    }
  } else if (sub->import) {
    if (sub->import->name) {
      fprintf (out, "%s", sub->import->name);
    } else {
      fprintf (out, "%s_%08X", sub->import->libname, sub->import->nid);
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
    switch (val->val.variable->type) {
    case VARIABLE_ARGUMENT:
      fprintf (out, "arg%d", val->val.variable->name.val.intval);
      break;
    case VARIABLE_LOCAL:
      fprintf (out, "local%d", val->val.variable->info);
      break;
    case VARIABLE_CONSTANT:
      fprintf (out, "0x%08X", val->val.variable->info);
      break;
    case VARIABLE_TEMP:
      fprintf (out, "(");
      print_operation (out, val->val.variable->def, 0, TRUE);
      fprintf (out, ")");
      break;
    default:
      print_value (out, &val->val.variable->name);
      fprintf (out, "/* Invalid block %d %d */",
          val->val.variable->def->block->node.dfsnum,
          val->val.variable->def->type);
      break;
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
void print_asm_reglist (FILE *out, list regs, int identsize, int options)
{
  element el;
  ident_line (out, identsize);
  fprintf (out, "  : ");

  el = list_head (regs);
  while (el) {
    struct value *val = element_getvalue (el);
    if (el != list_head (regs))
      fprintf (out, ", ");
    fprintf (out, "\"=r\"(");
    print_value (out, val);
    fprintf (out, ")");
    el = element_next (el);
  }
  fprintf (out, "\n");
}

static
void print_asm (FILE *out, struct operation *op, int identsize, int options)
{
  struct location *loc;

  ident_line (out, identsize);
  fprintf (out, "__asm__ (\n");
  for (loc = op->begin; ; loc++) {
    ident_line (out, identsize);
    fprintf (out, "  \"%s\"\n", allegrex_disassemble (loc->opc, loc->address, FALSE));
    if (loc == op->end) break;
  }
  if (list_size (op->results) != 0 || list_size (op->operands) != 0) {
    print_asm_reglist (out, op->results, identsize, options);
    if (list_size (op->operands) != 0) {
      print_asm_reglist (out, op->operands, identsize, options);
    }
  }

  ident_line (out, identsize);
  fprintf (out, ");\n");
}

static
void print_binaryop (FILE *out, struct operation *op, const char *opsymbol, int options)
{
  if (!(options & OPTS_DEFERRED)) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
  }
  print_value (out, list_headvalue (op->operands));
  fprintf (out, " %s ", opsymbol);
  print_value (out, list_tailvalue (op->operands));
}

void print_complexop (FILE *out, struct operation *op, const char *opsymbol, int options)
{
  element el;

  if (list_size (op->results) != 0 && !(options & OPTS_DEFERRED)) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
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
void print_ext (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2, *val3;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val3->val.intval);
  if (!(options & OPTS_DEFERRED)) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
  }

  fprintf (out, "(");
  print_value (out, val1);
  fprintf (out, " >> %d)", val2->val.intval);
  fprintf (out, " & 0x%08X", mask);
}

static
void print_ins (FILE *out, struct operation *op, int options)
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
  if (!(options & OPTS_DEFERRED)) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
  }

  fprintf (out, "(");
  print_value (out, val2);
  fprintf (out, " & 0x%08X) | (", ~(mask << val3->val.intval));
  print_value (out, val1);
  fprintf (out, " & 0x%08X)", mask);
}

static
void print_nor (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2;
  int simple = 0;

  val1 = list_headvalue (op->operands);
  val2 = list_tailvalue (op->operands);

  if (val1->val.intval == 0 || val2->val.intval == 0) {
    simple = 1;
    if (val1->val.intval == 0) val1 = val2;
  }

  if (!(options & OPTS_DEFERRED)) {
    print_value (out, list_headvalue (op->results));
    fprintf (out, " = ");
  }

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
void print_movnz (FILE *out, struct operation *op, int ismovn, int options)
{
  struct value *val1, *val2, *val3;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);
  result = list_headvalue (op->results);

  if (!(options & OPTS_DEFERRED)) {
    print_value (out, result);
    fprintf (out, " = ");
  }

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

static
void print_slt (FILE *out, struct operation *op, int isunsigned, int options)
{
  struct value *val1, *val2;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el);
  result = list_headvalue (op->results);

  if (!(options & OPTS_DEFERRED)) {
    print_value (out, result);
    fprintf (out, " = ");
  }

  fprintf (out, "(");

  print_value (out, val1);
  fprintf (out, " < ");
  print_value (out, val2);
  fprintf (out, ")");
}

static
void print_condition (FILE *out, struct operation *op, int options)
{
  fprintf (out, "if (");
  if (options & OPTS_REVERSECOND) fprintf (out, "!(");
  print_value (out, list_headvalue (op->operands));
  switch (op->insn) {
  case I_BNE:
    fprintf (out, " != ");
    break;
  case I_BEQ:
    fprintf (out, " == ");
    break;
  case I_BGEZ:
  case I_BGEZAL:
    fprintf (out, " >= 0");
    break;
  case I_BGTZ:
    fprintf (out, " > 0");
    break;
  case I_BLEZ:
    fprintf (out, " <= 0");
    break;
  case I_BLTZ:
  case I_BLTZAL:
    fprintf (out, " < 0");
    break;
  default:
    break;
  }
  if (list_size (op->operands) == 2)
    print_value (out, list_tailvalue (op->operands));

  if (options & OPTS_REVERSECOND) fprintf (out, ")");
  fprintf (out, ")");
}



void print_operation (FILE *out, struct operation *op, int identsize, int options)
{
  int nosemicolon = FALSE;

  if (op->type == OP_ASM) {
    print_asm (out, op, identsize, options);
    return;
  }

  if (op->type == OP_INSTRUCTION) {
    if (op->begin->insn->flags & (INSN_JUMP))
      return;
  } else if (op->type == OP_NOP || op->type == OP_START ||
             op->type == OP_END || op->type == OP_PHI) {
    return;
  }

  ident_line (out, identsize);
  if (op->type == OP_INSTRUCTION) {
    switch (op->insn) {
    case I_ADD:  print_binaryop (out, op, "+", options);     break;
    case I_ADDU: print_binaryop (out, op, "+", options);     break;
    case I_SUB:  print_binaryop (out, op, "-", options);     break;
    case I_SUBU: print_binaryop (out, op, "-", options);     break;
    case I_XOR:  print_binaryop (out, op, "^", options);     break;
    case I_AND:  print_binaryop (out, op, "&", options);     break;
    case I_OR:   print_binaryop (out, op, "|", options);     break;
    case I_SRAV: print_binaryop (out, op, ">>", options);    break;
    case I_SRLV: print_binaryop (out, op, ">>", options);    break;
    case I_SLLV: print_binaryop (out, op, "<<", options);    break;
    case I_INS:  print_ins (out, op, options);               break;
    case I_EXT:  print_ext (out, op, options);               break;
    case I_MIN:  print_complexop (out, op, "MIN", options);  break;
    case I_MAX:  print_complexop (out, op, "MAX", options);  break;
    case I_BITREV: print_complexop (out, op, "BITREV", options); break;
    case I_CLZ:  print_complexop (out, op, "CLZ", options);  break;
    case I_CLO:  print_complexop (out, op, "CLO", options);  break;
    case I_NOR:  print_nor (out, op, options);               break;
    case I_MOVN: print_movnz (out, op, TRUE, options);       break;
    case I_MOVZ: print_movnz (out, op, FALSE, options);      break;
    case I_SLT:  print_slt (out, op, FALSE, options);        break;
    case I_SLTU: print_slt (out, op, TRUE, options);         break;
    case I_LW:   print_complexop (out, op, "LW", options);   break;
    case I_LB:   print_complexop (out, op, "LB", options);   break;
    case I_LBU:  print_complexop (out, op, "LBU", options);  break;
    case I_LH:   print_complexop (out, op, "LH", options);   break;
    case I_LHU:  print_complexop (out, op, "LHU", options);  break;
    case I_LL:   print_complexop (out, op, "LL", options);   break;
    case I_LWL:  print_complexop (out, op, "LWL", options);  break;
    case I_LWR:  print_complexop (out, op, "LWR", options);  break;
    case I_SW:   print_complexop (out, op, "SW", options);   break;
    case I_SH:   print_complexop (out, op, "SH", options);   break;
    case I_SB:   print_complexop (out, op, "SB", options);   break;
    case I_SC:   print_complexop (out, op, "SC", options);   break;
    case I_SWL:  print_complexop (out, op, "SWL", options);  break;
    case I_SWR:  print_complexop (out, op, "SWR", options);  break;
    default:
      if (op->begin->insn->flags & INSN_BRANCH) {
        print_condition (out, op, options);
        nosemicolon = TRUE;
      }
      break;
    }
  } else if (op->type == OP_MOVE) {
    if (!options) {
      print_value (out, list_headvalue (op->results));
      fprintf (out, " = ");
    }
    print_value (out, list_headvalue (op->operands));
  } else if (op->type == OP_CALL) {
    if (op->block->info.call.calltarget) {
      print_subroutine_name (out, op->block->info.call.calltarget);
    } else {
      fprintf (out, "CALL");
    }
    fprintf (out, " ()");
  }

  if (!(options & OPTS_DEFERRED)) {
    if (nosemicolon) fprintf (out, "\n");
    else fprintf (out, ";\n");
  }
}



