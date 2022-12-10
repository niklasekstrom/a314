#include <exec/types.h>

#include "protocol.h"

extern void set_pi_irq();
extern void clear_cp_irq();

extern void a314base_write_mem(__reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length);
extern void a314base_read_mem(__reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length);

extern void read_pi_cap(struct ComAreaPtrs *cap);
extern void write_amiga_cap(struct ComAreaPtrs *cap);

extern void clear_cap();
extern void update_restart_counter();

extern void read_from_r2a(UBYTE *dst, UBYTE offset, int length);
extern void write_to_a2r(struct ComAreaPtrs *cap, UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data);

extern int probe_interface();
