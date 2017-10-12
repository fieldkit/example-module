#include <Arduino.h>
#include <stdarg.h>

#include "debug.h"

const uint32_t FK_DEBUG_LINE_MAX = 128;

debug_hook_fn_t global_hook_fn = nullptr;
void *global_hook_arg = nullptr;

void debug_add_hook(debug_hook_fn_t hook, void *arg) {
    global_hook_fn = hook;
    global_hook_arg = arg;
}

void debug_write(const char *str) {
    Serial.print(str);
    if (global_hook_fn != nullptr) {
        global_hook_fn(str, global_hook_arg);
    }
}

void debugf(char *fmt, ...) {
    char buf[FK_DEBUG_LINE_MAX];
    va_list args;
    va_start(args, fmt );
    vsnprintf(buf, FK_DEBUG_LINE_MAX, fmt, args);
    va_end(args);

    debug_write(buf);
}

void debugfln(char *fmt = "", ...) {
    char buf[FK_DEBUG_LINE_MAX];
    va_list args;
    va_start(args, fmt );
    size_t w = vsnprintf(buf, FK_DEBUG_LINE_MAX - 2, fmt, args);
    va_end(args);

    buf[w    ] = '\r';
    buf[w + 1] = '\n';
    buf[w + 2] = 0;

    debug_write(buf);
}

extern "C" char *sbrk(int32_t i);

uint32_t fk_free_memory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}

void __fk_assert(const char *msg, const char *file, int lineno) {
    debugfln("ASSERTION: %s:%d '%s'", file, lineno, msg);
    Serial.flush();
    abort();
}
