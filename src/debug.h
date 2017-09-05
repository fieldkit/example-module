#ifndef FK_DEBUG_INCLUDED
#define FK_DEBUG_INCLUDED

#include <stdlib.h>
#include <stdint.h>

void debugf(char *fmt, ...);
void debugfln(char *fmt, ...);

#define fk_assert(EX) (void)((EX) || (__fk_assert (#EX, __FILE__, __LINE__),0))

void __fk_assert(const char *msg, const char *file, int lineno);

#endif
