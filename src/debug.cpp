#include <Arduino.h>
#include <stdarg.h>

#include "debug.h"

const uint32_t FK_DEBUG_LINE_MAX = 128;

void debugf(char *fmt, ...) {
    char buf[FK_DEBUG_LINE_MAX];
    va_list args;
    va_start(args, fmt );
    vsnprintf(buf, FK_DEBUG_LINE_MAX, fmt, args);
    va_end(args);
    Serial.print(buf);
}

void debugfln(char *fmt = "", ...) {
    char buf[FK_DEBUG_LINE_MAX];
    va_list args;
    va_start(args, fmt );
    vsnprintf(buf, FK_DEBUG_LINE_MAX, fmt, args);
    va_end(args);
    Serial.print(buf);
    Serial.println();
}

void __fk_assert(const char *msg, const char *file, int lineno) {
    debugfln("ASSERTION: %s:%d '%s'", file, lineno, msg);
    Serial.flush();
    abort();
}
