#include <exec/types.h>
#include <devices/timer.h>
#include <hardware/intbits.h>

#include <proto/alib.h>
#include <proto/exec.h>

#include <string.h>

#include "pi_if.h"
#include "debug.h"
#include "device.h"
#include "protocol.h"
#include "memory_allocator.h"
#include "config_file.h"

// Warning 367 says that an eight bit shift will ignore a byte value,
// which is correct, but intentional.
#pragma dontwarn 367

#define DEFAULT_CLOCKPORT_ADDRESS	0xd80001

#define REG_SRAM	0
#define REG_IRQ		1
#define REG_ADDR_LO	2
#define REG_ADDR_HI	3

#define REG_IRQ_SET	0x80
#define REG_IRQ_CLR	0x00
#define REG_IRQ_PI	0x02
#define REG_IRQ_CP	0x01

#define CP_REG_PTR(cpa, reg) ((volatile UBYTE *)(cpa + (reg << 2)))

#define SysBase (*(struct ExecBase **)4)

extern void IntServer();

void set_pi_irq(struct A314Device *dev)
{
	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_IRQ) = REG_IRQ_SET | REG_IRQ_PI;
}

void clear_amiga_irq(struct A314Device *dev)
{
	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_IRQ) = REG_IRQ_CLR | REG_IRQ_CP;
}

void a314base_write_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_write_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(cpa, REG_SRAM);

	for (int i = 0; i < length; i++)
		*p = *src++;

	Enable();
}

void a314base_read_mem(__reg("a6") struct A314Device *dev, __reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_read_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(cpa, REG_SRAM);

	for (int i = 0; i < length; i++)
		*dst++ = *p;

	Enable();
}

void read_pi_cap(struct A314Device *dev)
{
	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = (CAP_BASE + 0) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((CAP_BASE + 0) >> 8) & 0xff;

	dev->cap.r2a_tail = *CP_REG_PTR(cpa, REG_SRAM);
	dev->cap.a2r_head = *CP_REG_PTR(cpa, REG_SRAM);

	Enable();
}

void write_amiga_cap(struct A314Device *dev)
{
	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = (CAP_BASE + 2) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((CAP_BASE + 2) >> 8) & 0xff;

	*CP_REG_PTR(cpa, REG_SRAM) = dev->cap.a2r_tail;
	*CP_REG_PTR(cpa, REG_SRAM) = dev->cap.r2a_head;

	Enable();
}

void read_from_r2a(struct A314Device *dev, UBYTE *dst, UBYTE offset, int length)
{
	if (!length)
		return;

	ULONG cpa = dev->clockport_address;
	volatile UBYTE *p = CP_REG_PTR(cpa, REG_SRAM);

	Disable();

	*CP_REG_PTR(cpa, REG_ADDR_LO) = (R2A_BASE + offset) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((R2A_BASE + offset) >> 8) & 0xff;

	for (int i = 0; i < length; i++)
	{
		*dst++ = *p;
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(cpa, REG_ADDR_LO) = (R2A_BASE + 0) & 0xff;
			*CP_REG_PTR(cpa, REG_ADDR_HI) = ((R2A_BASE + 0) >> 8) & 0xff;
		}
	}

	Enable();
}

void write_to_a2r(struct A314Device *dev, UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data)
{
	dbg_trace("Enter: write_to_a2r, type=$b, stream_id=$b, length=$b", type, stream_id, length);

	struct PktHdr hdr = {length, type, stream_id};

	Disable();

	UBYTE offset = dev->cap.a2r_tail;

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = (A2R_BASE + offset) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((A2R_BASE + offset) >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(cpa, REG_SRAM);

	for (int i = 0; i < sizeof(hdr); i++)
	{
		*p = ((UBYTE *)&hdr)[i];
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(cpa, REG_ADDR_LO) = (A2R_BASE + 0) & 0xff;
			*CP_REG_PTR(cpa, REG_ADDR_HI) = ((A2R_BASE + 0) >> 8) & 0xff;
		}
	}

	for (int i = 0; i < length; i++)
	{
		*p = *data++;
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(cpa, REG_ADDR_LO) = (A2R_BASE + 0) & 0xff;
			*CP_REG_PTR(cpa, REG_ADDR_HI) = ((A2R_BASE + 0) >> 8) & 0xff;
		}
	}

	dev->cap.a2r_tail = offset;

	Enable();
}

