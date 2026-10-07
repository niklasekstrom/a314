#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <exec/io.h>

#include <devices/trackdisk.h>
#include <devices/newstyle.h>
#include <devices/scsidisk.h>

#include <dos/dos.h>

#include <proto/exec.h>

#include <string.h>

#include "a314scsi.h"
#include "scsi.h"
#include "alib.h"
#include "kprintf.h"

#define SysBase (*(struct ExecBase**)4)

extern void task_body(void);

const char device_name[] = DEVICE_NAME;
const char id_string[] = DEVICE_NAME " 1.0 (4.7.2026)";

// ---------------------------------------------------------------------------
// Drive lookup / discovery
// ---------------------------------------------------------------------------

struct Drive* find_drive(struct A314ScsiBase* base, UWORD number)
{
    if (number >= A314SCSI_MAX_DRIVES)
    {
        return NULL;
    }
    return base->drives[number];
}

static BOOL discover_drive(struct A314ScsiBase* base, struct Drive* d)
{
    kprintf("a314scsi: discover unit %ld\n", (ULONG)d->number);

    if (scsi_inquiry(base, d) != 0)
    {
        kprintf("a314scsi: INQUIRY failed\n");
        return FALSE;
    }

    kprintf("a314scsi: INQUIRY type=%ld\n", (ULONG)d->devtype);

    // A few TEST UNIT READY tries in case the medium needs a moment;
    // the a314 round-trip itself paces the retries.
    for (int i = 0; i < 5; i++)
    {
        if (scsi_test_ready(base, d) == 0)
        {
            break;
        }
    }

    if (scsi_read_capacity(base, d) != 0)
    {
        kprintf("a314scsi: READ CAPACITY failed\n");
        return FALSE;
    }

    scsi_mode_sense_wp(base, d);

    d->flags |= DRVF_PRESENT;

    kprintf("a314scsi: unit %ld blocks=%ld bs=%ld\n", (ULONG)d->number, d->blocks, d->blocksize);

    return TRUE;
}

struct Drive* make_drive(struct A314ScsiBase* base, UWORD number)
{
    if (number >= A314SCSI_MAX_DRIVES)
        return NULL;

    if (base->drives[number])
        return base->drives[number];

    struct Drive* d = (struct Drive*)AllocMem(sizeof(struct Drive), MEMF_PUBLIC | MEMF_CLEAR);
    if (!d)
        return NULL;

    d->number = number;
    d->blocksize = 512;
    d->blockshift = 9;

    base->drives[number] = d;

    if (!discover_drive(base, d))
    {
        base->drives[number] = NULL;
        FreeMem(d, sizeof(struct Drive));
        return NULL;
    }

    return d;
}

// ---------------------------------------------------------------------------
// Command handling (io task context)
// ---------------------------------------------------------------------------

static LONG map_error(struct Drive* d, LONG err)
{
    if (err == 0)
        return 0;

    // Transport failures carry no sense data; pass them through unchanged.
    if (err != HFERR_BadStatus)
        return err;

    // Refine a CHECK CONDITION into a specific reason via the sense key that
    // a314_scsi captured in d->sense - so filesystems see write-protect,
    // not-ready, etc. instead of a generic error.
    switch (d->sense[2] & SCSI_SENSE_KEY_MASK)
    {
        case SCSI_SK_DATA_PROTECT:   return TDERR_WriteProt;
        case SCSI_SK_NOT_READY:      return TDERR_DiskChanged;
        case SCSI_SK_UNIT_ATTENTION: return TDERR_DiskChanged;
        case SCSI_SK_HARDWARE_ERROR: return HFERR_BadStatus;
        default:                     return TDERR_NotSpecified;
    }
}

