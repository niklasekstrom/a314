/*
 * Copyright (c) 2022 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/memory.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <devices/timer.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

#define PICMD_SERVICE_NAME "picmd"

struct MsgPort *sync_mp;
struct MsgPort *async_mp;

struct A314_IORequest *read_ior;
struct A314_IORequest *sync_ior;
struct timerequest *tr;

ULONG socket;

UBYTE arbuf[256];

BOOL pending_a314_read = FALSE;
BOOL stream_closed = FALSE;

// length, rows, cols, components_count, args_count, args...
static const char start_msg_const[] = "\x00\x00" "\x00\x19" "\x00\x50" "\x00" "\x02" "\x04" "date" "\x05" "+%s%z";
#define START_MSG_LEN (sizeof(start_msg_const) - 1)

void start_a314_cmd(struct MsgPort *reply_port, struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	ior->a314_Request.io_Message.mn_ReplyPort = reply_port;
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = socket;
	ior->a314_Buffer = buffer;
	ior->a314_Length = length;
	SendIO((struct IORequest *)ior);
}

BYTE a314_connect(char *name)
{
	socket = time(NULL);
	start_a314_cmd(sync_mp, sync_ior, A314_CONNECT, name, strlen(name));
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

BYTE a314_write(char *buffer, int length)
{
	start_a314_cmd(sync_mp, sync_ior, A314_WRITE, buffer, length);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

BYTE a314_eos()
{
	start_a314_cmd(sync_mp, sync_ior, A314_EOS, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

BYTE a314_reset()
{
	start_a314_cmd(sync_mp, sync_ior, A314_RESET, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

void start_a314_read()
{
	start_a314_cmd(async_mp, read_ior, A314_READ, arbuf, 255);
	pending_a314_read = TRUE;
}

void handle_read_data()
{
	static BOOL timestamp_handled = FALSE;

	if (timestamp_handled)
		return;

	UBYTE *p = read_ior->a314_Buffer;
	int len = read_ior->a314_Length;

	unsigned long time_zone = 0;

	for (int i = 0; i < len; i++)
	{
		if (p[i] == '+' || p[i] == '-')
		{
			unsigned long hours = ((p[i+1] - '0') * 10) + (p[i+2] - '0');
			unsigned long minutes = ((p[i+3] - '0') * 10) + (p[i+4] - '0');
			time_zone = (hours * 60 + minutes) * 60;
			if (p[i] == '-')
				time_zone = -time_zone;

			p[i] = 0;
			break;
		}
	}

	unsigned long ts = strtoul(p, NULL, 10);
	ts -= 252460800; // Adjust base from 1970-01-01 to 1978-01-01.
	ts += time_zone;

	tr->tr_time.tv_secs = ts;
	tr->tr_node.io_Command = TR_SETSYSTIME;
	DoIO((struct IORequest *)tr);

	timestamp_handled = TRUE;
}

void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = read_ior->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		handle_read_data();
		start_a314_read();
	}
	else if (res == A314_READ_EOS)
	{
		a314_eos();
		stream_closed = TRUE;
	}
	else if (res == A314_READ_RESET)
	{
		stream_closed = TRUE;
	}
}

int main(int argc, char **argv)
{
	sync_mp = CreatePort(NULL, 0);
	async_mp = CreatePort(NULL, 0);

	sync_ior = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	read_ior = (struct A314_IORequest *)CreateExtIO(async_mp, sizeof(struct A314_IORequest));

	tr = (struct timerequest *)CreateExtIO(sync_mp, sizeof(struct timerequest));

	if (!sync_mp || !async_mp || !sync_ior || !read_ior || !tr)
	{
		printf("Unable to allocate enough memory\n");
		goto fail1;
	}

	if (OpenDevice(TIMERNAME, 0, (struct IORequest *)tr, 0))
	{
		printf("Unable to open " TIMERNAME "\n");
		goto fail1;
	}

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)sync_ior, 0))
	{
		printf("Unable to open " A314_NAME "\n");
		goto fail2;
	}

	memcpy(read_ior, sync_ior, sizeof(struct A314_IORequest));

	if (a314_connect(PICMD_SERVICE_NAME) != A314_CONNECT_OK)
	{
		printf("Unable to connect to " PICMD_SERVICE_NAME " service\n");
		goto fail3;
	}

	char start_msg[START_MSG_LEN];
	memcpy(start_msg + 2, start_msg_const + 2, START_MSG_LEN - 2);
	*(UWORD *)start_msg = START_MSG_LEN;

	a314_write(start_msg, START_MSG_LEN);

	start_a314_read();

	ULONG portsig = 1L << async_mp->mp_SigBit;
	BOOL did_reset = FALSE;

	while (TRUE)
	{
		ULONG signal = Wait(portsig | SIGBREAKF_CTRL_C);

		if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(async_mp))
		                handle_a314_read_completed();
		}

		if ((signal & SIGBREAKF_CTRL_C) && !did_reset)
		{
			a314_reset();
			did_reset = TRUE;
		}

		if (stream_closed && !pending_a314_read)
			break;
	}

fail3:
	CloseDevice((struct IORequest *)sync_ior);

fail2:
	CloseDevice((struct IORequest *)tr);

fail1:
	if (tr)
		DeleteExtIO((struct IORequest *)tr);

	if (read_ior)
		DeleteExtIO((struct IORequest *)read_ior);

	if (sync_ior)
		DeleteExtIO((struct IORequest *)sync_ior);

	if (async_mp)
		DeletePort(async_mp);

	if (sync_mp)
		DeletePort(sync_mp);

	return 0;
}
