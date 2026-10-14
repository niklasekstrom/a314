/*
 * Copyright (c) 2026 Jeremy Bauer
 *
 * A314Info - display Raspberry Pi system statistics on the Amiga.
 *
 * Connects to the "a314info" service on the Pi (a314info.py) and
 * displays per-core CPU usage, memory usage, kernel version and root
 * filesystem disk usage, refreshed at a configurable interval.
 *
 * UI: MUI 3.8+ when muimaster.library is present, otherwise a GadTools
 * fallback. Requires AmigaOS 2.04+.
 *
 * Usage (Shell):  A314Info [REFRESH=<ms>] [MUI] [GT] [TXT]
 *                 REFRESH in 500..30000, default 2000
 *                 MUI/GT force the corresponding UI; TXT prints one
 *                 statistics snapshot on the console and exits.
 * From Workbench: REFRESH, MUI, GT and TXT tooltypes on the icon.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <devices/timer.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>

#include <dos/dos.h>
#include <dos/rdargs.h>

#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <graphics/rastport.h>

#include <workbench/workbench.h>
#include <workbench/icon.h>

#include <utility/tagitem.h>
#include <utility/hooks.h>

#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/icon.h>
#include <proto/muimaster.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../a314device/a314.h"

#define SERVICE_NAME	"a314info"
#define REQ_STATS	0x01

#define MAX_CORES	8
#define READ_CHUNK_SIZE	255

/* Reply frame: u32 body_len + body (proto_ver, ncores, cpu[8],
 * reserved u16[2], mem_total, mem_avail, disk_total,
 * disk_free, kernel[64]) */
#define BODY_LEN	94
#define FRAME_LEN	(4 + BODY_LEN)

#define MIN_REFRESH_MS	500
#define DEF_REFRESH_MS	2000
#define MAX_REFRESH_MS	30000

/* On a failed connect, wait this many ticks before retrying. */
#define CONNECT_RETRY_TICKS	4

/* Reset the socket when a request stays pending for this many ticks. */
#define REQ_STALL_TICKS	5

/* Seconds a single exchange may take in TXT mode. */
#define TXT_TIMEOUT_S	8

enum { IF_MUI, IF_GT, IF_TXT };
static int ui_mode = IF_TXT;

#define APPNAME		"A314Info"
static const char verstag[] = "$VER: A314Info 1.1 (01.11.2026)";

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d)	((ULONG)(a) << 24 | (ULONG)(b) << 16 | \
				(ULONG)(c) << 8 | (ULONG)(d))
#endif

/*
 * vbcc ships no link library for the variadic MUI constructors, so
 * provide the same small glue as the MUI SDK's DLib sources.
 */
#pragma stdargs-on
Object *MUI_NewObject(char *classname, Tag tag1, ...)
{
	return MUI_NewObjectA(classname, (struct TagItem *)&tag1);
}

Object *MUI_MakeObject(LONG type, ...)
{
	return MUI_MakeObjectA(type, (ULONG *)(((ULONG)&type) + 4));
}
#pragma stdargs-off

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *MUIMasterBase;
struct Library *IconBase;

/* ------------------------------------------------ */
/* Statistics frame                                  */
/* ------------------------------------------------ */

struct Stats
{
	UBYTE ncores;
	UBYTE cpu[MAX_CORES];
	ULONG mem_total_kb;
	ULONG mem_avail_kb;
	ULONG disk_total_mb;
	ULONG disk_free_mb;
	char kernel[64];
};

static struct Stats stats;
static BOOL have_stats = FALSE;

/* UI callbacks, defined below. */
void ui_status(const char *s);
void gt_status(const char *s);
static void ui_stats(void);
static void mui_stats(void);
static void gt_stats(void);

static ULONG rd32(UBYTE *p)
{
	return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) |
		((ULONG)p[2] << 8) | (ULONG)p[3];
}

static void decode_stats(UBYTE *body)
{
	stats.ncores = body[1] > MAX_CORES ? MAX_CORES : body[1];
	memcpy(stats.cpu, body + 2, MAX_CORES);
	stats.mem_total_kb = rd32(body + 14);
	stats.mem_avail_kb = rd32(body + 18);
	stats.disk_total_mb = rd32(body + 22);
	stats.disk_free_mb = rd32(body + 26);
	memcpy(stats.kernel, body + 30, 63);
	stats.kernel[63] = 0;
	have_stats = TRUE;
}

/* ------------------------------------------------ */
/* a314.device client                                */
/* ------------------------------------------------ */

static struct MsgPort *a314_mp;		/* async request/read completions */
static struct MsgPort *sync_mp;		/* reset completions */
static struct A314_IORequest *a314_sync;
static struct A314_IORequest *a314_req;	/* connect and stats requests */
static struct A314_IORequest *a314_read;

static ULONG a314_socket;
static BOOL a314_opened = FALSE;
static BOOL connected = FALSE;
static BOOL read_pending = FALSE;
static BOOL req_pending = FALSE;
static BOOL req_is_connect = FALSE;
static ULONG req_stall = 0;
static UBYTE req_buf;

static UBYTE read_buf[READ_CHUNK_SIZE];
static UBYTE frame_buf[FRAME_LEN];
static ULONG frame_have = 0;

static ULONG connect_wait = 0;

