#ifndef __NIDS_H
#define __NIDS_H

#include "hash.h"

void nids_free (struct hashtable *nids);
struct hashtable *nids_load_xml (const char *path);
void nids_print (struct hashtable *nids);

#endif /* __NIDS_H */
