
#ifndef __OUTPUT_H
#define __OUTPUT_H

#include <stddef.h>
#include "code.h"

#define OUT_PRINT_DFS          1
#define OUT_PRINT_RDFS         2
#define OUT_PRINT_DOMINATOR    4
#define OUT_PRINT_RDOMINATOR   8
#define OUT_PRINT_FRONTIER    16
#define OUT_PRINT_RFRONTIER   32
#define OUT_PRINT_PHIS        64
#define OUT_PRINT_CODE       128
#define OUT_PRINT_STRUCTURES 256
#define OUT_PRINT_EDGE_TYPES 512

#define OPTS_DEFERRED          1
#define OPTS_REVERSECOND       2


void ident_line (FILE *out, int size);
void get_base_name (char *filename, char *basename, size_t len);
void print_value (FILE *out, struct value *val);
void print_operation (FILE *out, struct operation *op, int identsize, int options);
void print_complexop (FILE *out, struct operation *op, const char *opsymbol, int options);
void print_subroutine_name (FILE *out, struct subroutine *sub);
void print_subroutine_declaration (FILE *out, struct subroutine *sub);

int print_code (struct code *c, char *filename, int verbosity);
int print_graph (struct code *c, char *prxname, int options);

#endif /* __OUTPUT_H */
