#ifndef __A314_PROTOCOL_H
#define __A314_PROTOCOL_H

#include <exec/types.h>

// Packet types that are sent across the physical channel.
#define PKT_DRIVER_STARTED		1
#define PKT_DRIVER_SHUTTING_DOWN	2
#define PKT_SETTINGS			3
#define PKT_CONNECT			4
#define PKT_CONNECT_RESPONSE		5
#define PKT_DATA			6
#define PKT_EOS				7
#define PKT_RESET			8

#pragma pack(push, 1)
struct PktHdr
{
	UBYTE length;
	UBYTE type;
	UBYTE stream_id;
};
#pragma pack(pop)

#define A2R_BASE 0
#define R2A_BASE 256
#define CAP_BASE 512

struct ComAreaPtrs
{
	volatile UBYTE r2a_tail;		// Written by Pi.
	volatile UBYTE a2r_head;		// Written by Pi.
	volatile UBYTE a2r_tail;		// Written by Amiga.
	volatile UBYTE r2a_head;		// Written by Amiga.
};

#endif /* __A314_PROTOCOL_H */
