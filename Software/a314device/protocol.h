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

// The communication area, used to create the physical channel.
struct ComArea
{
	volatile UBYTE a2r_tail;
	volatile UBYTE r2a_head;
	volatile UBYTE r2a_tail;
	volatile UBYTE a2r_head;
	UBYTE a2r_buffer[256];
	UBYTE r2a_buffer[256];
};
