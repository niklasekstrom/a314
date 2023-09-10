#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>

#include <proto/exec.h>

#include <string.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "debug.h"
#include "pi_if.h"
#include "fix_mem_region.h"
#include "handshake_fe.h"

#define SysBase (*(struct ExecBase **)4)

// TODO: Currently the list of addresses to probe is hard coded.
// We may want to add more dynamic detection of where the
// shared memory ends up in the memory space.
static ULONG probe_addresses[] = {0x040000, 0xC40000, INVALID_A314_ADDRESS};

#define QUARTER_MB		(256*1024)

extern void IntServer();

static void add_interrupt_handlers(struct A314Device *dev)
{
	memset(&dev->vertb_interrupt, 0, sizeof(struct Interrupt));
	dev->vertb_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->vertb_interrupt.is_Node.ln_Pri = -60;
	dev->vertb_interrupt.is_Node.ln_Name = device_name;
	dev->vertb_interrupt.is_Data = (APTR)dev;
	dev->vertb_interrupt.is_Code = IntServer;

	AddIntServer(INTB_VERTB, &dev->vertb_interrupt);

	memset(&dev->ports_interrupt, 0, sizeof(struct Interrupt));
	dev->ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->ports_interrupt.is_Node.ln_Pri = 0;
	dev->ports_interrupt.is_Node.ln_Name = device_name;
	dev->ports_interrupt.is_Data = (APTR)dev;
	dev->ports_interrupt.is_Code = IntServer;

	AddIntServer(INTB_PORTS, &dev->ports_interrupt);
}

static BOOL detect_board_present(ULONG address)
{
	Disable();

	volatile UWORD *p = (volatile UWORD *)address;
	UWORD old_value = *p;

	*p = 0x413a;
	*p = 0xfeed;
	*p = 0xc0de;
	*p = 0xa314;
	*p = 0xa314;

	UWORD new_value = *p;

	*p = old_value;

	Enable();

	return new_value == 0x413a;
}

static void set_com_area_base_address(struct A314Device *dev)
{
	Disable();
	volatile UWORD *p = &(dev->ca->lock_word);
	*p = 0x413a;
	*p = 0xfeed;
	*p = 0xc0de;
	*p = 0xa314;
	*p = 0xa314;
	Enable();
}

static int probe_board_address(struct A314Device *dev)
{
	if (!check_overlap_region(dev->a314_mem_address, dev->a314_mem_address + QUARTER_MB))
		return FALSE;

	if (!detect_board_present(dev->a314_mem_address))
		return FALSE;

	return TRUE;
}

int probe_pi_interface(struct A314Device *dev)
{
	for (int i = 0; ; i++)
	{
		ULONG address = probe_addresses[i];
		if (address == INVALID_A314_ADDRESS)
			return FALSE;

		dev->a314_mem_address = address;

		if (probe_board_address(dev))
			break;
	}

	if (!fix_memory(dev))
		return FALSE;

	dev->ca = (struct ComArea *)AllocMem(sizeof(struct ComArea), MEMF_A314 | MEMF_CLEAR);
	if (dev->ca == NULL)
	{
		dbg_error("Unable to allocate A314 memory for com area\n");
		return FALSE;
	}

	return TRUE;
}

void setup_pi_interface(struct A314Device *dev)
{
	set_com_area_base_address(dev);

	// Clear all enable and event bits, for both sides.
	dev->ca->interrupt = 0x1f1f;

	// Set BASE_ADDRESS interrupt bit.
	dev->ca->interrupt = 0x8000 | IRQ_BASE_ADDRESS;

	add_interrupt_handlers(dev);

	// Enable R2A_TAIL interrupt.
	dev->ca->interrupt = 0x8000 | (IRQ_R2A_TAIL << 8);
}

void read_from_r2a(struct A314Device *dev, UBYTE *dst, UBYTE offset, int length)
{
	UBYTE *r2a_buffer = dev->ca->r2a_buffer;
	for (int i = 0; i < length; i++)
		*dst++ = r2a_buffer[offset++];
}

void write_to_a2r(struct A314Device *dev, UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data)
{
	struct ComArea *ca = dev->ca;
	UBYTE index = ca->cap.a2r_tail;
	UBYTE *a2r_buffer = ca->a2r_buffer;
	a2r_buffer[index++] = length;
	a2r_buffer[index++] = type;
	a2r_buffer[index++] = stream_id;
	for (int i = 0; i < (int)length; i++)
		a2r_buffer[index++] = *data++;
	ca->cap.a2r_tail = index;
}
