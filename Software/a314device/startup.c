#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>
#include <proto/exec.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "fix_mem_region.h"
#include "cmem.h"
#include "debug.h"

#define TASK_PRIORITY 80
#define TASK_STACK_SIZE 1024

UWORD fw_version;

struct MsgPort task_mp;
struct Task *task;
struct ComArea *ca;

struct Interrupt vertb_interrupt;
struct Interrupt ports_interrupt;

extern void task_main();
extern void init_sockets();
extern void IntServer();

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

static struct Task *create_task(char *name, long priority, char *initialPC, unsigned long stacksize)
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

static void init_message_port()
{
	task_mp.mp_Node.ln_Name = device_name;
	task_mp.mp_Node.ln_Pri = 0;
	task_mp.mp_Node.ln_Type = NT_MSGPORT;
	task_mp.mp_Flags = PA_SIGNAL;
	task_mp.mp_SigBit = SIGB_MSGPORT;
	task_mp.mp_SigTask = task;
	NewList(&(task_mp.mp_MsgList));
}

static void add_interrupt_handlers()
{
	vertb_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	vertb_interrupt.is_Node.ln_Pri = -60;
	vertb_interrupt.is_Node.ln_Name = device_name;
	vertb_interrupt.is_Data = (APTR)task;
	vertb_interrupt.is_Code = IntServer;

	AddIntServer(INTB_VERTB, &vertb_interrupt);

	ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	ports_interrupt.is_Node.ln_Pri = 0;
	ports_interrupt.is_Node.ln_Name = device_name;
	ports_interrupt.is_Data = (APTR)task;
	ports_interrupt.is_Code = IntServer;

	AddIntServer(INTB_PORTS, &ports_interrupt);
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

BOOL task_start()
{
	fw_version = read_fw_version();

	if (!fix_memory())
		return FALSE;

	detect_and_write_address_swap();

	ca = (struct ComArea *)AllocMem(sizeof(struct ComArea), MEMF_A314 | MEMF_CLEAR);
	if (ca == NULL)
	{
		debug_printf("Unable to allocate A314 memory for com area\n");
		return FALSE;
	}

	task = create_task(device_name, TASK_PRIORITY, (void *)task_main, TASK_STACK_SIZE);
	if (task == NULL)
	{
		debug_printf("Unable to create task\n");
		FreeMem(ca, sizeof(struct ComArea));
		return FALSE;
	}

	init_message_port();
	init_sockets();

	write_cmem_safe(A_ENABLE_ADDRESS, 0);
	read_cmem_safe(A_EVENTS_ADDRESS);

	write_base_address(translate_address_a314(ca));

	write_cmem_safe(R_EVENTS_ADDRESS, R_EVENT_BASE_ADDRESS);

	add_interrupt_handlers();

	write_cmem_safe(A_ENABLE_ADDRESS, A_EVENT_R2A_TAIL);

	return TRUE;
}
