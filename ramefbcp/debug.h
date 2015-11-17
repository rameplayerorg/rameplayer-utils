#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif


#define DEBUG_SUPPORT


#ifdef DEBUG_SUPPORT
extern int g_debug_info; // 1 if enabled
#endif // DEBUG_SUPPORT

extern void dbg_printf(const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif // !DEBUG_H_INCLUDED
