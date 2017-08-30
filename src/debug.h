#ifndef FK_DEBUG_INCLUDED
#define FK_DEBUG_INCLUDED

#include <stdlib.h>
#include <stdint.h>

const uint32_t FK_DEBUG_LINE_MAX = 128;

void debugf(char *fmt, ...);
void debugfln(char *fmt, ...);

#endif
