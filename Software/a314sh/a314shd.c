/*
 * Copyright (c) 2026 Jeremy Bauer
 *
 * a314shd is a daemon that executes commands sent from the Raspberry Pi.
 * It connects to the "a314sh" service on the Pi, receives command
 * requests, executes them using dos.library Execute(), and sends back
 * the captured output and return code.
 *
 * Command line arguments: none. Stop with ctrl-c or break command.
 * Typically started in the background with: run >NIL: a314shd
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

#define SERVICE_NAME        "a314sh"
#define REQ_EXEC            1

#define CMD_INPUT           "NIL:"
#define CMD_OUTPUT          "T:a314shd.out"

#define MAX_REQUEST_SIZE    65536
#define READ_CHUNK_SIZE     255
#define WRITE_CHUNK_SIZE    224

#define LENGTH_FIELD_SIZE   4   /* 32-bit big-endian length field */
#define RC_FIELD_SIZE       4   /* 32-bit big-endian return code field */

#define FILE_BUF_SIZE       512
#define OUT_BUF_INITIAL_CAP 4096
#define ERR_MSG_SIZE        80

/* Retry delay between connect attempts: 200 ticks (~4 s), polled every 5 ticks for ctrl-c. */
#define CONNECT_RETRY_TICKS 200
#define RETRY_POLL_TICKS    5

static struct MsgPort *sync_mp;
static struct MsgPort *async_mp;

static struct A314_IORequest *read_ior;
static struct A314_IORequest *sync_ior;

static ULONG socket;

static UBYTE arbuf[READ_CHUNK_SIZE];

static BOOL pending_a314_read = FALSE;
static BOOL stream_closed = FALSE;
static BOOL quit = FALSE;

static UBYTE req_hdr[LENGTH_FIELD_SIZE];
static int req_hdr_len = 0;

static UBYTE *req_buf = NULL;
static ULONG req_len = 0;
static ULONG req_have = 0;

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

static BYTE sync_a314_cmd(struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	start_a314_cmd(sync_mp, ior, cmd, buffer, length);

	ULONG signal = Wait((1L << sync_mp->mp_SigBit) | SIGBREAKF_CTRL_C);
	if (signal & SIGBREAKF_CTRL_C)
	{
		AbortIO((struct IORequest *)ior);
		WaitIO((struct IORequest *)ior);
		quit = TRUE;
		return A314_READ_RESET;
	}

	GetMsg(sync_mp);
	return ior->a314_Request.io_Error;
}

static BYTE a314_connect(char *name)
{
	return sync_a314_cmd(sync_ior, A314_CONNECT, name, strlen(name));
}

static BYTE a314_write(char *buffer, int length)
{
	return sync_a314_cmd(sync_ior, A314_WRITE, buffer, length);
}

static BYTE a314_reset()
{
	return sync_a314_cmd(sync_ior, A314_RESET, NULL, 0);
}

static void start_a314_read()
{
	start_a314_cmd(async_mp, read_ior, A314_READ, arbuf, READ_CHUNK_SIZE);
	pending_a314_read = TRUE;
}

static void send_response(LONG rc, UBYTE *out, ULONG out_len)
{
	ULONG payload_len = RC_FIELD_SIZE + out_len;

	UBYTE hdr[LENGTH_FIELD_SIZE + RC_FIELD_SIZE];
	hdr[0] = (UBYTE)(payload_len >> 24);
	hdr[1] = (UBYTE)(payload_len >> 16);
	hdr[2] = (UBYTE)(payload_len >> 8);
	hdr[3] = (UBYTE)payload_len;
	hdr[4] = (UBYTE)(rc >> 24);
	hdr[5] = (UBYTE)(rc >> 16);
	hdr[6] = (UBYTE)(rc >> 8);
	hdr[7] = (UBYTE)rc;

	a314_write(hdr, sizeof(hdr));

	for (ULONG off = 0; off < out_len && !quit; off += WRITE_CHUNK_SIZE)
	{
		ULONG chunk = out_len - off;
		if (chunk > WRITE_CHUNK_SIZE)
			chunk = WRITE_CHUNK_SIZE;
		a314_write(out + off, chunk);
	}
}

