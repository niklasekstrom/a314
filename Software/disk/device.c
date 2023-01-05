/*
 * Copyright (c) 2021 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/ports.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <libraries/filehandler.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <libraries/romboot_base.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

#include "debug.h"

// VBCC unnecessairly warns when a loop has a single iteration.
#pragma dontwarn 208

#define SysBase (*(struct ExecBase **)4)
#define A314Base (dev->a314_base)

#ifndef TD_GETGEOMETRY
// Needed to compile with AmigaOS 1.3 headers.
#define TD_GETGEOMETRY    (CMD_NONSTD+13)

#define DG_DIRECT_ACCESS    0
#define DGF_REMOVABLE       1

struct DriveGeometry
{
    ULONG    dg_SectorSize;
    ULONG    dg_TotalSectors;
    ULONG    dg_Cylinders;
    ULONG    dg_CylSectors;
    ULONG    dg_Heads;
    ULONG    dg_TrackSectors;
    ULONG    dg_BufMemType;
    UBYTE    dg_DeviceType;
    UBYTE    dg_Flags;
    UWORD    dg_Reserved;
};
#endif

// Defines.

#define DEVICE_NAME             "a314disk.device"
#define SERVICE_NAME            "disk"

#define TASK_PRIORITY           10
#define TASK_STACK_SIZE         2048

#define UNIT_COUNT              4
#define AUTOBOOT_UNIT_COUNT     1

#define SIGB_A314               30
#define SIGB_OP_REQ             29

#define SIGF_A314               (1UL << SIGB_A314)
#define SIGF_OP_REQ             (1UL << SIGB_OP_REQ)

#define BOOT_PRIORITY           -5

// TRACK_SIZE = 5632
#define TRACK_SIZE              (NUMSECS * TD_SECTOR)

#define MSG_READ_TRACK_REQ      1
#define MSG_WRITE_TRACK_REQ     2
#define MSG_READ_TRACK_RES      3
#define MSG_WRITE_TRACK_RES     4
#define MSG_INSERT_NOTIFY       5
#define MSG_EJECT_NOTIFY        6
#define MSG_SET_GEOMETRY        7

#define OP_RES_OK               0
#define OP_RES_NOT_PRESENT      1
#define OP_RES_WRITE_PROTECTED  2

// Structs.

struct DriveState
{
    struct IOStdReq *change_int;
    struct Interrupt *remove_int;
    ULONG *env_vec;
    volatile ULONG change_num;
    volatile ULONG opencnt;
    volatile BOOL present;
    volatile BOOL write_protected;
    UWORD unit;
    UWORD heads;
    UWORD sectors_per_track;
    ULONG cylinders;
};

#pragma pack(push, 1)
struct RequestMsg
{
    UBYTE kind;
    UBYTE unit;
    UWORD length;
    ULONG offset;
    ULONG address;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ResponseMsg
{
    UBYTE kind;
    UBYTE unit;
    union
    {
        UBYTE error; // READ_/WRITE_TRACK_RES
        UBYTE writable; // INSERT_NOTIFY
        struct // SET_GEOMETRY
        {
            UBYTE heads;
            UBYTE sectors_per_track;
            ULONG cylinders;
        } geometry;
    } u;
};
#pragma pack(pop)

struct DiskDevice
{
    struct Library lib;

    BPTR saved_seg_list;

    struct MsgPort op_req_mp;
    struct MsgPort a314_mp;

    struct A314_IORequest read_ior;
    struct A314_IORequest write_ior;
    struct A314_IORequest reset_ior;

    struct Library *a314_base;

    ULONG track_buffer_address;

    struct DriveState drive_states[UNIT_COUNT];

    void *task_stack;
    struct Task task;

    struct List request_queue;
    volatile struct IOStdReq *pending_operation;

    LONG a314_socket;

    BOOL pending_connect;
    BOOL pending_read;
    BOOL pending_write;
    BOOL pending_reset;
    BOOL connection_resetted;

    struct RequestMsg req_msg;
    struct ResponseMsg res_msg;
};

// Constants.

const char device_name[] = DEVICE_NAME;
const char id_string[] = DEVICE_NAME " 1.0 (29 April 2021)";

static const char service_name[] = SERVICE_NAME;
static const char a314_device_name[] = A314_NAME;

// Procedures.

void NewList(struct List *l)
{
    l->lh_Head = (struct Node *)&(l->lh_Tail);
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

static void send_a314_cmd(struct DiskDevice *dev, struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
    ior->a314_Request.io_Command = cmd;
    ior->a314_Request.io_Error = 0;
    ior->a314_Socket = dev->a314_socket;
    ior->a314_Buffer = buffer;
    ior->a314_Length = length;
    SendIO((struct IORequest *)ior);
}

static void send_request_if_possible(struct DiskDevice *dev)
{
    if (dev->connection_resetted || dev->pending_operation)
        return;

    struct IOStdReq *ior = NULL;

    while (TRUE)
    {
        ior = (struct IOStdReq *)RemHead(&dev->request_queue);
        if (!ior)
            return;

        struct DriveState *ds = (struct DriveState *)ior->io_Unit;
        if (ds->present)
        {
            dev->req_msg.unit = ds->unit;
            break;
        }
        else
        {
            ior->io_Error = TDERR_DiskChanged;
            ReplyMsg(&ior->io_Message);
        }
    }

    dev->pending_operation = ior;

    // TODO: Should handle that a read/write request can be larger
    // than the track buffer, by splitting up transfer.

    switch (ior->io_Command)
    {
    case CMD_READ:
        dev->req_msg.kind = MSG_READ_TRACK_REQ;
        break;

    case TD_FORMAT:
    case CMD_WRITE:
        WriteMemA314(dev->track_buffer_address, ior->io_Data, ior->io_Length);
        dev->req_msg.kind = MSG_WRITE_TRACK_REQ;
        break;
    }

    dev->req_msg.length = ior->io_Length;
    dev->req_msg.offset = ior->io_Offset;
    dev->req_msg.address = dev->track_buffer_address;

    send_a314_cmd(dev, &dev->write_ior, A314_WRITE, (char *)&dev->req_msg, sizeof(struct RequestMsg));
    dev->pending_write = TRUE;
}

static void handle_eject_msg(struct DriveState *ds)
{
    ds->change_num++;
    ds->present = FALSE;

    struct Interrupt *remove_int = ds->remove_int;
    if (remove_int)
        Cause(remove_int);

    struct IOStdReq *change_int = ds->change_int;
    if (change_int)
        Cause((struct Interrupt *)change_int->io_Data);
}

static void handle_insert_msg(struct DriveState *ds, struct ResponseMsg *msg)
{
    ds->write_protected = msg->u.writable == 0;
    ds->change_num++;
    ds->present = TRUE;

    struct Interrupt *remove_int = ds->remove_int;
    if (remove_int)
        Cause(remove_int);

    struct IOStdReq *change_int = ds->change_int;
    if (change_int)
        Cause((struct Interrupt *)change_int->io_Data);
}

static void handle_set_geometry_msg(struct DriveState *ds, struct ResponseMsg *msg)
{
    ds->heads = msg->u.geometry.heads;
    ds->sectors_per_track = msg->u.geometry.sectors_per_track;
    ds->cylinders = msg->u.geometry.cylinders;

    ULONG *ev = ds->env_vec;
    if (ev)
    {
        ev[DE_NUMHEADS] = msg->u.geometry.heads;
        ev[DE_BLKSPERTRACK] = msg->u.geometry.sectors_per_track;
        ev[DE_UPPERCYL] = msg->u.geometry.cylinders - 1;
    }
}

static void handle_response_msg(struct DiskDevice *dev)
{
    struct DriveState *ds = &dev->drive_states[dev->res_msg.unit];

    switch (dev->res_msg.kind)
    {
    case MSG_READ_TRACK_RES:
    case MSG_WRITE_TRACK_RES:
        switch (dev->res_msg.u.error)
        {
        case OP_RES_OK:
            if (dev->res_msg.kind == MSG_READ_TRACK_RES)
                ReadMemA314(dev->pending_operation->io_Data, dev->track_buffer_address, dev->pending_operation->io_Length);
            dev->pending_operation->io_Actual = dev->pending_operation->io_Length;
            break;

        case OP_RES_NOT_PRESENT:
            dev->pending_operation->io_Error = TDERR_DiskChanged;
            break;

        case OP_RES_WRITE_PROTECTED:
            dev->pending_operation->io_Error = TDERR_WriteProt;
            break;
        }

        ReplyMsg(&dev->pending_operation->io_Message);
        dev->pending_operation = NULL;
        break;

    case MSG_EJECT_NOTIFY:
        handle_eject_msg(ds);
        break;

    case MSG_INSERT_NOTIFY:
        handle_insert_msg(ds, &dev->res_msg);
        break;

    case MSG_SET_GEOMETRY:
        handle_set_geometry_msg(ds, &dev->res_msg);
        break;
    }
}

static void handle_a314_reply(struct DiskDevice *dev, struct A314_IORequest *ior)
{
    switch (ior->a314_Request.io_Command)
    {
    case A314_CONNECT:
        dev->pending_connect = FALSE;
        if (ior->a314_Request.io_Error != A314_CONNECT_OK)
            dev->connection_resetted = TRUE;
        break;

    case A314_READ:
        dev->pending_read = FALSE;
        if (dev->read_ior.a314_Request.io_Error == A314_READ_OK)
            handle_response_msg(dev);
        else if (dev->read_ior.a314_Request.io_Error == A314_READ_RESET)
            dev->connection_resetted = TRUE;
        break;

    case A314_WRITE:
        dev->pending_write = FALSE;
        if (dev->write_ior.a314_Request.io_Error != A314_WRITE_OK)
            dev->connection_resetted = TRUE;
        break;

    case A314_RESET:
        dev->pending_reset = FALSE;
        break;
    }

    // TODO: If connection resets (Pi shuts down or crashes)
    // then should eject disks from all drives automatically,
    // and reply to any pending requests.

    if (!dev->connection_resetted && !dev->pending_connect && !dev->pending_read)
    {
        send_a314_cmd(dev, &dev->read_ior, A314_READ, (char *)&dev->res_msg, sizeof(struct ResponseMsg));
        dev->pending_read = TRUE;
    }
}

static void task_main()
{
    struct DiskDevice *dev = (struct DiskDevice *)FindTask(NULL)->tc_UserData;

    send_a314_cmd(dev, &dev->read_ior, A314_CONNECT, (char *)service_name, strlen(service_name));
    dev->pending_connect = TRUE;

    while (TRUE)
    {
        ULONG sigs = Wait(SIGF_A314 | SIGF_OP_REQ);

        if (sigs & SIGF_A314)
        {
            struct A314_IORequest *ior;
            while ((ior = (struct A314_IORequest *)GetMsg(&dev->a314_mp)))
                handle_a314_reply(dev, ior);
        }

        if (sigs & SIGF_OP_REQ)
        {
            struct IOStdReq *ior;
            while ((ior = (struct IOStdReq *)GetMsg(&dev->op_req_mp)))
                AddTail(&dev->request_queue, &ior->io_Message.mn_Node);
        }

        send_request_if_possible(dev);
    }
}

static void init_task(struct DiskDevice *dev)
{
    char *stack = dev->task_stack;
    struct Task *task = &dev->task;
    task->tc_Node.ln_Type = NT_TASK;
    task->tc_Node.ln_Pri = TASK_PRIORITY;
    task->tc_Node.ln_Name = (char *)device_name;
    task->tc_SPLower = (APTR)stack;
    task->tc_SPUpper = (APTR)(stack + TASK_STACK_SIZE);
    task->tc_SPReg = (APTR)(stack + TASK_STACK_SIZE);
    NewList(&task->tc_MemEntry);
    task->tc_UserData = (void *)dev;

    AddTask(task, (void *)task_main, 0);
}

static void init_message_port(struct MsgPort *mp, UBYTE sig_bit, struct Task *sig_task)
{
    mp->mp_Node.ln_Type = NT_MSGPORT;
    mp->mp_Flags = PA_SIGNAL;
    mp->mp_SigBit = sig_bit;
    mp->mp_SigTask = sig_task;
    NewList(&mp->mp_MsgList);
}

static void init_a314_ioreq(struct A314_IORequest *ior, struct MsgPort *mp)
{
    ior->a314_Request.io_Message.mn_ReplyPort = mp;
    ior->a314_Request.io_Message.mn_Length = sizeof(struct A314_IORequest);
    ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
}

static void get_geometry(struct DriveState *ds, struct DriveGeometry *geom)
{
    ULONG numtracks = ds->cylinders * ds->heads;
    geom->dg_SectorSize = TD_SECTOR;
    geom->dg_TotalSectors = numtracks * ds->sectors_per_track;
    geom->dg_Cylinders = ds->cylinders;
    geom->dg_CylSectors = ds->sectors_per_track * ds->heads;
    geom->dg_Heads = ds->heads;
    geom->dg_TrackSectors = ds->sectors_per_track;
    geom->dg_BufMemType = 0;
    geom->dg_DeviceType = DG_DIRECT_ACCESS;
    geom->dg_Flags = DGF_REMOVABLE;
}

static void begin_io(__reg("a6") struct DiskDevice *dev, __reg("a1") struct IOStdReq *ior)
{
    if (!ior || !ior->io_Unit)
        return;

    struct DriveState *ds = (void *)ior->io_Unit;

    dbg("begin_io, unit=$b, command=$w, flags=$w", ds->unit, ior->io_Command, ior->io_Flags);

    ior->io_Error = 0;
    ior->io_Actual = 0;

    switch (ior->io_Command)
    {
    case CMD_RESET:
    case CMD_CLEAR:
    case CMD_UPDATE:
    case TD_MOTOR:
    case TD_SEEK:
        break;

    case TD_REMOVE:
        ds->remove_int = (struct Interrupt *)ior->io_Data;
        break;

    case TD_PROTSTATUS:
        ior->io_Actual = ds->write_protected;
        break;

    case TD_CHANGESTATE:
        ior->io_Actual = ds->present ? 0 : 1;
        break;

    case TD_CHANGENUM:
        ior->io_Actual = ds->change_num;
        break;

    case TD_GETDRIVETYPE:
        ior->io_Actual = DRIVE3_5;
        break;

    case TD_GETNUMTRACKS:
        ior->io_Actual = ds->cylinders * ds->heads;
        break;

    case TD_GETGEOMETRY:
        get_geometry(ds, (struct DriveGeometry *)ior->io_Data);
        break;

    case TD_ADDCHANGEINT:
        if (ds->change_int)
            ior->io_Error = IOERR_ABORTED;
        else
        {
            ds->change_int = ior;
            ior->io_Flags &= ~IOF_QUICK;
            ior = NULL;
        }
        break;

    case TD_REMCHANGEINT:
        if (ds->change_int == ior)
            ds->change_int = NULL;
        break;

    case CMD_READ:
        dbg("CMD_READ, unit=$b, offset=$l, length=$l, data=$l", ds->unit, ior->io_Offset, ior->io_Length, ior->io_Data);
        if (!ds->present)
            ior->io_Error = TDERR_DiskChanged;
        else
        {
            PutMsg(&dev->op_req_mp, &ior->io_Message);
            ior->io_Flags &= ~IOF_QUICK;
            ior = NULL;
        }
        break;

    case TD_FORMAT:
    case CMD_WRITE:
        dbg("CMD_WRITE, unit=$b, offset=$l, length=$l, data=$l", ds->unit, ior->io_Offset, ior->io_Length, ior->io_Data);
        if (!ds->present)
            ior->io_Error = TDERR_DiskChanged;
        else if (ds->write_protected)
            ior->io_Error = TDERR_WriteProt;
        else
        {
            PutMsg(&dev->op_req_mp, &ior->io_Message);
            ior->io_Flags &= ~IOF_QUICK;
            ior = NULL;
        }
        break;

    default:
        ior->io_Error = IOERR_NOCMD;
        break;
    }

    if (ior && !(ior->io_Flags & IOF_QUICK))
        ReplyMsg(&ior->io_Message);
}

static ULONG abort_io(__reg("a6") struct DiskDevice *dev, __reg("a1") struct IOStdReq *ior)
{
    return IOERR_NOCMD;
}

static ULONG get_socket_id()
{
    struct timerequest tr;
    memset(&tr, 0, sizeof(tr));
    tr.tr_node.io_Message.mn_Length = sizeof(tr);
    tr.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    OpenDevice(TIMERNAME, UNIT_VBLANK, &tr.tr_node, 0);
    tr.tr_node.io_Command = TR_GETSYSTIME;
    DoIO(&tr.tr_node);
    CloseDevice(&tr.tr_node);

    return tr.tr_time.tv_secs * TICKS_PER_SECOND;
}

struct MountListEntry
{
    struct DeviceNode dev_node;
    struct FileSysStartupMsg fssm;
    ULONG env_vec[20];
    char drive_name[8];
    char device_name[20];
    struct BootNode boot_node;
};

static ULONG env_vec_template[20] = {
    19,             // DE_TABLESIZE
    128,            // DE_SIZEBLOCK
    0,              // DE_SECORG
    2,              // DE_NUMHEADS
    1,              // DE_SECSPERBLK
    11,             // DE_BLKSPERTRACK
    2,              // DE_RESERVEDBLKS
    0,              // DE_PREFAC
    0,              // DE_INTERLEAVE
    0,              // DE_LOWCYL
    79,             // DE_UPPERCYL
    2,              // DE_NUMBUFFERS
    0,              // DE_BUFMEMTYPE
    TRACK_SIZE,     // DE_MAXTRANSFER
    0x7fffffff,     // DE_MASK
    BOOT_PRIORITY,  // DE_BOOTPRI
    ID_DOS_DISK,    // DE_DOSTYPE
    0,              // DE_BAUD
    0,              // DE_CONTROL
    2               // DE_BOOTBLOCKS
};

static void add_boot_node(struct ExpansionBase *expansion_base, struct DriveState *ds)
{
    struct MountListEntry *mle = AllocMem(sizeof(struct MountListEntry), MEMF_PUBLIC | MEMF_CLEAR);

    //mle->dev_node.dn_Type = DLT_DEVICE;
    mle->dev_node.dn_StackSize = 600;
    mle->dev_node.dn_Priority = 10;
    mle->dev_node.dn_Startup = ((ULONG)(&mle->fssm)) >> 2;
    mle->dev_node.dn_Name = ((ULONG)(&mle->drive_name[0])) >> 2;

    mle->fssm.fssm_Unit = ds->unit;
    mle->fssm.fssm_Device = ((ULONG)(&mle->device_name[0])) >> 2;
    mle->fssm.fssm_Environ = ((ULONG)(&mle->env_vec[0])) >> 2;

    memcpy(mle->env_vec, env_vec_template, sizeof(env_vec_template));
    ds->env_vec = mle->env_vec;

    *(ULONG *)(&mle->drive_name[0]) = (3 << 24) | ('P' << 16) | ('D' << 8) | ('0' + ds->unit);

    mle->device_name[0] = sizeof(device_name);
    memcpy(&mle->device_name[1], device_name, sizeof(device_name));

    mle->boot_node.bn_Node.ln_Type = NT_BOOTNODE;
    mle->boot_node.bn_Node.ln_Pri = BOOT_PRIORITY;
    mle->boot_node.bn_DeviceNode = (ULONG)&mle->dev_node;

    Forbid();
    Enqueue(&expansion_base->MountList, &mle->boot_node.bn_Node);
    Permit();
}

static struct Library *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct DiskDevice *dev)
{
    dev->saved_seg_list = seg_list;

    dev->lib.lib_Node.ln_Type = NT_DEVICE;
    dev->lib.lib_Node.ln_Name = (char *)device_name;
    dev->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    dev->lib.lib_Version = 1;
    dev->lib.lib_Revision = 0;
    dev->lib.lib_IdString = (APTR)id_string;

    init_message_port(&dev->a314_mp, SIGB_A314, &dev->task);
    init_message_port(&dev->op_req_mp, SIGB_OP_REQ, &dev->task);

    init_a314_ioreq(&dev->read_ior, &dev->a314_mp);

    if (OpenDevice((char *)a314_device_name, 0, &dev->read_ior.a314_Request, 0))
        goto fail1;

    dev->a314_base = &(dev->read_ior.a314_Request.io_Device->dd_Library);

    memcpy(&dev->write_ior, &dev->read_ior, sizeof(struct A314_IORequest));
    memcpy(&dev->reset_ior, &dev->read_ior, sizeof(struct A314_IORequest));

    dev->track_buffer_address = AllocMemA314(TRACK_SIZE);
    if (dev->track_buffer_address == INVALID_A314_ADDRESS)
        goto fail2;

    for (int i = 0; i < UNIT_COUNT; i++)
    {
        struct DriveState *ds = &dev->drive_states[i];
        ds->unit = i;
        ds->heads = 2; // Default to floppy disk geometry.
        ds->sectors_per_track = 11;
        ds->cylinders = 80;
    }

    NewList(&dev->request_queue);

    dev->a314_socket = get_socket_id();

    dev->task_stack = AllocMem(TASK_STACK_SIZE, MEMF_CLEAR);
    if (!dev->task_stack)
        goto fail3;

    if (!seg_list)
    {
        struct ExpansionBase *expansion_base = (struct ExpansionBase *)OpenLibrary(EXPANSIONNAME, 0);

        if (expansion_base)
        {
            for (int i = 0; i < AUTOBOOT_UNIT_COUNT; i++)
                add_boot_node(expansion_base, &dev->drive_states[i]);

            CloseLibrary(&expansion_base->LibNode);
        }
    }

    init_task(dev);

    dbg_init();
    dbg("Started");

    return &dev->lib;

fail3:
    FreeMemA314(dev->track_buffer_address, TRACK_SIZE);

fail2:
    CloseDevice(&dev->read_ior.a314_Request);

fail1:
    FreeMem((char *)dev - dev->lib.lib_NegSize, dev->lib.lib_NegSize + dev->lib.lib_PosSize);
    return NULL;
}

static BPTR expunge(__reg("a6") struct DiskDevice *dev)
{
    // Currently no support for unloading device driver.
    if (dev->lib.lib_OpenCnt != 0)
    {
        dev->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    return 0;
}

static void open(__reg("a6") struct DiskDevice *dev, __reg("a1") struct IORequest *ior, __reg("d0") ULONG unitnum, __reg("d1") ULONG flags)
{
    dbg("open device, unit=$l, ior=$l, flags=$l", (ULONG)unitnum, (ULONG)ior, (ULONG)flags);

    ior->io_Error = IOERR_OPENFAIL;
    ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    if (unitnum >= UNIT_COUNT)
        return;

    struct DriveState *ds = &dev->drive_states[unitnum];
    ds->opencnt++;
    dev->lib.lib_OpenCnt++;

    ior->io_Unit = (void *)ds;
    ior->io_Error = 0;
}

static BPTR close(__reg("a6") struct DiskDevice *dev, __reg("a1") struct IORequest *ior)
{
    dbg("close device, ior=$l", ior);

    if (!ior || !ior->io_Unit)
        return 0;

    struct DriveState *ds = (void *)ior->io_Unit;
    ds->opencnt--;

    ior->io_Device = NULL;
    ior->io_Unit = NULL;

    dev->lib.lib_OpenCnt--;

    if (dev->lib.lib_OpenCnt == 0 && (dev->lib.lib_Flags & LIBF_DELEXP))
        return expunge(dev);

    return 0;
}

static ULONG device_vectors[] =
{
    (ULONG)open,
    (ULONG)close,
    (ULONG)expunge,
    0,
    (ULONG)begin_io,
    (ULONG)abort_io,
    -1,
};

ULONG auto_init_tables[] =
{
    sizeof(struct DiskDevice),
    (ULONG)device_vectors,
    0,
    (ULONG)init_device,
};
