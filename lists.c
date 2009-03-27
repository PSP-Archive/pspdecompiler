
#include <stdlib.h>

#include "lists.h"
#include "utils.h"

#define LLIST_ALLOC_SIZE 4096

struct _llist_pool {
  llist free;
  llist allocated;
};

llist_pool llist_create (void)
{
  llist_pool result = (llist_pool) xmalloc (sizeof (struct _llist_pool));
  result->free = NULL;
  result->allocated = NULL;
  return result;
}

void llist_destroy (llist_pool pool)
{
  llist el, ne;
  for (el = pool->allocated; el; el = ne) {
    ne = el->next;
    free (el);
  }
  pool->free = NULL;
  pool->allocated = NULL;
  free (pool);
}


llist llist_alloc (llist_pool pool)
{
  llist l;
  if (!pool->free) {
    int i;
    l = (llist) xmalloc (LLIST_ALLOC_SIZE * sizeof (struct _llist));
    l->next = pool->allocated;
    pool->allocated = l;
    for (i = 1; i < LLIST_ALLOC_SIZE - 1; i++) {
      l[i].next = &l[i + 1];
    }
    l[i].next = NULL;
    pool->free = &l[1];
  }
  l = pool->free;
  pool->free = l->next;
  return l;
}

llist llist_add (llist_pool pool, llist l, void *val)
{
  llist el = llist_alloc (pool);
  el->value = val;
  el->next = l;
  return el;
}

void llist_free (llist_pool pool, llist el)
{
  el->next = pool->free;
  pool->free = el;
}

void llist_freeall (llist_pool pool, llist l)
{
  llist nl;
  while (l) {
    nl = l->next;
    l->next = pool->free;
    pool->free = l;
    l = nl;
  }
}
