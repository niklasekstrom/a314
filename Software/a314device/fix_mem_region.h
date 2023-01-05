#include <exec/types.h>

#include "device.h"

extern BOOL fix_memory(struct A314Device *dev);

extern ULONG a314base_translate_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address);
extern ULONG a314base_alloc_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG length);
extern void a314base_free_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("d1") ULONG length);
extern void a314base_write_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length);
extern void a314base_read_mem(__reg("a6") struct A314Device *dev, __reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length);
