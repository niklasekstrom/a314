#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>
#include <devices/timer.h>

#include <proto/exec.h>

#include <string.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "debug.h"
#include "cp_pi_if.h"
#include "memory_allocator.h"

#define SysBase (*(struct ExecBase **)4)

#define TASK_PRIORITY 80
#define TASK_STACK_SIZE 1024

extern void task_main();
extern void init_sockets(struct A314Device *dev);
extern void IntServer();

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

static BOOL create_task(struct A314Device *dev)
{
	char *stack = AllocMem(TASK_STACK_SIZE, MEMF_CLEAR);
	if (stack == NULL)
		return FALSE;

	struct Task *task = &dev->task;
	memset(task, 0, sizeof(struct Task));
	task->tc_Node.ln_Type = NT_TASK;
	task->tc_Node.ln_Pri = TASK_PRIORITY;
	task->tc_Node.ln_Name = device_name;
	task->tc_SPLower = (APTR)stack;
	task->tc_SPUpper = (APTR)(stack + TASK_STACK_SIZE);
	task->tc_SPReg = (APTR)(stack + TASK_STACK_SIZE);
	task->tc_UserData = (void *)dev;

	AddTask(task, (void *)task_main, 0);
	return TRUE;
}

static void init_message_port(struct A314Device *dev)
{
	struct MsgPort *mp = &dev->task_mp;
	memset(mp, 0, sizeof(struct MsgPort));
	mp->mp_Node.ln_Name = device_name;
	mp->mp_Node.ln_Pri = 0;
	mp->mp_Node.ln_Type = NT_MSGPORT;
	mp->mp_Flags = PA_SIGNAL;
	mp->mp_SigBit = SIGB_MSGPORT;
	mp->mp_SigTask = &dev->task;
	NewList(&(mp->mp_MsgList));
}

static void add_interrupt_handler(struct A314Device *dev)
{
	memset(&dev->exter_interrupt, 0, sizeof(struct Interrupt));
	dev->exter_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	dev->exter_interrupt.is_Node.ln_Pri = 0;
	dev->exter_interrupt.is_Node.ln_Name = device_name;
	dev->exter_interrupt.is_Data = (APTR)&dev->task;
	dev->exter_interrupt.is_Code = IntServer;

	AddIntServer(INTB_EXTER, &dev->exter_interrupt);
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

static int probe_interface()
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

static int probe_interface_retries()
{
	for (int i = 7; i >= 0; i--)
	{
		if (probe_interface())
			return TRUE;

		if (i == 0 || !delay_1s())
			break;
	}
	return FALSE;
}

BOOL task_start(struct A314Device *dev)
{
	if (!probe_interface_retries())
		return FALSE;

	memset(&dev->cap, 0, sizeof(dev->cap));

	Disable();
	set_cp_address(CAP_BASE);
	for (int i = 0; i < 4; i++)
		*CP_REG_PTR(REG_SRAM) = 0;
	Enable();

	init_memory_allocator(dev);

	if (!create_task(dev))
	{
		dbg("Unable to create task stack\n");
		return FALSE;
	}

	init_message_port(dev);
	init_sockets(dev);

	clear_cp_irq();

	add_interrupt_handler(dev);

	// FIXME: The current scheme is that a314d is notified that a314.device
	// has restarted through a restart counter variable.
	// Another option would be to have an event that specifically signals that
	// a314.device has restarted.
	// A third option is for the interface to monitor the RESET_n signal,
	// and notify a314d that the Amiga has been reset/restarted.

	Disable();
	set_cp_address(CAP_BASE + 4);
	UBYTE restart_counter = *CP_REG_PTR(REG_SRAM);

	set_cp_address(CAP_BASE + 4);
	*CP_REG_PTR(REG_SRAM) = restart_counter + 1;
	*CP_REG_PTR(REG_SRAM) = 0xa3;
	*CP_REG_PTR(REG_SRAM) = 0x14;
	Enable();

	set_pi_irq();

	return TRUE;
}
