/*
 * Copyright (c) 2018 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/memory.h>

#include <libraries/dos.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>

#include <hardware/intbits.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

struct Library *A314Base = NULL;

UBYTE *bpl_ptr1;
UBYTE *bpl_ptr2;

int curr_bpl = 0;

struct Screen *screen = NULL;

ULONG socket;

struct A314_IORequest *cmsg;
struct A314_IORequest *rmsg;
UBYTE arbuf[256];
struct A314_IORequest *wmsg;
UBYTE awbuf[256];

struct StandardPacket sp;
UBYTE rbuf[128];

BOOL pending_a314_read = FALSE;
BOOL pending_a314_write = FALSE;
BOOL pending_a314_reset = FALSE;

BOOL pending_con_read = FALSE;


BOOL waiting_for_frame_response = FALSE;
BOOL close_after_next_frame_response = FALSE;
BOOL no_more_frames = FALSE;


BOOL stream_closed = FALSE;

ULONG vblank_counter = 0;
ULONG req_next_frame_at = 0;


struct VBlankData
{
	struct Task *task;
	ULONG signal;
};

struct VBlankData vblank_data;
extern void VBlankServer();
struct Interrupt vblank_interrupt;


struct Process *proc;
struct FileHandle *cis;

struct MsgPort *mp;

void start_con_read()
{
	sp.sp_Msg.mn_Node.ln_Type = NT_MESSAGE;
	sp.sp_Msg.mn_Node.ln_Pri = 0;
	sp.sp_Msg.mn_Node.ln_Name = (char *)&(sp.sp_Pkt);
	sp.sp_Msg.mn_Length = sizeof(struct StandardPacket);
	sp.sp_Msg.mn_ReplyPort = mp;
	sp.sp_Pkt.dp_Link = &(sp.sp_Msg);
	sp.sp_Pkt.dp_Port = mp;
	sp.sp_Pkt.dp_Type = ACTION_READ;
	sp.sp_Pkt.dp_Arg1 = cis->fh_Arg1;
	sp.sp_Pkt.dp_Arg2 = (LONG)&rbuf[0];
	sp.sp_Pkt.dp_Arg3 = 127;
	PutMsg(cis->fh_Type, &(sp.sp_Msg));

	pending_con_read = TRUE;
}


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



void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = rmsg->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		waiting_for_frame_response = FALSE;

		if (close_after_next_frame_response)
		{
			start_a314_reset();
			stream_closed = TRUE;
			return;
		}

		if (*((UWORD *)&arbuf[0]) == 0)
		{
			printf("No more frames, ending now.\n");
			no_more_frames = TRUE;
			return;
		}

		UWORD *pal = (UWORD *)(&arbuf[2]);
		UWORD *ct = (UWORD *)(screen->ViewPort.ColorMap->ColorTable);
		for (int i = 0; i < 16; i++)
			ct[i] = pal[i];

		UBYTE *bpl_ptr;
		if (curr_bpl == 0)
			bpl_ptr = bpl_ptr1;
		else
			bpl_ptr = bpl_ptr2;

		screen->BitMap.Planes[0] = &bpl_ptr[10240*0];
		screen->BitMap.Planes[1] = &bpl_ptr[10240*1];
		screen->BitMap.Planes[2] = &bpl_ptr[10240*2];
		screen->BitMap.Planes[3] = &bpl_ptr[10240*3];

		MakeScreen(screen);
		RethinkDisplay();

		start_a314_read();
	}
	else if (res == A314_READ_RESET)
	{
		stream_closed = TRUE;
	}
}

void handle_a314_write_completed()
{
	pending_a314_write = FALSE;

	if (stream_closed)
		return;

	int res = wmsg->a314_Request.io_Error;
	if (res == A314_WRITE_RESET)
	{
		stream_closed = TRUE;
	}
}

void handle_a314_reset_completed()
{
	pending_a314_reset = FALSE;
}




void handle_con_read_completed()
{
	pending_con_read = FALSE;

	if (stream_closed)
		return;

	int len = sp.sp_Pkt.dp_Res1;
	rbuf[len] = 0;

	if (strncmp(rbuf, "exit", 4) == 0)
	{
		close_after_next_frame_response = TRUE;

		if (!waiting_for_frame_response)
		{
			start_a314_reset();
			stream_closed = TRUE;
		}
	}

	if (!stream_closed)
		start_con_read();
}

void handle_vblank_signal()
{
	if (req_next_frame_at <= vblank_counter && !waiting_for_frame_response && !no_more_frames)
	{
		curr_bpl = (curr_bpl + 1) & 1;

		*((UBYTE *)&awbuf[0]) = (UBYTE)curr_bpl;
		start_a314_write(1);

		waiting_for_frame_response = TRUE;
		req_next_frame_at += 4;
	}
	vblank_counter += 1;
}

int main()
{
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);

	proc = (struct Process *)FindTask(NULL);
	cis = (struct FileHandle *)BADDR(proc->pr_CIS);

	mp = CreatePort(NULL, 0);
	cmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)cmsg, 0) != 0)
	{
		printf("Unable to open a314.device\n");

		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 0;
	}

	A314Base = &(cmsg->a314_Request.io_Device->dd_Library);

	wmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	rmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	memcpy(wmsg, cmsg, sizeof(struct A314_IORequest));
	memcpy(rmsg, cmsg, sizeof(struct A314_IORequest));

	if (a314_connect("videoplayer") != A314_CONNECT_OK)
	{
		printf("Unable to connect to videoplayer\n");

		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 0;
	}

	bpl_ptr1 = AllocMem(10240*4, MEMF_A314);
	bpl_ptr2 = AllocMem(10240*4, MEMF_A314);
	if (bpl_ptr1 == NULL || bpl_ptr2 == NULL)
	{
		printf("Unable to allocate A314 memory\n");

		if (bpl_ptr1 != NULL)
			FreeMem(bpl_ptr1, 10240*4);

		sync_a314_reset();

		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		return 0;
	}

	*((ULONG *)&awbuf[0]) = TranslateAddressA314(bpl_ptr1);
	*((ULONG *)&awbuf[4]) = TranslateAddressA314(bpl_ptr2);

	sync_a314_write(8);

	struct BitMap bmp =
	{
		40, 256, 0, 4, 0,
		{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
	};

	bmp.Planes[0] = &bpl_ptr1[10240*0];
	bmp.Planes[1] = &bpl_ptr1[10240*1];
	bmp.Planes[2] = &bpl_ptr1[10240*2];
	bmp.Planes[3] = &bpl_ptr1[10240*3];

	struct NewScreen new_screen =
	{
		0, 0,
		320, 256, 4,
		0, 1,
		0, CUSTOMSCREEN | CUSTOMBITMAP | SCREENQUIET,
		NULL, "Videoplayer", NULL, &bmp
	};

	screen = OpenScreen(&new_screen);
	if (screen == NULL)
	{
		printf("Unable to create screen\n");

		FreeMem(bpl_ptr2, 10240*4);
		FreeMem(bpl_ptr1, 10240*4);

		sync_a314_reset();

		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);;
		return 0;
	}

	start_con_read();
	start_a314_read();

	BYTE vblank_sigbit = AllocSignal(-1);

	ULONG vblanksig = 1 << vblank_sigbit;
	ULONG portsig = 1L << mp->mp_SigBit;

	vblank_data.task = FindTask(NULL);
	vblank_data.signal = vblanksig;

	vblank_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	vblank_interrupt.is_Node.ln_Pri = -60;
	vblank_interrupt.is_Node.ln_Name = "videoplayer-vblank-interrupt";
	vblank_interrupt.is_Data = (APTR)&vblank_data;
	vblank_interrupt.is_Code = VBlankServer;

	AddIntServer(INTB_VERTB, &vblank_interrupt);


	BOOL asked_to_pak = FALSE;

	while (TRUE)
	{
		ULONG signal = Wait(vblanksig | portsig | SIGBREAKF_CTRL_C);
		if (signal & vblanksig)
		{
			handle_vblank_signal();
		}

		if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(mp))
			{
				if (msg == (struct Message *)&sp)
					handle_con_read_completed();
				else if (msg == (struct Message *)rmsg)
					handle_a314_read_completed();
				else if (msg == (struct Message *)wmsg && wmsg->a314_Request.io_Command == A314_WRITE)
					handle_a314_write_completed();
				else if (msg == (struct Message *)cmsg && cmsg->a314_Request.io_Command == A314_RESET)
					handle_a314_reset_completed();
			}
		}

		if (signal & SIGBREAKF_CTRL_C)
		{
			close_after_next_frame_response = TRUE;

			if (!waiting_for_frame_response)
			{
				start_a314_reset();
				stream_closed = TRUE;
			}
		}

		if (stream_closed && !pending_a314_read && !pending_a314_write && !pending_a314_reset)
		{
			if (!pending_con_read)
				break;
			else if (!asked_to_pak)
			{
				fputs("*** Connection closed. Press enter to exit...\n", stdout);
				fflush(stdout);
				asked_to_pak = TRUE;
			}
		}
	}

	RemIntServer(INTB_VERTB, &vblank_interrupt);
	FreeSignal(vblank_sigbit);
	CloseScreen(screen);

	FreeMem(bpl_ptr2, 10240*4);
	FreeMem(bpl_ptr1, 10240*4);

	CloseDevice((struct IORequest *)cmsg);
	DeleteExtIO((struct IORequest *)rmsg);
	DeleteExtIO((struct IORequest *)wmsg);
	DeleteExtIO((struct IORequest *)cmsg);
	DeletePort(mp);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)IntuitionBase);;
	return 0;
}