static void start_cmd(struct A314_IORequest *ior, UWORD cmd, char *buf, int len)
{
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = a314_socket;
	ior->a314_Buffer = buf;
	ior->a314_Length = len;
	SendIO((struct IORequest *)ior);
}

static BYTE sync_cmd(struct A314_IORequest *ior, UWORD cmd, char *buf, int len)
{
	start_cmd(ior, cmd, buf, len);
	return WaitIO((struct IORequest *)ior);
}

static void start_read(void)
{
	start_cmd(a314_read, A314_READ, (char *)read_buf, READ_CHUNK_SIZE);
	read_pending = TRUE;
}

/* Connects and stats requests run asynchronously so that a slow or
 * dead Pi never blocks the user interface: the device only answers a
 * request once the Pi does, and AbortIO is a no-op on a314.device, so
 * a synchronous wait could hang the whole application. */

static void a314_start_connect(void)
{
	a314_socket++;
	start_cmd(a314_req, A314_CONNECT, (char *)SERVICE_NAME,
		strlen(SERVICE_NAME));
	req_pending = TRUE;
	req_is_connect = TRUE;
}

static void a314_start_stats_request(void)
{
	req_buf = REQ_STATS;
	start_cmd(a314_req, A314_WRITE, (char *)&req_buf, 1);
	req_pending = TRUE;
	req_is_connect = FALSE;
}

static void handle_req_done(void)
{
	BYTE err = a314_req->a314_Request.io_Error;

	req_pending = FALSE;
	req_stall = 0;

	if (req_is_connect)
	{
		if (err == A314_CONNECT_OK)
		{
			connected = TRUE;
			connect_wait = 0;
			frame_have = 0;
			ui_status("Connected");
			start_read();

			/* Fetch the first stats frame without waiting for a tick. */
			a314_start_stats_request();
		}
		else
		{
			ui_status("Waiting for Pi");
		}
	}
	else if (connected && err != A314_WRITE_OK)
	{
		connected = FALSE;
		ui_status("Disconnected - retrying");
	}
}

static void consume(UBYTE *p, int len)
{
	while (len > 0)
	{
		int take = (int)(FRAME_LEN - frame_have);

		if (take > len)
			take = len;

		memcpy(frame_buf + frame_have, p, take);
		frame_have += take;
		p += take;
		len -= take;

		if (frame_have >= 4 && rd32(frame_buf) != BODY_LEN)
		{
			/* lost sync; drop and resync on the next frame */
			frame_have = 0;
		}
		else if (frame_have == FRAME_LEN)
		{
			if (frame_buf[4] == 1)
			{
				decode_stats(frame_buf + 4);
				ui_stats();
			}
			frame_have = 0;
		}
	}
}

static void handle_read_done(void)
{
	read_pending = FALSE;

	if (!connected)
		return;

	if (a314_read->a314_Request.io_Error == A314_READ_OK)
	{
		consume(read_buf, a314_read->a314_Length);
		if (connected)
			start_read();
	}
	else
	{
		/* A314_READ_EOS or A314_READ_RESET */
		connected = FALSE;
		ui_status("Disconnected - retrying");
	}
}

/* RESET is answered immediately by the device and completes every
 * pending request on the socket (a314.device cannot abort requests).
 * Once RESET has been sent, WaitIO on a pending request always
 * returns at once. */
static void a314_hard_reset(void)
{
	sync_cmd(a314_sync, A314_RESET, NULL, 0);

	if (req_pending)
	{
		WaitIO((struct IORequest *)a314_req);
		req_pending = FALSE;
	}
	if (read_pending)
	{
		WaitIO((struct IORequest *)a314_read);
		read_pending = FALSE;
	}
	req_stall = 0;
	connected = FALSE;
}

static void a314_disconnect(void)
{
	if (connected || req_pending || read_pending)
		a314_hard_reset();
}

/* Drain completed a314 device requests from the reply port. */
static void handle_a314_port(void)
{
	struct Message *msg;
	while ((msg = GetMsg(a314_mp)) != NULL)
	{
		if (msg == (struct Message *)a314_read)
			handle_read_done();
		else if (msg == (struct Message *)a314_req)
			handle_req_done();
	}
}

/* ------------------------------------------------ */
/* Refresh timer                                     */
/* ------------------------------------------------ */

static struct MsgPort *timer_mp;
static struct timerequest *timer_ior;
static ULONG refresh_ms = DEF_REFRESH_MS;

static void arm_timer(void)
{
	timer_ior->tr_node.io_Command = TR_ADDREQUEST;
	timer_ior->tr_node.io_Error = 0;
	timer_ior->tr_time.tv_secs = refresh_ms / 1000;
	timer_ior->tr_time.tv_micro = (refresh_ms % 1000) * 1000;
	SendIO((struct IORequest *)timer_ior);
}

static void handle_timer(void)
{
	BOOL fired = FALSE;

	while (GetMsg(timer_mp) != NULL)
		fired = TRUE;

	if (!fired)
		return;

	if (req_pending)
	{
		/* A connect or write is still unanswered; if the Pi stays
		 * silent for too long, drop the socket and retry. */
		if (++req_stall >= REQ_STALL_TICKS)
		{
			a314_hard_reset();
			connect_wait = CONNECT_RETRY_TICKS;
			ui_status("Waiting for Pi");
		}
	}
	else if (!connected)
	{
		if (connect_wait == 0)
		{
			a314_start_connect();
			connect_wait = CONNECT_RETRY_TICKS;
		}
		else
		{
			connect_wait--;
			ui_status("Waiting for Pi");
		}
	}
	else
	{
		a314_start_stats_request();
	}

	arm_timer();
}

