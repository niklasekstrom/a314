#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "a314.h"
#include "fix_mem_region.h"

#define HALF_MB (512*1024)
#define ONE_MB (1024*1024)

ULONG lower_bank_address = -1;
ULONG upper_bank_address = -1;

struct MemChunkList
{
	struct MemChunk *first;
	struct MemChunk *last;
	ULONG free;
};

void add_chunk(struct MemChunkList *l, struct MemChunk *mc)
{
	if (l->first == NULL)
		l->first = mc;
	else
		l->last->mc_Next = mc;
	l->last = mc;
	l->free += mc->mc_Bytes;
}

struct MemHeader *split_region(struct MemHeader *lower, ULONG split_at)
{
	struct MemHeader *upper = (struct MemHeader *)AllocMem(sizeof(struct MemHeader), MEMF_PUBLIC | MEMF_CLEAR);

	struct MemChunkList ll = {NULL, NULL, 0};
	struct MemChunkList ul = {NULL, NULL, 0};

	struct MemChunk *mc = lower->mh_First;

	while (mc != NULL)
	{
		struct MemChunk *next_chunk = mc->mc_Next;
		mc->mc_Next = NULL;

		ULONG start = (ULONG)mc;
		ULONG end = start + mc->mc_Bytes;

		if (end <= split_at)
			add_chunk(&ll, mc);
		else if (split_at <= start)
			add_chunk(&ul, mc);
		else
		{
			mc->mc_Bytes = split_at - start;
			add_chunk(&ll, mc);

			struct MemChunk *new_chunk = (struct MemChunk *)split_at;
			new_chunk->mc_Next = NULL;
			new_chunk->mc_Bytes = end - split_at;
			add_chunk(&ul, new_chunk);
		}
		mc = next_chunk;
	}

	upper->mh_Node.ln_Type = NT_MEMORY;
	upper->mh_Node.ln_Pri = lower->mh_Node.ln_Pri;
	upper->mh_Node.ln_Name = lower->mh_Node.ln_Name; // Use a custom name?
	upper->mh_Attributes = lower->mh_Attributes;

	lower->mh_First = ll.first;
	upper->mh_First = ul.first;

	upper->mh_Lower = (APTR)split_at;
	upper->mh_Upper = lower->mh_Upper;
	lower->mh_Upper = (APTR)split_at;

	lower->mh_Free = ll.free;
	upper->mh_Free = ul.free;

	return upper;
}

BOOL overlap(struct MemHeader *mh, ULONG lower, ULONG upper)
{
	return lower < (ULONG)(mh->mh_Upper) && (ULONG)(mh->mh_Lower) < upper;
}

BOOL fix_memory()
{
	Forbid();

	struct List *memlist = &(SysBase->MemList);
	struct Node *node;
	struct MemHeader *mh;

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (mh->mh_Attributes & MEMF_A314)
		{
			lower_bank_address = (ULONG)(mh->mh_Lower) & ~(HALF_MB - 1);
			if (lower_bank_address == ONE_MB)
				upper_bank_address = lower_bank_address + HALF_MB;

			Permit();
			return TRUE;
		}
	}

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (overlap(mh, 0xc00000, 0xc80000))
		{
			Remove((struct Node *)mh);
			mh->mh_Node.ln_Pri = -20;
			mh->mh_Attributes |= MEMF_A314;
			Enqueue(memlist, (struct Node*)mh);

			lower_bank_address = 0xc00000;

			Permit();
			return TRUE;
		}
	}

	struct MemHeader *chip_mh = NULL;

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (overlap(mh, 0x0, 0x200000))
		{
			chip_mh = mh;
			break;
		}
	}

	if (chip_mh == NULL || (ULONG)(chip_mh->mh_Upper) <= 0x80000)
	{
		Permit();
		return FALSE;
	}

	// Split chip memory region into motherboard and A314 memory regions.
	ULONG split_at = (ULONG)(chip_mh->mh_Upper) > ONE_MB ? ONE_MB : HALF_MB;

	mh = split_region(chip_mh, split_at);
	mh->mh_Node.ln_Pri = -20;
	mh->mh_Attributes |= MEMF_A314;
	Enqueue(memlist, (struct Node*)mh);

	lower_bank_address = split_at;
	if (split_at == ONE_MB)
		upper_bank_address = lower_bank_address + HALF_MB;

	Permit();
	return TRUE;
}

ULONG translate_address_a314(__reg("a0") void *address)
{
	if (lower_bank_address != -1)
	{
		ULONG offset = (ULONG)address - lower_bank_address;
		if (offset < HALF_MB)
			return offset;
	}

	if (upper_bank_address != -1)
	{
		ULONG offset = (ULONG)address - upper_bank_address;
		if (offset < HALF_MB)
			return HALF_MB + offset;
	}

	return -1;
}
