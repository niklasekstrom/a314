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
#include "fix_mem_region.h"
#include "cmem.h"
#include "debug.h"

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

BOOL task_start(struct A314Device *dev)
{
	dev->fw_flags = read_fw_flags();

	if (!fix_memory(dev))
		return FALSE;

	detect_and_write_address_swap();

	dev->ca = (struct ComArea *)AllocMem(sizeof(struct ComArea), MEMF_A314 | MEMF_CLEAR);
	if (dev->ca == NULL)
	{
		debug_printf("Unable to allocate A314 memory for com area\n");
		return FALSE;
	}

	if (!create_task(dev))
	{
		debug_printf("Unable to create task stack\n");
		FreeMem(dev->ca, sizeof(struct ComArea));
		return FALSE;
	}

	init_message_port(dev);
	init_sockets(dev);

	write_cmem_safe(A_ENABLE_ADDRESS, 0);
	read_cmem_safe(A_EVENTS_ADDRESS);

	write_base_address(a314base_translate_address(dev, dev->ca));

	write_cmem_safe(R_EVENTS_ADDRESS, R_EVENT_BASE_ADDRESS);

	add_interrupt_handlers(dev);

	write_cmem_safe(A_ENABLE_ADDRESS, A_EVENT_R2A_TAIL);

	return TRUE;
}