/* ------------------------------------------------ */
/* MUI user interface                                */
/* ------------------------------------------------ */

static Object *m_app;
static Object *m_win;
static Object *m_coregroup;
static Object *m_corerow[MAX_CORES];
static Object *m_coregauge[MAX_CORES];
static Object *m_kernel, *m_status;
static Object *m_memgauge, *m_memtxt, *m_diskgauge, *m_disktxt;
static Object *m_slider, *m_slidertxt;
static Object *m_menu_about, *m_menu_quit;
static UBYTE m_cores_shown = MAX_CORES;	/* every row starts as a group member */

static char slider_buf[16];

static ULONG refresh_changed(__reg("a0") struct Hook *hook, __reg("a2") Object *obj, __reg("a1") APTR msg)
{
	ULONG v = refresh_ms;
	get(m_slider, MUIA_Numeric_Value, &v);
	if (v < MIN_REFRESH_MS)
		v = MIN_REFRESH_MS;
	if (v > MAX_REFRESH_MS)
		v = MAX_REFRESH_MS;
	refresh_ms = v;
	sprintf(slider_buf, "%ld ms", (long)v);
	set(m_slidertxt, MUIA_Text_Contents, (ULONG)slider_buf);
	return 0;
}

static struct Hook refresh_hook = {
	{NULL, NULL}, (HOOKFUNC)refresh_changed, NULL, NULL
};

void ui_status(const char *s)
{
	if (m_app)
		set(m_status, MUIA_Text_Contents, (ULONG)s);
	else
		gt_status(s);
}

static void mui_stats(void)
{
	int i;

	if (!have_stats || !m_app)
		return;

	set(m_kernel, MUIA_Text_Contents, (ULONG)stats.kernel);

	if (stats.ncores != m_cores_shown)
	{
		/* Resize the group to hold exactly one row per real core.
		 * Hiding surplus rows with MUIA_ShowMe proved unreliable
		 * (the group could end up collapsed and empty), so they are
		 * physically removed instead and re-added if the reported
		 * count ever grows.  A corrupt ncores of zero keeps all
		 * rows rather than emptying the group. */
		UBYTE want = stats.ncores ? stats.ncores : MAX_CORES;

		DoMethod(m_coregroup, MUIM_Group_InitChange);
		for (i = m_cores_shown; i < want; i++)
			DoMethod(m_coregroup, OM_ADDMEMBER, (ULONG)m_corerow[i]);
		for (i = want; i < m_cores_shown; i++)
			DoMethod(m_coregroup, OM_REMMEMBER, (ULONG)m_corerow[i]);
		DoMethod(m_coregroup, MUIM_Group_ExitChange);
		m_cores_shown = want;
	}

	for (i = 0; i < stats.ncores; i++)
		set(m_coregauge[i], MUIA_Gauge_Current, (ULONG)stats.cpu[i]);

	if (stats.mem_total_kb)
	{
		ULONG used = stats.mem_total_kb - stats.mem_avail_kb;
		ULONG pct = (used * 100 + stats.mem_total_kb / 2) / stats.mem_total_kb;
		static char buf[32];
		set(m_memgauge, MUIA_Gauge_Current, pct);
		sprintf(buf, "%ld / %ld MB", (long)(used / 1024), (long)(stats.mem_total_kb / 1024));
		set(m_memtxt, MUIA_Text_Contents, (ULONG)buf);
	}

	if (stats.disk_total_mb)
	{
		ULONG used = stats.disk_total_mb - stats.disk_free_mb;
		ULONG pct = (used * 100 + stats.disk_total_mb / 2) / stats.disk_total_mb;
		static char buf[32];
		set(m_diskgauge, MUIA_Gauge_Current, pct);
		sprintf(buf, "%ld / %ld MB", (long)used, (long)stats.disk_total_mb);
		set(m_disktxt, MUIA_Text_Contents, (ULONG)buf);
	}
}

/* Re-add any CPU rows that were removed from the group so that the
 * whole object tree is freed again when the application is disposed. */
static void mui_reattach_rows(void)
{
	if (m_coregroup && m_cores_shown != MAX_CORES)
	{
		int i;

		DoMethod(m_coregroup, MUIM_Group_InitChange);
		for (i = m_cores_shown; i < MAX_CORES; i++)
			DoMethod(m_coregroup, OM_ADDMEMBER, (ULONG)m_corerow[i]);
		DoMethod(m_coregroup, MUIM_Group_ExitChange);
	}
}

