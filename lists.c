
#include <stdlib.h>
#include <string.h>

#include "lists.h"
#include "utils.h"

#define POOL_ALLOC_ELEMENTS 4096
#define POOL_ALLOC_LISTS      16

struct _element {
  struct _list_pool *const pool;

  struct _list *lst;
  struct _element *next;
  struct _element *prev;

  void *value;
  void *const addend;
};

struct _list {
  struct _list_pool *const pool;
  struct _element *head;
  struct _element *tail;
  struct _list *next;
  int size;
};

struct _list_pool {
  struct _element *free_el;
  struct _list    *free_lst;
  struct _element *alloc_el;
  struct _list    *alloc_lst;

  size_t addend_size;
};

list_pool pool_create (size_t addend_size)
{
  list_pool result = (list_pool) xmalloc (sizeof (struct _list_pool));
  result->free_el = NULL;
  result->free_lst = NULL;
  result->alloc_el = NULL;
  result->alloc_lst = NULL;
  result->addend_size = addend_size;
  return result;
}

void pool_destroy (list_pool pool)
{
  element el, ne;
  list l, nl;

  for (el = pool->alloc_el; el; el = ne) {
    ne = el->next;
    if (el->addend)
      free ((void *) el->addend);
    free (el);
  }

  for (l = pool->alloc_lst; l; l = nl) {
    nl = l->next;
    free (l);
  }

  free (pool);
}


static
element element_alloc (list_pool pool)
{
  element el;
  struct _list_pool **pptr;

  if (!pool->free_el) {
    int i;

    el = (element) xmalloc (POOL_ALLOC_ELEMENTS * sizeof (struct _element));
    memset (el, 0, POOL_ALLOC_ELEMENTS * sizeof (struct _element));

    el->next = pool->alloc_el;
    pool->alloc_el = el;

    for (i = 0; i < POOL_ALLOC_ELEMENTS; i++) {
      pptr = (struct _list_pool **) &el[i].pool;
      *pptr = pool;
    }

    if (pool->addend_size) {
      char *addend = xmalloc (POOL_ALLOC_ELEMENTS * pool->addend_size);
      void **ptr;

      for (i = 0; i < POOL_ALLOC_ELEMENTS; i++) {
        ptr = (void **) &el[i].addend;  *ptr = addend;
        addend += pool->addend_size;
      }
    }

    for (i = 1; i < POOL_ALLOC_ELEMENTS - 1; i++)
      el[i].next = &el[i + 1];
    el[i].next = NULL;
    pool->free_el = &el[1];
  }
  el = pool->free_el;
  pool->free_el = el->next;

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
  el->next = pool->free_el;
  el->lst = NULL;
  el->prev = NULL;
  el->value = NULL;
  pool->free_el = el;
}


static
list list_alloc (list_pool pool)
{
  list l;
  struct _list_pool **pptr;

  if (!pool->free_lst) {
    int i;

    l = (list) xmalloc (POOL_ALLOC_LISTS * sizeof (struct _list));
    memset (l, 0, POOL_ALLOC_LISTS * sizeof (struct _list));

    l->next = pool->alloc_lst;
    pool->alloc_lst = l;

    for (i = 0; i < POOL_ALLOC_LISTS; i++) {
      pptr = (struct _list_pool **) &l[i].pool;
      *pptr = pool;
    }

    for (i = 1; i < POOL_ALLOC_LISTS - 1; i++)
      l[i].next = &l[i + 1];
    l[i].next = NULL;
    pool->free_lst = &l[1];
  }
  l = pool->free_lst;
  pool->free_lst = l->next;
  l->head = l->tail = NULL;
  l->size = 0;
  return l;
}

static
void list_free (list l)
{
  list_pool pool;
  pool = l->pool;

  l->next = pool->free_lst;

  l->head = l->tail = NULL;
  l->size = 0;
  pool->free_lst = l;
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

void *element_addendum (element el)
{
  return el->addend;
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








