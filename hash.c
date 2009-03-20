
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hash.h"
#include "utils.h"
#include "types.h"

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

void hashtable_destroy (struct hashtable *ht, traversefunction destroyfn, void *arg)
{
  struct entry *e, *ne;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = ne) {
      ne = e->next;
      if (destroyfn)
        destroyfn (e->key, e->value, e->hash, arg);
      free (e);
    }
  }
  free (ht->table);
  free (ht);
}

static
void destroy_entry (void *key, void *value, unsigned int hash, void *arg)
{
  if (key)   free (key);
  if (value) free (value);
}

void hashtable_destroy_all (struct hashtable *ht)
{
  hashtable_destroy (ht, &destroy_entry, NULL);
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
  hashtable_inserth (ht, key, value, ht->hashfn (key));
}

void hashtable_inserth (struct hashtable *ht, void *key, void *value, unsigned int hash)
{
  unsigned int index;
  struct entry *e;

  if (ht->entrycount >= ht->loadlimit) {
    hashtable_grow (ht);
  }
  e = (struct entry *) xmalloc (sizeof (struct entry));
  e->hash = hash;
  index = INDEX_FOR (e->hash, ht->tablelength);
  e->key = key;
  e->value = value;
  e->next = ht->table[index];
  ht->entrycount++;
  ht->table[index] = e;
}

void *hashtable_search (struct hashtable *ht, void *key, void **key_found)
{
  return hashtable_searchh (ht, key, key_found, ht->hashfn (key));
}

void *hashtable_searchh (struct hashtable *ht, void *key, void **key_found, unsigned int hash)
{
  struct entry *e;
  unsigned int index;

  index = INDEX_FOR (hash, ht->tablelength);
  for (e = ht->table[index]; e; e = e->next) {
    if (hash != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key, hash))
        continue;
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
  struct entry *e;
  unsigned int index;

  index = INDEX_FOR (hash, ht->tablelength);
  for (e = ht->table[index]; e; e = e->next) {
    if (hash != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key, hash))
        continue;
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
  struct entry *e;
  struct entry **prev;
  void *value;
  unsigned int index;

  index = INDEX_FOR (hash, ht->tablelength);
  for (prev = &(ht->table[index]); (e = *prev) ; prev = &e->next) {
    if (hash != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key, hash))
        continue;

    *prev = e->next;
    ht->entrycount--;
    value = e->value;
    if (key_found)
      *key_found = e->key;
    free (e);
    return value;
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

int hashtable_merge (struct hashtable *out, struct hashtable *in, repeatedfunction repeatedfn, void *arg)
{
  struct entry *e;
  unsigned int i;

  if (in->hashfn != out->hashfn ||
      in->eqfn != out->eqfn) {
    error (__FILE__ ": incompatible types during merge");
    return 0;
  }

  for (i = 0; i < in->tablelength; i++) {
    for (e = in->table[i]; e; e = e->next) {
      void *key, *value;
      value = hashtable_searchh (out, e->key, &key, e->hash);
      if (value) {
        if (repeatedfn)
          repeatedfn (key, value, e->key, e->value, e->hash, arg);
      } else {
        hashtable_inserth (out, e->key, e->value, e->hash);
      }
    }
  }

  return 1;
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


#ifdef TEST_HASH

static
void print_all (void *key, void *val, unsigned int hash, void *arg)
{
  report (" key: (%s)%p   val: (%s)%p   hash:%d   arg:(%s)%p\n", key, key, val, val, hash, arg, arg);
}

static
void repeated (void *outkey, void *outval, void *inkey, void *inval, unsigned int hash, void *arg)
{
  report ("Repeated:  outkey: (%s)%p  outval: (%s)%p  inkey: (%s)%p  inval: (%s)%p hash: %d  arg: (%s)%p\n", outkey, outkey, outval, outval, inkey, inkey, inval, inval, hash, arg, arg);
}

int main (int argc, char **argv)
{
  struct hashtable *ht, *another;
  char *key1 = "key1";
  char *val1 = "val1";
  char *key2 = "key2";
  char *val2 = "val2";
  char buf[256];
  void *key_found = NULL;
  void *val = NULL;

  buf[0] = 'k';
  buf[1] = 'e';
  buf[2] = 'y';
  buf[3] = '1';
  buf[4] = '\0';

  report ("Key 1: (%s)%p\n", key1, key1);
  report ("Val 1: (%s)%p\n", val1, val1);
  report ("Key 2: (%s)%p\n", key2, key2);
  report ("Val 2: (%s)%p\n", val2, val2);
  report ("Buffer: (%s)%p\n", buf, buf);

  ht = hashtable_create (64, &hashtable_hash_string, &hashtable_string_compare);
  another = hashtable_create (64, &hashtable_hash_string, &hashtable_string_compare);

  report ("\nInserting key: (%s)%p val: (%s)%p\n", key1, key1, val1, val1);
  report ("count = %d\n", hashtable_count (ht));
  hashtable_insert (ht, key1, val1);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nSearching key: (%s)%p\n", buf, buf);
  key_found = NULL;
  val = hashtable_search (ht, buf, &key_found);
  report (" val: (%s)%p key_found: (%s)%p\n", val, val, key_found, key_found);

  report ("\nPrinting all\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nRemoving key: (%s)%p\n", buf, buf);
  hashtable_remove (ht, buf, NULL);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting all\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nSearching again  key: (%s)%p\n", buf, buf);
  key_found = NULL;
  val = hashtable_search (ht, buf, &key_found);
  report (" val: (%s)%p key_found: (%s)%p\n", val, val, key_found, key_found);

  report ("\nTring merge with:\n");
  hashtable_insert (another, key1, val1);
  hashtable_traverse (another, &print_all, buf);
  report ("count = %d\n", hashtable_count (ht));

  hashtable_merge (ht, another, &repeated, buf);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nTring merge again\n");
  hashtable_remove (another, key1, NULL);
  hashtable_insert (another, buf, val1);
  hashtable_insert (another, key2, val2);
  hashtable_traverse (another, &print_all, buf);
  report ("count = %d\n", hashtable_count (ht));

  hashtable_merge (ht, another, &repeated, buf);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nInserting key: (%s)%p val: (%s)%p with hash 0\n", key1, key1, val1, val1);
  report ("count = %d\n", hashtable_count (ht));
  hashtable_inserth (ht, key1, val1, 0);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nHas key: (%s)%p\n", buf, buf);
  report ("%d\n", hashtable_haskey (ht, buf, NULL));

  report ("\nHas key: (%s)%p with hash 0\n", buf, buf);
  report ("%d\n", hashtable_haskeyh (ht, buf, NULL, 0));

  report ("\nRemoving key: (%s)%p\n", buf, buf);
  hashtable_remove (ht, buf, NULL);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting all\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nRemoving key: (%s)%p with hash 0\n", buf, buf);
  hashtable_removeh (ht, buf, NULL, 0);
  report ("count = %d\n", hashtable_count (ht));

  report ("\nPrinting all\n");
  hashtable_traverse (ht, &print_all, buf);

  report ("\nDestroying all\n");
  hashtable_destroy (ht, NULL, NULL);
  hashtable_destroy (another, NULL, NULL);
  return 0;
}
#endif /* TEST_HASH */
