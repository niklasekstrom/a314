#include <exec/types.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/lists.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <libraries/filehandler.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>

#include <devices/hardblocks.h>

#include <proto/exec.h>

#include <string.h>

#include "a314scsi.h"
#include "kprintf.h"

#define SysBase (*(struct ExecBase**)4)

#define RDB_SCAN_LIMIT 16 // the RDB must live within the first 16 blocks
#define MAX_LOADED_FS 8

// fhb_PatchFlags bits (which DeviceNode fields the FSHB overrides).
#define FSPF_TYPE       (1 << 0)
#define FSPF_STACKSIZE  (1 << 4)
#define FSPF_PRIORITY   (1 << 5)
#define FSPF_GLOBALVEC  (1 << 8)

struct LoadedFS
{
    ULONG   dostype;
    BPTR    seglist;
    ULONG   globalvec;
    ULONG   patchflags;
    ULONG   stacksize;
    LONG    priority;
    ULONG   dntype;
};

struct MountEntry
{
    struct          DeviceNode dev_node;
    struct          FileSysStartupMsg fssm;
    ULONG           env_vec[20];
    char            drive_name[36];  // BCPL string
    char            exec_name[24];   // BCPL string
    struct BootNode boot_node;
};

static ULONG sum_block(const ULONG* p, ULONG longs)
{
    ULONG sum = 0;
    for (ULONG i = 0; i < longs; i++)
    {
        sum += p[i];
    }
    return sum;
}

static BOOL read_block(struct A314ScsiBase* base, struct Drive* d, ULONG block, APTR buf)
{
    return scsi_rw(base, d, block, 1, buf, FALSE) == 0;
}

// Copy a BCPL-style string (len byte + chars) into a BCPL field.
static void set_bcpl(char* dst, const char* src, UBYTE len)
{
    dst[0] = len;
    for (UBYTE i = 0; i < len; i++)
    {
        dst[1 + i] = src[i];
    }
}

// Walk the RDB FileSysHeaderBlock list, loading each filesystem's seglist.
static ULONG load_filesystems(struct A314ScsiBase* base, struct Drive* d, ULONG fshList, struct LoadedFS* tab)
{
    APTR buf = AllocMem(d->blocksize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buf)
    {
        return 0;
    }

    ULONG n = 0;
    ULONG b = fshList;
    while (b != 0xFFFFFFFFUL && b != 0 && n < MAX_LOADED_FS)
    {
        if (!read_block(base, d, b, buf))
        {
            break;
        }

        struct FileSysHeaderBlock* fhb = (struct FileSysHeaderBlock*)buf;
        if (fhb->fhb_ID != IDNAME_FILESYSHEADER)
        {
            break;
        }

        ULONG dostype = fhb->fhb_DosType;
        ULONG seg = fhb->fhb_SegListBlocks;
        ULONG next = fhb->fhb_Next;

        kprintf("a314scsi: FSHD dostype=%lx segblock=%ld\n", dostype, seg);

        BPTR seglist = load_seglist(base, d, seg);
        if (seglist)
        {
            tab[n].dostype      = dostype;
            tab[n].seglist      = seglist;
            tab[n].globalvec    = fhb->fhb_GlobalVec;
            tab[n].patchflags   = fhb->fhb_PatchFlags;
            tab[n].stacksize    = fhb->fhb_StackSize;
            tab[n].priority     = fhb->fhb_Priority;
            tab[n].dntype       = fhb->fhb_Type;
            n++;
        }

        b = next;
    }

    FreeMem(buf, d->blocksize);
    return n;
}

static void mount_partition(struct A314ScsiBase* base, struct Drive* d, struct PartitionBlock* pb,
                            struct LoadedFS* fstab, ULONG fscount, struct ExpansionBase* expansion)
{
    struct MountEntry* me = (struct MountEntry*)AllocMem(sizeof(struct MountEntry), MEMF_PUBLIC | MEMF_CLEAR);
    if (!me)
    {
        return;
    }

    const ULONG* env = (const ULONG*)pb->pb_Environment;
    ULONG env_longs = env[0] + 1; // de_TableSize + 1
    if (env_longs > 20)
    {
        env_longs = 20;
    }
    for (ULONG i = 0; i < env_longs; i++)
    {
        me->env_vec[i] = env[i];
    }

    // Drive name (from the partition) and exec device name, as BCPL strings.
    UBYTE dn_len = ((const UBYTE*)pb->pb_DriveName)[0];
    if (dn_len > 34)
    {
        dn_len = 34;
    }
    set_bcpl(me->drive_name, (const char*)&pb->pb_DriveName[1], dn_len);
    set_bcpl(me->exec_name, device_name, (UBYTE)strlen(device_name));

    me->fssm.fssm_Unit          = d->number * A314SCSI_UNIT_STEP;
    me->fssm.fssm_Device        = MKBADDR(me->exec_name);
    me->fssm.fssm_Environ       = MKBADDR(me->env_vec);
    me->fssm.fssm_Flags         = 0;

