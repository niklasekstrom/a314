#ifndef __A314_DEVICE_H
#define __A314_DEVICE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <libraries/dos.h>

struct A314Device
{
    struct Library lib;

    BPTR saved_seg_list;
    BOOL running;

    ULONG bank_address[4];
    UWORD is_a600;

    ULONG fw_flags;

    struct MsgPort task_mp;
    struct Task *task;

    struct ComArea *ca;

    struct Interrupt vertb_interrupt;
    struct Interrupt ports_interrupt;

    struct List active_sockets;

    struct Socket *send_queue_head;
    struct Socket *send_queue_tail;

    UBYTE next_stream_id;
};

extern char device_name[];
extern char id_string[];

#endif /* __A314_DEVICE_H */
