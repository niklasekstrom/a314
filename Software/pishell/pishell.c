#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/memory.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"

struct FileHandle *con;

struct MsgPort *sync_mp;
struct MsgPort *async_mp;

ULONG socket;

struct A314_IORequest *wmsg;
UBYTE awbuf[256];
int awlen = 0;

struct A314_IORequest *rmsg;
UBYTE arbuf[256];

struct StandardPacket wsp;
UBYTE cwbuf[256];
struct StandardPacket rsp;
UBYTE crbuf[16];

BOOL pending_a314_read = FALSE;
BOOL pending_con_wait = FALSE;
BOOL stream_closed = FALSE;

void set_screen_mode(LONG mode)
{
	wsp.sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	wsp.sp_Msg.mn_Node.ln_Pri = 0;
	wsp.sp_Msg.mn_Node.ln_Name = (char *)&(wsp.sp_Pkt);
	wsp.sp_Msg.mn_Length = sizeof(struct StandardPacket);
	wsp.sp_Msg.mn_ReplyPort = sync_mp;
	wsp.sp_Pkt.dp_Link = &(wsp.sp_Msg);
	wsp.sp_Pkt.dp_Port = sync_mp;
	wsp.sp_Pkt.dp_Type = ACTION_SCREEN_MODE;
	wsp.sp_Pkt.dp_Arg1 = mode;
	PutMsg(con->fh_Type, &(wsp.sp_Msg));

	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
}

void con_write(char *s, int length)
{
	wsp.sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	wsp.sp_Msg.mn_Node.ln_Pri = 0;
	wsp.sp_Msg.mn_Node.ln_Name = (char *)&(wsp.sp_Pkt);
	wsp.sp_Msg.mn_Length = sizeof(struct StandardPacket);
	wsp.sp_Msg.mn_ReplyPort = sync_mp;
	wsp.sp_Pkt.dp_Link = &(wsp.sp_Msg);
	wsp.sp_Pkt.dp_Port = sync_mp;
	wsp.sp_Pkt.dp_Type = ACTION_WRITE;
	wsp.sp_Pkt.dp_Arg1 = con->fh_Arg1;
	wsp.sp_Pkt.dp_Arg2 = (LONG)s;
	wsp.sp_Pkt.dp_Arg3 = length;
	PutMsg(con->fh_Type, &(wsp.sp_Msg));

	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
}

void con_read()
{
	rsp.sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	rsp.sp_Msg.mn_Node.ln_Pri = 0;
	rsp.sp_Msg.mn_Node.ln_Name = (char *)&(rsp.sp_Pkt);
	rsp.sp_Msg.mn_Length = sizeof(struct StandardPacket);
	rsp.sp_Msg.mn_ReplyPort = sync_mp;
	rsp.sp_Pkt.dp_Link = &(rsp.sp_Msg);
	rsp.sp_Pkt.dp_Port = sync_mp;
	rsp.sp_Pkt.dp_Type = ACTION_READ;
	rsp.sp_Pkt.dp_Arg1 = con->fh_Arg1;
	rsp.sp_Pkt.dp_Arg2 = (LONG)&crbuf[0];
	rsp.sp_Pkt.dp_Arg3 = 15;
	PutMsg(con->fh_Type, &(rsp.sp_Msg));

	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
}

void start_con_wait()
{
	rsp.sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	rsp.sp_Msg.mn_Node.ln_Pri = 0;
	rsp.sp_Msg.mn_Node.ln_Name = (char *)&(rsp.sp_Pkt);
	rsp.sp_Msg.mn_Length = sizeof(struct StandardPacket);
	rsp.sp_Msg.mn_ReplyPort = async_mp;
	rsp.sp_Pkt.dp_Link = &(rsp.sp_Msg);
	rsp.sp_Pkt.dp_Port = async_mp;
	rsp.sp_Pkt.dp_Type = ACTION_WAIT_CHAR;
	rsp.sp_Pkt.dp_Arg1 = 100000;
	PutMsg(con->fh_Type, &(rsp.sp_Msg));

	pending_con_wait = TRUE;
}


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
	start_a314_cmd(sync_mp, wmsg, A314_CONNECT, name, strlen(name));
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return wmsg->a314_Request.io_Error;
}

