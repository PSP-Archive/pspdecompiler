#ifndef __ANALYSER_H
#define __ANALYSER_H

#include "code.h"

struct code* analyse_code (struct prx *p);
void free_code (struct code *c);

#endif /* __ANALYSER_H */
