/*
 * Copyright (c) 2018 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/memory.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>

#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../a314device/a314.h"

#define PICMD_SERVICE_NAME "picmd"

#define ID_314_DISK (('3' << 24) | ('1' << 16) | ('4' << 8))

#define FLAG_INPUT_FILE		0x0001
#define FLAG_OUTPUT_FILE	0x0002

struct StartMsgHeader
{
	UWORD length;
	UWORD flags;
	short rows;
	short cols;
	UBYTE component_count;
	UBYTE arg_count;
};

static struct MsgPort *sync_mp;
static struct MsgPort *async_mp;

static struct A314_IORequest *read_ior;
static struct A314_IORequest *sync_ior;

static BPTR input_file;
static BPTR output_file;

static BOOL console_input;
static BOOL console_output;

static struct FileHandle *con;

static ULONG socket;

static UBYTE arbuf[256];
static UBYTE input_read_buf[64];

static struct StandardPacket sync_sp;
static struct StandardPacket wait_sp;

static BOOL pending_a314_read = FALSE;
static BOOL pending_input_read = FALSE;
static BOOL stream_open = FALSE;

static void put_fh_sp(struct FileHandle *fh, struct StandardPacket *sp, struct MsgPort *reply_port, LONG action, LONG arg1, LONG arg2, LONG arg3)
{
	sp->sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	sp->sp_Msg.mn_Node.ln_Pri = 0;
	sp->sp_Msg.mn_Node.ln_Name = (char *)&(sp->sp_Pkt);
	sp->sp_Msg.mn_Length = sizeof(struct StandardPacket);
	sp->sp_Msg.mn_ReplyPort = reply_port;
	sp->sp_Pkt.dp_Link = &(sp->sp_Msg);
	sp->sp_Pkt.dp_Port = reply_port;
	sp->sp_Pkt.dp_Type = action;
	sp->sp_Pkt.dp_Arg1 = arg1;
	sp->sp_Pkt.dp_Arg2 = arg2;
	sp->sp_Pkt.dp_Arg3 = arg3;
	PutMsg(fh->fh_Type, &(sp->sp_Msg));
}

static LONG set_screen_mode(LONG mode)
{
	put_fh_sp(con, &sync_sp, sync_mp, ACTION_SCREEN_MODE, mode, 0, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_sp.sp_Pkt.dp_Res1;
}

static void start_wait_char()
{
	put_fh_sp(con, &wait_sp, async_mp, ACTION_WAIT_CHAR, 100000, 0, 0);
	pending_input_read = TRUE;
}

static void start_input_read()
{
	struct FileHandle *fh = (struct FileHandle *)BADDR(input_file);
	if (fh->fh_Arg1 == 0)
	{
		wait_sp.sp_Pkt.dp_Res1 = 0;
	}
	else
	{
		put_fh_sp(fh, &wait_sp, async_mp, ACTION_READ, fh->fh_Arg1, (LONG)input_read_buf, sizeof(input_read_buf));
		pending_input_read = TRUE;
	}
}

static void start_a314_cmd(struct MsgPort *reply_port, struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	ior->a314_Request.io_Message.mn_ReplyPort = reply_port;
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = socket;
	ior->a314_Buffer = buffer;
	ior->a314_Length = length;
	SendIO((struct IORequest *)ior);
}

static BYTE a314_connect(char *name)
{
	socket = time(NULL);
	start_a314_cmd(sync_mp, sync_ior, A314_CONNECT, name, strlen(name));
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

static BYTE a314_write(char *buffer, int length)
{
	start_a314_cmd(sync_mp, sync_ior, A314_WRITE, buffer, length);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

static BYTE a314_eos()
{
	start_a314_cmd(sync_mp, sync_ior, A314_EOS, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

static BYTE a314_reset()
{
	start_a314_cmd(sync_mp, sync_ior, A314_RESET, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_ior->a314_Request.io_Error;
}

static void start_a314_read()
{
	start_a314_cmd(async_mp, read_ior, A314_READ, arbuf, 255);
	pending_a314_read = TRUE;
}

static void handle_wait_char_completed()
{
	pending_input_read = FALSE;

	if (!stream_open)
		return;

	if (wait_sp.sp_Pkt.dp_Res1 == DOSFALSE)
	{
		start_wait_char();
	}
	else
	{
		int len = Read(input_file, input_read_buf, sizeof(input_read_buf));

		if (len == 0 || len == -1)
		{
			a314_reset();
			stream_open = FALSE;
		}
		else
		{
			a314_write(input_read_buf, len);
			start_wait_char();
		}
	}
}

static void handle_input_read_completed()
{
	pending_input_read = FALSE;

	if (!stream_open)
		return;

	LONG bytes_read = wait_sp.sp_Pkt.dp_Res1;
	if (bytes_read == -1) // Error.
	{
		a314_reset();
		stream_open = FALSE;
	}
	else if (bytes_read == 0) // End of file.
	{
		a314_eos();
	}
	else
	{
		a314_write(input_read_buf, bytes_read);
		start_input_read();
	}
}

static void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (!stream_open)
		return;

	int res = read_ior->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		UBYTE *p = read_ior->a314_Buffer;
		int len = read_ior->a314_Length;

		Write(output_file, p, len);
		start_a314_read();
	}
	else if (res == A314_READ_EOS)
	{
		a314_reset();
		stream_open = FALSE;
	}
	else if (res == A314_READ_RESET)
	{
		stream_open = FALSE;
	}
}

static void create_and_send_start_msg(BPTR current_dir, int argc, char **argv, short rows, short cols)
{
	int buf_len = sizeof(struct StartMsgHeader);

	int component_count = 0;
	UBYTE *components[20];

	if (current_dir != 0)
	{
		struct FileLock *fl = (struct FileLock *)BADDR(current_dir);
		struct DeviceList *dl = (struct DeviceList *)BADDR(fl->fl_Volume);

		if (dl->dl_DiskType == ID_314_DISK)
		{
			struct FileInfoBlock *fib = AllocMem(sizeof(struct FileInfoBlock), 0);

			BPTR lock = DupLock(current_dir);

			while (lock != 0)
			{
				if (Examine(lock, fib) == 0)
				{
					UnLock(lock);
					break;
				}

				int n = strlen(fib->fib_FileName);
				UBYTE *p = AllocMem(n + 1, 0);
				p[0] = (UBYTE)n;
				memcpy(&p[1], fib->fib_FileName, n);
				components[component_count++] = p;

				buf_len += n + 1;

				BPTR child = lock;
				lock = ParentDir(child);
				UnLock(child);
			}

			FreeMem(fib, sizeof(struct FileInfoBlock));
		}
	}

	for (int i = 1; i < argc; i++)
		buf_len += strlen(argv[i]) + 1;

	UBYTE *buffer = AllocMem(buf_len, 0);

	UWORD flags = (console_input ? 0 : FLAG_INPUT_FILE) |
			(console_output ? 0 : FLAG_OUTPUT_FILE);

	struct StartMsgHeader *hdr = (struct StartMsgHeader *)buffer;
	hdr->length = buf_len;
	hdr->flags = flags;
	hdr->rows = rows;
	hdr->cols = cols;
	hdr->component_count = component_count;
	hdr->arg_count = argc - 1;

	UBYTE *p = buffer + sizeof(struct StartMsgHeader);

	for (int i = 0; i < component_count; i++)
	{
		UBYTE *q = components[component_count - 1 - i];
		int n = *q;
		memcpy(p, q, n + 1);
		p += n + 1;
		FreeMem(q, n + 1);
	}

	for (int i = 1; i < argc; i++)
	{
		UBYTE *q = (UBYTE *)argv[i];
		int n = strlen(q);
		*p++ = (UBYTE)n;
		memcpy(p, q, n);
		p += n;
	}

	for (int i = 0; i < buf_len; i += 224)
	{
		int to_write = buf_len - i;
		if (to_write > 224)
			to_write = 224;
		a314_write(buffer + i, to_write);
	}

	FreeMem(buffer, buf_len);
}

int main(int argc, char **argv)
{
	struct Process *proc = (struct Process *)FindTask(NULL);

	input_file = proc->pr_CIS;
	output_file = proc->pr_COS;

	console_input = IsInteractive(input_file);
	console_output = IsInteractive(output_file);

	sync_mp = CreatePort(NULL, 0);
	async_mp = CreatePort(NULL, 0);

	sync_ior = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	read_ior = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));

	if (!sync_mp || !async_mp || !sync_ior || !read_ior)
	{
		printf("Unable to allocate enough memory\n");
		goto fail1;
	}

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)sync_ior, 0))
	{
		printf("Unable to open " A314_NAME "\n");
		goto fail1;
	}

	memcpy(read_ior, sync_ior, sizeof(struct A314_IORequest));

	if (a314_connect(PICMD_SERVICE_NAME) != A314_CONNECT_OK)
	{
		printf("Unable to connect to " PICMD_SERVICE_NAME " service\n");
		goto fail2;
	}

	stream_open = TRUE;

	// The interactions with the console are described here:
	// https://wiki.amigaos.net/wiki/Console_Device

	if (console_input)
		con = (struct FileHandle *)BADDR(input_file);
	else if (console_output)
		con = (struct FileHandle *)BADDR(output_file);

	if (con)
		set_screen_mode(DOSTRUE);

	int rows = 25;
	int cols = 80;

	if (console_output)
	{
		// Window Status Request
		Write(output_file, "\x9b" "0 q", 4);

		int len = Read(output_file, arbuf, 32);	// "\x9b" "1;1;33;77 r"
		if (len < 10 || arbuf[len - 1] != 'r')
		{
			printf("Failure to receive window bounds report\n");
			a314_reset();
			goto fail3;
		}

		// Set Raw Events
		Write(output_file, "\x9b" "12{", 4); // 12 = Window resized

		int start = 5;
		int ind = start;
		while (arbuf[ind] != ';')
			ind++;
		arbuf[ind] = 0;
		rows = atoi(arbuf + start);
		ind++;
		start = ind;
		while (arbuf[ind] != ' ')
			ind++;
		arbuf[ind] = 0;
		cols = atoi(arbuf + start);
	}

	create_and_send_start_msg(proc->pr_CurrentDir, argc, argv, (short)rows, (short)cols);

	if (console_input)
		start_wait_char();
	else
	{
		start_input_read();
		if (!pending_input_read)
			handle_input_read_completed();
	}

	start_a314_read();

	ULONG portsig = 1L << async_mp->mp_SigBit;

	while (stream_open || pending_a314_read || pending_input_read)
	{
		ULONG signal = Wait(portsig | SIGBREAKF_CTRL_C);

		if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(async_mp))
			{
				if (msg == (struct Message *)&wait_sp)
				{
					if (console_input)
						handle_wait_char_completed();
					else
						handle_input_read_completed();
				}
				else if (msg == (struct Message *)read_ior)
					handle_a314_read_completed();
			}
		}
	}

fail3:
	if (con)
		set_screen_mode(DOSFALSE);

fail2:
	CloseDevice((struct IORequest *)sync_ior);

fail1:
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
