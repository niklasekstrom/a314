#include <proto/exec.h>

#include "cmem.h"

#define SysBase (*(struct ExecBase **)4)

#define CLOCK_PORT_ADDRESS	0xdc0000
#define BASE_ADDRESS_LEN	6

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

void write_cmem_safe(int index, UBYTE value)
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	write_cp_nibble(index, value);
	write_cp_nibble(13, prev_regd);
	Enable();
}

UBYTE read_cmem_safe(int index)
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	UBYTE value = read_cp_nibble(index);
	write_cp_nibble(13, prev_regd);
	Enable();
	return value;
}

void write_base_address(ULONG ba)
{
	ba |= 1;

	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);

	write_cp_nibble(0, 0);

	for (int i = BASE_ADDRESS_LEN - 1; i >= 0; i--)
	{
		ULONG v = (ba >> (i * 4)) & 0xf;
		write_cp_nibble(i, (UBYTE)v);
	}

	write_cp_nibble(13, prev_regd);
	Enable();
}

ULONG read_fw_flags()
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);

	write_cp_nibble(10, 0);

	ULONG flags = 0;
	for (int i = 0; i < 4; i++)
		flags |= ((ULONG)read_cp_nibble(10)) << (4 * i);

	write_cp_nibble(13, prev_regd);
	Enable();

	return flags;
}
