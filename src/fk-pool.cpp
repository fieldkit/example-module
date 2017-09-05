#include <string.h>

#include "fk-pool.h"
#include "debug.h"

bool fk_pool_create(fk_pool_t **pool, size_t size) {
    fk_pool_t *fkp = nullptr;
    fkp = (fk_pool_t *)malloc(sizeof(fk_pool_t) + size);

    size_t aligned = sizeof(fk_pool_t) + (4 - (size % 4));
    fkp->block = ((uint8_t *)fkp) + aligned;
    fkp->ptr = fkp->block;
    fkp->size = size;
    fkp->remaining = size;

    // debugfln("pcreate: 0x%x ptr=0x%x block=0x%x", (uint8_t *)fkp, fkp->ptr, fkp->block);

    (*pool) = fkp;

    return true;
}

bool fk_pool_free(fk_pool_t *pool) {
    free((void *)pool);
    // debugfln("  pfree: 0x%x", pool);
    return true;
}

void fk_pool_empty(fk_pool_t *pool) {
    pool->ptr = pool->block;
    pool->remaining = pool->size;
    // debugfln(" pempty: 0x%x", pool);
}

void *fk_pool_malloc(fk_pool_t *pool, size_t size) {
    size_t aligned = size + (4 - (size % 4));
    uint8_t *p = pool->ptr;
    pool->ptr += aligned;
    pool->remaining -= aligned;
    // debugfln(" palloc: 0x%x %d %d (0x%x %d)", pool, size, aligned, p, p - pool->block);
    return (void *)p;
}

char *fk_pool_strdup(fk_pool_t *pool, const char *str) {
    size_t len = strlen(str);
    char *p = (char *)fk_pool_malloc(pool, len + 1);
    memcpy(p, str, len);
    p[len] = 0;
    return p;
}

size_t fk_pool_used(fk_pool_t *pool) {
    return pool->size - pool->remaining;
}
