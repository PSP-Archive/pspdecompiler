
#ifndef __LISTS_H
#define __LISTS_H


#define LLIST_ADD(pool, list, value) \
  (list) = llist_add ((pool), (list), (value))

typedef
struct _llist {
  struct _llist *next;
  void *value;
} *llist;

struct _llist_pool;

typedef struct _llist_pool *llist_pool;

llist_pool llist_create (void);
llist llist_alloc (llist_pool pool);
llist llist_add (llist_pool pool, llist l, void *val);
void llist_free (llist_pool pool, llist el);
void llist_freeall (llist_pool pool, llist l);
void llist_destroy (llist_pool pool);

#endif /* __LISTS_H */
