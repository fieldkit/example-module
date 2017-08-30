#include <Arduino.h>
#include <stdarg.h>

#include "debug.h"

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
