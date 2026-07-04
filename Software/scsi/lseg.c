#include <exec/types.h>
#include <exec/memory.h>

#include <libraries/dos.h> // BPTR, MKBADDR
#include <dos/doshunks.h>  // HUNK_*

#include <devices/hardblocks.h> // IDNAME_LOADSEG, struct LoadSegBlock

#include <proto/exec.h>

#include <string.h>
#include <stddef.h> // offsetof

#include "a314scsi.h"
#include "kprintf.h"

#define SysBase (*(struct ExecBase**)4)

#define LSEG_DATA_OFFSET offsetof(struct LoadSegBlock, lsb_LoadData)
#define LSEG_MAX_BLOCKS 8192

// Read one LSEG block into blk and validate it. On success returns TRUE and
// stores lsb_Next; on read error or a non-LSEG block returns FALSE.
static BOOL read_lseg_block(struct A314ScsiBase* base, struct Drive* d, ULONG b, UBYTE* blk, ULONG* next)
{
    if (scsi_rw(base, d, b, 1, blk, FALSE) != 0)
    {
        return FALSE;
    }

    struct LoadSegBlock* lsb = (struct LoadSegBlock*)blk;
    if (lsb->lsb_ID != IDNAME_LOADSEG)
    {
        return FALSE;
    }
    *next = lsb->lsb_Next;
    return TRUE;
}

// Reassemble the LSEG chain into one contiguous buffer. Returns the buffer
// (caller FreeMem's *out_len bytes) or NULL. The chain is walked twice - once to
// size the buffer, once to fill it - rather than growing it dynamically.
static UBYTE* read_lseg_chain(struct A314ScsiBase* base, struct Drive* d, ULONG first, ULONG* out_len)
{
    ULONG bs = d->blocksize;
    ULONG per = bs - LSEG_DATA_OFFSET;

    UBYTE* blk = (UBYTE*)AllocMem(bs, MEMF_PUBLIC);
    if (!blk)
    {
        return NULL;
    }

    ULONG next;

    ULONG count = 0;
    ULONG b = first;
    while (b != 0xFFFFFFFFUL && b != 0 && count < LSEG_MAX_BLOCKS)
    {
        if (!read_lseg_block(base, d, b, blk, &next))
        {
            break;
        }
        count++;
        b = next;
    }

    if (!count)
    {
        FreeMem(blk, bs);
        return NULL;
    }

    ULONG total = count * per;
    UBYTE* buf = (UBYTE*)AllocMem(total, MEMF_PUBLIC);
    if (!buf)
    {
        FreeMem(blk, bs);
        return NULL;
    }

    b = first;
    ULONG off = 0;
    for (ULONG i = 0; i < count; i++)
    {
        if (!read_lseg_block(base, d, b, blk, &next))
        {
            break;
        }
        memcpy(buf + off, ((struct LoadSegBlock*)blk)->lsb_LoadData, per);
        off += per;
        b = next;
    }

    FreeMem(blk, bs);
    *out_len = total;
    return buf;
}

