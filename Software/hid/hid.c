/*
 * Copyright (c) 2022 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/interrupts.h>

#include <devices/input.h>
#include <devices/inputevent.h>
#include <libraries/dos.h>

#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"

#define SERVICE_NAME "hid"

static struct MsgPort *mp;
static ULONG socket;

static struct A314_IORequest *cmsg;
static struct A314_IORequest *rmsg;
static struct A314_IORequest *wmsg;

static UBYTE arbuf[256];
static UBYTE awbuf[256];

static BOOL pending_a314_read = FALSE;
static BOOL pending_a314_write = FALSE;
static BOOL pending_a314_reset = FALSE;

static BOOL stream_closed = FALSE;

static struct MsgPort *id_mp;
static struct IOStdReq *id_req;
static struct InputEvent generated_event;
static struct Interrupt input_event_handler;
static UWORD last_qualifier;

#pragma pack(push, 1)
struct EventMessage
{
	WORD dx;
	WORD dy;
	UWORD qualifier;
	UBYTE code;
};
#pragma pack(pop)

static void start_a314_cmd(struct A314_IORequest *msg, UWORD command, char *buffer, int length)
{
	msg->a314_Request.io_Command = command;
	msg->a314_Request.io_Error = 0;

	msg->a314_Socket = socket;
	msg->a314_Buffer = buffer;
	msg->a314_Length = length;

	SendIO((struct IORequest *)msg);
}

static LONG a314_connect(char *name)
{
	socket = time(NULL);
	start_a314_cmd(cmsg, A314_CONNECT, name, strlen(name));
	return WaitIO((struct IORequest *)cmsg);
}

static void start_a314_read()
{
	start_a314_cmd(rmsg, A314_READ, arbuf, 255);
	pending_a314_read = TRUE;
}

static void start_a314_write(int length)
{
	start_a314_cmd(wmsg, A314_WRITE, awbuf, length);
	pending_a314_write = TRUE;
}

static LONG sync_a314_write(int length)
{
	start_a314_write(length);
	pending_a314_write = FALSE;
	return WaitIO((struct IORequest *)wmsg);
}

static void start_a314_reset()
{
	start_a314_cmd(cmsg, A314_RESET, NULL, 0);
	pending_a314_reset = TRUE;
}

static LONG sync_a314_reset()
{
	start_a314_reset();
	pending_a314_reset = FALSE;
	return WaitIO((struct IORequest *)cmsg);
}

static struct InputEvent *event_handler_func(__reg("a0") struct InputEvent *ie)
{
	if (ie->ie_Class == IECLASS_RAWKEY || ie->ie_Class == IECLASS_RAWMOUSE)
		last_qualifier = ie->ie_Qualifier;
	return ie;
}

static void add_event_handler()
{
	input_event_handler.is_Code = (void *)event_handler_func;
	input_event_handler.is_Data = NULL;
	input_event_handler.is_Node.ln_Pri = 70;
	input_event_handler.is_Node.ln_Name = SERVICE_NAME;

	id_req->io_Data = (void *)&input_event_handler;
	id_req->io_Command = IND_ADDHANDLER;
	DoIO((struct IORequest *)id_req);
}

static void rem_event_handler()
{
	id_req->io_Data = (void *)&input_event_handler;
	id_req->io_Command = IND_REMHANDLER;
	DoIO((struct IORequest *)id_req);
}

static void write_input_event()
{
	struct EventMessage *em = (struct EventMessage *)arbuf;

	UWORD qualifier = em->qualifier;
	if (qualifier & IEQUALIFIER_INTERRUPT)
	{
		generated_event.ie_Class = IECLASS_RAWKEY;
		qualifier = (qualifier & 0x7000) | (last_qualifier & 0x00ff);
	}
	else if (qualifier & IEQUALIFIER_RELATIVEMOUSE)
	{
		generated_event.ie_Class = IECLASS_RAWMOUSE;
		qualifier |= last_qualifier & 0x00ff; // Last keyboard qualifiers.
	}
	else
	{
		generated_event.ie_Class = IECLASS_RAWKEY;
		qualifier |= last_qualifier & 0x7000; // Last mouse qualifiers.
	}
	generated_event.ie_Code = em->code;
	generated_event.ie_Qualifier = qualifier;
	generated_event.ie_X = em->dx;
	generated_event.ie_Y = em->dy;

	// Other fields in generated_event are zero.
	// TODO: Would it be preferable to set ie_TimeStamp?

	id_req->io_Data = (void *)&generated_event;
	id_req->io_Command = IND_WRITEEVENT;
	id_req->io_Length = sizeof(struct InputEvent);
	DoIO((struct IORequest *)id_req);
}

static void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = rmsg->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		write_input_event();
		start_a314_read();
	}
	else if (res == A314_READ_EOS)
	{
		start_a314_reset();
		stream_closed = TRUE;
	}
	else if (res == A314_READ_RESET)
		stream_closed = TRUE;
}

int main()
{
	LONG old_priority = SetTaskPri(FindTask(NULL), 60);

	mp = CreatePort(NULL, 0);
	cmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)cmsg, 0) != 0)
	{
		printf("Unable to open a314.device\n");
		goto fail_out1;
	}

	wmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	rmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	memcpy(wmsg, cmsg, sizeof(struct A314_IORequest));
	memcpy(rmsg, cmsg, sizeof(struct A314_IORequest));

	if (a314_connect(SERVICE_NAME) != A314_CONNECT_OK)
	{
		printf("Unable to connect to " SERVICE_NAME " service\n");
		goto fail_out2;
	}

	id_mp = CreatePort(NULL, 0);
	id_req = (struct IOStdReq *)CreateExtIO(id_mp, sizeof(struct IOStdReq));
	OpenDevice("input.device", 0, (struct IORequest *)id_req, 0);

	add_event_handler();

	start_a314_read();

	ULONG portsig = 1 << mp->mp_SigBit;

	printf("Press ctrl-c to exit...\n");

	while (TRUE)
	{
		ULONG signal = Wait(portsig | SIGBREAKF_CTRL_C);

		if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(mp))
			{
				if (msg == (struct Message *)rmsg)
					handle_a314_read_completed();
				else if (msg == (struct Message *)wmsg)
					pending_a314_write = FALSE;
				else if (msg == (struct Message *)cmsg)
					pending_a314_reset = FALSE;
			}
		}

		if (signal & SIGBREAKF_CTRL_C)
		{
			start_a314_reset();
			stream_closed = TRUE;
		}

		if (stream_closed && !pending_a314_read && !pending_a314_write && !pending_a314_reset)
			break;
	}

	rem_event_handler();

	CloseDevice((struct IORequest *)id_req);
	DeleteExtIO((struct IORequest *)id_req);
	DeletePort(id_mp);
fail_out2:
	CloseDevice((struct IORequest *)cmsg);
	DeleteExtIO((struct IORequest *)rmsg);
	DeleteExtIO((struct IORequest *)wmsg);
fail_out1:
	DeleteExtIO((struct IORequest *)cmsg);
	DeletePort(mp);
	SetTaskPri(FindTask(NULL), old_priority);
	return 0;
}
