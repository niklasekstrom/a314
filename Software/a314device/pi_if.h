#include <exec/types.h>

#include "device.h"

extern int probe_pi_interface(struct A314Device *dev);
extern void setup_pi_interface(struct A314Device *dev);

extern void read_from_r2a(struct A314Device *dev, UBYTE *dst, UBYTE offset, int length);
extern void write_to_a2r(struct A314Device *dev, UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data);

extern void read_pi_cap(struct A314Device *dev);
extern void write_amiga_cap(struct A314Device *dev);

extern void set_pi_irq(struct A314Device *dev);
extern void clear_amiga_irq(struct A314Device *dev);

extern ULONG a314base_translate_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address);
extern ULONG a314base_alloc_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG length);
extern void a314base_free_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("d1") ULONG length);
extern void a314base_write_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length);
extern void a314base_read_mem(__reg("a6") struct A314Device *dev, __reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length);
