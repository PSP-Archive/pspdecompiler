#ifndef __HASH_H
#define __HASH_H

#include <stddef.h>

struct hashtable;

typedef unsigned int (*hashfunction) (void *key);
typedef int (*equalsfunction) (void *key1, void *key2, unsigned int hash);
typedef void (*traversefunction) (void *key, void *value, unsigned int hash, void *arg);

struct hashtable *hashtable_create (unsigned int size, hashfunction hashfn, equalsfunction eqfn);
void hashtable_free (struct hashtable *ht, traversefunction destroyfn, void *arg);
void hashtable_free_all (struct hashtable *ht);

unsigned int hashtable_count (struct hashtable *ht);

void hashtable_insert (struct hashtable *ht, void *key, void *value);
void hashtable_inserth (struct hashtable *ht, void *key, void *value, unsigned int hash);
void *hashtable_search (struct hashtable *ht, void *key, void **key_found);
void *hashtable_searchh (struct hashtable *ht, void *key, void **key_found, unsigned int hash);
int hashtable_haskey (struct hashtable *ht, void *key, void **key_found);
int hashtable_haskeyh (struct hashtable *ht, void *key, void **key_found, unsigned int hash);
void *hashtable_remove (struct hashtable *ht, void *key, void **key_found);
void *hashtable_removeh (struct hashtable *ht, void *key, void **key_found, unsigned int hash);

void hashtable_traverse (struct hashtable *ht, traversefunction traversefn, void *arg);

int hashtable_string_compare (void *key1, void *key2, unsigned int hash);
int hashtable_pointer_compare (void *key1, void *key2, unsigned int hash);

unsigned int hashtable_hash_bytes (unsigned char *key, size_t len);
unsigned int hashtable_hash_string (void *key);

#endif /* __HASH_H */