BYTE a314_write(int length)
{
	start_a314_cmd(sync_mp, wmsg, A314_WRITE, awbuf, length);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return wmsg->a314_Request.io_Error;
}

BYTE a314_eos()
{
	start_a314_cmd(sync_mp, wmsg, A314_EOS, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return wmsg->a314_Request.io_Error;
}

BYTE a314_reset()
{
	start_a314_cmd(sync_mp, wmsg, A314_RESET, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return wmsg->a314_Request.io_Error;
}

void start_a314_read()
{
	start_a314_cmd(async_mp, rmsg, A314_READ, arbuf, 255);
	pending_a314_read = TRUE;
}


#define CSI 0x9b
#define ESC 0x1b

void process_con_char(UBYTE c)
{
	if (c == CSI)
	{
		awbuf[awlen++] = ESC;
		awbuf[awlen++] = '[';
	}
	else
	{
		awbuf[awlen++] = c;
	}
}

void handle_con_wait_completed()
{
	pending_con_wait = FALSE;

	if (stream_closed)
		return;

	if (rsp.sp_Pkt.dp_Res1 == DOSFALSE)
	{
		start_con_wait();
	}
	else
	{
		con_read();

		int len = rsp.sp_Pkt.dp_Res1;
		if (len == 0 || len == -1)
		{
			int l = sprintf(cwbuf, "CON read problem -- got length %d\n", len);
			con_write(cwbuf, l);

			a314_reset();
			stream_closed = TRUE;
		}
		else
		{
			awlen = 0;

			for (int i = 0; i < len; i++)
				process_con_char(crbuf[i]);

			if (awlen != 0)
				a314_write(awlen);

			start_con_wait();
		}
	}
}

void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = rmsg->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		UBYTE *p = rmsg->a314_Buffer;
		int len = rmsg->a314_Length;

		con_write(p, len);
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

int main()
{
	struct Process *proc = (struct Process *)FindTask(NULL);

	con = (struct FileHandle *)BADDR(proc->pr_CIS);

	sync_mp = CreatePort(NULL, 0);
	if (sync_mp == NULL)
	{
		printf("Unable to create sync reply message port\n");
		return 0;
	}

	async_mp = CreatePort(NULL, 0);
	if (async_mp == NULL)
	{
		printf("Unable to create async reply message port\n");
		DeletePort(sync_mp);
		return 0;
	}

	wmsg = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	if (wmsg == NULL)
	{
		printf("Unable to create io request for writes\n");
		DeletePort(async_mp);
		DeletePort(sync_mp);
		return 0;
	}

	rmsg = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	if (rmsg == NULL)
	{
		printf("Unable to create io request for reads\n");
		DeleteExtIO((struct IORequest *)wmsg);
		DeletePort(async_mp);
		DeletePort(sync_mp);
		return 0;
	}

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)wmsg, 0) != 0)
	{
		printf("Unable to open a314.device\n");
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeletePort(async_mp);
		DeletePort(sync_mp);
		return 0;
	}

	memcpy(rmsg, wmsg, sizeof(struct A314_IORequest));

	ULONG portsig = 1L << async_mp->mp_SigBit;

	if (a314_connect("pishell") != A314_CONNECT_OK)
	{
		printf("Unable to connect to pishell\n");
		CloseDevice((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeletePort(async_mp);
		DeletePort(sync_mp);
		return 0;
	}

	set_screen_mode(DOSTRUE);

	start_con_wait();
	start_a314_read();

	while (TRUE)
	{
		ULONG signal = Wait(portsig | SIGBREAKF_CTRL_C);

		if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(async_mp))
			{
				if (msg == (struct Message *)&rsp)
					handle_con_wait_completed();
				else if (msg == (struct Message *)rmsg)
					handle_a314_read_completed();
			}
		}

		if (stream_closed && !pending_a314_read && !pending_con_wait)
			break;
	}

	set_screen_mode(DOSFALSE);

	CloseDevice((struct IORequest *)wmsg);
	DeleteExtIO((struct IORequest *)rmsg);
	DeleteExtIO((struct IORequest *)wmsg);
	DeletePort(async_mp);
	DeletePort(sync_mp);
	return 0;
}
