#ifndef FK_DEBUG_INCLUDED
#define FK_DEBUG_INCLUDED

#include <stdlib.h>
#include <stdint.h>

const uint32_t FK_DEBUG_LINE_MAX = 128;

void debugf(char *fmt, ...);
void debugfln(char *fmt, ...);

extern void __fk_assert(const char *msg, const char *file, int lineno);

#define fk_assert(EX) (void)((EX) || (__fk_assert (#EX, __FILE__, __LINE__),0))

#endif