#define CPU_ROW(n) \
	Child, m_corerow[n] = HGroup, \
		Child, Label("Core " #n), \
		Child, m_coregauge[n] = GaugeObject, \
			GaugeFrame, \
			MUIA_Gauge_Max, 100, \
			MUIA_Gauge_Current, 0, \
			MUIA_Gauge_Horiz, TRUE, \
			MUIA_Gauge_InfoText, "%ld%%", \
		End, \
	End,

static BOOL mui_main(void)
{
	m_app = ApplicationObject,
		MUIA_Application_Title, (ULONG)APPNAME,
		MUIA_Application_Version, (ULONG)verstag,
		MUIA_Application_Copyright, (ULONG)"(C) 2026 Jeremy Bauer",
		MUIA_Application_Author, (ULONG)"Jeremy Bauer",
		MUIA_Application_Description, (ULONG)"Raspberry Pi system information",
		MUIA_Application_Base, (ULONG)"A314INFO",
		MUIA_Application_SingleTask, TRUE,
		MUIA_Application_Menustrip, MenustripObject,
			MUIA_Family_Child, MenuObject,
				MUIA_Menu_Title, (ULONG)"Project",
				MUIA_Family_Child, m_menu_about = MenuitemObject,
					MUIA_Menuitem_Title, (ULONG)"About",
				End,
				MUIA_Family_Child, m_menu_quit = MenuitemObject,
					MUIA_Menuitem_Title, (ULONG)"Quit",
					MUIA_Menuitem_Shortcut, (ULONG)"Q",
				End,
			End,
		End,
		SubWindow, m_win = WindowObject,
			MUIA_Window_Title, (ULONG)APPNAME,
			MUIA_Window_ID, MAKE_ID('A', '3', 'I', 'N'),
			WindowContents, ScrollgroupObject,
				MUIA_Scrollgroup_FreeHoriz, FALSE,
				MUIA_Scrollgroup_FreeVert, TRUE,
				MUIA_Scrollgroup_Contents, VGroupV,
				Child, VGroup,
					GroupFrameT("Raspberry Pi"),
					Child, ColGroup(2),
						Child, Label("Kernel"),
						Child, m_kernel = TextObject,
							TextFrame,
							MUIA_Text_Contents, (ULONG)"-",
						End,
						Child, Label("Status"),
						Child, m_status = TextObject,
							TextFrame,
							MUIA_Text_Contents, (ULONG)"Connecting",
						End,
					End,
				End,
				Child, m_coregroup = VGroup,
					GroupFrameT("CPU usage"),
					CPU_ROW(0)
					CPU_ROW(1)
					CPU_ROW(2)
					CPU_ROW(3)
					CPU_ROW(4)
					CPU_ROW(5)
					CPU_ROW(6)
					CPU_ROW(7)
				End,
				Child, VGroup,
					GroupFrameT("Memory"),
					Child, ColGroup(3),
						Child, Label("Use"),
						Child, m_memgauge = GaugeObject,
							GaugeFrame,
							MUIA_Gauge_Max, 100,
							MUIA_Gauge_Current, 0,
							MUIA_Gauge_Horiz, TRUE,
							MUIA_Gauge_InfoText, (ULONG)"%ld%% in use",
						End,
						Child, m_memtxt = TextObject,
							MUIA_Text_Contents, (ULONG)"-",
							MUIA_FixWidthTxt, (ULONG)"00000 / 00000 MB",
						End,
					End,
				End,
				Child, VGroup,
					GroupFrameT("Disk (/)"),
					Child, ColGroup(3),
						Child, Label("Use"),
						Child, m_diskgauge = GaugeObject,
							GaugeFrame,
							MUIA_Gauge_Max, 100,
							MUIA_Gauge_Current, 0,
							MUIA_Gauge_Horiz, TRUE,
							MUIA_Gauge_InfoText, (ULONG)"%ld%% in use",
						End,
						Child, m_disktxt = TextObject,
							MUIA_Text_Contents, (ULONG)"-",
							MUIA_FixWidthTxt, (ULONG)"00000 / 00000 MB",
						End,
					End,
				End,
				Child, VGroup,
					GroupFrameT("Options"),
					Child, ColGroup(3),
						Child, Label("Refresh"),
						Child, m_slider = SliderObject,
							MUIA_Numeric_Min, MIN_REFRESH_MS,
							MUIA_Numeric_Max, MAX_REFRESH_MS,
							MUIA_Numeric_Value, refresh_ms,
							MUIA_ShortHelp, (ULONG)"Statistics refresh interval in milliseconds",
						End,
						Child, m_slidertxt = TextObject,
							MUIA_Text_Contents, (ULONG)"",
							MUIA_FixWidthTxt, (ULONG)"30000 ms",
						End,
					End,
				End,
			End,
			End,
		End,
	End;

	if (!m_app)
		return FALSE;

	sprintf(slider_buf, "%ld ms", (long)refresh_ms);
	set(m_slidertxt, MUIA_Text_Contents, (ULONG)slider_buf);

	DoMethod(m_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		(ULONG)m_app, 2, MUIM_Application_ReturnID,
		MUIV_Application_ReturnID_Quit);

	DoMethod(m_app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
		(ULONG)m_app, 3, MUIM_Set, MUIA_Application_Iconified, FALSE);

	DoMethod(m_app, MUIM_Notify, MUIA_Application_DoubleStart, MUIV_EveryTime,
		(ULONG)m_win, 1, MUIM_Window_ToFront);

	DoMethod(m_menu_quit, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
		(ULONG)m_app, 2, MUIM_Application_ReturnID,
		MUIV_Application_ReturnID_Quit);

	DoMethod(m_menu_about, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
		(ULONG)m_app, 2, MUIM_Application_AboutMUI, (ULONG)m_win);

	DoMethod(m_slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
		(ULONG)m_app, 2, MUIM_CallHook, (ULONG)&refresh_hook);

	if (a314_opened)
		a314_start_connect();
	arm_timer();

	set(m_win, MUIA_Window_Open, TRUE);

	{
		ULONG is_open = 0;
		get(m_win, MUIA_Window_Open, &is_open);

		if (is_open)
		{
			ULONG a314sig = 1L << a314_mp->mp_SigBit;
			ULONG timersig = 1L << timer_mp->mp_SigBit;
			ULONG signals = 0;

			while (DoMethod(m_app, MUIM_Application_NewInput, &signals)
				!= MUIV_Application_ReturnID_Quit)
			{
				/* MUIM_Application_NewInput expects signals to hold
				 * the result of Wait() (or 0), so reuse it. */
				signals = Wait(signals | a314sig | timersig |
					SIGBREAKF_CTRL_C);

				if (signals & a314sig)
					handle_a314_port();
				if (signals & timersig)
					handle_timer();
				if (signals & SIGBREAKF_CTRL_C)
					break;
			}
		}
		else
		{
			mui_reattach_rows();
			MUI_DisposeObject(m_app);
			m_app = NULL;
			return FALSE;
		}
	}

	set(m_win, MUIA_Window_Open, FALSE);
	mui_reattach_rows();
	MUI_DisposeObject(m_app);
	m_app = NULL;
	return TRUE;
}

/* ------------------------------------------------ */
/* GadTools / Intuition fallback UI                  */
/* ------------------------------------------------ */

#define GT_W		380
#define GT_LAB_X	8
#define GT_LAB_W	60
#define GT_BAR_X	72
#define GT_BAR_W	200
#define GT_VAL_X	280
#define GT_ROW_H	16

#define GT_ROW_KERNEL	0
#define GT_ROW_STATUS	1
#define GT_ROW_HDR	2	/* "CPU usage" header */
#define GT_ROW_CORE0	3	/* 8 core rows */
#define GT_ROW_MEM	(GT_ROW_CORE0 + MAX_CORES)
#define GT_ROW_DISK	(GT_ROW_MEM + 1)
#define GT_ROW_SLIDER	(GT_ROW_DISK + 2)

static struct Window *gt_win;
static struct Screen *gt_screen;
static APTR gt_vi;
static struct Gadget *gt_glist;	/* head of the GadTools gadget list */
static struct Gadget *gt_slider;
static int gt_xoff, gt_yoff;	/* window border sizes: RPort (0,0) is the
				 * top-left of the whole window, so all
				 * drawing must be shifted into the inner
				 * area by the border dimensions */
static char gt_status_text[64] = "Connecting";

static void gt_text(int row, int x, const char *s, int clearw)
{
	struct RastPort *rp = gt_win->RPort;
	int y = gt_yoff + 8 + row * GT_ROW_H + 11;

	x += gt_xoff;
	if (clearw)
	{
		SetAPen(rp, 0);
		RectFill(rp, x, y - 9, x + clearw - 1, y + 2);
	}
	Move(rp, x, y);
	SetAPen(rp, 1);
	Text(rp, (char *)s, strlen(s));
}

static void gt_bar(int row, int pct)
{
	struct RastPort *rp = gt_win->RPort;
	int x = gt_xoff + GT_BAR_X;
	int y = gt_yoff + 8 + row * GT_ROW_H + 3;
	int w = GT_BAR_W;
	int h = 10;
	int fill = (w - 2) * pct / 100;

	SetAPen(rp, 1);
	RectFill(rp, x, y, x + w - 1, y + h - 1);
	SetAPen(rp, 0);
	RectFill(rp, x + 1 + fill, y + 1, x + w - 2, y + h - 2);
}

void gt_status(const char *s)
{
	if (!gt_win)
	{
		strncpy(gt_status_text, s, sizeof(gt_status_text) - 1);
		gt_status_text[sizeof(gt_status_text) - 1] = 0;
		return;
	}
	strncpy(gt_status_text, s, sizeof(gt_status_text) - 1);
	gt_status_text[sizeof(gt_status_text) - 1] = 0;
	gt_text(GT_ROW_STATUS, GT_BAR_X, gt_status_text, GT_W - GT_BAR_X - 8);
}

static void gt_stats(void)
{
	int i;
	char buf[64];

	if (!have_stats || !gt_win)
		return;

	gt_text(GT_ROW_KERNEL, GT_BAR_X, stats.kernel, GT_W - GT_BAR_X - 8);

	for (i = 0; i < MAX_CORES; i++)
	{
		if (i < stats.ncores)
		{
			sprintf(buf, "%d%%", (int)stats.cpu[i]);
			gt_bar(GT_ROW_CORE0 + i, stats.cpu[i]);
			gt_text(GT_ROW_CORE0 + i, GT_VAL_X, buf, GT_W - GT_VAL_X - 8);
		}
		else
		{
			/* clear the whole unused row: label, bar and value */
			SetAPen(gt_win->RPort, 0);
			RectFill(gt_win->RPort, gt_xoff + GT_LAB_X,
				gt_yoff + 8 + (GT_ROW_CORE0 + i) * GT_ROW_H + 2,
				gt_xoff + GT_W - 9,
				gt_yoff + 8 + (GT_ROW_CORE0 + i) * GT_ROW_H + 14);
		}
	}

	if (stats.mem_total_kb)
	{
		ULONG used = stats.mem_total_kb - stats.mem_avail_kb;
		ULONG pct = (used * 100 + stats.mem_total_kb / 2) / stats.mem_total_kb;
		gt_bar(GT_ROW_MEM, pct);
		sprintf(buf, "%ld / %ld MB", (long)(used / 1024), (long)(stats.mem_total_kb / 1024));
		gt_text(GT_ROW_MEM, GT_VAL_X, buf, GT_W - GT_VAL_X - 8);
	}

	if (stats.disk_total_mb)
	{
		ULONG used = stats.disk_total_mb - stats.disk_free_mb;
		ULONG pct = (used * 100 + stats.disk_total_mb / 2) / stats.disk_total_mb;
		gt_bar(GT_ROW_DISK, pct);
		sprintf(buf, "%ld / %ld MB", (long)used, (long)stats.disk_total_mb);
		gt_text(GT_ROW_DISK, GT_VAL_X, buf, GT_W - GT_VAL_X - 8);
	}
}

static void gt_paint_all(void)
{
	struct RastPort *rp = gt_win->RPort;
	int i;
	char buf[16];

	SetAPen(rp, 0);
	RectFill(rp, gt_xoff, gt_yoff, gt_xoff + GT_W - 1,
		gt_yoff + 8 + GT_ROW_SLIDER * GT_ROW_H);

	gt_text(GT_ROW_KERNEL, GT_LAB_X, "Kernel:", 0);
	gt_text(GT_ROW_STATUS, GT_LAB_X, "Status:", 0);
	gt_text(GT_ROW_STATUS, GT_BAR_X, gt_status_text, 0);
	gt_text(GT_ROW_HDR, GT_LAB_X, "CPU usage:", 0);

	for (i = 0; i < MAX_CORES; i++)
	{
		if (have_stats && i >= stats.ncores)
			continue;
		sprintf(buf, "Core %d", i);
		gt_text(GT_ROW_CORE0 + i, GT_LAB_X, buf, 0);
	}

	gt_text(GT_ROW_MEM, GT_LAB_X, "Memory:", 0);
	gt_text(GT_ROW_DISK, GT_LAB_X, "Disk (/):", 0);
	gt_text(GT_ROW_SLIDER, GT_LAB_X, "Refresh:", 0);

	if (have_stats)
		gt_stats();
}

static void gt_main(void)
{
	struct NewGadget ng;
	ULONG winsig, a314sig, timersig;
	BOOL quit = FALSE;

	gt_screen = LockPubScreen(NULL);
	if (!gt_screen)
		return;

	gt_vi = GetVisualInfo(gt_screen, TAG_DONE);
	if (!gt_vi)
	{
		UnlockPubScreen(NULL, gt_screen);
		return;
	}

	gt_win = OpenWindowTags(NULL,
		WA_Title, (ULONG)APPNAME,
		WA_Left, 40, WA_Top, 40,
		WA_InnerWidth, GT_W,
		WA_InnerHeight, 8 + (GT_ROW_SLIDER + 2) * GT_ROW_H,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP |
			IDCMP_REFRESHWINDOW,
		WA_CloseGadget, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_Activate, TRUE,
		WA_PubScreen, (ULONG)gt_screen,
		TAG_DONE);

	if (!gt_win)
	{
		FreeVisualInfo(gt_vi);
		UnlockPubScreen(NULL, gt_screen);
		return;
	}

	static struct TextAttr topaz8 = { "topaz.font", 8, 0, 0 };

	/* Drawing and gadget positions are relative to the whole window,
	 * so shift everything into the inner area by the border sizes. */
	gt_xoff = gt_win->BorderLeft;
	gt_yoff = gt_win->BorderTop;

	memset(&ng, 0, sizeof(ng));
	ng.ng_LeftEdge = gt_xoff + GT_BAR_X;
	ng.ng_TopEdge = gt_yoff + 8 + GT_ROW_SLIDER * GT_ROW_H + 3;
	ng.ng_Width = GT_BAR_W;
	ng.ng_Height = 12;
	ng.ng_GadgetText = NULL;
	ng.ng_TextAttr = &topaz8;
	ng.ng_GadgetID = 1;
	ng.ng_Flags = 0;
	ng.ng_VisualInfo = gt_vi;
	ng.ng_UserData = NULL;

	/* CreateGadget() requires a non-NULL "previous" gadget, so a
	 * context gadget is created first to head the list. */
	gt_glist = NULL;
	if (CreateContext(&gt_glist))
	{
		gt_slider = CreateGadget(SLIDER_KIND, gt_glist, &ng,
			GTSL_Min, MIN_REFRESH_MS,
			GTSL_Max, MAX_REFRESH_MS,
			GTSL_Level, refresh_ms,
			GTSL_LevelFormat, (ULONG)"%ld ms",
			GTSL_MaxLevelLen, 8,
			GTSL_LevelPlace, PLACETEXT_RIGHT,
			TAG_DONE);
	}

	if (gt_slider)
	{
		AddGList(gt_win, gt_glist, -1, -1, NULL);
		RefreshGList(gt_glist, gt_win, NULL, -1);
	}

	if (a314_opened)
		a314_start_connect();
	arm_timer();
	gt_paint_all();

	winsig = 1L << gt_win->UserPort->mp_SigBit;
	a314sig = 1L << a314_mp->mp_SigBit;
	timersig = 1L << timer_mp->mp_SigBit;

	while (!quit)
	{
		ULONG s = Wait(winsig | a314sig | timersig | SIGBREAKF_CTRL_C);

		if (s & winsig)
		{
			struct IntuiMessage *imsg;
			while ((imsg = (struct IntuiMessage *)GetMsg(gt_win->UserPort)) != NULL)
			{
				ULONG iclass = imsg->Class;
				struct Gadget *gad = (struct Gadget *)imsg->IAddress;
				ReplyMsg((struct Message *)imsg);

				if (iclass == IDCMP_CLOSEWINDOW)
					quit = TRUE;
				else if (iclass == IDCMP_REFRESHWINDOW)
				{
					BeginRefresh(gt_win);
					gt_paint_all();
					GT_RefreshWindow(gt_win, NULL);
					EndRefresh(gt_win, TRUE);
				}
				else if (iclass == IDCMP_GADGETUP && gad == gt_slider)
				{
					ULONG v = refresh_ms;
					GT_GetGadgetAttrs(gt_slider, gt_win, NULL,
						GTSL_Level, (ULONG)&v, TAG_DONE);
					if (v < MIN_REFRESH_MS)
						v = MIN_REFRESH_MS;
					if (v > MAX_REFRESH_MS)
						v = MAX_REFRESH_MS;
					refresh_ms = v;
				}
			}
		}

		if (s & a314sig)
			handle_a314_port();
		if (s & timersig)
			handle_timer();
		if (s & SIGBREAKF_CTRL_C)
			quit = TRUE;
	}

	if (gt_glist)
	{
		RemoveGList(gt_win, gt_glist, -1);
		FreeGadgets(gt_glist);
		gt_glist = NULL;
		gt_slider = NULL;
	}
	CloseWindow(gt_win);
	gt_win = NULL;
	FreeVisualInfo(gt_vi);
	UnlockPubScreen(NULL, gt_screen);
}

/* Dispatch a stats update to the active UI. */
static void ui_stats(void)
{
	if (m_app)
		mui_stats();
	else
		gt_stats();
}

/* ------------------------------------------------ */
/* Console (TXT) mode                                */
/* ------------------------------------------------ */

static BOOL txt_main(void)
{
	ULONG a314sig = 1L << a314_mp->mp_SigBit;
	ULONG timersig = 1L << timer_mp->mp_SigBit;
	BOOL ever_connected = FALSE;
	int i;

	a314_start_connect();

	/* One timeout covers the connect, request and reply; on expiry a
	 * pending request is retired by a314_disconnect() in cleanup. */
	timer_ior->tr_node.io_Command = TR_ADDREQUEST;
	timer_ior->tr_node.io_Error = 0;
	timer_ior->tr_time.tv_secs = TXT_TIMEOUT_S;
	timer_ior->tr_time.tv_micro = 0;
	SendIO((struct IORequest *)timer_ior);

	while (!have_stats)
	{
		ULONG s;

		if (!req_pending && !read_pending)
			break;	/* connect refused or stream ended */

		s = Wait(a314sig | timersig);

		if (s & a314sig)
		{
			handle_a314_port();
			if (connected)
				ever_connected = TRUE;
		}
		if (s & timersig)
			break;
	}

	if (!have_stats)
	{
		if (ever_connected)
			printf("No statistics received\n");
		else
			printf("Unable to connect to %s service\n", SERVICE_NAME);
		return FALSE;
	}

	printf("Kernel:  %s\n", stats.kernel);
	printf("Cores:   %d\n", (int)stats.ncores);
	for (i = 0; i < stats.ncores; i++)
		printf("Core %d:  %d%% busy\n", i, (int)stats.cpu[i]);

	if (stats.mem_total_kb)
		printf("Memory:  %ld / %ld MB used, %ld MB available\n",
			(long)((stats.mem_total_kb - stats.mem_avail_kb) / 1024),
			(long)(stats.mem_total_kb / 1024),
			(long)(stats.mem_avail_kb / 1024));

	if (stats.disk_total_mb)
		printf("Disk:    %ld / %ld MB used, %ld MB free\n",
			(long)(stats.disk_total_mb - stats.disk_free_mb),
			(long)stats.disk_total_mb,
			(long)stats.disk_free_mb);

	return TRUE;
}

/* ------------------------------------------------ */
/* Startup                                           */
/* ------------------------------------------------ */

static void parse_args(void)
{
	struct Process *proc = (struct Process *)FindTask(NULL);

	if (proc->pr_CLI)
	{
		/* Started from Shell: parse REFRESH/K/N and the UI switches. */
		LONG result[4];
		struct RDArgs *args;

		result[0] = result[1] = result[2] = result[3] = 0;
		args = ReadArgs("REFRESH/K/N,MUI/S,GT/S,TXT/S", result, NULL);
		if (args)
		{
			if (result[0])
				refresh_ms = *(LONG *)result[0];
			if (result[3])
				ui_mode = IF_TXT;
			else if (result[2])
				ui_mode = IF_GT;
			else if (result[1])
				ui_mode = IF_MUI;
			FreeArgs(args);
		}
	}
	else
	{
		/* Started from Workbench: read the tooltypes. */
		IconBase = OpenLibrary("icon.library", 36);
		if (IconBase)
		{
			struct DiskObject *dobj = GetDiskObject("PROGDIR:" APPNAME);
			if (dobj)
			{
				STRPTR v = FindToolType((CONST_STRPTR *)dobj->do_ToolTypes,
					"REFRESH");
				if (v)
					refresh_ms = atoi(v);
				if (FindToolType((CONST_STRPTR *)dobj->do_ToolTypes,
						"TXT"))
					ui_mode = IF_TXT;
				else if (FindToolType((CONST_STRPTR *)dobj->do_ToolTypes,
						"GT"))
					ui_mode = IF_GT;
				else if (FindToolType((CONST_STRPTR *)dobj->do_ToolTypes,
						"MUI"))
					ui_mode = IF_MUI;
				FreeDiskObject(dobj);
			}
			CloseLibrary(IconBase);
		}
	}

	if (refresh_ms < MIN_REFRESH_MS)
		refresh_ms = MIN_REFRESH_MS;
	if (refresh_ms > MAX_REFRESH_MS)
		refresh_ms = MAX_REFRESH_MS;
}

void cleanup(void) {

	if (a314_opened)
		a314_disconnect();

	if (timer_ior && timer_ior->tr_node.io_Device)
	{
		AbortIO((struct IORequest *)timer_ior);
		WaitIO((struct IORequest *)timer_ior);
		CloseDevice((struct IORequest *)timer_ior);
	}

	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);

	if (a314_sync && a314_sync->a314_Request.io_Device)
		CloseDevice((struct IORequest *)a314_sync);

	if (a314_read)
		DeleteExtIO((struct IORequest *)a314_read);
	if (a314_req)
		DeleteExtIO((struct IORequest *)a314_req);
	if (a314_sync)
		DeleteExtIO((struct IORequest *)a314_sync);
	if (timer_ior)
		DeleteExtIO((struct IORequest *)timer_ior);
	if (a314_mp)
		DeletePort(a314_mp);
	if (sync_mp)
		DeletePort(sync_mp);
	if (timer_mp)
		DeletePort(timer_mp);
}

