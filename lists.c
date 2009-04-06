
#include <stdlib.h>
#include <string.h>

#include "lists.h"
#include "alloc.h"
#include "utils.h"

struct _element {
  struct _list_pool *const pool;

  struct _list *lst;
  struct _element *next;
  struct _element *prev;

  void *value;
};

struct _list {
  struct _list_pool *const pool;
  struct _element *head;
  struct _element *tail;
  struct _list *next;
  int size;
};

struct _list_pool {
  fixedpool lstpool;
  fixedpool elmpool;
};

list_pool pool_create (size_t numelms, size_t numlsts)
{
  list_pool result = (list_pool) xmalloc (sizeof (struct _list_pool));
  result->elmpool = fixedpool_create (sizeof (struct _element), numelms);
  result->lstpool = fixedpool_create (sizeof (struct _list), numlsts);
  return result;
}

void pool_destroy (list_pool pool)
{
  fixedpool_destroy (pool->lstpool);
  fixedpool_destroy (pool->elmpool);
  free (pool);
}


static
element element_alloc (list_pool pool)
{
  element el;
  el = fixedpool_alloc (pool->elmpool);

  el->prev = NULL;
  el->next = NULL;
  el->value = NULL;
  el->lst = NULL;

  return el;
}

static
void element_free (element el)
{
  list_pool pool;
  pool = el->pool;
  fixedpool_free (pool->elmpool, el);
}


static
list list_alloc (list_pool pool)
{
  list l;
  l = fixedpool_alloc (pool->lstpool);
  l->head = l->tail = NULL;
  l->size = 0;
  return l;
}

static
void list_free (list l)
{
  list_pool pool;
  pool = l->pool;
  fixedpool_free (pool->lstpool, l);
}


list list_create (list_pool pool)
{
  return list_alloc (pool);
}

void list_destroy (list l)
{
  list_reset (l);
  list_free (l);
}

void list_reset (list l)
{
  element el, ne;
  for (el = l->head; el; el = ne) {
    ne = el->next;
    element_free (el);
  }
  l->head = l->tail = NULL;
  l->size = 0;
}

int list_size (list l)
{
  return l->size;
}

element list_head (list l)
{
  return l->head;
}

element list_tail (list l)
{
  return l->tail;
}

element list_inserthead (list l, void *val)
{
  element el = element_alloc (l->pool);
  el->value = val;
  el->next = l->head;
  el->lst = l;
  if (l->head) l->head->prev = el;
  l->head = el;
  if (!l->tail) l->tail = el;
  l->size++;
  return el;
}

element list_inserttail (list l, void *val)
{
  element el = element_alloc (l->pool);
  el->value = val;
  el->prev = l->tail;
  el->lst = l;
  if (l->tail) l->tail->next = el;
  l->tail = el;
  if (!l->head) l->head = el;
  l->size++;
  return el;
}

void list_removehead (list l)
{
  element_remove (l->head);
}

void list_removetail (list l)
{
  element_remove (l->tail);
}

void *element_value (element el)
{
  return el->value;
}

element element_next (element el)
{
  return el->next;
}

element element_previous (element el)
{
  return el->prev;
}

void element_remove (element el)
{
  if (!el->lst) return;
  el->lst->size--;

  if (!el->next) {
    el->lst->tail = el->prev;
  } else {
    el->next->prev = el->prev;
  }

  if (!el->prev) {
    el->lst->head = el->next;
  } else {
    el->prev->next = el->next;
  }
  element_free (el);
}


#ifdef TEST_LISTS

struct info {
  int v1;
  int v2;
};

int main (int argc, char **argv)
{
  list_pool pool;
  list l1, l2;
  element el;

  pool = pool_create (sizeof (struct info));

  l1 = list_create (pool);
  report ("List 1 size: %d\n", list_size (l1));

  l2 = list_create (pool);
  report ("List 2 size: %d\n", list_size (l2));

  el = list_inserthead (l1, NULL);
  report ("Element %p\n", el);

  el = list_inserthead (l1, NULL);
  report ("Element %p\n", el);

  report ("List 1 head: %p\n", list_head (l1));
  report ("List 1 tail: %p\n", list_tail (l1));
  report ("List 1 size: %d\n\n", list_size (l1));

  element_remove (el);
  report ("List 1 head: %p\n", list_head (l1));
  report ("List 1 tail: %p\n", list_tail (l1));
  report ("List 1 size: %d\n\n", list_size (l1));

  list_reset (l1);
  report ("List 1 size: %d\n", list_size (l1));

  list_destroy (l1);

  el = list_inserthead (l2, NULL);
  report ("Element %p\n", el);

  el = list_inserthead (l2, NULL);
  report ("Element %p\n", el);

  pool_destroy (pool);
  return 0;
}

#endif /* TEST_LISTS */








