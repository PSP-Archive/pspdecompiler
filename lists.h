
#ifndef __LISTS_H
#define __LISTS_H

#include <stddef.h>

struct _element;
typedef struct _element *element;

struct _list;
typedef struct _list *list;

struct _list_pool;
typedef struct _list_pool *list_pool;

list_pool pool_create (size_t addend_size);
void pool_destroy (list_pool pool);

list list_create (list_pool pool);
void list_destroy (list l);
void list_reset (list l);
int  list_size (list l);

element list_head (list l);
element list_tail (list l);

element list_inserthead (list l, void *val);
element list_inserttail (list l, void *val);

void list_removehead (list l);
void list_removetail (list l);

void *element_value (element el);
void *element_addendum (element el);

element element_next (element el);
element element_previous (element el);

void element_remove (element el);


#endif /* __LISTS_H */
