#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/errors.h>

#include <dos/dos.h> // TICKS_PER_SECOND

#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <proto/exec.h>


#include <string.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

#include "a314scsi.h"
#include "a314scsi_protocol.h"
#include "scsi.h"
#include "alib.h"
#include "kprintf.h"

#define SysBase (*(struct ExecBase**)4)

// Redirect the proto_a314.h helper macros to the per-device library base.
#undef A314Base
#define A314Base (base->a314base)

static const char service_name[] = "scsi";
static const char a314_name[] = A314_NAME;

static ULONG get_socket_id(void)
{
    struct timerequest tr;

    memset(&tr, 0, sizeof(tr));
    tr.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    if (OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest*)&tr, 0))
    {
        return 0x53435349UL; // 'SCSI'
    }

    tr.tr_node.io_Command = TR_GETSYSTIME;
    DoIO((struct IORequest*)&tr);
    CloseDevice((struct IORequest*)&tr);

    return (tr.tr_time.tv_secs * TICKS_PER_SECOND) ^ 0x53435349UL;
}

// Open a314.device and reserve resources. This MUST run in init_device's caller
// context (a normal process), NOT the freshly-created io task: opening the device
// from a just-added task races with a314.device's own startup and hangs.
BOOL a314_open_device(struct A314ScsiBase* base)
{
    // OpenDevice needs a reply port, but the io task owns the real one; use a
    // throwaway port here and let a314_connect() install the io task's port.
    struct MsgPort* tmp = CreatePort(NULL, 0);
    if (!tmp)
    {
        return FALSE;
    }

    memset(&base->a314ior, 0, sizeof(base->a314ior));
    base->a314ior.a314_Request.io_Message.mn_ReplyPort = tmp;
    base->a314ior.a314_Request.io_Message.mn_Length = sizeof(base->a314ior);

    if (OpenDevice((STRPTR)a314_name, 0, &base->a314ior.a314_Request, 0))
    {
        kprintf("a314scsi: OpenDevice(a314.device) FAILED\n");
        DeletePort(tmp);
        return FALSE;
    }
    base->a314base = &base->a314ior.a314_Request.io_Device->dd_Library;

    DeletePort(tmp);

    base->socket = get_socket_id();

    base->bounce = AllocMemA314(A314SCSI_MAX_XFER);
    if (base->bounce == INVALID_A314_ADDRESS)
    {
        kprintf("a314scsi: AllocMemA314 FAILED\n");
        return FALSE;
    }

    return TRUE;
}

// Connect to the host 'scsi' service. Runs in the io task, which owns the reply
// port used for this and every subsequent A314_WRITE/READ transaction.
BOOL a314_connect(struct A314ScsiBase* base)
{
    base->a314port = CreatePort(NULL, 0);
    if (!base->a314port)
    {
        return FALSE;
    }

    base->a314ior.a314_Request.io_Message.mn_ReplyPort = base->a314port;

    base->a314ior.a314_Request.io_Command = A314_CONNECT;
    base->a314ior.a314_Request.io_Error = 0;
    base->a314ior.a314_Socket = base->socket;
    base->a314ior.a314_Buffer = (STRPTR)service_name;
    base->a314ior.a314_Length = strlen(service_name);
    DoIO(&base->a314ior.a314_Request);

    if (base->a314ior.a314_Request.io_Error != A314_CONNECT_OK)
    {
        kprintf("a314scsi: CONNECT failed err=%ld\n", (ULONG)base->a314ior.a314_Request.io_Error);
        return FALSE;
    }

    base->connected = 1;
    kprintf("a314scsi: connected to 'scsi' (socket=%lx)\n", base->socket);
    return TRUE;
}