    me->dev_node.dn_Type        = 0;
    me->dev_node.dn_StackSize   = 600;
    me->dev_node.dn_Priority    = 10;
    me->dev_node.dn_Startup     = MKBADDR(&me->fssm);
    me->dev_node.dn_Name        = MKBADDR(me->drive_name);
    me->dev_node.dn_GlobalVec   = -1;

    // If the partition's DOSType matches a filesystem we loaded from the RDB,
    // point the DeviceNode at it (patching the fields the FSHB requests).
    ULONG dostype = env[DE_DOSTYPE];
    for (ULONG i = 0; i < fscount; i++)
    {
        if (fstab[i].dostype == dostype)
        {
            me->dev_node.dn_SegList = fstab[i].seglist;
            if (fstab[i].patchflags & FSPF_GLOBALVEC)
            {
                me->dev_node.dn_GlobalVec = fstab[i].globalvec;
            }
            if (fstab[i].patchflags & FSPF_TYPE)
            {
                me->dev_node.dn_Type = fstab[i].dntype;
            }
            if (fstab[i].patchflags & FSPF_STACKSIZE)
            {
                me->dev_node.dn_StackSize = fstab[i].stacksize;
            }
            if (fstab[i].patchflags & FSPF_PRIORITY)
            {
                me->dev_node.dn_Priority = fstab[i].priority;
            }
            kprintf("a314scsi: partition FS dostype=%lx -> seglist %lx\n", dostype, (ULONG)fstab[i].seglist);
            break;
        }
    }

    LONG bootpri = (LONG)env[DE_BOOTPRI];
    BOOL bootable = (pb->pb_Flags & PBFF_BOOTABLE) ? TRUE : FALSE;

    me->boot_node.bn_Node.ln_Type   = NT_BOOTNODE;
    me->boot_node.bn_Node.ln_Pri    = bootable ? (BYTE)bootpri : -128;
    me->boot_node.bn_Node.ln_Name   = NULL;
    me->boot_node.bn_DeviceNode     = (APTR)&me->dev_node;

    kprintf("a314scsi: mount partition (bootpri=%ld bootable=%ld)\n", bootpri, (ULONG)bootable);

    Forbid();
    Enqueue(&expansion->MountList, &me->boot_node.bn_Node);
    Permit();
}

void mount_drive(struct A314ScsiBase* base, struct Drive* d)
{
    kprintf("a314scsi: mount_drive unit=%ld blocksize=%ld\n", (ULONG)d->number, d->blocksize);

    if (d->flags & DRVF_MOUNTED)
    {
        return;
    }

    // One block buffer serves the whole routine: first the RDB scan, then each
    // partition block in turn.
    APTR buf = AllocMem(d->blocksize, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buf)
    {
        kprintf("a314scsi: mount_drive AllocMem FAILED\n");
        return;
    }

    // Find the RigidDiskBlock within the first few sectors.
    struct RigidDiskBlock* rdb = NULL;
    for (ULONG block = 0; block < RDB_SCAN_LIMIT; block++)
    {
        if (!read_block(base, d, block, buf))
        {
            kprintf("a314scsi: read block %ld FAILED\n", block);
            break;
        }

        struct RigidDiskBlock* r = (struct RigidDiskBlock*)buf;
        if (r->rdb_ID == IDNAME_RIGIDDISK && sum_block((ULONG*)buf, r->rdb_SummedLongs) == 0)
        {
            rdb = r;
            break;
        }
    }

    if (rdb)
    {
        kprintf("a314scsi: RDB found on unit %ld\n", (ULONG)d->number);

        // Grab the chain heads before buf gets reused for partition blocks.
        ULONG fsh_list = rdb->rdb_FileSysHeaderList;
        ULONG part_block = rdb->rdb_PartitionList;

        struct LoadedFS fstab[MAX_LOADED_FS];
        ULONG fscount = load_filesystems(base, d, fsh_list, fstab);
        kprintf("a314scsi: %ld filesystem(s) loaded\n", fscount);

        // Enqueue every partition on the expansion MountList (opened once).
        struct ExpansionBase* expansion = (struct ExpansionBase*)OpenLibrary((STRPTR)EXPANSIONNAME, 0);
        if (expansion)
        {
            while (part_block != 0xFFFFFFFFUL && part_block != 0)
            {
                if (!read_block(base, d, part_block, buf))
                {
                    break;
                }

                struct PartitionBlock* pb = (struct PartitionBlock*)buf;
                if (pb->pb_ID != IDNAME_PARTITION)
                {
                    break;
                }

                mount_partition(base, d, pb, fstab, fscount, expansion);
                part_block = pb->pb_Next;
            }
            CloseLibrary((struct Library*)expansion);
        }

        d->flags |= DRVF_MOUNTED;
    }
    else
    {
        kprintf("a314scsi: no RDB on unit %ld\n", (ULONG)d->number);
    }

    FreeMem(buf, d->blocksize);
}