static void do_read_write(struct A314ScsiBase* base, struct Drive* d, struct IOStdReq* ior, BOOL write)
{
    UWORD shift = d->blockshift;
    ULONG cmd = ior->io_Command;

    ULONG off_lo = ior->io_Offset;
    ULONG off_hi = 0;
    if (cmd == TD_READ64        || 
        cmd == TD_WRITE64       || 
        cmd == NSCMD_TD_READ64  || 
        cmd == NSCMD_TD_WRITE64 )
    {
        off_hi = ior->io_Actual; // TD64: high 32 bits of the byte offset
    }

    // Byte offset (off_hi:off_lo) -> 32-bit LBA in block units (<= 2 TB).
    ULONG lba = (off_lo >> shift) | (shift ? (off_hi << (32 - shift)) : 0);
    ULONG blocks = ior->io_Length >> shift;

    UBYTE* buf = (UBYTE*)ior->io_Data;
    ULONG max_blocks = A314SCSI_MAX_XFER >> shift;
    LONG err = 0;

    while (blocks > 0 && err == 0)
    {
        ULONG n = (blocks > max_blocks) ? max_blocks : blocks;

        err = scsi_rw(base, d, lba, n, buf, write);

        buf += (n << shift);
        blocks -= n;
        lba += n;
    }

    ior->io_Actual = err ? 0 : ior->io_Length;
    ior->io_Error = map_error(d, err);
}

static void get_geometry(struct Drive* d, struct IOStdReq* ior)
{
    struct DriveGeometry* g = (struct DriveGeometry*)ior->io_Data;

    // Derive a plausible CHS from the block count (SCSI reports no CHS):
    // 1 head, 32 sectors/track.
    ULONG spt = 32;
    ULONG heads = 1;

    memset(g, 0, sizeof(struct DriveGeometry));
    g->dg_SectorSize = d->blocksize;
    g->dg_TotalSectors = d->blocks;
    g->dg_Cylinders = d->blocks / (spt * heads);
    g->dg_CylSectors = spt * heads;
    g->dg_Heads = heads;
    g->dg_TrackSectors = spt;
    g->dg_BufMemType = MEMF_PUBLIC;
    g->dg_DeviceType = d->devtype;
    g->dg_Flags = (d->flags & DRVF_REMOVABLE) ? DGF_REMOVABLE : 0;
}

static const UWORD nsd_supported[] =
{
    CMD_READ,
    CMD_WRITE,
    CMD_UPDATE,
    CMD_CLEAR,
    TD_MOTOR,
    TD_SEEK,
    TD_FORMAT,
    TD_REMOVE,
    TD_CHANGENUM,
    TD_CHANGESTATE,
    TD_PROTSTATUS,
    TD_GETGEOMETRY,
    TD_READ64,
    TD_WRITE64,
    TD_SEEK64,
    NSCMD_TD_READ64,
    NSCMD_TD_WRITE64,
    NSCMD_TD_SEEK64,
    HD_SCSICMD,
    0,
};

static BOOL device_query(struct IOStdReq* ior)
{
    struct NSDeviceQueryResult* q = (struct NSDeviceQueryResult*)ior->io_Data;

    if (!q)
        return FALSE;

    q->nsdqr_DevQueryFormat = 0;
    q->nsdqr_SizeAvailable = sizeof(struct NSDeviceQueryResult);
    q->nsdqr_DeviceType = NSDEVTYPE_TRACKDISK;
    q->nsdqr_DeviceSubType = 0;
    q->nsdqr_SupportedCommands = (APTR)nsd_supported;
    ior->io_Actual = sizeof(struct NSDeviceQueryResult);
    return TRUE;
}

static void process_request(struct A314ScsiBase* base, struct IOStdReq* ior)
{
    struct Drive* d = (struct Drive*)ior->io_Unit;

    ior->io_Error = 0;

    switch (ior->io_Command)
    {
        case CMD_READ:
        case TD_READ64:
        case NSCMD_TD_READ64:
            if (!d || !(d->flags & DRVF_PRESENT))
            {
                ior->io_Error = TDERR_DiskChanged;
            }
            else
            {
                do_read_write(base, d, ior, FALSE);
            }
            break;

        case CMD_WRITE:
        case TD_FORMAT:
        case TD_WRITE64:
        case NSCMD_TD_WRITE64:
            if (!d || !(d->flags & DRVF_PRESENT))
            {
                ior->io_Error = TDERR_DiskChanged;
            }
            else if (d->flags & DRVF_WRITEPROT)
            {
                ior->io_Error = TDERR_WriteProt;
            }
            else
            {
                do_read_write(base, d, ior, TRUE);
            }
            break;

        case HD_SCSICMD:
            ior->io_Error = a314_scsi(base, d, (struct SCSICmd*)ior->io_Data);
            break;

        case TD_GETGEOMETRY:
            if (d && (d->flags & DRVF_PRESENT))
            {
                get_geometry(d, ior);
            }
            else
            {
                ior->io_Error = TDERR_DiskChanged;
            }
            break;

        default:
            ior->io_Error = IOERR_NOCMD;
            break;
    }
}

