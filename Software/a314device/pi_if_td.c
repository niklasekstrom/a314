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
#include "cmem.h"

#define SysBase (*(struct ExecBase **)4)

extern void IntServer();

static void add_interrupt_handlers(struct A314Device *dev)
{
	memset(&dev->vertb_interrupt, 0, sizeof(struct Interrupt));
	dev->vertb_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->vertb_interrupt.is_Node.ln_Pri = -60;
	dev->vertb_interrupt.is_Node.ln_Name = device_name;
	dev->vertb_interrupt.is_Data = (APTR)&dev->task;
	dev->vertb_interrupt.is_Code = IntServer;

	AddIntServer(INTB_VERTB, &dev->vertb_interrupt);

	memset(&dev->ports_interrupt, 0, sizeof(struct Interrupt));
	dev->ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->ports_interrupt.is_Node.ln_Pri = 0;
	dev->ports_interrupt.is_Node.ln_Name = device_name;
	dev->ports_interrupt.is_Data = (APTR)&dev->task;
	dev->ports_interrupt.is_Code = IntServer;

	AddIntServer(INTB_PORTS, &dev->ports_interrupt);
}

static void detect_and_write_address_swap()
{
	// Only looking at VPOSR Agnus identification at this point.
	// Could add more dynamic identification of address mapping.

	UWORD vposr = *(UWORD *)0xdff004;
	UWORD agnus = (vposr & 0x7f00) >> 8;

	UBYTE swap = (agnus == 0x00 || agnus == 0x10) ? 0x1 : 0x0;

	write_cmem_safe(CMEM_CFG_ADDRESS, swap);
}

int probe_pi_interface(struct A314Device *dev)
{
	dev->fw_flags = read_fw_flags();

	if (!fix_memory(dev))
		return FALSE;

	detect_and_write_address_swap();

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
	write_cmem_safe(A_ENABLE_ADDRESS, 0);
	read_cmem_safe(A_EVENTS_ADDRESS);

	write_base_address(a314base_translate_address(dev, dev->ca));

	write_cmem_safe(R_EVENTS_ADDRESS, R_EVENT_BASE_ADDRESS);

	add_interrupt_handlers(dev);

	write_cmem_safe(A_ENABLE_ADDRESS, A_EVENT_R2A_TAIL);
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
