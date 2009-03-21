
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hash.h"
#include "utils.h"
#include "types.h"

#define DEFAULT_ALLOC_ENTRIES 512
#define INDEX_FOR(hash, size) ((hash) & ((size) - 1))

struct entry {
  void *key, *value;
  unsigned int hash;
  struct entry *next;
};

struct hashtable {
  unsigned int tablelength;
  unsigned int entrycount;
  unsigned int loadlimit;
  struct entry **table;
  hashfunction hashfn;
  equalsfunction eqfn;

  struct entry *alloclist;
  struct entry *nextavail;
};


static
struct entry *alloc_entry (struct hashtable *ht)
{
  struct entry *e;
  if (!ht->nextavail) {
    int i;
    e = (struct entry *) xmalloc (DEFAULT_ALLOC_ENTRIES * sizeof (struct entry));
    e->next = ht->alloclist;
    ht->alloclist = e;

    for (i = 1; i < DEFAULT_ALLOC_ENTRIES - 1; i++)
      e[i].next = &e[i + 1];
    e[i].next = NULL;

    ht->nextavail = &e[1];
  }
  e = ht->nextavail;
  ht->nextavail = e->next;
  return e;
}

static
void free_entry (struct hashtable *ht, struct entry *e)
{
  e->next = ht->nextavail;
  ht->nextavail = e;
}

struct hashtable *hashtable_create (unsigned int size, hashfunction hashfn, equalsfunction eqfn)
{
  struct hashtable *ht;

  ht = (struct hashtable *) xmalloc (sizeof (struct hashtable));
  ht->table = (struct entry **) xmalloc (sizeof (struct entry *) * size);
  memset (ht->table, 0, size * sizeof (struct entry *));

  ht->tablelength = size;
  ht->entrycount = 0;
  ht->hashfn = hashfn;
  ht->eqfn = eqfn;
  ht->loadlimit = size >> 1;
  ht->alloclist = NULL;
  ht->nextavail = NULL;

  return ht;
}

void hashtable_free (struct hashtable *ht, traversefunction destroyfn, void *arg)
{
  struct entry *e, *ne;

  if (destroyfn)
    hashtable_traverse (ht, destroyfn, arg);

  for (e = ht->alloclist; e; e = ne) {
    ne = e->next;
    free (e);
  }

  free (ht->table);
  free (ht);
}

static
void free_all_callback (void *key, void *value, unsigned int hash, void *arg)
{
  if (key)   free (key);
  if (value) free (value);
}

void hashtable_free_all (struct hashtable *ht)
{
  hashtable_free (ht, &free_all_callback, NULL);
}


static
void hashtable_grow (struct hashtable *ht)
{
  struct entry **newtable;
  struct entry *e, *ne;
  unsigned int newsize, i, index;

  newsize = ht->tablelength << 1;

  newtable = (struct entry **) xmalloc (sizeof (struct entry *) * newsize);
  memset (newtable, 0, newsize * sizeof (struct entry *));

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = ne) {
      ne = e->next;
      index = INDEX_FOR (e->hash, newsize);
      e->next = newtable[index];
      newtable[index] = e;
    }
  }

  free (ht->table);
  ht->table = newtable;
  ht->tablelength = newsize;
  ht->loadlimit = newsize >> 1;
}

unsigned int hashtable_count (struct hashtable *ht)
{
  return ht->entrycount;
}

void hashtable_insert (struct hashtable *ht, void *key, void *value)
{
  hashtable_inserth (ht, key, value, ht->hashfn (key));
}

void hashtable_inserth (struct hashtable *ht, void *key, void *value, unsigned int hash)
{
  unsigned int index;
  struct entry *e;

  if (ht->entrycount >= ht->loadlimit) {
    hashtable_grow (ht);
  }
  e = alloc_entry (ht);
  e->hash = hash;
  index = INDEX_FOR (e->hash, ht->tablelength);
  e->key = key;
  e->value = value;
  e->next = ht->table[index];
  ht->entrycount++;
  ht->table[index] = e;
}


static
struct entry *find_entry (struct hashtable *ht, void *key, unsigned int hash, int remove)
{
  struct entry *e;
  struct entry **prev;
  unsigned int index;

  index = INDEX_FOR (hash, ht->tablelength);
  for (prev = &(ht->table[index]); (e = *prev) ; prev = &e->next) {
    if (hash != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key, hash))
        continue;

    if (remove) {
      *prev = e->next;
      ht->entrycount--;
      free_entry (ht, e);
    }

    return e;
  }
  return NULL;
}

void *hashtable_search (struct hashtable *ht, void *key, void **key_found)
{
  return hashtable_searchh (ht, key, key_found, ht->hashfn (key));
}

void *hashtable_searchh (struct hashtable *ht, void *key, void **key_found, unsigned int hash)
{
  struct entry *e = find_entry (ht, key, hash, 0);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return e->value;
  }
  return NULL;
}

int hashtable_haskey (struct hashtable *ht, void *key, void **key_found)
{
  return hashtable_haskeyh (ht, key, key_found, ht->hashfn (key));
}

int hashtable_haskeyh (struct hashtable *ht, void *key, void **key_found, unsigned int hash)
{
  struct entry *e = find_entry (ht, key, hash, 0);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return TRUE;
  }
  return FALSE;
}

void *hashtable_remove (struct hashtable *ht, void *key, void **key_found)
{
  return hashtable_removeh (ht, key, key_found, ht->hashfn (key));
}

void *hashtable_removeh (struct hashtable *ht, void *key, void **key_found, unsigned int hash)
{
  struct entry *e = find_entry (ht, key, hash, 1);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return e->value;
  }
  return NULL;
}


void hashtable_traverse (struct hashtable *ht, traversefunction traversefn, void *arg)
{
  struct entry *e;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = e->next) {
      traversefn (e->key, e->value, e->hash, arg);
    }
  }
}

int hashtable_string_compare (void *key1, void *key2, unsigned int hash)
{
  return (strcmp (key1, key2) == 0);
}

int hashtable_pointer_compare (void *key1, void *key2, unsigned int hash)
{
  return (key1 == key2);
}


unsigned int hashtable_hash_bytes (unsigned char *key, size_t len)
{
  unsigned int hash = 0;
  size_t i;

  for (i = 0; i < len; i++) {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

unsigned int hashtable_hash_string (void *key)
{
  unsigned int hash = 0;
  unsigned char *bytes = (unsigned char *) key;

  while (*bytes) {
    hash += *bytes++;
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}
