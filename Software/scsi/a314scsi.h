#ifndef A314SCSI_H
#define A314SCSI_H

#include <exec/types.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/lists.h>
#include <exec/libraries.h>

#include <dos/dos.h> // BPTR

#include <devices/scsidisk.h> // struct SCSICmd

#include "../a314device/a314.h" // struct A314_IORequest

#define DEVICE_NAME "a314scsi.device"
#define DEVICE_VERSION 1
#define DEVICE_REVISION 0

// How many drive units the host can serve (unit number -> HDF).
#define A314SCSI_MAX_DRIVES 4

// Amiga unit numbering follows the scsi.device convention: unit = LUN*10 + ID.
// Each host disk is exposed as its own LUN (ID 0), so valid units are 0, 10,
// 20, ... - which is exactly what HDToolBox probes.
#define A314SCSI_UNIT_STEP 10

#define TASK_PRIORITY 10
#define TASK_STACK_SIZE 4096

// Largest single a314 transfer == the a314-memory bounce buffer.
#define A314SCSI_MAX_XFER (4UL * 1024)

// Drive flags.
#define DRVF_PRESENT (1 << 0)
#define DRVF_WRITEPROT (1 << 1)
#define DRVF_REMOVABLE (1 << 2)
#define DRVF_MOUNTED (1 << 3)

struct Drive
{
    UWORD           number;         // unit number
    UWORD           opencount;
    UWORD           flags;          // DRVF_*
    UBYTE           devtype;        // INQUIRY peripheral device type
    UBYTE           blockshift;     // log2(blocksize)
    ULONG           blocksize;
    ULONG           blocks;         // total block count
    UBYTE           sense[24];      // last auto/request sense
};

struct A314ScsiBase
{
    struct Device           device;
    struct ExecBase*        sysbase;
    BPTR                    seglist;

    struct Drive*           drives[A314SCSI_MAX_DRIVES];

    struct MsgPort*         reqport;        // command queue to the io task
    struct Task             iotask;         // embedded io task (tc_UserData -> base)
    APTR                    stack;

    // a314 transport (opened and used only by the io task)
    struct MsgPort*         a314port;
    struct A314_IORequest   a314ior;
    struct Library*         a314base;
    ULONG                   socket;
    ULONG                   bounce;         // a314-space bounce buffer
    UBYTE                   connected;

    // io-task startup handshake back to init_device
    struct Task*            inittask;
    ULONG                   initsig;
    volatile UBYTE          started;
};

// device.c
extern const char device_name[];
struct Drive* find_drive(struct A314ScsiBase* base, UWORD number);
struct Drive* make_drive(struct A314ScsiBase* base, UWORD number);

// scsicmd.c
LONG scsi_test_ready(struct A314ScsiBase* base, struct Drive* d);
LONG scsi_inquiry(struct A314ScsiBase* base, struct Drive* d);
LONG scsi_read_capacity(struct A314ScsiBase* base, struct Drive* d);
LONG scsi_mode_sense_wp(struct A314ScsiBase* base, struct Drive* d);
LONG scsi_rw(struct A314ScsiBase* base, struct Drive* d, ULONG lba, ULONG blocks, APTR buf, BOOL write);

// a314hw.c
BOOL a314_open_device(struct A314ScsiBase* base);
BOOL a314_connect(struct A314ScsiBase* base);
LONG a314_scsi(struct A314ScsiBase* base, struct Drive* d, struct SCSICmd* cmd);

// rdb.c
void mount_drive(struct A314ScsiBase* base, struct Drive* d);

// lseg.c
BPTR load_seglist(struct A314ScsiBase* base, struct Drive* d, ULONG first_block);

#endif
