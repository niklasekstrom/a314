#ifndef __A314_DEVICE_H
#define __A314_DEVICE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <libraries/dos.h>

#include "protocol.h"

struct A314Device
{
    struct Library lib;

    BPTR saved_seg_list;
    BOOL running;

    void *first_chunk;

    struct Task task;
    struct MsgPort task_mp;

    struct Interrupt exter_interrupt;

    struct List active_sockets;

    struct Socket *send_queue_head;
    struct Socket *send_queue_tail;

    struct ComAreaPtrs cap;

    UBYTE next_stream_id;
};

extern char device_name[];
extern char id_string[];

#endif /* __A314_DEVICE_H */
