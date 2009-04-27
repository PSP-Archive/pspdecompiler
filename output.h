
#ifndef __OUTPUT_H
#define __OUTPUT_H

#include <stddef.h>
#include "code.h"

void get_base_name (char *filename, char *basename, size_t len);
void print_value (FILE *out, struct value *val);
void print_subroutine_name (FILE *out, struct subroutine *sub);

int print_code (struct code *c, char *filename);
int print_graph (struct code *c, char *prxname);

#endif /* __OUTPUT_H */
