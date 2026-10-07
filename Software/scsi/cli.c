#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/nodes.h>
#include <exec/lists.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <libraries/filehandler.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include <string.h>

#include "a314scsi.h"
#include "alib.h"
#include "kprintf.h"

#define SysBase (*(struct ExecBase**)4)

#define MAX_PENDING 16

// Is this DeviceNode one of ours (its exec device name == a314scsi.device)?
static BOOL is_ours(struct DeviceNode* dn)
{
    if (!dn || !dn->dn_Startup)
    {
        return FALSE;
    }

    struct FileSysStartupMsg* fssm = (struct FileSysStartupMsg*)BADDR(dn->dn_Startup);
    if (!fssm->fssm_Device)
    {
        return FALSE;
    }

    const UBYTE* name = (const UBYTE*)BADDR(fssm->fssm_Device);
    UBYTE len = (UBYTE)strlen(DEVICE_NAME);
    return name[0] == len && memcmp(&name[1], DEVICE_NAME, len) == 0;
}

int memcmp(const void* a, const void* b, size_t n)
{
    const UBYTE* pa = (const UBYTE*)a;
    const UBYTE* pb = (const UBYTE*)b;
    for (size_t i = 0; i < n; i++)
    {
        if (pa[i] != pb[i])
        {
            return (int)pa[i] - (int)pb[i];
        }
    }
    return 0;
}

LONG cli_main(void)
{
    kprintf("a314scsi: cli enter\n");
    struct Library* ExpansionBase = OpenLibrary((STRPTR)EXPANSIONNAME, 0);
    if (!ExpansionBase)
    {
        return RETURN_FAIL;
    }

    struct MsgPort* port = CreatePort(NULL, 0);
    if (!port)
    {
        CloseLibrary(ExpansionBase);
        return RETURN_FAIL;
    }

    // Opening the device runs its io task, which discovers and enqueues the
    // partitions. If it wasn't resident yet this also loads it.
    struct IOStdReq ior;
    memset(&ior, 0, sizeof(ior));
    ior.io_Message.mn_ReplyPort = port;
    ior.io_Message.mn_Length = sizeof(ior);

    BOOL opened = FALSE;
    for (UWORD u = 0; u < A314SCSI_MAX_DRIVES; u++)
    {
        if (!OpenDevice((STRPTR)DEVICE_NAME, u * A314SCSI_UNIT_STEP, (struct IORequest*)&ior, 0))
        {
            opened = TRUE;
            break;
        }
    }
    if (!opened)
    {
        kprintf("a314scsi: cli OpenDevice FAILED\n");
        DeletePort(port);
        CloseLibrary(ExpansionBase);
        return RETURN_FAIL;
    }

    // Detach our BootNodes from the MountList under Forbid, then AddDosNode them
    // outside it (AddDosNode with ADNF_STARTPROC creates a process). Removing
    // them makes repeated runs idempotent.
    struct DeviceNode* pending[MAX_PENDING];
    LONG pri[MAX_PENDING];
    UWORD count = 0;

    struct List* ml = &((struct ExpansionBase*)ExpansionBase)->MountList;

    Forbid();
    struct Node* n = ml->lh_Head;
    while (n->ln_Succ && count < MAX_PENDING)
    {
        struct Node* next = n->ln_Succ;
        struct BootNode* bn = (struct BootNode*)n;
        if (is_ours((struct DeviceNode*)bn->bn_DeviceNode))
        {
            Remove(n);
            pending[count] = (struct DeviceNode*)bn->bn_DeviceNode;
            pri[count] = (LONG)(BYTE)bn->bn_Node.ln_Pri;
            count++;
        }
        n = next;
    }
    Permit();

    for (UWORD i = 0; i < count; i++)
    {
        kprintf("a314scsi: cli AddDosNode pri=%ld\n", pri[i]);
        AddDosNode(pri[i], ADNF_STARTPROC, pending[i]);
    }

    CloseDevice((struct IORequest*)&ior);
    DeletePort(port);
    CloseLibrary(ExpansionBase);

    kprintf("a314scsi: cli mounted %ld node(s)\n", (LONG)count);
    return RETURN_OK;
}
