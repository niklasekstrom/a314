#ifndef KPRINTF_H
#define KPRINTF_H

#ifdef ENABLE_KPRINTF
void kprintf(const char* fmt, ...);
#else
#define kprintf(...)  do { } while (0)
#endif

#endif
