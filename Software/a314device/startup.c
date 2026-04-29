#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>

#include <proto/exec.h>

#include <string.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "debug.h"
#include "pi_if.h"

#define SysBase (*(struct ExecBase **)4)

#define TASK_PRIORITY 80
#define TASK_STACK_SIZE 1024

extern void task_main();
extern void init_sockets(struct A314Device *dev);

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

static BOOL setup_task(struct A314Device *dev)
{
	char *stack = AllocMem(TASK_STACK_SIZE, MEMF_CLEAR);
	if (stack == NULL)
		return FALSE;

	struct Task *task = &dev->task;
	memset(task, 0, sizeof(struct Task));
	task->tc_Node.ln_Type = NT_TASK;
	task->tc_Node.ln_Pri = TASK_PRIORITY;
	task->tc_Node.ln_Name = (char *)device_name;
	task->tc_SPLower = (APTR)stack;
	task->tc_SPUpper = (APTR)(stack + TASK_STACK_SIZE);
	task->tc_SPReg = (APTR)(stack + TASK_STACK_SIZE);
	task->tc_UserData = (void *)dev;

	return TRUE;
}

static void init_message_port(struct A314Device *dev)
{
	struct MsgPort *mp = &dev->task_mp;
	memset(mp, 0, sizeof(struct MsgPort));
	mp->mp_Node.ln_Name = (char *)device_name;
	mp->mp_Node.ln_Pri = 0;
	mp->mp_Node.ln_Type = NT_MSGPORT;
	mp->mp_Flags = PA_SIGNAL;
	mp->mp_SigBit = SIGB_MSGPORT;
	mp->mp_SigTask = &dev->task;
	NewList(&(mp->mp_MsgList));
}

void init_bounce_buffer(struct A314Device *dev)
{
	dev->bounce_buffer_address = a314base_alloc_mem(dev, BOUNCE_BUFFER_SLOTS * BOUNCE_SLOT_SIZE);

	if (dev->bounce_buffer_address != INVALID_A314_ADDRESS)
	{
		dev->bounce_slot_count = BOUNCE_BUFFER_SLOTS / 2;

		struct PktBounceAllocated pkt = {dev->bounce_buffer_address, BOUNCE_SLOT_SIZE, BOUNCE_BUFFER_SLOTS};

		write_to_a2r(dev, PKT_BOUNCE_ALLOCATED, 0, sizeof(pkt), (UBYTE *)&pkt);
	}
}

BOOL task_start(struct A314Device *dev)
{
	if (!setup_task(dev))
	{
		dbg_error("Unable to create task stack\n");
		return FALSE;
	}

	if (!probe_pi_interface(dev))
	{
		FreeMem(dev->task.tc_SPLower, TASK_STACK_SIZE);
		return FALSE;
	}

	init_message_port(dev);
	init_sockets(dev);

	setup_pi_interface(dev);

	init_bounce_buffer(dev);

	AddTask(&dev->task, (void *)task_main, 0);

	return TRUE;
}
