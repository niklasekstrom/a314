#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include <string.h>

#include "a314.h"
#include "fix_mem_region.h"
#include "cmem.h"
#include "device.h"

#define SysBase (*(struct ExecBase **)4)

#define FW_FLAG_AUTODETECT	1
#define FW_FLAG_A600		2

#define HALF_MB (512*1024)
#define ONE_MB (1024*1024)
#define TWO_MB (2*1024*1024)

extern ULONG check_a314_mapping(__reg("a0") void *address);

struct MemChunkList
{
	struct MemChunk *first;
	struct MemChunk *last;
	ULONG free;
};

static void add_chunk(struct MemChunkList *l, struct MemChunk *mc)
{
	if (l->first == NULL)
		l->first = mc;
	else
		l->last->mc_Next = mc;
	l->last = mc;
	l->free += mc->mc_Bytes;
}

static struct MemHeader *split_region(struct MemHeader *lower, ULONG split_at)
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

static BOOL overlap(struct MemHeader *mh, ULONG lower, ULONG upper)
{
	return lower < (ULONG)(mh->mh_Upper) && (ULONG)(mh->mh_Lower) < upper;
}

static void mark_region_a314(ULONG address, ULONG size)
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

static void detect_block_mapping(ULONG present_blocks, ULONG *bank_address)
{
	struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
	if (!GfxBase)
		return;

	Disable();
	WaitBlit();

	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);

	for (ULONG block = 0; block < 32; block++)
	{
		if (present_blocks & (1UL << block))
		{
			ULONG address = block << 19;
			ULONG sram_address = check_a314_mapping((void *)address);

			if ((sram_address & 1) == 0)
			{
				BOOL match = FALSE;
				for (ULONG bank = 0; bank < 4; bank++)
				{
					if (sram_address == bank * HALF_MB)
					{
						if (bank_address[bank] != -1)
						{
							// This is not the first address that maps to this A314 block.
							// I currently don't understand how this could happen.
							// This should be logged, so that it can be investigated further.
						}

						bank_address[bank] = address;
						match = TRUE;
						break;
					}
				}

				if (!match)
				{
					// I currently don't understand how this could happen.
					// This should be logged, so that it can be investigated further.
				}
			}
		}
	}

	write_cp_nibble(13, prev_regd);
	Enable();

	CloseLibrary((struct Library *)GfxBase);
}

static void detect_block_mapping_heuristically(ULONG present_blocks, ULONG *bank_address)
{
	if ((present_blocks & 0xc) == 0xc)
	{
		bank_address[0] = ONE_MB;
		bank_address[1] = ONE_MB + HALF_MB;
	}
	else if (present_blocks & 0x01000000)
		bank_address[0] = 0xc00000;
	else if (present_blocks & 0x00000002)
		bank_address[0] = HALF_MB;
}

static ULONG get_present_blocks()
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

BOOL fix_memory(struct A314Device *dev)
{
	ULONG *bank_address = dev->bank_address;
	for (int i = 0; i < 4; i++)
		bank_address[i] = -1;

	dev->is_a600 = FALSE;

	Forbid();

	ULONG present_blocks = get_present_blocks();

	// Only test blocks in chip mem and slow mem ranges.
	present_blocks &= 0x0700000f; // 0b00000111000000000000000000001111

	if (dev->fw_flags & FW_FLAG_A600)
	{
		dev->is_a600 = TRUE;
		detect_block_mapping(present_blocks, bank_address);

		BOOL all_present = TRUE;
		for (int i = 0; i < 4; i++)
			if (bank_address[i] != i * HALF_MB)
				all_present = FALSE;

		if (all_present)
			mark_region_a314(0, TWO_MB);

		Permit();
		return all_present;
	}

	if (dev->fw_flags & FW_FLAG_AUTODETECT)
		detect_block_mapping(present_blocks, bank_address);
	else
		detect_block_mapping_heuristically(present_blocks, bank_address);

	if (bank_address[0] == -1 && bank_address[1] == -1)
	{
		// Didn't detect any A314 memory; the a314.device Open() should fail.
		// Should log the reason why this happened.
		Permit();
		return FALSE;
	}

	if (bank_address[0] == -1)
	{
		mark_region_a314(bank_address[1], HALF_MB);
	}
	else if (bank_address[1] == -1)
	{
		mark_region_a314(bank_address[0], HALF_MB);
	}
	else
	{
		ULONG lo = bank_address[0];
		ULONG hi = bank_address[1];
		if (lo > hi)
		{
			lo = bank_address[1];
			hi = bank_address[0];
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

static ULONG cpu_to_a314_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address)
{
	if (dev->is_a600)
	{
		ULONG offset = (ULONG)address;
		return offset < TWO_MB ? offset : INVALID_A314_ADDRESS;
	}

	if (dev->bank_address[0] != -1)
	{
		ULONG offset = (ULONG)address - dev->bank_address[0];
		if (offset < HALF_MB)
			return offset;
	}

	if (dev->bank_address[1] != -1)
	{
		ULONG offset = (ULONG)address - dev->bank_address[1];
		if (offset < HALF_MB)
			return HALF_MB + offset;
	}

	return INVALID_A314_ADDRESS;
}

static void *a314_to_cpu_address(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address)
{
	ULONG bank = address >> 19;
	ULONG offset = address & ((1 << 19) - 1);
	return (void *)(dev->bank_address[bank] + offset);
}

ULONG a314base_translate_address(__reg("a6") struct A314Device *dev, __reg("a0") void *address)
{
	return cpu_to_a314_address(dev, address);
}

ULONG a314base_alloc_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG length)
{
	void *p = AllocMem(length, MEMF_A314);
	if (!p)
		return INVALID_A314_ADDRESS;
	return cpu_to_a314_address(dev, p);
}

void a314base_free_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	void *p = a314_to_cpu_address(dev, address);
	FreeMem(p, length);
}

void a314base_write_mem(__reg("a6") struct A314Device *dev, __reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length)
{
	UBYTE *dst = a314_to_cpu_address(dev, address);
	memcpy(dst, src, length);
}

void a314base_read_mem(__reg("a6") struct A314Device *dev, __reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	UBYTE *src = a314_to_cpu_address(dev, address);
	memcpy(dst, src, length);
}