// ---------------------------------------------------------------------------
// Processing task
// ---------------------------------------------------------------------------

static void startup(struct A314ScsiBase* base)
{
    kprintf("a314scsi: task startup\n");

    base->reqport = CreatePort(NULL, 0);
    if (!base->reqport)
    {
        kprintf("a314scsi: CreatePort FAILED\n");
        return;
    }

    // a314.device was opened in init_device's caller context (see init_device).
    // Here in the io task we only connect - the reply port must be owned by this
    // task, which does every subsequent A314 transaction.
    if (!base->a314base || !a314_connect(base))
    {
        kprintf("a314scsi: connect FAILED\n");
        return;
    }

    // Probe every unit the host might serve; absent units fail INQUIRY in
    // make_drive() and are simply skipped.
    for (UWORD unit = 0; unit < A314SCSI_MAX_DRIVES; unit++)
    {
        struct Drive* d = make_drive(base, unit);
        if (d)
            mount_drive(base, d);
    }
}

void task_body(void)
{
    struct A314ScsiBase* base = (struct A314ScsiBase*)FindTask(NULL)->tc_UserData;

    startup(base);

    // Tell init_device we finished starting up (success or not).
    base->started = TRUE;
    if (base->inittask)
        Signal(base->inittask, base->initsig);

    if (!base->reqport)
        return; // startup failed; the device is unusable

    for (;;)
    {
        WaitPort(base->reqport);

        struct IOStdReq* ior;
        while ((ior = (struct IOStdReq*)GetMsg(base->reqport)))
        {
            process_request(base, ior);
            ReplyMsg(&ior->io_Message);
        }
    }
}

// ---------------------------------------------------------------------------
// Device vectors
// ---------------------------------------------------------------------------

static void begin_io(__reg("a6") struct A314ScsiBase* base,
                     __reg("a1") struct IOStdReq* ior)
{
    ior->io_Error = 0;

    switch (ior->io_Command)
    {
        case NSCMD_DEVICEQUERY:
            if (!device_query(ior))
            {
                ior->io_Error = IOERR_NOCMD;
            }
            break;

        case TD_CHANGESTATE:
        {
            struct Drive* d = (struct Drive*)ior->io_Unit;
            ior->io_Actual = (d && (d->flags & DRVF_PRESENT)) ? 0 : 1;
            break;
        }

        case TD_CHANGENUM:
            // Fixed disk: the media never changes, so the count stays 0.
            ior->io_Actual = 0;
            break;

        case TD_PROTSTATUS:
        {
            struct Drive* d = (struct Drive*)ior->io_Unit;
            ior->io_Actual = (d && (d->flags & DRVF_WRITEPROT)) ? 1 : 0;
            break;
        }

        case CMD_CLEAR:
        case CMD_UPDATE:
        case TD_MOTOR:
        case TD_SEEK:
        case TD_SEEK64:
        case NSCMD_TD_SEEK64:
        case TD_REMOVE:
            // No-ops.
            break;

        default:
            // Handled asynchronously by the io task.
            ior->io_Flags &= ~IOF_QUICK;
            PutMsg(base->reqport, &ior->io_Message);
            return;
    }

    if (!(ior->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ior->io_Message);
    }
}

static ULONG abort_io(__reg("a6") struct A314ScsiBase* base,
                      __reg("a1") struct IOStdReq* ior)
{
    return IOERR_NOCMD;
}

