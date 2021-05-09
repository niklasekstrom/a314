#include <exec/types.h>

#include "device.h"

extern ULONG translate_address_a314(__reg("a6") struct A314Device *dev, __reg("a0") void *address);
extern BOOL fix_memory(struct A314Device *dev);
