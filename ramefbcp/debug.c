#include <stdio.h>
#include <stdarg.h>
#include "debug.h"


int g_debug_info = 0; // 1 if enabled


void dbg_printf(const char *fmt, ...)
{
#ifdef DEBUG_SUPPORT
    if (!g_debug_info)
        return;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}
