
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "output.h"
#include "allegrex.h"
#include "utils.h"

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
void print_subroutine (FILE *out, struct code *c, struct subroutine *sub)
{
  struct location *loc;
  fprintf (out, "/**\n * Subroutine at address 0x%08X\n", sub->location->address);
  fprintf (out, " */\n");
  if (sub->export) {
    if (sub->export->name) {
      fprintf (out, "void %s (void)\n", sub->export->name);
    } else {
      fprintf (out, "void nid_%08X (void)\n", sub->export->nid);
    }
  } else {
    fprintf (out, "void sub_%05X (void)\n", sub->location->address);
  }
  fprintf (out, "{\n");

  for (loc = sub->location; ; loc++) {
    fprintf (out, "  %s\n", allegrex_disassemble (loc->opc, loc->address, FALSE));
    if (loc == sub->end) break;
  }
  fprintf (out, "}\n\n");
}

static
void print_cfile (FILE *out, struct code *c, char *headerfilename)
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

int print_code (struct code *c, char *filename)
{
  char *temp;
  FILE *cout, *hout;
  int len;

  temp = strrchr (filename, '/');
  if (temp) filename = &temp[1];


  len = strlen (filename);
  if (len < 5) {
    error (__FILE__ ": invalid file name `%s'", filename);
    return 0;
  }

  filename[len - 2] = '\0';
  filename[len - 3] = 'c';

  cout = fopen (filename, "w");
  if (!cout) {
    xerror (__FILE__ ": can't open file for writing `%s'", filename);
    return 0;
  }

  filename[len - 3] = 'h';
  hout = fopen (filename, "w");
  if (!hout) {
    xerror (__FILE__ ": can't open file for writing `%s'", filename);
    return 0;
  }


  print_header (hout, c, filename);
  print_cfile (cout, c, filename);

  fclose (cout);
  fclose (hout);
  return 1;
}

