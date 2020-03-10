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

void mark_region_a314(ULONG address, ULONG size)
{
	struct List *memlist = &(SysBase->MemList);

	for (struct Node *node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct MemHeader *mh = (struct MemHeader *)node;
		if (overlap(mh, address, address + size))
		{
			if ((ULONG)mh->mh_Lower < address)
			{
				mh->mh_Attributes &= ~MEMF_A314;
				mh = split_region(mh, address);
			}
			else
				Remove((struct Node *)mh);

			if (address + size < (ULONG)mh->mh_Upper)
			{
				struct MemHeader *new_mh = split_region(mh, address + size);
				new_mh->mh_Attributes &= ~MEMF_A314;
				Enqueue(memlist, (struct Node *)new_mh);
			}

			mh->mh_Node.ln_Pri = -20;
			mh->mh_Attributes |= MEMF_A314;
			Enqueue(memlist, (struct Node *)mh);
			return;
		}
	}
}

void detect_block_mapping_heuristically(ULONG present_blocks)
{
	if ((present_blocks & 0xc) == 0xc)
	{
		lower_bank_address = ONE_MB;
		upper_bank_address = ONE_MB + HALF_MB;
	}
	else if (present_blocks & 0x01000000)
		lower_bank_address = 0xc00000;
	else if (present_blocks & 0x00000002)
		lower_bank_address = HALF_MB;
}

ULONG get_present_blocks()
{
	ULONG present_blocks = 0;

	for (struct Node *node = SysBase->MemList.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct MemHeader *mh = (struct MemHeader *)node;
		ULONG block = (ULONG)mh->mh_Lower >> 19;
		ULONG address = block << 19;
		while (address < (ULONG)mh->mh_Upper)
		{
			present_blocks |= 1UL << block;
			block++;
			address = block << 19;
		}
	}

	return present_blocks;
}

BOOL fix_memory()
{
	Forbid();

	ULONG present_blocks = get_present_blocks();

	// Only test blocks in chip mem and slow mem ranges.
	present_blocks &= 0x0700000f; // 0b00000111000000000000000000001111

	detect_block_mapping_heuristically(present_blocks);

	if (lower_bank_address == -1 && upper_bank_address == -1)
	{
		// Didn't detect any A314 memory; the a314.device Open() should fail.
		// Should log the reason why this happened.
		Permit();
		return FALSE;
	}

	if (lower_bank_address == -1)
	{
		mark_region_a314(upper_bank_address, HALF_MB);
	}
	else if (upper_bank_address == -1)
	{
		mark_region_a314(lower_bank_address, HALF_MB);
	}
	else
	{
		ULONG lo = lower_bank_address;
		ULONG hi = upper_bank_address;
		if (lo > hi)
		{
			lo = upper_bank_address;
			hi = lower_bank_address;
		}

		if (lo + HALF_MB == hi)
		{
			mark_region_a314(lo, ONE_MB);
		}
		else
		{
			mark_region_a314(lo, HALF_MB);
			mark_region_a314(hi, HALF_MB);
		}
	}

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
