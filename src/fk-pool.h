#ifndef FK_POOL_INCLUDED
#define FK_POOL_INCLUDED

#include <stdlib.h>
#include <stdint.h>

typedef struct fk_pool_t {
    const char *name;
    uint8_t *block;
    uint8_t *ptr;
    size_t remaining;
    size_t size;
    fk_pool_t *child;
    fk_pool_t *sibling;
} fk_pool_t;

bool fk_pool_create(fk_pool_t **pool, size_t size, fk_pool_t *parent);

bool fk_pool_free(fk_pool_t *pool);

void fk_pool_empty(fk_pool_t *pool);

void *fk_pool_malloc(fk_pool_t *pool, size_t size);

size_t fk_pool_used(fk_pool_t *pool);

#endif
