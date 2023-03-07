#ifndef __A314_DEVICE_H
#define __A314_DEVICE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <libraries/dos.h>

#include "protocol.h"

struct A314Device
{
	struct Library lib;

	BPTR saved_seg_list;
	BOOL running;

#if defined(MODEL_TD)
	ULONG bank_address[4];
	UWORD is_a600;

	ULONG fw_flags;

	struct ComArea *ca;

	struct Interrupt vertb_interrupt;
	struct Interrupt ports_interrupt;
#elif defined(MODEL_CP)
	struct ComAreaPtrs cap;

	void *first_chunk;

	struct Interrupt exter_interrupt;
#endif

	struct Task task;
	struct MsgPort task_mp;

	struct List active_sockets;

	struct Socket *send_queue_head;
	struct Socket *send_queue_tail;

	UBYTE next_stream_id;
};

#if defined(MODEL_TD)
#define CAP_PTR(dev) (&(dev->ca->cap))
#elif defined(MODEL_CP)
#define CAP_PTR(dev) (&(dev->cap))
#endif

extern char device_name[];
extern char id_string[];

#endif /* __A314_DEVICE_H */