static void send_error_response(LONG rc, char *msg)
{
	send_response(rc, (UBYTE *)msg, strlen(msg));
}

static UBYTE *out_buf = NULL;
static ULONG out_cap = 0;
static ULONG out_len = 0;

static void clear_output()
{
	out_len = 0;
}

static void append_output(UBYTE *data, ULONG len)
{
	if (out_len + len > out_cap)
	{
		ULONG ncap = out_cap ? out_cap : OUT_BUF_INITIAL_CAP;
		while (ncap < out_len + len)
			ncap *= 2;

		UBYTE *nb = AllocMem(ncap, 0);
		if (!nb)
			return;

		if (out_buf)
		{
			memcpy(nb, out_buf, out_len);
			FreeMem(out_buf, out_cap);
		}

		out_buf = nb;
		out_cap = ncap;
	}

	memcpy(out_buf + out_len, data, len);
	out_len += len;
}

static void free_output()
{
	if (out_buf)
	{
		FreeMem(out_buf, out_cap);
		out_buf = NULL;
		out_cap = 0;
	}
	out_len = 0;
}

static void read_output_file()
{
	BPTR fh = Open(CMD_OUTPUT, MODE_OLDFILE);
	if (!fh)
		return;

	UBYTE tmp[FILE_BUF_SIZE];
	LONG n;

	while ((n = Read(fh, tmp, sizeof(tmp))) > 0)
		append_output(tmp, n);

	Close(fh);
}

static void execute_command(UBYTE *cmd, ULONG len)
{
	struct Process *proc = (struct Process *)FindTask(NULL);
	struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(proc->pr_CLI);

	if (!cli)
	{
		send_error_response(RETURN_FAIL, "a314shd: unable to execute commands without a CLI\n");
		return;
	}

	char *cmdline = AllocMem(len + 1, MEMF_CLEAR);
	if (!cmdline)
	{
		send_error_response(RETURN_FAIL, "a314shd: out of memory\n");
		return;
	}
	memcpy(cmdline, cmd, len);

	BPTR out = Open(CMD_OUTPUT, MODE_NEWFILE);
	if (!out)
	{
		send_error_response(RETURN_FAIL, "a314shd: unable to open " CMD_OUTPUT "\n");
		FreeMem(cmdline, len + 1);
		return;
	}

	BPTR in = Open(CMD_INPUT, MODE_OLDFILE);

	LONG ok = Execute(cmdline, in, out);

	if (in)
		Close(in);
	Close(out);

	clear_output();
	read_output_file();

	LONG rc;

	if (ok)
	{
		rc = cli->cli_ReturnCode;
	}
	else
	{
		rc = RETURN_FAIL;

		char msg[ERR_MSG_SIZE];
		int n = sprintf(msg, "a314shd: Execute() failed, IoErr = %ld\n", cli->cli_Result2);
		append_output((UBYTE *)msg, n);
	}

	send_response(rc, out_buf, out_len);
	free_output();

	FreeMem(cmdline, len + 1);
	DeleteFile(CMD_OUTPUT);
}

static void handle_request(UBYTE *req, ULONG len)
{
	if (req[0] == REQ_EXEC && len > 1)
		execute_command(req + 1, len - 1);
	else
		send_error_response(RETURN_FAIL, "a314shd: unknown request\n");
}

static void clear_request_state()
{
	req_hdr_len = 0;
	req_len = 0;
	req_have = 0;

	if (req_buf)
	{
		FreeMem(req_buf, req_len);
		req_buf = NULL;
	}
}

static void protocol_error()
{
	stream_closed = TRUE;
	a314_reset();
}

