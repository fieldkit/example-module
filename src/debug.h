#ifndef FK_DEBUG_INCLUDED
#define FK_DEBUG_INCLUDED

#include <stdlib.h>
#include <stdint.h>

typedef void (*debug_hook_fn_t)(const char *str, void *arg);

void debug_add_hook(debug_hook_fn_t hook, void *arg);
void debugf(char *fmt, ...);
void debugfln(char *fmt, ...);

uint32_t fk_free_memory();

#define fk_assert(EX) (void)((EX) || (__fk_assert (#EX, __FILE__, __LINE__),0))

void __fk_assert(const char *msg, const char *file, int lineno);

#endif
