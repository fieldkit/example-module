#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "debug.h"
#include "fk-pool.h"

#define POOL_DEBUG(f, ...)  // debugfln(f, __VA_ARGS__)

bool fk_pool_create(fk_pool_t **pool, size_t size, fk_pool_t *parent) {
    fk_pool_t *fkp = nullptr;

    if (parent == nullptr) {
        fkp = (fk_pool_t *)malloc(sizeof(fk_pool_t) + size);
    }
    else {
        fkp = (fk_pool_t *)fk_pool_malloc(parent, sizeof(fk_pool_t) + size);
    }

    size_t aligned = sizeof(fk_pool_t) + (4 - (size % 4));
    fkp->name = nullptr;
    fkp->block = ((uint8_t *)fkp) + aligned;
    fkp->ptr = fkp->block;
    fkp->size = size;
    fkp->remaining = size;
    fkp->child = nullptr;
    fkp->sibling = nullptr;

    if (parent != nullptr) {
        if (parent->child != nullptr) {
            fk_pool_t *iter = parent->child;
            while (iter->sibling != nullptr) {
                iter->sibling = iter->sibling;
            }
            iter->sibling = fkp;
        }
        else {
            parent->child = fkp;
        }
    }

    POOL_DEBUG("fkpcreate: 0x%x size=%d ptr=0x%x block=0x%x (free=%d)", (uint8_t *)fkp, size, fkp->ptr, fkp->block, fk_free_memory());

    (*pool) = fkp;

    return true;
}

bool fk_pool_free(fk_pool_t *pool) {
    fk_assert(pool != nullptr);

    pool->size = 0;
    pool->remaining = 0;

    free((void *)pool);

    POOL_DEBUG("  fkpfree: 0x%x", pool);

    return true;
}

void fk_pool_empty(fk_pool_t *pool) {
    fk_assert(pool != nullptr);

    pool->ptr = pool->block;
    pool->remaining = pool->size;

    POOL_DEBUG(" fkpempty: 0x%x", pool);
}

void *fk_pool_malloc(fk_pool_t *pool, size_t size) {
    size_t aligned = size + (4 - (size % 4));

    POOL_DEBUG(" fkpalloc: 0x%x size=%d aligned=%d (free=%d)", pool, size, aligned, pool->remaining - aligned);

    fk_assert(pool != nullptr);
    fk_assert(pool->size >= aligned);
    fk_assert(pool->remaining >= aligned);

    uint8_t *p = pool->ptr;
    pool->ptr += aligned;
    pool->remaining -= aligned;

    return (void *)p;
}

size_t fk_pool_used(fk_pool_t *pool) {
    fk_assert(pool != nullptr);

    return pool->size - pool->remaining;
}
