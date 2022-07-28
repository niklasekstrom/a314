#include <exec/types.h>

#include <proto/exec.h>

#include "cp_pi_if.h"
#include "debug.h"

#define SysBase (*(struct ExecBase **)4)

void set_pi_irq()
{
	*CP_REG_PTR(REG_IRQ) = REG_IRQ_SET | REG_IRQ_PI;
}

void clear_cp_irq()
{
	*CP_REG_PTR(REG_IRQ) = REG_IRQ_CLR | REG_IRQ_CP;
}

void set_cp_address(USHORT address)
{
	*CP_REG_PTR(REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (address >> 8) & 0xff;
}

void a314base_write_mem(__reg("d0") ULONG address, __reg("a0") UBYTE *src, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_write_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	for (int i = 0; i < length; i++)
		*p = *src++;

	Enable();
}

void a314base_read_mem(__reg("a0") UBYTE *dst, __reg("d0") ULONG address, __reg("d1") ULONG length)
{
	dbg_trace("Enter a314base_read_mem, address=$l, length=$l", address, length);

	if (!length)
		return;

	Disable();

	*CP_REG_PTR(REG_ADDR_LO) = address & 0xff;
	*CP_REG_PTR(REG_ADDR_HI) = (address >> 8) & 0xff;

	volatile UBYTE *p = CP_REG_PTR(REG_SRAM);

	for (int i = 0; i < length; i++)
		*dst++ = *p;

	Enable();
}
