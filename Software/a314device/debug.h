#define DEBUG 0

extern void dbg_init();
extern void dbg(const char* fmt, ...);

#if !DEBUG

#define dbg_error(...)
#define dbg_warning(...)
#define dbg_info(...)
#define dbg_trace(...)

#else

extern void dbg_init();
extern void dbg(const char* fmt, ...);

#define dbg_error(...) dbg(__VA_ARGS__)
#define dbg_warning(...) dbg(__VA_ARGS__)
#define dbg_info(...) dbg(__VA_ARGS__)
#define dbg_trace(...) dbg(__VA_ARGS__)

#endif
