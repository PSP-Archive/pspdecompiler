
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
    fprintf (out, "_%d", val->val.variable->varnum);
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



