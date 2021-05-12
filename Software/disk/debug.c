#include <exec/types.h>
#include <libraries/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "debug.h"

#if !DEBUG
void dbg_init()
{
}

void debug_process_run()
{
}

void dbg(const char* fmt, ...)
{
}
#else

#define LOG_SIZE 16

extern void debug_process_seglist();
static const char debug_process_name[] = "logger-process";
static struct Process *debug_process = NULL;

static char msg_bufs[LOG_SIZE][128];
static volatile msg_lengths[LOG_SIZE];
static volatile UWORD dbg_head = 0;
static volatile UWORD dbg_tail = 0;
static volatile UWORD dbg_reserved_tail = 0;
static volatile BOOL dropped_message = FALSE;

static const char print_dropped[] = "A message was dropped\n";

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

void dbg_init()
{
    SysBase = *(struct ExecBase **)4;
    DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 0);

    struct MsgPort *process_mp = CreateProc(
        (char *)debug_process_name,
        20,
        ((ULONG)debug_process_seglist) >> 2,
        2048);

    if (process_mp)
        debug_process = (struct Process *)((char *)process_mp - sizeof(struct Task));
}

void debug_process_run()
{
    BPTR dbg_con = Open("CON:200/0/440/256/a314disk", MODE_NEWFILE);

    while (TRUE)
    {
        Wait(SIGBREAKF_CTRL_D);

        while (dbg_head != dbg_tail)
        {
            Write(dbg_con, msg_bufs[dbg_head], msg_lengths[dbg_head]);
            msg_lengths[dbg_head] = 0;
            dbg_head = (dbg_head + 1) & (LOG_SIZE - 1);
        }

        if (dropped_message)
        {
            Write(dbg_con, (char *)print_dropped, sizeof(print_dropped) - 1);
            dropped_message = FALSE;
        }
    }
}

void dbg(const char* fmt, ...)
{
    Disable();
    int my_slot = dbg_reserved_tail;
    int next_slot = (my_slot + 1) & (LOG_SIZE - 1);

    if (next_slot == dbg_head)
    {
        dropped_message = TRUE;
        Enable();
        return;
    }

    dbg_reserved_tail = next_slot;
    Enable();

    char numbuf[16];

    const char *p = fmt;
    char *q = msg_bufs[my_slot];

    ULONG *args = (ULONG *)&fmt;
    args++;

    while (*p != 0)
    {
        char c = *p++;
        if (c == '$')
        {
            c = *p++;
            if (c == 'b')
            {
                ULONG x = *args++;
                *q++ = '$';
                for (int i = 0; i < 2; i++)
                {
                    int ni = (x >> ((1 - i) * 4)) & 0xf;
                    *q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
                }
            }
            else if (c == 'w')
            {
                ULONG x = *args++;
                *q++ = '$';
                for (int i = 0; i < 4; i++)
                {
                    int ni = (x >> ((3 - i) * 4)) & 0xf;
                    *q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
                }
            }
            else if (c == 'l')
            {
                ULONG x = *args++;
                *q++ = '$';
                for (int i = 0; i < 8; i++)
                {
                    int ni = (x >> ((7 - i) * 4)) & 0xf;
                    *q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
                }
            }
            else if (c == 'S')
            {
                unsigned char *s = (unsigned char *)*args++;
                int l = *s++;
                for (int i = 0; i < l; i++)
                    *q++ = *s++;
            }
            else if (c == 's')
            {
                unsigned char *s = (unsigned char *)*args++;
                while (*s)
                    *q++ = *s++;
            }
            else if (c == '$')
            {
                *q++ = '$';
            }
            else
            {
                *q++ = '$';
                *q++ = c;
            }
        }
        else
        {
            *q++ = c;
        }
    }
    *q++ = '\n';

    Disable();
    msg_lengths[my_slot] = q - msg_bufs[my_slot];

    while (dbg_tail != dbg_reserved_tail && msg_lengths[dbg_tail] != 0)
        dbg_tail = (dbg_tail + 1) & (LOG_SIZE - 1);
    Enable();

    if (debug_process)
        Signal(&debug_process->pr_Task, SIGBREAKF_CTRL_D);
}
#endif