int main(void)
{
	int rc = RETURN_OK;

	a314_mp = CreatePort(NULL, 0);
	sync_mp = CreatePort(NULL, 0);
	a314_sync = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	a314_req = (struct A314_IORequest *)CreateExtIO(a314_mp, sizeof(struct A314_IORequest));
	a314_read = (struct A314_IORequest *)CreateExtIO(a314_mp, sizeof(struct A314_IORequest));

	timer_mp = CreatePort(NULL, 0);
	timer_ior = (struct timerequest *)CreateExtIO(timer_mp, sizeof(struct timerequest));

	if (!a314_mp || !sync_mp || !a314_sync || !a314_req || !a314_read || !timer_mp || !timer_ior)
	{
		printf("Unable to create resources\n");
		cleanup();
		return RETURN_FAIL;
	}

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)a314_sync, 0) != 0)
	{
		printf("Unable to open " A314_NAME "\n");
		cleanup();
		return RETURN_FAIL;
	}
	a314_opened = TRUE;
	memcpy(a314_req, a314_sync, sizeof(struct A314_IORequest));
	a314_req->a314_Request.io_Message.mn_ReplyPort = a314_mp;
	memcpy(a314_read, a314_sync, sizeof(struct A314_IORequest));
	a314_read->a314_Request.io_Message.mn_ReplyPort = a314_mp;

	if (OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)timer_ior, 0) != 0)
	{
		printf("Unable to open timer.device\n");
		cleanup();
		return RETURN_FAIL;
	}

	parse_args();

	a314_socket = (ULONG)time(NULL);

	if (ui_mode == IF_TXT)
	{
		rc = txt_main() ? RETURN_OK : RETURN_WARN;
		cleanup();
		return rc;
	}

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
	if (!IntuitionBase)
	{
		printf("A314Info UI mode requires AmigaOS 2.04 or newer\n");
		cleanup();
		return RETURN_FAIL;
	}

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
	if (!GfxBase)
	{
		cleanup();
		return RETURN_FAIL;
	}

	{
		BOOL mui_ok = FALSE;

		if (ui_mode != IF_GT)
		{
			MUIMasterBase = OpenLibrary("muimaster.library", 19);
			if (MUIMasterBase)
			{
				mui_ok = mui_main();
				CloseLibrary(MUIMasterBase);
				MUIMasterBase = NULL;
				if (!mui_ok && ui_mode == IF_MUI)
				{
					printf("A314Info: unable to open MUI window\n");
					rc = RETURN_FAIL;
				}
			}
			else if (ui_mode == IF_MUI)
			{
				printf("A314Info: muimaster.library V19 not available\n");
				rc = RETURN_FAIL;
			}
		}

		if (!mui_ok && ui_mode != IF_MUI)
		{
			GadToolsBase = OpenLibrary("gadtools.library", 36);
			if (GadToolsBase)
			{
				gt_main();
				CloseLibrary(GadToolsBase);
				GadToolsBase = NULL;
			}
			else
			{
				printf("A314Info requires MUI 3.8 or gadtools.library\n");
				rc = RETURN_FAIL;
			}
		}
	}

	return rc;
}
