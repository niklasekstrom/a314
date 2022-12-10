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
#include "cp_pi_if.h"
#include "memory_allocator.h"

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
	task->tc_Node.ln_Name = device_name;
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
	mp->mp_Node.ln_Name = device_name;
	mp->mp_Node.ln_Pri = 0;
	mp->mp_Node.ln_Type = NT_MSGPORT;
	mp->mp_Flags = PA_SIGNAL;
	mp->mp_SigBit = SIGB_MSGPORT;
	mp->mp_SigTask = &dev->task;
	NewList(&(mp->mp_MsgList));
}

BOOL task_start(struct A314Device *dev)
{
	if (!probe_interface())
		return FALSE;

	if (!setup_task(dev))
	{
		dbg("Unable to create task stack\n");
		return FALSE;
	}

	init_memory_allocator(dev);
	init_message_port(dev);
	init_sockets(dev);

	setup_cp_pi_if(dev);

	AddTask(&dev->task, (void *)task_main, 0);

	return TRUE;
}
