#include <exec/types.h>

#include <proto/exec.h>

#include "a314.h"
#include "device.h"
#include "debug.h"

#define SysBase (*(struct ExecBase **)4)

#define MEM_SIZE	65536
#define MEM_START	(512 + 8)

struct MyMemChunk
{
	struct MyMemChunk *next;
	USHORT address;
	USHORT length;
};

void init_memory_allocator(struct A314Device *dev)
{
	struct MyMemChunk *mc = (struct MyMemChunk *)AllocMem(sizeof(struct MyMemChunk), 0);
	mc->next = NULL;
	mc->address = MEM_START;
	mc->length = MEM_SIZE - MEM_START;
	dev->first_chunk = mc;
}

ULONG a314base_translate_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address)
{
	dbg_trace("Enter: a314base_translate_address");

	// It is an error if the application calls this routine.
	return INVALID_A314_ADDRESS;
}

ULONG a314base_alloc_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG length)
{
	dbg_trace("Enter: a314base_alloc_mem, length=$l", length);

	length = (length + 3) & ~3;

	Forbid();

	ULONG address = INVALID_A314_ADDRESS;
	struct MyMemChunk *prev = NULL;
	struct MyMemChunk *mc = dev->first_chunk;
	while (mc)
	{
		if (mc->length >= length)
		{
			address = mc->address;
			if (mc->length == length)
			{
				if (prev)
					prev->next = mc->next;
				else
					dev->first_chunk = mc->next;
				FreeMem(mc, sizeof(struct MyMemChunk));
			}
			else
			{
				mc->address += length;
				mc->length -= length;
			}
			break;
		}
		prev = mc;
		mc = mc->next;
	}

	Permit();

	dbg_trace("Leave: a314base_alloc_mem, address=$l", address);
	return address;
}

void a314base_free_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	dbg_trace("Enter: a314base_free_mem, address=$l, length=$l", address, length);

	length = (length + 3) & ~3;

	Forbid();

	struct MyMemChunk *prev = NULL;
	struct MyMemChunk *succ = dev->first_chunk;

	while (succ)
	{
		if (succ->address > address)
			break;
		prev = succ;
		succ = succ->next;
	}

	if (prev && prev->address + prev->length == address)
	{
		prev->length += length;

		if (succ && prev->address + prev->length == succ->address)
		{
			prev->length += succ->length;
			prev->next = succ->next;
			FreeMem(succ, sizeof(struct MyMemChunk));
		}
	}
	else if (succ && address + length == succ->address)
	{
		succ->address = address;
		succ->length += length;
	}
	else
	{
		struct MyMemChunk *mc = (struct MyMemChunk *)AllocMem(sizeof(struct MyMemChunk), 0);
		mc->next = succ;
		mc->address = address;
		mc->length = length;
		if (prev)
			prev->next = mc;
		else
			dev->first_chunk = mc;
	}

	Permit();
}