BPTR load_seglist(struct A314ScsiBase* base, struct Drive* d, ULONG first)
{
    ULONG len = 0;
    UBYTE* data = read_lseg_chain(base, d, first, &len);
    if (!data)
    {
        kprintf("a314scsi: LSEG read failed\n");
        return 0;
    }

    ULONG* p = (ULONG*)data;
    ULONG* end = (ULONG*)(data + len);

    if (*p++ != HUNK_HEADER)
    {
        kprintf("a314scsi: not a HUNK_HEADER\n");
        FreeMem(data, len);
        return 0;
    }

    // Skip resident-library name list.
    while (p < end && *p)
    {
        ULONG n = *p++;
        p += n;
    }
    p++; // terminating 0

    p++; // table size (unused)
    ULONG firstHunk = *p++;
    ULONG lastHunk = *p++;
    ULONG numHunks = lastHunk - firstHunk + 1;

    APTR* seg = (APTR*)AllocMem(numHunks * sizeof(APTR), MEMF_PUBLIC | MEMF_CLEAR);
    ULONG* segBytes = (ULONG*)AllocMem(numHunks * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
    if (!seg || !segBytes)
    {
        if (seg)
        {
            FreeMem(seg, numHunks * sizeof(APTR));
        }
        if (segBytes)
        {
            FreeMem(segBytes, numHunks * sizeof(ULONG));
        }
        FreeMem(data, len);
        return 0;
    }

    // Allocate each hunk as a SegList node: [ULONG length][BPTR next][data...].
    for (ULONG i = 0; i < numHunks; i++)
    {
        ULONG raw = *p++;
        ULONG longs = raw & 0x3FFFFFFFUL;
        ULONG mf = MEMF_PUBLIC | MEMF_CLEAR;
        ULONG memtype = raw & 0xC0000000UL;
        if (memtype == 0x40000000UL)
        {
            mf = MEMF_CHIP | MEMF_CLEAR;
        }
        else if (memtype == 0xC0000000UL)
        {
            p++; // explicit memflags longword - skip, allocate public
        }

        ULONG bytes = longs * 4;
        UBYTE* a = (UBYTE*)AllocMem(bytes + 8, mf);
        if (!a)
        {
            // Out of memory mid-load - only realistic at coldstart OOM, where the
            // boot is failing anyway. Bail and leak; the unwind isn't worth it.
            kprintf("a314scsi: hunk alloc failed\n");
            return 0;
        }
        seg[i] = a;
        segBytes[i] = bytes;
    }

    // Parse the hunks into the allocated segments.
    ULONG idx = 0;
    while (p < end && idx < numHunks)
    {
        ULONG htype = *p++ & 0x3FFFFFFFUL;

        if (htype == HUNK_CODE || htype == HUNK_DATA)
        {
            ULONG longs = *p++;
            memcpy((UBYTE*)seg[idx] + 8, p, longs * 4);
            p += longs;
        }
        else if (htype == HUNK_BSS)
        {
            p++; // size (already allocated and cleared)
        }
        else if (htype == HUNK_RELOC32)
        {
            for (;;)
            {
                ULONG cnt = *p++;
                if (!cnt)
                {
                    break;
                }
                ULONG refHunk = *p++;
                if (refHunk >= numHunks)
                {
                    // Malformed LSEG: don't index seg[] out of bounds. Skip this
                    // block's offsets and keep the parse position valid.
                    kprintf("a314scsi: reloc refHunk %ld >= %ld\n", refHunk, numHunks);
                    p += cnt;
                    continue;
                }
                UBYTE* thisData = (UBYTE*)seg[idx] + 8;
                ULONG addBase = (ULONG)((UBYTE*)seg[refHunk] + 8);
                for (ULONG j = 0; j < cnt; j++)
                {
                    ULONG offset = *p++;
                    UBYTE* loc = thisData + offset; // may be odd -> byte-wise
                    ULONG v = ((ULONG)loc[0] << 24) | ((ULONG)loc[1] << 16) | ((ULONG)loc[2] << 8) | loc[3];
                    v += addBase;
                    loc[0] = (UBYTE)(v >> 24);
                    loc[1] = (UBYTE)(v >> 16);
                    loc[2] = (UBYTE)(v >> 8);
                    loc[3] = (UBYTE)v;
                }
            }
        }
        else if (htype == HUNK_SYMBOL)
        {
            // { namelen-longs, name..., value } * , terminated by 0
            while (p < end && *p)
            {
                ULONG n = *p++;
                p += n;  // name
                p++;     // value
            }
            p++; // terminating 0
        }
        else if (htype == HUNK_DEBUG || htype == HUNK_NAME)
        {
            ULONG n = *p++;
            p += n;
        }
        else if (htype == HUNK_END)
        {
            idx++;
        }
        else
        {
            kprintf("a314scsi: bad hunk type %lx\n", htype);
            break;
        }
    }

    // Link the SegList and build the result BPTR (points at each node's next field).
    for (ULONG i = 0; i < numHunks; i++)
    {
        ULONG* node = (ULONG*)seg[i];
        node[0] = segBytes[i] + 8;
        node[1] = (i + 1 < numHunks) ? (ULONG)MKBADDR((UBYTE*)seg[i + 1] + 4) : 0;
    }

    BPTR result = MKBADDR((UBYTE*)seg[0] + 4);

    FreeMem(seg, numHunks * sizeof(APTR));
    FreeMem(segBytes, numHunks * sizeof(ULONG));
    FreeMem(data, len);

    kprintf("a314scsi: loaded seglist %ld hunks -> BPTR %lx\n", numHunks, (ULONG)result);
    return result;
}
