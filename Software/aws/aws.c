/*
 * Copyright (c) 2021 Niklas Ekström
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
#include <graphics/gfx.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

#define AWS_REQ_OPEN_WINDOW 1
#define AWS_RES_OPEN_WINDOW_FAIL 2
#define AWS_RES_OPEN_WINDOW_SUCCESS 3
#define AWS_REQ_CLOSE_WINDOW 4
#define AWS_REQ_FLIP_BUFFER 5
#define AWS_REQ_WB_SCREEN_INFO 6
#define AWS_RES_WB_SCREEN_INFO 7
#define AWS_EVENT_CLOSE_WINDOW 8
#define AWS_EVENT_FLIP_DONE 9

struct WindowInfo
{
	struct WindowInfo *next;
	struct WindowInfo *prev;
	struct Window *window;
	struct BitMap bm;
	UBYTE *buffer;
	UWORD buf_width;
	UWORD buf_height;
	UBYTE buf_depth;
	UBYTE wid;
	char title[66];
};

static struct WindowInfo *windows_head = NULL;
static struct WindowInfo *windows_tail = NULL;

struct Library *A314Base;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static struct MsgPort *mp;
static struct MsgPort *wmp;

static struct A314_IORequest *cmsg;
static struct A314_IORequest *rmsg;
static struct A314_IORequest *wmsg;

static ULONG socket;

static UBYTE arbuf[256];
static UBYTE awbuf[256];

static BOOL pending_a314_read = FALSE;
static BOOL pending_a314_write = FALSE;
static BOOL pending_a314_reset = FALSE;

static BOOL stream_closed = FALSE;

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

static void wait_a314_write_complete()
{
	if (pending_a314_write)
	{
		WaitIO((struct IORequest *)wmsg);
		pending_a314_write = FALSE;
	}
}

static void start_a314_write(int length)
{
	start_a314_cmd(wmsg, A314_WRITE, awbuf, length);
	pending_a314_write = TRUE;
}

static void start_a314_reset()
{
	start_a314_cmd(cmsg, A314_RESET, NULL, 0);
	pending_a314_reset = TRUE;
}

static struct WindowInfo *find_window_by_id(ULONG wid)
{
	struct WindowInfo *wi = windows_head;
	while (wi)
	{
		if (wi->wid == wid)
			break;
		wi = wi->next;
	}
	return wi;
}

static struct WindowInfo *find_window(struct Window *w)
{
	struct WindowInfo *wi = windows_head;
	while (wi)
	{
		if (wi->window == w)
			break;
		wi = wi->next;
	}
	return wi;
}

static void append_window(struct WindowInfo *wi)
{
	wi->next = NULL;
	wi->prev = windows_tail;
	if (windows_tail)
		windows_tail->next = wi;
	else
		windows_head = wi;
	windows_tail = wi;
}

static void remove_window(struct WindowInfo *wi)
{
	if (wi->prev)
		wi->prev->next = wi->next;
	else
		windows_head = wi->next;

	if (wi->next)
		wi->next->prev = wi->prev;
	else
		windows_tail = wi->prev;

	wi->prev = NULL;
	wi->next = NULL;
}

static void handle_req_open_window()
{
	struct WindowInfo *wi = (struct WindowInfo *)AllocMem(sizeof(struct WindowInfo), MEMF_CLEAR);

	UBYTE wid = arbuf[1];
	UWORD left = *(UWORD *)&arbuf[2];
	UWORD top = *(UWORD *)&arbuf[4];
	UWORD width = *(UWORD *)&arbuf[6];
	UWORD height = *(UWORD *)&arbuf[8];

	int title_len = rmsg->a314_Length - 10;
	if (title_len > sizeof(wi->title) - 1)
		title_len = sizeof(wi->title) - 1;
	memcpy(wi->title, &arbuf[10], title_len);

	struct NewWindow nw =
	{
		left, top,
		width, height,
		0, 1,
		0,
		WINDOWCLOSE | ACTIVATE | WINDOWDEPTH | WINDOWDRAG | SIMPLE_REFRESH,
		NULL, NULL,
		wi->title,
		NULL, NULL,
		0, 0,
		0, 0,
		WBENCHSCREEN
	};

	struct Window *w = OpenWindow(&nw);
	if (w)
	{
		w->UserPort = mp;
		ModifyIDCMP(w, CLOSEWINDOW | REFRESHWINDOW);

		UWORD bw = w->Width - (w->BorderLeft + w->BorderRight);
		UWORD bh = w->Height - (w->BorderTop + w->BorderBottom);
		UWORD bd = w->WScreen->BitMap.Depth;

		ULONG bpr = ((bw + 15) & ~15) >> 3;
		void *buffer = AllocMem(bpr * bh * bd, MEMF_A314 | MEMF_CHIP);
		if (!buffer)
			CloseWindow(w);
		else
		{
			wi->window = w;
			wi->buffer = buffer;
			wi->buf_width = bw;
			wi->buf_height = bh;
			wi->buf_depth = bd;
			wi->wid = wid;

			wi->bm.BytesPerRow = bpr;
			wi->bm.Rows = bh;
			wi->bm.Depth = bd;
			for (int i = 0; i < bd; i++)
				wi->bm.Planes[i] = wi->buffer + bpr * bh * i;

			append_window(wi);

			wait_a314_write_complete();

			awbuf[0] = AWS_RES_OPEN_WINDOW_SUCCESS;
			awbuf[1] = wid;
			*(ULONG *)&awbuf[2] = TranslateAddressA314(buffer);
			*(UWORD *)&awbuf[6] = bw;
			*(UWORD *)&awbuf[8] = bh;
			*(UWORD *)&awbuf[10] = bd;
			start_a314_write(12);
			return;
		}
	}

	FreeMem(wi, sizeof(struct WindowInfo));

	wait_a314_write_complete();
	awbuf[0] = AWS_RES_OPEN_WINDOW_FAIL;
	awbuf[1] = wid;
	start_a314_write(2);
}

static void close_window(struct WindowInfo *wi)
{
	remove_window(wi);

	ULONG buffer_size = wi->bm.BytesPerRow * wi->bm.Rows * wi->bm.Depth;
	FreeMem(wi->buffer, buffer_size);

	wi->window->UserPort = NULL;
	CloseWindow(wi->window);

	FreeMem(wi, sizeof(struct WindowInfo));
}

static void handle_req_close_window()
{
	UBYTE wid = arbuf[1];
	struct WindowInfo *wi = find_window_by_id(wid);
	if (wi)
		close_window(wi);
}

static void redraw_window(struct WindowInfo *wi)
{
	struct RastPort rp;
	rp.Layer = NULL;
	rp.BitMap = &wi->bm;
	struct Window *w = wi->window;
	ClipBlit(&rp, 0, 0, w->RPort, w->BorderLeft, w->BorderTop, wi->buf_width, wi->buf_height, 0xc0);
}

static void handle_req_flip_buffer()
{
	UBYTE wid = arbuf[1];
	struct WindowInfo *wi = find_window_by_id(wid);
	if (wi)
	{
		redraw_window(wi);

		wait_a314_write_complete();
		awbuf[0] = AWS_EVENT_FLIP_DONE;
		awbuf[1] = wi->wid;
		start_a314_write(2);
	}
}

static struct Screen *find_wb_screen()
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

static void handle_req_wb_screen_info()
{
	struct Screen *screen = find_wb_screen();

	wait_a314_write_complete();

	awbuf[0] = AWS_RES_WB_SCREEN_INFO;
	awbuf[1] = 0;

	UWORD *p = (UWORD *)&awbuf[2];
	*p++ = screen->Width;
	*p++ = screen->Height;
	*p++ = screen->BitMap.Depth;

	struct ColorMap *cm = screen->ViewPort.ColorMap;
	int n = 1 << screen->BitMap.Depth;
	if (n > cm->Count)
		n = cm->Count;
	for (int i = 0; i < n; i++)
		*p++ = ((UWORD *)cm->ColorTable)[i] & 0xfff;

	start_a314_write((UBYTE *)p - awbuf);
}

static void handle_intui_message(struct IntuiMessage *im)
{
	ULONG class = im->Class;
	struct WindowInfo *wi = find_window(im->IDCMPWindow);
	ReplyMsg((struct Message *)im);

	switch (class)
	{
		case CLOSEWINDOW:
			wait_a314_write_complete();
			awbuf[0] = AWS_EVENT_CLOSE_WINDOW;
			awbuf[1] = wi->wid;
			start_a314_write(2);
			break;

		case REFRESHWINDOW:
			BeginRefresh(wi->window);
			redraw_window(wi);
			EndRefresh(wi->window, TRUE);
			break;
	}
}

static void handle_a314_read_completed()
{
	pending_a314_read = FALSE;

	if (stream_closed)
		return;

	int res = rmsg->a314_Request.io_Error;
	if (res == A314_READ_OK)
	{
		switch (arbuf[0])
		{
			case AWS_REQ_OPEN_WINDOW:
				handle_req_open_window();
				break;
			case AWS_REQ_CLOSE_WINDOW:
				handle_req_close_window();
				break;
			case AWS_REQ_FLIP_BUFFER:
				handle_req_flip_buffer();
				break;
			case AWS_REQ_WB_SCREEN_INFO:
				handle_req_wb_screen_info();
				break;
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

int main()
{
	LONG old_priority = SetTaskPri(FindTask(NULL), 20);

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);

	mp = CreatePort(NULL, 0);
	wmp = CreatePort(NULL, 0);
	cmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)cmsg, 0) != 0)
	{
		printf("Unable to open a314.device\n");
		goto fail_out1;
	}

	A314Base = &(cmsg->a314_Request.io_Device->dd_Library);

	rmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	wmsg = (struct A314_IORequest *)CreateExtIO(mp, sizeof(struct A314_IORequest));
	memcpy(rmsg, cmsg, sizeof(struct A314_IORequest));
	memcpy(wmsg, cmsg, sizeof(struct A314_IORequest));
	wmsg->a314_Request.io_Message.mn_ReplyPort = wmp;

	if (a314_connect("awsproxy") != A314_CONNECT_OK)
	{
		printf("Unable to connect to awsproxy\n");
		goto fail_out2;
	}

	start_a314_read();

	ULONG mp_sig = 1 << mp->mp_SigBit;
	ULONG wmp_sig = 1 << wmp->mp_SigBit;

	printf("Press ctrl-c to exit...\n");

	while (TRUE)
	{
		ULONG signal = Wait(mp_sig | wmp_sig | SIGBREAKF_CTRL_C);

		if (signal & wmp_sig)
		{
			struct Message *msg = GetMsg(wmp);
			if (msg)
				pending_a314_write = FALSE;
		}

		if (signal & mp_sig)
		{
			struct Message *msg;
			while (msg = GetMsg(mp))
			{
				if (msg == (struct Message *)rmsg)
					handle_a314_read_completed();
				else if (msg == (struct Message *)cmsg)
					pending_a314_reset = FALSE;
				else
					handle_intui_message((struct IntuiMessage *)msg);
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

	while (windows_head)
		close_window(windows_head);

fail_out2:
	DeleteExtIO((struct IORequest *)wmsg);
	DeleteExtIO((struct IORequest *)rmsg);
	CloseDevice((struct IORequest *)cmsg);
fail_out1:
	DeleteExtIO((struct IORequest *)cmsg);
	DeletePort(wmp);
	DeletePort(mp);
	CloseLibrary((struct Library *)IntuitionBase);
	CloseLibrary((struct Library *)GfxBase);
	SetTaskPri(FindTask(NULL), old_priority);
	return 0;
}
