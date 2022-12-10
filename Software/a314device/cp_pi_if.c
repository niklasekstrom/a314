#include <exec/types.h>
#include <devices/timer.h>

#include <proto/exec.h>

#include "cp_pi_if.h"
#include "debug.h"
#include "protocol.h"

#define CLOCK_PORT_ADDRESS	0xd80000

#define REG_SRAM	0
#define REG_IRQ		1
#define REG_ADDR_LO	2
#define REG_ADDR_HI	3

#define REG_IRQ_SET	0x80
#define REG_IRQ_CLR	0x00
#define REG_IRQ_PI	0x02
#define REG_IRQ_CP	0x01

#define CP_REG_PTR(reg) ((volatile UBYTE *)CLOCK_PORT_ADDRESS + (reg << 2) + 3)

#define SysBase (*(struct ExecBase **)4)

void set_pi_irq()
{
	*CP_REG_PTR(REG_IRQ) = REG_IRQ_SET | REG_IRQ_PI;
}

void clear_cp_irq()
{
	*CP_REG_PTR(REG_IRQ) = REG_IRQ_CLR | REG_IRQ_CP;
}

void a314base_write_mem(__reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_write_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	for (int i = 0; i < length; i++)
		*p = *src++;

	Enable();
}

void a314base_read_mem(__reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_read_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	for (int i = 0; i < length; i++)
		*dst++ = *p;

	Enable();
}

void clear_cap()
{
	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = CAP_BASE & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (CAP_BASE >> 8) & 0xff;

	for (int i = 0; i < 4; i++)
		*CP_REG_PTR(REG_SRAM) = 0;

	Enable();
}

void update_restart_counter()
{
	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = (CAP_BASE + 4) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((CAP_BASE + 4) >> 8) & 0xff;

	UBYTE restart_counter = *CP_REG_PTR(REG_SRAM);

	*CP_REG_PTR(REG_ADDR_LO) = (CAP_BASE + 4) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((CAP_BASE + 4) >> 8) & 0xff;

	*CP_REG_PTR(REG_SRAM) = restart_counter + 1;
	*CP_REG_PTR(REG_SRAM) = 0xa3;
	*CP_REG_PTR(REG_SRAM) = 0x14;

	Enable();
}

void read_pi_cap(struct ComAreaPtrs *cap)
{
	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = (CAP_BASE + 0) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((CAP_BASE + 0) >> 8) & 0xff;

	cap->r2a_tail = *CP_REG_PTR(REG_SRAM);
	cap->a2r_head = *CP_REG_PTR(REG_SRAM);

	Enable();
}

void write_amiga_cap(struct ComAreaPtrs *cap)
{
	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = (CAP_BASE + 2) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((CAP_BASE + 2) >> 8) & 0xff;

	*CP_REG_PTR(REG_SRAM) = cap->a2r_tail;
	*CP_REG_PTR(REG_SRAM) = cap->r2a_head;

	Enable();
}

void read_from_r2a(UBYTE *dst, UBYTE offset, int length)
{
	if (!length)
		return;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = (R2A_BASE + offset) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((R2A_BASE + offset) >> 8) & 0xff;

	for (int i = 0; i < length; i++)
	{
		*dst++ = *p;
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(REG_ADDR_LO) = (R2A_BASE + 0) & 0xff;
			*CP_REG_PTR(REG_ADDR_HI) = ((R2A_BASE + 0) >> 8) & 0xff;
		}
	}

	Enable();
}

void write_to_a2r(struct ComAreaPtrs *cap, UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data)
{
	dbg_trace("Enter: write_to_a2r, type=$b, stream_id=$b, length=$b", type, stream_id, length);

	struct PktHdr hdr = {length, type, stream_id};

	Disable();

	UBYTE offset = cap->a2r_tail;

	*CP_REG_PTR(REG_ADDR_LO) = (A2R_BASE + offset) & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = ((A2R_BASE + offset) >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	for (int i = 0; i < sizeof(hdr); i++)
	{
		*p = ((UBYTE *)&hdr)[i];
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(REG_ADDR_LO) = (A2R_BASE + 0) & 0xff;
			*CP_REG_PTR(REG_ADDR_HI) = ((A2R_BASE + 0) >> 8) & 0xff;
		}
	}

	for (int i = 0; i < length; i++)
	{
		*p = *data++;
		offset++;
		if (offset == 0)
		{
			*CP_REG_PTR(REG_ADDR_LO) = (A2R_BASE + 0) & 0xff;
			*CP_REG_PTR(REG_ADDR_HI) = ((A2R_BASE + 0) >> 8) & 0xff;
		}
	}

	cap->a2r_tail = offset;

	Enable();
}

static int probe_interface_once()
{
	int found = FALSE;

	Disable();
	*CP_REG_PTR(REG_ADDR_HI) = CAP_BASE >> 8;
	*CP_REG_PTR(REG_ADDR_LO) = 6;
	*CP_REG_PTR(REG_SRAM) = 0x99;
	*CP_REG_PTR(REG_SRAM) = 0xbd;
	*CP_REG_PTR(REG_ADDR_LO) = 7;
	if (*CP_REG_PTR(REG_SRAM) == 0xbd)
	{
		*CP_REG_PTR(REG_ADDR_LO) = 6;
		if (*CP_REG_PTR(REG_SRAM) == 0x99)
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

int probe_interface()
{
	for (int i = 7; i >= 0; i--)
	{
		if (probe_interface_once())
			return TRUE;

		if (i == 0 || !delay_1s())
			break;
	}
	return FALSE;
}
