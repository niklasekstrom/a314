#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "fix_mem_region.h"
#include "cmem.h"
#include "debug.h"

struct MsgPort task_mp;
struct Task *task;
struct ComArea *ca;

extern void task_main();
extern void init_sockets();

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

struct Task *CreateTask(char *name, long priority, char *initialPC, unsigned long stacksize)
{
	char *stack = AllocMem(stacksize, MEMF_CLEAR);
	if (stack == NULL)
		return NULL;

	struct Task *tc = AllocMem(sizeof(struct Task), MEMF_CLEAR | MEMF_PUBLIC);
	if (tc == NULL)
	{
		FreeMem(stack, stacksize);
		return NULL;
	}

	tc->tc_Node.ln_Type = NT_TASK;
	tc->tc_Node.ln_Pri = priority;
	tc->tc_Node.ln_Name = name;
	tc->tc_SPLower = (APTR)stack;
	tc->tc_SPUpper = (APTR)(stack + stacksize);
	tc->tc_SPReg = (APTR)(stack + stacksize);

	AddTask(tc, initialPC, 0);
	return tc;
}

void fix_address_mapping()
{
	// Only looking at VPOSR Agnus identification at this point.
	// Could add more dynamic identification of address mapping.

	UWORD vposr = *(UWORD *)0xdff004;
	UWORD agnus = (vposr & 0x7f00) >> 8;

	UBYTE swap = (agnus == 0x00 || agnus == 0x10) ? 0x1 : 0x0;

	write_cmem_safe(11, swap);
}

BOOL task_start()
{
	if (!fix_memory())
		return FALSE;

	fix_address_mapping();

	ca = (struct ComArea *)AllocMem(sizeof(struct ComArea), MEMF_A314 | MEMF_CLEAR);
	if (ca == NULL)
	{
		debug_printf("Unable to allocate A314 memory for com area\n");
		return FALSE;
	}

	task = CreateTask(device_name, 80, (void *)task_main, 1024);
	if (task == NULL)
	{
		debug_printf("Unable to create task\n");
		FreeMem(ca, sizeof(struct ComArea));
		return FALSE;
	}

	task_mp.mp_Node.ln_Name = device_name;
	task_mp.mp_Node.ln_Pri = 0;
	task_mp.mp_Node.ln_Type = NT_MSGPORT;
	task_mp.mp_Flags = PA_SIGNAL;
	task_mp.mp_SigBit = SIGB_MSGPORT;
	task_mp.mp_SigTask = task;

	NewList(&(task_mp.mp_MsgList));

	init_sockets();

	return TRUE;
}
