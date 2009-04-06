
#ifndef __ALLOC_H
#define __ALLOC_H

#include <stddef.h>

struct _fixedpool;
typedef struct _fixedpool *fixedpool;

fixedpool fixedpool_create (size_t size, size_t grownum);
void fixedpool_destroy (fixedpool p);

void fixedpool_grow (fixedpool p, void *ptr, size_t size);
void *fixedpool_alloc (fixedpool p);
void fixedpool_free (fixedpool p, void *ptr);

#endif /* __ALLOC_H */
