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

// TODO: These constants should be the same for TD/FE and CP.
// Must change on both sides at the same time.
#if defined(MODEL_TD)
#define CAP_BASE 0
#define A2R_BASE 4
#define R2A_BASE 260

struct ComAreaPtrs
{
	volatile UBYTE a2r_tail;
	volatile UBYTE r2a_head;
	volatile UBYTE r2a_tail;
	volatile UBYTE a2r_head;
};

// The communication area, used to create the physical channel.
struct ComArea
{
	struct ComAreaPtrs cap;
	UBYTE a2r_buffer[256];
	UBYTE r2a_buffer[256];
};
#elif defined(MODEL_FE)
#define HANDSHAKE_BASE 0
#define CAP_BASE 4
#define A2R_BASE 8
#define R2A_BASE 264

struct ComAreaPtrs
{
	volatile UBYTE a2r_tail;
	volatile UBYTE r2a_head;
	volatile UBYTE r2a_tail;
	volatile UBYTE a2r_head;
};

struct ComArea
{
	volatile UWORD lock_word;
	volatile UWORD interrupt;
	struct ComAreaPtrs cap;
	UBYTE a2r_buffer[256];
	UBYTE r2a_buffer[256];
};
#elif defined(MODEL_CP)
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
#endif

#endif /* __A314_PROTOCOL_H */
