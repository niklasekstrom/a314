#include <exec/types.h>

// Addresses to variables in CMEM.
#define CMEM_CFG_ADDRESS        11
#define R_EVENTS_ADDRESS	12
#define R_ENABLE_ADDRESS	13
#define A_EVENTS_ADDRESS	14
#define A_ENABLE_ADDRESS	15

// Events that are communicated via IRQ from Amiga to Raspberry.
#define R_EVENT_A2R_TAIL	1
#define R_EVENT_R2A_HEAD	2
#define R_EVENT_BASE_ADDRESS	4

// Events that are communicated from Raspberry to Amiga.
#define A_EVENT_R2A_TAIL	1
#define A_EVENT_A2R_HEAD	2

extern void write_cp_nibble(int index, UBYTE value);
extern UBYTE read_cp_nibble(int index);

extern void write_cmem_safe(int index, UBYTE value);
extern UBYTE read_cmem_safe(int index);

extern void write_base_address(ULONG ba);