static LONG a314_txn(struct A314ScsiBase* base, struct A314ScsiReq* req, struct A314ScsiRes* res)
{
    base->a314ior.a314_Request.io_Command = A314_WRITE;
    base->a314ior.a314_Request.io_Error = 0;
    base->a314ior.a314_Socket = base->socket;
    base->a314ior.a314_Buffer = (STRPTR)req;
    base->a314ior.a314_Length = sizeof(struct A314ScsiReq);
    DoIO(&base->a314ior.a314_Request);
    if (base->a314ior.a314_Request.io_Error != A314_WRITE_OK)
    {
        return IOERR_ABORTED;
    }

    base->a314ior.a314_Request.io_Command = A314_READ;
    base->a314ior.a314_Request.io_Error = 0;
    base->a314ior.a314_Socket = base->socket;
    base->a314ior.a314_Buffer = (STRPTR)res;
    base->a314ior.a314_Length = sizeof(struct A314ScsiRes);
    DoIO(&base->a314ior.a314_Request);
    if (base->a314ior.a314_Request.io_Error != A314_READ_OK)
    {
        return IOERR_ABORTED;
    }

    return 0;
}

LONG a314_scsi(struct A314ScsiBase* base, struct Drive* d, struct SCSICmd* cmd)
{
    struct A314ScsiReq req;
    struct A314ScsiRes res;

    if (!base->connected)
    {
        return IOERR_ABORTED;
    }

    if (cmd->scsi_Length > A314SCSI_MAX_XFER)
    {
        return IOERR_ABORTED; // caller must chunk
    }

    UBYTE dir;
    if (cmd->scsi_Length == 0)
    {
        dir = A314SCSI_DIR_NONE;
    }
    else if (cmd->scsi_Flags & SCSIF_READ)
    {
        dir = A314SCSI_DIR_READ;
    }
    else
    {
        dir = A314SCSI_DIR_WRITE;
    }

    // Resolve the a314-space address of the caller buffer, else use the bounce.
    ULONG addr = 0;
    BOOL bounced = FALSE;
    if (cmd->scsi_Length)
    {
        addr = TranslateAddressA314((APTR)cmd->scsi_Data);
        if (addr == INVALID_A314_ADDRESS)
        {
            addr = base->bounce;
            bounced = TRUE;
            if (dir == A314SCSI_DIR_WRITE)
            {
                WriteMemA314(base->bounce, (APTR)cmd->scsi_Data, cmd->scsi_Length);
            }
        }
    }

    memset(&req, 0, sizeof(req));
    req.kind = A314SCSI_CMD_REQ;
    req.unit = d ? (UBYTE)d->number : 0;
    req.cdb_len = (UBYTE)cmd->scsi_CmdLength;
    if (req.cdb_len > A314SCSI_MAX_CDB)
    {
        req.cdb_len = A314SCSI_MAX_CDB;
    }
    memcpy(req.cdb, cmd->scsi_Command, req.cdb_len);
    req.direction = dir;
    req.data_length = cmd->scsi_Length;
    req.address = addr;

    LONG err = a314_txn(base, &req, &res);
    if (err)
    {
        kprintf("a314scsi: txn err=%ld\n", err);
        return err;
    }

    cmd->scsi_Status = res.scsi_status;
    cmd->scsi_Actual = res.actual_length;

    if (dir == A314SCSI_DIR_READ && bounced && res.actual_length)
    {
        ULONG n = res.actual_length;
        if (n > cmd->scsi_Length)
        {
            n = cmd->scsi_Length;
        }
        ReadMemA314((APTR)cmd->scsi_Data, base->bounce, n);
    }

    if (res.scsi_status != SCSI_STATUS_GOOD)
    {
        if (cmd->scsi_SenseData && cmd->scsi_SenseLength >= 14)
        {
            memset(cmd->scsi_SenseData, 0, cmd->scsi_SenseLength);
            cmd->scsi_SenseData[0] = SCSI_SENSE_CURRENT_FIXED;
            cmd->scsi_SenseData[2] = res.sense_key;
            cmd->scsi_SenseData[7] = SCSI_SENSE_ADD_LEN;
            cmd->scsi_SenseData[12] = res.asc;
            cmd->scsi_SenseData[13] = res.ascq;
            cmd->scsi_SenseActual = 14;
        }
        return HFERR_BadStatus;
    }

    return 0;
}
