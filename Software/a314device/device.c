#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/ports.h>
#include <libraries/dos.h>
#include <proto/exec.h>

#include "device.h"
#include "a314.h"
#include "startup.h"
#include "fix_mem_region.h"

#define SysBase (*(struct ExecBase **)4)

char device_name[] = A314_NAME;
char id_string[] = A314_NAME " 1.0 (25 Aug 2018)";

static struct Library *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct A314Device *dev)
{
	dev->saved_seg_list = seg_list;
	dev->running = FALSE;

	// We are being called from InitResident() in initializers.asm.
	// MakeLibrary() was executed before we got here.

	dev->lib.lib_Node.ln_Type = NT_DEVICE;
	dev->lib.lib_Node.ln_Name = device_name;
	dev->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	dev->lib.lib_Version = 1;
	dev->lib.lib_Revision = 0;
	dev->lib.lib_IdString = (APTR)id_string;

	// AddDevice() is executed after we return.
	return &dev->lib;
}

static BPTR expunge(__reg("a6") struct A314Device *dev)
{
	// There is currently no support for unloading a314.device.

	if (TRUE) //dev->lib_OpenCnt != 0)
	{
		dev->lib.lib_Flags |= LIBF_DELEXP;
		return 0;
	}

	/*
	BPTR seg_list = saved_seg_list;
	Remove(&dev->lib_Node);
	FreeMem((char *)dev - dev->lib_NegSize, dev->lib_NegSize + dev->lib_PosSize);
	return seg_list;
	*/
}

static void open(__reg("a6") struct A314Device *dev, __reg("a1") struct A314_IORequest *ior, __reg("d0") ULONG unitnum, __reg("d1") ULONG flags)
{
	dev->lib.lib_OpenCnt++;

	if (dev->lib.lib_OpenCnt == 1 && !dev->running)
	{
		if (!task_start(dev))
		{
			ior->a314_Request.io_Error = IOERR_OPENFAIL;
			ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
			dev->lib.lib_OpenCnt--;
			return;
		}
		dev->running = TRUE;
	}

	ior->a314_Request.io_Error = 0;
	ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
}

static BPTR close(__reg("a6") struct A314Device *dev, __reg("a1") struct A314_IORequest *ior)
{
	ior->a314_Request.io_Device = NULL;
	ior->a314_Request.io_Unit = NULL;

	dev->lib.lib_OpenCnt--;

	if (dev->lib.lib_OpenCnt == 0 && (dev->lib.lib_Flags & LIBF_DELEXP))
		return expunge(dev);

	return 0;
}

static void begin_io(__reg("a6") struct A314Device *dev, __reg("a1") struct A314_IORequest *ior)
{
	PutMsg(&dev->task_mp, (struct Message *)ior);
	ior->a314_Request.io_Flags &= ~IOF_QUICK;
}

static ULONG abort_io(__reg("a6") struct A314Device *dev, __reg("a1") struct A314_IORequest *ior)
{
	// There is currently no support for aborting an IORequest.
	return IOERR_NOCMD;
}

static ULONG device_vectors[] =
{
	(ULONG)open,
	(ULONG)close,
	(ULONG)expunge,
	0,
	(ULONG)begin_io,
	(ULONG)abort_io,
	(ULONG)a314base_translate_address,
	(ULONG)a314base_alloc_mem,
	(ULONG)a314base_free_mem,
	(ULONG)a314base_write_mem,
	(ULONG)a314base_read_mem,
	-1,
};

ULONG auto_init_tables[] =
{
	sizeof(struct A314Device),
	(ULONG)device_vectors,
	0,
	(ULONG)init_device,
};
