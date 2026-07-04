#ifndef A314SCSI_PROTOCOL_H
#define A314SCSI_PROTOCOL_H

#include <exec/types.h>

// Wire protocol between a314scsi.device (Amiga) and the host "scsi" service.
//
// A SCSI CDB is tunneled to the host; bulk data is NOT inlined but moved
// through a314 shared memory at 'address' (an a314-space address).
//
// Python struct formats (host side):
//   request : ">BBBB16sBBHII"  (32 bytes)
//   response: ">BBBBBBHI"      (12 bytes)

#define A314SCSI_CMD_REQ    1   // Amiga -> host
#define A314SCSI_CMD_RES    2   // host  -> Amiga

#define A314SCSI_DIR_NONE   0
#define A314SCSI_DIR_READ   1   // device -> host: host writes data into a314 mem
#define A314SCSI_DIR_WRITE  2   // host -> device: host reads data out of a314 mem

#define A314SCSI_MAX_CDB 16

#pragma pack(push, 1)

struct A314ScsiReq
{
    UBYTE kind;                  // A314SCSI_CMD_REQ         @0
    UBYTE unit;                  // drive/unit number        @1
    UBYTE cdb_len;               // 6/10/12/16               @2
    UBYTE reserved0;             //                          @3
    UBYTE cdb[A314SCSI_MAX_CDB]; //                          @4
    UBYTE direction;             // A314SCSI_DIR_*           @20
    UBYTE pad;                   //                          @21
    UWORD reserved1;             //                          @22
    ULONG data_length;           // expected transfer bytes  @24
    ULONG address;               // a314-space data address  @28
}; // sizeof == 32

struct A314ScsiRes
{
    UBYTE kind;          // A314SCSI_CMD_RES           @0
    UBYTE scsi_status;   // 0=GOOD, 2=CHECK CONDITION  @1
    UBYTE sense_key;     //                            @2
    UBYTE asc;           //                            @3
    UBYTE ascq;          //                            @4
    UBYTE pad;           //                            @5
    UWORD reserved;      //                            @6
    ULONG actual_length; //                            @8
}; // sizeof == 12

#pragma pack(pop)

// SCSI status byte values.
#define SCSI_STATUS_GOOD            0x00
#define SCSI_STATUS_CHECK_CONDITION 0x02
#define SCSI_STATUS_BUSY            0x08

#endif
