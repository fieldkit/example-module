#ifndef FK_POOL_INCLUDED
#define FK_POOL_INCLUDED

#include <stdlib.h>
#include <stdint.h>

typedef struct fk_pool_t {
    uint8_t *block;
    uint8_t *ptr;
    size_t remaining;
    size_t size;
} fk_pool_t;

bool fk_pool_create(fk_pool_t **pool, size_t size);

bool fk_pool_free(fk_pool_t *pool);

void fk_pool_empty(fk_pool_t *pool);

void *fk_pool_malloc(fk_pool_t *pool, size_t size);

char *fk_pool_strdup(fk_pool_t *pool, const char *str);

#endif
