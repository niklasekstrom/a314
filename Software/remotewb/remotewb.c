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

#include <devices/input.h>
#include <devices/inputevent.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/dos.h>

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

struct MsgPort *mp;

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

BOOL stream_closed = FALSE;

struct MsgPort *id_mp;
struct IOStdReq *id_req;
struct InputEvent generated_event;

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

struct Screen *find_wb_screen()
{
	struct Screen *screen = IntuitionBase->FirstScreen;
	while (screen)
	{
		if ((screen->Flags & SCREENTYPE) == WBENCHSCREEN)
			return screen;
		screen = screen->NextScreen;
	}
	return NULL;
}

int blen = 0;

void append_ulong(ULONG x)
{
	*((ULONG *)&awbuf[blen]) = x;
	blen += 4;
}

void append_uword(UWORD x)
{
	*((UWORD *)&awbuf[blen]) = x;
	blen += 2;
}

WORD last_b = 0;
WORD kbd_qual = 0;

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

	UWORD qual = IEQUALIFIER_RELATIVEMOUSE | kbd_qual;
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

void send_generated_keyboard_event(UBYTE up, UBYTE kc)
{
	generated_event.ie_NextEvent = NULL;
	generated_event.ie_Class = IECLASS_RAWKEY;
	generated_event.ie_SubClass = 0;
	generated_event.ie_Code = up | kc;

	UWORD qual = kbd_qual;
	if (last_b & 1)
		qual |= IEQUALIFIER_LEFTBUTTON;
	if (last_b & 2)
		qual |= IEQUALIFIER_RBUTTON;
	switch (kc)
	{
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x3d:
	case 0x3e:
	case 0x3f:
	case 0x4a:
	case 0x2d:
	case 0x2e:
	case 0x2f:
	case 0x5e:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	case 0x43:
	case 0x0f:
	case 0x3c:
		qual |= IEQUALIFIER_NUMERICPAD;
		break;
	}
	generated_event.ie_Qualifier = qual;

	generated_event.ie_X = 0;
	generated_event.ie_Y = 0;

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

		WORD *p = (WORD *)&arbuf[0];

		WORD cx = IntuitionBase->MouseX;
		WORD cy = IntuitionBase->MouseY;

		while (length > 0)
		{
			WORD x = *p++;

			if (x & 0x4000)
			{
				UBYTE up = (x & 0x2000) ? 0x80 : 0;
				UBYTE kc = x & 0x7f;

				if (kc >= 0x60 && kc <= 0x67)
				{
					UWORD qual = 1 << (kc - 0x60);
					if (up)
						kbd_qual &= ~qual;
					else
						kbd_qual |= qual;
				}

				send_generated_keyboard_event(up, kc);

				length -= 2;
			}
			else
			{
				WORD y = *p++;
				WORD b = *p++;

				if (b == last_b)
				{
					send_generated_mouse_event(x - cx, y - cy, last_b);
					cx = x;
					cy = y;
				}
				else
				{
					if (cx != x || cy != y)
					{
						send_generated_mouse_event(x - cx, y - cy, last_b);
						cx = x;
						cy = y;
					}
					send_generated_mouse_event(0, 0, b);
					last_b = b;
				}

				length -= 6;
			}
		}

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

void handle_con_read_completed()
{
	pending_con_read = FALSE;

	if (stream_closed)
		return;

	int len = sp.sp_Pkt.dp_Res1;
	rbuf[len] = 0;

	if (strncmp(rbuf, "exit", 4) == 0)
	{
		start_a314_reset();
		stream_closed = TRUE;
	}

	if (!stream_closed)
		start_con_read();
}

void handle_vblank_signal()
{
	blen = 0;
	append_uword(IntuitionBase->MouseX);
	append_uword(IntuitionBase->MouseY);
	
	if (!pending_a314_write)
		start_a314_write(blen);
}

int main()
{
	proc = (struct Process *)FindTask(NULL);
	cis = (struct FileHandle *)BADDR(proc->pr_CIS);

	mp = CreatePort(NULL, 0);
	cmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)cmsg, 0) != 0)
	{
		printf("Unable to open a314.device\n");

		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		return 0;
	}

	A314Base = &(cmsg->a314_Request.io_Device->dd_Library);

	wmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	rmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	memcpy(wmsg, cmsg, sizeof(struct A314_IORequest));
	memcpy(rmsg, cmsg, sizeof(struct A314_IORequest));

	if (a314_connect("remotewb") != A314_CONNECT_OK)
	{
		printf("Unable to connect to remotewb service\n");

		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		return 0;
	}

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);

	Forbid();

	struct Screen *screen = find_wb_screen();
	if (!screen)
	{
		Permit();

		printf("Unable to find workbench screen\n");

		sync_a314_reset();

		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		return 0;
	}

	struct BitMap *bm = &(screen->BitMap);

	if (screen->Width != 640 || screen->Height != 256 || bm->Depth != 3 || (bm->BytesPerRow != 80 && bm->BytesPerRow != 240))
	{
		Permit();

		printf("Wrong screen resolution; it is %hdx%hdx%hhu but must be 640x256x3\n", screen->Width, screen->Height, bm->Depth);

		sync_a314_reset();

		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		CloseDevice((struct IORequest *)cmsg);
		DeleteExtIO((struct IORequest *)rmsg);
		DeleteExtIO((struct IORequest *)wmsg);
		DeleteExtIO((struct IORequest *)cmsg);
		DeletePort(mp);
		return 0;
	}

	append_uword(screen->Width);
	append_uword(screen->Height);
	append_uword(bm->Depth);
	append_uword(bm->BytesPerRow);

	ULONG ptr = TranslateAddressA314(bm->Planes[0]);

	if (ptr == -1)
	{
		int depth = bm->Depth;
		int size = 80 * 256;

		UBYTE *p = AllocMem(size * depth, MEMF_A314);
		if (!p)
		{
			Permit();

			printf("Unable to allocate enough A314 memory\n");

			sync_a314_reset();

			CloseLibrary((struct Library *)GfxBase);
			CloseLibrary((struct Library *)IntuitionBase);
			CloseDevice((struct IORequest *)cmsg);
			DeleteExtIO((struct IORequest *)rmsg);
			DeleteExtIO((struct IORequest *)wmsg);
			DeleteExtIO((struct IORequest *)cmsg);
			DeletePort(mp);
			return 0;
		}

		if (bm->BytesPerRow == 80)
		{
			UBYTE *old_planes[8];

			for (int i = 0; i < depth; i++)
			{
				UBYTE *op = bm->Planes[i];
				UBYTE *np = p + (i * size);
				memcpy(np, op, size);
				old_planes[i] = op;
				bm->Planes[i] = np;
			}

			RemakeDisplay();

			for (int i = 0; i < depth; i++)
				FreeMem(old_planes[i], size);

			ptr = TranslateAddressA314(bm->Planes[0]);
		}
		else if (bm->BytesPerRow == 240)
		{
			UBYTE *old_ptr = bm->Planes[0];

			memcpy(p, old_ptr, size * depth);

			for (int i = 0; i < depth; i++)
				bm->Planes[i] = p + (i * 80);

			RemakeDisplay();

			FreeMem(old_ptr, size * depth);

			ptr = TranslateAddressA314(bm->Planes[0]);
		}
	}

	append_ulong(ptr);

	struct ColorMap *cm = screen->ViewPort.ColorMap;
	append_uword(cm->Count);
	for (int i = 0; i < cm->Count; i++)
		append_uword(((UWORD *)cm->ColorTable)[i] & 0xfff);

	Permit();

	sync_a314_write(blen);

	id_mp = CreatePort(NULL, 0);
	id_req = (struct IOStdReq *)CreateExtIO(id_mp, sizeof(struct IOStdReq));
	OpenDevice("input.device", 0, (struct IORequest *)id_req, 0);

	start_con_read();
	start_a314_read();

	BYTE vblank_sigbit = AllocSignal(-1);

	ULONG vblanksig = 1 << vblank_sigbit;
	ULONG portsig = 1 << mp->mp_SigBit;

	vblank_data.task = FindTask(NULL);
	vblank_data.signal = vblanksig;

	vblank_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	vblank_interrupt.is_Node.ln_Pri = -60;
	vblank_interrupt.is_Node.ln_Name = NULL;
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
	
	CloseDevice((struct IORequest *)id_req);
	DeleteExtIO((struct IORequest *)id_req);
	DeletePort(id_mp);
	CloseLibrary((struct Library *)GfxBase);
	CloseLibrary((struct Library *)IntuitionBase);;
	CloseDevice((struct IORequest *)cmsg);
	DeleteExtIO((struct IORequest *)rmsg);
	DeleteExtIO((struct IORequest *)wmsg);
	DeleteExtIO((struct IORequest *)cmsg);
	DeletePort(mp);
	return 0;
}
