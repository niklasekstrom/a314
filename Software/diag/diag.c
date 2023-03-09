#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <stdio.h>
#include <string.h>

#include <proto/exec.h>

void print_agnus_id()
{
	UWORD vposr = *(UWORD *)0xdff004;
	UWORD agnus = (vposr & 0x7f00) >> 8;
	printf("Agnus id: %02x\n", agnus);
}

void print_memory_regions()
{
	Forbid();

	struct List *memlist = &(SysBase->MemList);
	struct Node *node;

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct MemHeader *mh = (struct MemHeader *)node;
		printf("Memory region:\n");
		printf("  Attributes: %8x\n", mh->mh_Attributes);
		printf("  Lower:      %8lx\n", (ULONG)mh->mh_Lower);
		printf("  Upper:      %8lx\n", (ULONG)mh->mh_Upper);
	}

	Permit();
}

void dump_mem(ULONG address, int len)
{
	printf("Dumping %d bytes starting at address %08lx:\n", len, address);
	UBYTE *p = (UBYTE *)address;
	int col = 0;
	while (len)
	{
		if (col == 0)
			printf("  ");
		printf("%02x ", *p++);
		col++;
		if (col == 16)
		{
			printf("\n");
			col = 0;
		}
		len--;
	}
}

#define CLOCK_PORT_ADDRESS	0xdc0000

void write_cp_nibble(int index, UBYTE value)
{
	volatile UBYTE *p = (UBYTE *)CLOCK_PORT_ADDRESS;
	p += 4 * index + 3;
	*p = value & 0xf;
}

UBYTE read_cp_nibble(int index)
{
	volatile UBYTE *p = (UBYTE *)CLOCK_PORT_ADDRESS;
	p += 4 * index + 3;
	return *p & 0xf;
}

void print_usage()
{
	printf( "Usage: diag command [args]\n" \
			" Commands:\n" \
			"   a -- print agnus id\n" \
			"   m -- print memory regions\n" \
			"   d <addr> <len> -- dump <len> bytes starting at address <addr>\n" \
			"   w <reg> <val> -- write <val> to cmem register <reg>\n" \
			"   r <reg> [<num>] -- read and print <num> cmem regs. starting at <reg>\n");
}

int main(int argc, char **argv)
{
	printf("Diagnostics for A314-td\n");
	if (argc == 1)
	{
		print_usage();
		return 0;
	}

	if (strcmp(argv[1], "a") == 0)
	{
		print_agnus_id();
	}
	else if (strcmp(argv[1], "m") == 0)
	{
		print_memory_regions();
	}
	else if (strcmp(argv[1], "d") == 0)
	{
		if (argc < 4)
		{
			print_usage();
			return 0;
		}

		ULONG address;
		ULONG length;
		sscanf(argv[2], "%lx", &address);
		sscanf(argv[3], "%lu", &length);
		dump_mem(address, length);
	}
	else if (strcmp(argv[1], "w") == 0)
	{
		if (argc < 4)
		{
			print_usage();
			return 0;
		}

		LONG reg;
		LONG val;
		sscanf(argv[2], "%ld", &reg);
		sscanf(argv[3], "%ld", &val);

		if (reg < 0 || reg > 15)
		{
			printf("reg must be in range 0..15\n");
			return 0;
		}
		if (val < 0 || val > 15)
		{
			printf("val must be in range 0..15\n");
			return 0;
		}

		Disable();
		UBYTE prev_regd = read_cp_nibble(13);
		write_cp_nibble(13, prev_regd | 8);
		write_cp_nibble(reg, val);
		write_cp_nibble(13, prev_regd);
		Enable();

		printf("Wrote value %lx to cmem regiser %ld\n", val, reg);
	}
	else if (strcmp(argv[1], "r") == 0)
	{
		if (argc < 3)
		{
			print_usage();
			return 0;
		}

		LONG reg;
		LONG num = 1;
		sscanf(argv[2], "%ld", &reg);
		if (argc >= 4)
			sscanf(argv[3], "%ld", &num);

		if (reg < 0 || reg > 15)
		{
			printf("reg must be in range 0..15\n");
			return 0;
		}
		if (num < 1 || num > 16)
		{
			printf("num must be in range 1..16\n");
			return 0;
		}

		printf("Dumping %ld cmem registers starting at register %ld:\n", num, reg);

		Disable();
		UBYTE prev_regd = read_cp_nibble(13);
		write_cp_nibble(13, prev_regd | 8);

		char buf[64];
		char *p = buf;

		while (num)
		{
			UBYTE val = read_cp_nibble(reg);
			if (val < 10)
				*p++ = '0' + val;
			else
				*p++ = 'a' + val - 10;
			*p++ = ' ';
			reg++;
			num--;
		}

		*p++ = 0;

		write_cp_nibble(13, prev_regd);
		Enable();

		printf("  %s\n", buf);
	}
	else
		print_usage();
	
	return 0;
}
