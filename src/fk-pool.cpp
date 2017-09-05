#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "debug.h"
#include "fk-pool.h"

#define POOL_DEBUG(f, ...)

bool fk_pool_create(fk_pool_t **pool, size_t size) {
    fk_pool_t *fkp = nullptr;
    fkp = (fk_pool_t *)malloc(sizeof(fk_pool_t) + size);

    size_t aligned = sizeof(fk_pool_t) + (4 - (size % 4));
    fkp->block = ((uint8_t *)fkp) + aligned;
    fkp->ptr = fkp->block;
    fkp->size = size;
    fkp->remaining = size;

    POOL_DEBUG("pcreate: 0x%x ptr=0x%x block=0x%x", (uint8_t *)fkp, fkp->ptr, fkp->block);

    (*pool) = fkp;

    return true;
}

bool fk_pool_free(fk_pool_t *pool) {
    fk_assert(pool != nullptr);

    free((void *)pool);

    POOL_DEBUG("  pfree: 0x%x", pool);

    return true;
}

void fk_pool_empty(fk_pool_t *pool) {
    fk_assert(pool != nullptr);

    pool->ptr = pool->block;
    pool->remaining = pool->size;

    POOL_DEBUG(" pempty: 0x%x", pool);
}

void *fk_pool_malloc(fk_pool_t *pool, size_t size) {
    size_t aligned = size + (4 - (size % 4));

    fk_assert(pool != nullptr);
    fk_assert(pool->remaining >= aligned);

    uint8_t *p = pool->ptr;
    pool->ptr += aligned;
    pool->remaining -= aligned;

    POOL_DEBUG(" palloc: 0x%x %d %d (0x%x %d)", pool, size, aligned, p, p - pool->block);

    return (void *)p;
}

size_t fk_pool_used(fk_pool_t *pool) {
    fk_assert(pool != nullptr);
    return pool->size - pool->remaining;
}
