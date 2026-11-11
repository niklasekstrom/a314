#include <exec/types.h>
#include <exec/errors.h>

#include <devices/scsidisk.h>

#include <string.h>

#include "a314scsi.h"
#include "a314scsi_protocol.h"
#include "scsi.h"
#include "kprintf.h"

// Response layouts only. CDBs are assembled byte-wise
#pragma pack(push, 1)

struct ReadCapacityData
{
    ULONG last_lba;
    ULONG block_size;
};

#pragma pack(pop)

static UBYTE shift_for(ULONG block_size)
{
    UBYTE s = 0;
    while ((1UL << s) < block_size && s < 31)
    {
        s++;
    }
    return s;
}

// Build a SCSICmd around a CDB and run it through the a314 transport.
static LONG run_cdb(struct A314ScsiBase* base, struct Drive* d, UBYTE* cdb, UBYTE cdb_len, APTR data, ULONG length,
                    UBYTE dir)
{
    struct SCSICmd cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.scsi_Data = (UWORD*)data;
    cmd.scsi_Length = length;
    cmd.scsi_Command = cdb;
    cmd.scsi_CmdLength = cdb_len;
    cmd.scsi_Flags = SCSIF_AUTOSENSE | ((dir == A314SCSI_DIR_READ) ? SCSIF_READ : 0);
    cmd.scsi_SenseData = d->sense;
    cmd.scsi_SenseLength = sizeof(d->sense);

    return a314_scsi(base, d, &cmd);
}

LONG scsi_test_ready(struct A314ScsiBase* base, struct Drive* d)
{
    UBYTE cdb[6];

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_CMD_TEST_UNIT_READY;
    return run_cdb(base, d, cdb, 6, NULL, 0, A314SCSI_DIR_NONE);
}

LONG scsi_inquiry(struct A314ScsiBase* base, struct Drive* d)
{
    UBYTE cdb[6];
    UBYTE buf[36];

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_CMD_INQUIRY;
    cdb[4] = sizeof(buf);

    memset(buf, 0, sizeof(buf));
    LONG err = run_cdb(base, d, cdb, 6, buf, sizeof(buf), A314SCSI_DIR_READ);
    if (err)
    {
        return err;
    }

    d->devtype = buf[0] & SCSI_INQ_PDT_MASK;
    if (buf[1] & SCSI_INQ_RMB)
    {
        d->flags |= DRVF_REMOVABLE;
    }

    return 0;
}

LONG scsi_read_capacity(struct A314ScsiBase* base, struct Drive* d)
{
    UBYTE cdb[10];
    UBYTE buf[8];

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_CMD_READ_CAPACITY_10;
    memset(buf, 0, sizeof(buf));

    LONG err = run_cdb(base, d, cdb, 10, buf, sizeof(struct ReadCapacityData), A314SCSI_DIR_READ);
    if (err)
    {
        return err;
    }

    struct ReadCapacityData* cap = (struct ReadCapacityData*)buf;
    ULONG bsize = cap->block_size;

    d->blocks = cap->last_lba + 1;

    if (bsize < 256 || bsize > 8192)
    {
        bsize = 512;
    }
    d->blocksize = bsize;
    d->blockshift = shift_for(bsize);

    return 0;
}

LONG scsi_mode_sense_wp(struct A314ScsiBase* base, struct Drive* d)
{
    UBYTE cdb[6];
    UBYTE buf[4];

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = SCSI_CMD_MODE_SENSE_6;
    cdb[2] = SCSI_MODE_PAGE_RETURN_ALL; // the header carries the WP bit
    cdb[4] = sizeof(buf);

    memset(buf, 0, sizeof(buf));
    LONG err = run_cdb(base, d, cdb, 6, buf, sizeof(buf), A314SCSI_DIR_READ);
    if (err)
    {
        return err; // not fatal for discovery
    }

    if (buf[2] & SCSI_MODE_HDR_WP)
    {
        d->flags |= DRVF_WRITEPROT;
    }

    return 0;
}

LONG scsi_rw(struct A314ScsiBase* base, struct Drive* d, ULONG lba, ULONG blocks, APTR buf, BOOL write)
{
    UBYTE cdb[10];
    ULONG length = blocks << d->blockshift;
    UBYTE dir = write ? A314SCSI_DIR_WRITE : A314SCSI_DIR_READ;

    memset(cdb, 0, sizeof(cdb));
    cdb[0] = write ? SCSI_CMD_WRITE_10 : SCSI_CMD_READ_10;
    cdb[2] = (UBYTE)(lba >> 24);
    cdb[3] = (UBYTE)(lba >> 16);
    cdb[4] = (UBYTE)(lba >> 8);
    cdb[5] = (UBYTE)lba;
    cdb[7] = (UBYTE)(blocks >> 8);
    cdb[8] = (UBYTE)blocks;
    return run_cdb(base, d, cdb, 10, buf, length, dir);
}
