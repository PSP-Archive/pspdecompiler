
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hash.h"
#include "utils.h"

#define INDEX_FOR(hash, size) ((hash) & ((size) - 1))

struct entry
{
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
};

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

  return ht;
}

void hashtable_destroy (struct hashtable *ht, traversefunction destroyfn)
{
  struct entry *e, *ne;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = ne) {
      ne = e->next;
      destroyfn (e->key, e->value);
      free (e);
    }
  }
  free (ht->table);
  free (ht);
}

static
void free_entry (void *key, void *value)
{
  free (key);
  free (value);
}

void hashtable_destroy_all (struct hashtable *ht)
{
  hashtable_destroy (ht, &free_entry);
}


static void hashtable_grow (struct hashtable *ht)
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
  unsigned int index;
  struct entry *e;
  if (ht->entrycount >= ht->loadlimit) {
    hashtable_grow (ht);
  }
  e = (struct entry *) xmalloc (sizeof (struct entry));
  e->hash = ht->hashfn (key);
  index = INDEX_FOR (e->hash, ht->tablelength);
  e->key = key;
  e->value = value;
  e->next = ht->table[index];
  ht->table[index] = e;
  ht->entrycount++;
}

void *hashtable_search (struct hashtable *ht, void *key, void **key_found)
{
  struct entry *e;
  unsigned int hashvalue, index;
  hashvalue = ht->hashfn (key);
  index = INDEX_FOR (hashvalue, ht->tablelength);
  for (e = ht->table[index]; e; e = e->next) {
    if (hashvalue != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key))
        continue;
    if (key_found) {
      *key_found = e->key;
    }
    return e->value;
  }
  return NULL;
}

void *hashtable_remove (struct hashtable *ht, void *key, void **key_found)
{
  struct entry *e;
  struct entry **previous;
  void *value;
  unsigned int hashvalue, index;

  hashvalue = ht->hashfn (key);
  index = INDEX_FOR (hashvalue, ht->tablelength);
  previous = &(ht->table[index]);
  for (e = *previous; e; e = e->next) {
    previous = &(e->next);
    if (hashvalue != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key))
        continue;

    *previous = e->next;
    ht->entrycount--;
    value = e->value;
    if (key_found) {
      *key_found = e->key;
    }
    free (e);
    return value;
  }
  return NULL;
}


void hashtable_traverse (struct hashtable *ht, traversefunction traversefn)
{
  struct entry *e;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = e->next) {
      traversefn (e->key, e->value);
    }
  }
}



unsigned int hash_bytes (unsigned char *key, size_t len)
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

unsigned int hash_string (char *key)
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
