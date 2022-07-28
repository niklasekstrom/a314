#include <exec/types.h>

#include "device.h"

extern void init_memory_allocator(struct A314Device *dev);
extern ULONG a314base_translate_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address);
extern ULONG a314base_alloc_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG length);
extern void a314base_free_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("d1") ULONG length);
