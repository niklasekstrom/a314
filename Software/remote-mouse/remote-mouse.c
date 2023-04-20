/*
 * Copyright (c) 2020 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/memory.h>

#include <devices/input.h>
#include <devices/inputevent.h>
#include <libraries/dos.h>

#include <proto/alib.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"

struct MsgPort *mp;
ULONG socket;

struct A314_IORequest *cmsg;
struct A314_IORequest *rmsg;
struct A314_IORequest *wmsg;

UBYTE arbuf[256];
UBYTE awbuf[256];

BOOL pending_a314_read = FALSE;
BOOL pending_a314_write = FALSE;
BOOL pending_a314_reset = FALSE;

BOOL stream_closed = FALSE;

struct MsgPort *id_mp;
struct IOStdReq *id_req;
struct InputEvent generated_event;

void start_a314_cmd(struct A314_IORequest *msg, UWORD command, char *buffer, int length)
{
	msg->a314_Request.io_Command = command;
	msg->a314_Request.io_Error = 0;

	msg->a314_Socket = socket;
	msg->a314_Buffer = buffer;
	msg->a314_Length = length;

	SendIO((struct IORequest *)msg);
}

LONG a314_connect(char *name)
{
	socket = time(NULL);
	start_a314_cmd(cmsg, A314_CONNECT, name, strlen(name));
	return WaitIO((struct IORequest *)cmsg);
}

void start_a314_read()
{
	start_a314_cmd(rmsg, A314_READ, arbuf, 255);
	pending_a314_read = TRUE;
}

void start_a314_write(int length)
{
	start_a314_cmd(wmsg, A314_WRITE, awbuf, length);
	pending_a314_write = TRUE;
}

LONG sync_a314_write(int length)
{
	start_a314_write(length);
	pending_a314_write = FALSE;
	return WaitIO((struct IORequest *)wmsg);
}

void start_a314_reset()
{
	start_a314_cmd(cmsg, A314_RESET, NULL, 0);
	pending_a314_reset = TRUE;
}

LONG sync_a314_reset()
{
	start_a314_reset();
	pending_a314_reset = FALSE;
	return WaitIO((struct IORequest *)cmsg);
}

WORD last_b = 0;

void send_generated_mouse_event(WORD dx, WORD dy, WORD b)
{
	generated_event.ie_NextEvent = NULL;
	generated_event.ie_Class = IECLASS_RAWMOUSE;
	generated_event.ie_SubClass = 0;

	UWORD code = IECODE_NOBUTTON;
	if (!(last_b & 1) && (b & 1))
		code = IECODE_LBUTTON;
	else if ((last_b & 1) && !(b & 1))
		code = IECODE_UP_PREFIX | IECODE_LBUTTON;
	else if (!(last_b & 2) && (b & 2))
		code = IECODE_RBUTTON;
	else if ((last_b & 2) && !(b & 2))
		code = IECODE_UP_PREFIX | IECODE_RBUTTON;
	generated_event.ie_Code = code;

	// Should include keyboard qualifiers.
	// Where do we get them from?

	UWORD qual = IEQUALIFIER_RELATIVEMOUSE;
	if (b & 1)
		qual |= IEQUALIFIER_LEFTBUTTON;
	if (b & 2)
		qual |= IEQUALIFIER_RBUTTON;
	generated_event.ie_Qualifier = qual;

	generated_event.ie_X = dx;
	generated_event.ie_Y = dy;

	id_req->io_Data = (void *)&generated_event;
	id_req->io_Command = IND_WRITEEVENT;
	id_req->io_Flags = 0;
	id_req->io_Length = sizeof(struct InputEvent);
	DoIO((struct IORequest *)id_req);
}

void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = rmsg->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		int length = rmsg->a314_Length;

		WORD dx = *(WORD *)&arbuf[0];
		WORD dy = *(WORD *)&arbuf[2];
		WORD b = *(BYTE *)&arbuf[4];

		send_generated_mouse_event(dx, dy, b);
		last_b = b;

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

	if (a314_connect("remote-mouse") != A314_CONNECT_OK)
	{
		printf("Unable to connect to remote-mouse service\n");
		goto fail_out2;
	}

	id_mp = CreatePort(NULL, 0);
	id_req = (struct IOStdReq *)CreateExtIO(id_mp, sizeof(struct IOStdReq));
	OpenDevice("input.device", 0, (struct IORequest *)id_req, 0);

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
