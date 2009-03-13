#ifndef __HASH_H
#define __HASH_H

#include <stddef.h>

struct hashtable;

typedef unsigned int (*hashfunction) (void *key);
typedef int (*equalsfunction) (void *key1, void *key2);
typedef void (*traversefunction) (void *key, void *value);

struct hashtable *hashtable_create (unsigned int size, hashfunction hashfn, equalsfunction eqfn);
void hashtable_destroy (struct hashtable *ht, traversefunction destroyfn);
void hashtable_destroy_all (struct hashtable *ht);

unsigned int hashtable_count (struct hashtable *ht);
void hashtable_insert (struct hashtable *ht, void *key, void *value);
void *hashtable_search (struct hashtable *ht, void *key, void **key_found);
void *hashtable_remove (struct hashtable *ht, void *key, void **key_found);
void hashtable_traverse (struct hashtable *ht, traversefunction traversefn);

int hashtable_pointer_compare (void *key1, void *key2);
int hashtable_string_compare (void *key1, void *key2);

unsigned int hash_bytes (unsigned char *key, size_t len);
unsigned int hash_string (char *key);

#endif /* __HASH_H */