static int probe_pi_interface_once(struct A314Device *dev)
{
	int found = FALSE;

	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = CAP_BASE >> 8;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = 6;
	*CP_REG_PTR(cpa, REG_SRAM) = 0x99;
	*CP_REG_PTR(cpa, REG_SRAM) = 0xbd;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = 7;
	if (*CP_REG_PTR(cpa, REG_SRAM) == 0xbd)
	{
		*CP_REG_PTR(cpa, REG_ADDR_LO) = 6;
		if (*CP_REG_PTR(cpa, REG_SRAM) == 0x99)
			found = TRUE;
	}
	Enable();

	return found;
}

static int delay_1s()
{
	int success = FALSE;

	struct timerequest *tr = AllocMem(sizeof(struct timerequest), MEMF_CLEAR);
	if (!tr)
		goto fail1;

	struct MsgPort *mp = AllocMem(sizeof(struct MsgPort), MEMF_CLEAR);
	if (!mp)
		goto fail2;

	mp->mp_Node.ln_Type = NT_MSGPORT;
	mp->mp_Flags = PA_SIGNAL;
	mp->mp_SigTask = FindTask(NULL);
	mp->mp_SigBit = SIGB_SINGLE;
	NewList(&mp->mp_MsgList);

	if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0))
		goto fail3;

	tr->tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
	tr->tr_node.io_Message.mn_ReplyPort = mp;
	tr->tr_node.io_Message.mn_Length = sizeof(sizeof(struct timerequest));
	tr->tr_node.io_Command = TR_ADDREQUEST;
	tr->tr_time.tv_secs = 1;
	DoIO((struct IORequest *)tr);

	success = TRUE;

	CloseDevice((struct IORequest *)tr);

fail3:
	FreeMem(mp, sizeof(struct MsgPort));

fail2:
	FreeMem(tr, sizeof(struct timerequest));

fail1:
	return success;
}

int probe_pi_interface(struct A314Device *dev)
{
	dev->clockport_address = DEFAULT_CLOCKPORT_ADDRESS;
	dev->interrupt_number = 6;

	read_and_parse_config_file(dev);

	for (int i = 7; i >= 0; i--)
	{
		if (probe_pi_interface_once(dev))
			return TRUE;

		if (i == 0 || !delay_1s())
			break;
	}
	return FALSE;
}

static void clear_cap(struct A314Device *dev)
{
	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = CAP_BASE & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = (CAP_BASE >> 8) & 0xff;

	for (int i = 0; i < 4; i++)
		*CP_REG_PTR(cpa, REG_SRAM) = 0;

	Enable();
}

static void update_restart_counter(struct A314Device *dev)
{
	Disable();

	ULONG cpa = dev->clockport_address;
	*CP_REG_PTR(cpa, REG_ADDR_LO) = (CAP_BASE + 4) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((CAP_BASE + 4) >> 8) & 0xff;

	UBYTE restart_counter = *CP_REG_PTR(cpa, REG_SRAM);

	*CP_REG_PTR(cpa, REG_ADDR_LO) = (CAP_BASE + 4) & 0xff;
	*CP_REG_PTR(cpa, REG_ADDR_HI) = ((CAP_BASE + 4) >> 8) & 0xff;

	*CP_REG_PTR(cpa, REG_SRAM) = restart_counter + 1;
	*CP_REG_PTR(cpa, REG_SRAM) = 0xa3;
	*CP_REG_PTR(cpa, REG_SRAM) = 0x14;

	Enable();
}

static void add_interrupt_handler(struct A314Device *dev)
{
	memset(&dev->int_x_interrupt, 0, sizeof(struct Interrupt));
	dev->int_x_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->int_x_interrupt.is_Node.ln_Pri = 0;
	dev->int_x_interrupt.is_Node.ln_Name = (char *)device_name;
	dev->int_x_interrupt.is_Data = (APTR)dev;
	dev->int_x_interrupt.is_Code = IntServer;

	LONG int_num = dev->interrupt_number == 6 ? INTB_EXTER :
		(dev->interrupt_number == 2 ? INTB_PORTS : INTB_VERTB);
	AddIntServer(int_num, &dev->int_x_interrupt);
}

void setup_pi_interface(struct A314Device *dev)
{
	init_memory_allocator(dev);

	memset(&dev->cap, 0, sizeof(dev->cap));

	clear_cap(dev);

	clear_amiga_irq(dev);

	add_interrupt_handler(dev);

	// FIXME: The current scheme is that a314d is notified that a314.device
	// has restarted through a restart counter variable.
	// Another option would be to have an event that specifically signals that
	// a314.device has restarted.
	// A third option is for the interface to monitor the RESET_n signal,
	// and notify a314d that the Amiga has been reset/restarted.

	update_restart_counter(dev);

	set_pi_irq(dev);
}