static void open(__reg("a6") struct A314ScsiBase* base,
                 __reg("a1") struct IOStdReq* ior,
                 __reg("d0") ULONG unitnum,
                 __reg("d1") ULONG flags)
{
    kprintf("a314scsi: open unit=%ld\n", unitnum);

    ior->io_Error = IOERR_OPENFAIL;
    ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    // Decode the Amiga unit (LUN*10 + ID) to a flat host unit. Each host disk is
    // a separate LUN with ID 0, so only 0, 10, 20, ... are valid. Done by
    // subtraction because a 32-bit divide isn't linked under -nostdlib.
    ULONG rem = unitnum;
    UWORD host = 0;
    while (rem >= A314SCSI_UNIT_STEP && host < A314SCSI_MAX_DRIVES)
    {
        rem -= A314SCSI_UNIT_STEP;
        host++;
    }
    if (rem != 0 || host >= A314SCSI_MAX_DRIVES)
    {
        return; // non-zero ID, or LUN out of range
    }

    struct Drive* d = find_drive(base, host);
    if (!d)
    {
        return;
    }

    ior->io_Unit = (struct Unit*)d;
    ior->io_Device = (struct Device*)base;
    d->opencount++;
    base->device.dd_Library.lib_OpenCnt++;
    ior->io_Error = 0;
}

static BPTR expunge(__reg("a6") struct A314ScsiBase* base)
{
    // No unload support.
    base->device.dd_Library.lib_Flags |= LIBF_DELEXP;
    return 0;
}

static BPTR close(__reg("a6") struct A314ScsiBase* base,
                  __reg("a1") struct IOStdReq* ior)
{
    struct Drive* d = (struct Drive*)ior->io_Unit;
    if (d && d->opencount)
    {
        d->opencount--;
    }

    ior->io_Unit = NULL;
    ior->io_Device = NULL;

    base->device.dd_Library.lib_OpenCnt--;
    return 0;
}

static struct Library* init_device(__reg("a6") struct ExecBase* sys_base,
                                   __reg("a0") BPTR seg_list,
                                   __reg("d0") struct A314ScsiBase* base)
{
    kprintf("a314scsi: init_device seg_list=%lx\n", (ULONG)seg_list);

    base->sysbase = sys_base;
    base->seglist = seg_list;

    base->device.dd_Library.lib_Node.ln_Type = NT_DEVICE;
    base->device.dd_Library.lib_Node.ln_Name = (char*)device_name;
    base->device.dd_Library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->device.dd_Library.lib_Version = DEVICE_VERSION;
    base->device.dd_Library.lib_Revision = DEVICE_REVISION;
    base->device.dd_Library.lib_IdString = (APTR)id_string;

    base->stack = AllocMem(TASK_STACK_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!base->stack)
    {
        kprintf("a314scsi: stack alloc FAILED\n");
        return NULL;
    }

    // Open a314.device here, in the caller's (normal process) context. Opening it
    // from the freshly-created io task hangs. If it fails the device still loads;
    // the io task just won't connect.
    if (!a314_open_device(base))
    {
        kprintf("a314scsi: a314_open_device FAILED\n");
    }

    BYTE sig = AllocSignal(-1);
    base->inittask = FindTask(NULL);
    base->initsig = (sig == -1) ? 0 : (1UL << sig);
    base->started = FALSE;

    struct Task* t = &base->iotask;
    t->tc_Node.ln_Type = NT_TASK;
    t->tc_Node.ln_Pri = TASK_PRIORITY;
    t->tc_Node.ln_Name = (char*)device_name;
    t->tc_SPLower = base->stack;
    t->tc_SPUpper = (APTR)((UBYTE*)base->stack + TASK_STACK_SIZE);
    t->tc_SPReg = t->tc_SPUpper;
    t->tc_UserData = base;
    NewList(&t->tc_MemEntry);

    kprintf("a314scsi: starting task\n");
    AddTask(t, (APTR)task_body, 0);

    if (sig != -1)
    {
        Wait(base->initsig);
        FreeSignal(sig);
    }

    base->inittask = NULL;

    kprintf("a314scsi: init_device done\n");
    return &base->device.dd_Library;
}

const ULONG device_vectors[] =
{
    (ULONG)open,
    (ULONG)close,
    (ULONG)expunge,
    0,
    (ULONG)begin_io,
    (ULONG)abort_io,
    (ULONG)-1,
};

const ULONG auto_init_tables[] =
{
    sizeof(struct A314ScsiBase),
    (ULONG)device_vectors,
    0,
    (ULONG)init_device,
};