static void consume_bytes(UBYTE *p, int len)
{
	while (len > 0 && !stream_closed)
	{
		if (req_hdr_len < LENGTH_FIELD_SIZE)
		{
			int take = LENGTH_FIELD_SIZE - req_hdr_len;
			if (take > len)
				take = len;

			memcpy(req_hdr + req_hdr_len, p, take);
			req_hdr_len += take;
			p += take;
			len -= take;

			if (req_hdr_len == LENGTH_FIELD_SIZE)
			{
				req_len = ((ULONG)req_hdr[0] << 24) | ((ULONG)req_hdr[1] << 16) |
					((ULONG)req_hdr[2] << 8) | (ULONG)req_hdr[3];

				if (req_len < 1 || req_len > MAX_REQUEST_SIZE)
				{
					protocol_error();
					return;
				}

				req_buf = AllocMem(req_len, 0);
				if (!req_buf)
				{
					protocol_error();
					return;
				}

				req_have = 0;
			}
		}
		else
		{
			ULONG take = req_len - req_have;
			if (take > (ULONG)len)
				take = len;

			memcpy(req_buf + req_have, p, take);
			req_have += take;
			p += take;
			len -= take;

			if (req_have == req_len)
			{
				UBYTE *req = req_buf;
				req_buf = NULL;
				handle_request(req, req_len);
				FreeMem(req, req_len);
				req_hdr_len = 0;
			}
		}
	}
}

static void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = read_ior->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		consume_bytes(read_ior->a314_Buffer, read_ior->a314_Length);

		if (!stream_closed)
			start_a314_read();
	}
	else if (res == A314_READ_EOS)
	{
		stream_closed = TRUE;
	}
	else if (res == A314_READ_RESET)
	{
		stream_closed = TRUE;
	}
}

static BOOL ctrl_c_pressed()
{
	return (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C) != 0;
}

static void cleanup()
{
	if (sync_ior && sync_ior->a314_Request.io_Device)
		CloseDevice((struct IORequest *)sync_ior);

	if (read_ior)
		DeleteExtIO((struct IORequest *)read_ior);

	if (sync_ior)
		DeleteExtIO((struct IORequest *)sync_ior);

	if (async_mp)
		DeletePort(async_mp);

	if (sync_mp)
		DeletePort(sync_mp);
}

int main(int argc, char **argv)
{
	sync_mp = CreatePort(NULL, 0);
	async_mp = CreatePort(NULL, 0);

	sync_ior = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	read_ior = (struct A314_IORequest *)CreateExtIO(async_mp, sizeof(struct A314_IORequest));

	if (!sync_mp || !async_mp || !sync_ior || !read_ior)
	{
		printf("Unable to create resources\n");
		cleanup();
		return RETURN_FAIL;
	}

	while (!quit)
	{
		if (!OpenDevice(A314_NAME, 0, (struct IORequest *)sync_ior, 0))
			break;

		printf("Unable to open device " A314_NAME ", retrying\n");

		for (int i = 0; i < CONNECT_RETRY_TICKS / RETRY_POLL_TICKS && !quit; i++)
		{
			Delay(RETRY_POLL_TICKS);
			if (ctrl_c_pressed())
				quit = TRUE;
		}
	}

	if (quit)
	{
		cleanup();
		return RETURN_OK;
	}

	memcpy(read_ior, sync_ior, sizeof(struct A314_IORequest));

	ULONG portsig = 1L << async_mp->mp_SigBit;

	while (!quit)
	{
		ULONG connect_attempts = 0;

		while (TRUE)
		{
			socket = (ULONG)time(NULL) + connect_attempts++;

			if (a314_connect(SERVICE_NAME) == A314_CONNECT_OK || quit)
				break;

			printf("Unable to connect to " SERVICE_NAME " service, retrying\n");

			for (int i = 0; i < CONNECT_RETRY_TICKS / RETRY_POLL_TICKS && !quit; i++)
			{
				Delay(RETRY_POLL_TICKS);
				if (ctrl_c_pressed())
					quit = TRUE;
			}
		}

		if (quit)
			break;

		printf("Connected to " SERVICE_NAME " service\n");

		stream_closed = FALSE;
		clear_request_state();
		start_a314_read();

		while (TRUE)
		{
			ULONG signal = Wait(portsig | SIGBREAKF_CTRL_C);

			if (signal & portsig)
			{
				struct Message *msg;
				while (msg = GetMsg(async_mp))
					handle_a314_read_completed();
			}

			if ((signal & SIGBREAKF_CTRL_C) && !quit)
			{
				a314_reset();
				quit = TRUE;
			}

			if ((stream_closed || quit) && !pending_a314_read)
				break;
		}

		clear_request_state();
	}

	cleanup();
	return RETURN_OK;
}
