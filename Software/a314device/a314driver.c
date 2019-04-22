/*
 * Copyright (c) 2018 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/execbase.h>

#include <hardware/intbits.h>

#include <libraries/dos.h>

#include <proto/exec.h>

#include <string.h>

#include "a314.h"

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

struct Task *CreateTask(char *name, long priority, char *initialPC, unsigned long stacksize)
{
	char *stack = AllocMem(stacksize, MEMF_CLEAR);
	if (stack == NULL)
		return NULL;

	struct Task *tc = AllocMem(sizeof(struct Task), MEMF_CLEAR | MEMF_PUBLIC);
	if (tc == NULL)
	{
		FreeMem(stack, stacksize);
		return NULL;
	}

	tc->tc_Node.ln_Type = NT_TASK;
	tc->tc_Node.ln_Pri = priority;
	tc->tc_Node.ln_Name = name;
	tc->tc_SPLower = (APTR)stack;
	tc->tc_SPUpper = (APTR)(stack + stacksize);
	tc->tc_SPReg = (APTR)(stack + stacksize);

	AddTask(tc, initialPC, 0);
	return tc;
}

#define DEBUG 0
//#define debug_printf(...) do { if (DEBUG) fprintf(stdout, __VA_ARGS__); } while (0)
#define debug_printf(...)

char device_name[] = A314_NAME;
char id_string[] = A314_NAME " 1.0 (25 Aug 2018)";

struct MyDevice
{
	struct Library md_Lib;
	BPTR md_SegList;
};

struct ExecBase *SysBase;



ULONG a314_membase = 0;

struct MemChunkList
{
	struct MemChunk *first;
	struct MemChunk *last;
	ULONG free;
};

void add_chunk(struct MemChunkList *l, struct MemChunk *mc)
{
	if (l->first == NULL)
		l->first = mc;
	else
		l->last->mc_Next = mc;
	l->last = mc;
	l->free += mc->mc_Bytes;
}

BOOL overlap(struct MemHeader *mh, ULONG lower, ULONG upper)
{
	return lower < (ULONG)(mh->mh_Upper) && (ULONG)(mh->mh_Lower) < upper;
}

BOOL fix_memory()
{
	Forbid();

	struct List *memlist = &(SysBase->MemList);
	struct Node *node;
	struct MemHeader *mh;

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (mh->mh_Attributes & MEMF_A314)
		{
			a314_membase = (ULONG)(mh->mh_Lower) & ~(512*1024 - 1);
			Permit();
			return TRUE;
		}
	}

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (overlap(mh, 0xc00000, 0xc80000))
		{
			Remove((struct Node *)mh);
			mh->mh_Node.ln_Pri = -20;
			mh->mh_Attributes |= MEMF_A314;
			AddTail(memlist, (struct Node*)mh);
			a314_membase = 0xc00000;
			Permit();
			return TRUE;
		}
	}

	struct MemHeader *chip_mh = NULL;

	for (node = memlist->lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		mh = (struct MemHeader *)node;
		if (overlap(mh, 0x0, 0x100000))
		{
			chip_mh = mh;
			break;
		}
	}

	if (chip_mh == NULL || (ULONG)(chip_mh->mh_Upper) <= 0x80000)
	{
		Permit();
		return FALSE;
	}

	if ((ULONG)(chip_mh->mh_Upper) > 0x100000)
		a314_membase = 0x100000;
	else
		a314_membase = 0x80000;

	mh = (struct MemHeader *)AllocMem(sizeof(struct MemHeader), MEMF_PUBLIC | MEMF_CLEAR);

	struct MemChunk *mc = chip_mh->mh_First;

	struct MemChunkList ol = {NULL, NULL, 0};
	struct MemChunkList nl = {NULL, NULL, 0};

	while (mc != NULL)
	{
		struct MemChunk *next_chunk = mc->mc_Next;
		mc->mc_Next = NULL;

		ULONG lower = (ULONG)mc;
		ULONG upper = lower + mc->mc_Bytes;

		if (upper <= a314_membase)
			add_chunk(&ol, mc);
		else if (a314_membase <= lower)
			add_chunk(&nl, mc);
		else
		{
			mc->mc_Bytes = a314_membase - lower;
			add_chunk(&ol, mc);

			struct MemChunk *new_chunk = (struct MemChunk *)a314_membase;
			new_chunk->mc_Next = NULL;
			new_chunk->mc_Bytes = upper - a314_membase;
			add_chunk(&nl, new_chunk);
		}
		mc = next_chunk;
	}

	mh->mh_Node.ln_Type = NT_MEMORY;
	mh->mh_Node.ln_Pri = -20;
	mh->mh_Node.ln_Name = chip_mh->mh_Node.ln_Name; // Use a custom name?
	mh->mh_Attributes = chip_mh->mh_Attributes | MEMF_A314;

	chip_mh->mh_First = ol.first;
	mh->mh_First = nl.first;

	mh->mh_Lower = (APTR)a314_membase;
	mh->mh_Upper = chip_mh->mh_Upper;
	chip_mh->mh_Upper = (APTR)a314_membase;

	chip_mh->mh_Free = ol.free;
	mh->mh_Free = nl.free;

	AddTail(memlist, (struct Node*)mh);

	Permit();
	return TRUE;
}













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

struct ComArea *ca;

int used_in_r2a()
{
	return (ca->r2a_tail - ca->r2a_head) & 255;
}

int used_in_a2r()
{
	return (ca->a2r_tail - ca->a2r_head) & 255;
}

BOOL room_in_a2r(int len)
{
	return used_in_a2r() + 3 + len <= 255;
}

void append_a2r_packet(UBYTE type, UBYTE stream_id, UBYTE length, UBYTE *data)
{
	UBYTE index = ca->a2r_tail;
	ca->a2r_buffer[index++] = length;
	ca->a2r_buffer[index++] = type;
	ca->a2r_buffer[index++] = stream_id;
	for (int i = 0; i < (int)length; i++)
		ca->a2r_buffer[index++] = *data++;
	ca->a2r_tail = index;
}







#define CLOCK_PORT_ADDRESS	0xdc0000

void write_cp_nibble(int index, UBYTE value)
{
	volatile UBYTE *p = (UBYTE *)CLOCK_PORT_ADDRESS;
	p += 4 * index + 3;
	*p = value & 0xf;
}

UBYTE read_cp_nibble(int index)
{
	volatile UBYTE *p = (UBYTE *)CLOCK_PORT_ADDRESS;
	p += 4 * index + 3;
	return *p & 0xf;
}

void write_base_address(void *p)
{
	ULONG ba = (ULONG)p - a314_membase;
	ba |= 1;

	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);

	write_cp_nibble(0, 0);

	for (int i = 4; i >= 0; i--)
	{
		ULONG v = (ba >> (i * 4)) & 0xf;
		write_cp_nibble(i, (UBYTE)v);
	}

	write_cp_nibble(13, prev_regd);
	Enable();
}


void fix_address_mapping()
{
	// Only looking at VPOSR Agnus identification at this point.
	// Could add more dynamic identification of address mapping.

	UWORD vposr = *(UWORD *)0xdff004;
	UWORD agnus = (vposr & 0x7f00) >> 8;

	UBYTE swap = (agnus == 0x00 || agnus == 0x10) ? 0x1 : 0x0;

	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	write_cp_nibble(11, swap);
	write_cp_nibble(13, prev_regd);
	Enable();
}




// Addresses to variables in CMEM.
#define R_EVENTS_ADDRESS	12
#define R_ENABLE_ADDRESS	13
#define A_EVENTS_ADDRESS	14
#define A_ENABLE_ADDRESS	15

// Events that are communicated via IRQ from Amiga to Raspberry.
#define R_EVENT_A2R_TAIL	1
#define R_EVENT_R2A_HEAD	2
#define R_EVENT_BASE_ADDRESS	4

// Events that are communicated from Raspberry to Amiga.
#define A_EVENT_R2A_TAIL        1
#define A_EVENT_A2R_HEAD        2

void set_rasp_irq(UBYTE events)
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	write_cp_nibble(R_EVENTS_ADDRESS, events);
	write_cp_nibble(13, prev_regd);
	Enable();
}

UBYTE read_a_events()
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	UBYTE events = read_cp_nibble(A_EVENTS_ADDRESS);
	write_cp_nibble(13, prev_regd);
	Enable();
	return events;
}

void write_a_enable(UBYTE enable)
{
	Disable();
	UBYTE prev_regd = read_cp_nibble(13);
	write_cp_nibble(13, prev_regd | 8);
	write_cp_nibble(A_ENABLE_ADDRESS, enable);
	write_cp_nibble(13, prev_regd);
	Enable();
}


extern void IntServer();
struct Interrupt vertb_interrupt;
struct Interrupt ports_interrupt;




// Used to store received data until application asks for it using a A314_READ.
struct QueuedData
{
	struct QueuedData *next;
	UWORD length;
	UBYTE data[];
};

// Socket flags, these are bit masks, can have many of them.
#define SOCKET_APP_EOS_RECV		0x0004
#define SOCKET_RAS_EOS_RECV		0x0008
#define SOCKET_RESET			0x0010
#define SOCKET_SHOULD_SEND_RESET	0x0020
#define SOCKET_APP_READ_EOS		0x0040
#define SOCKET_IN_SEND_QUEUE		0x0080

struct Socket
{
	struct MinNode node;

	void *sig_task;
	ULONG socket;

	UBYTE stream_id;
	UBYTE pad1;

	UWORD flags;

	struct A314_IORequest *pending_connect;
	struct A314_IORequest *pending_read;
	struct A314_IORequest *pending_write;

	struct Socket *next_in_send_queue;
	UWORD send_queue_required_length;

	// Data that is received on the stream, but the application didn't read yet.
	struct QueuedData *rq_head;
	struct QueuedData *rq_tail;
};

struct List active_sockets;








struct Socket *find_socket(void *sig_task, ULONG socket)
{
	for (struct Node *node = active_sockets.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct Socket *s = (struct Socket *)node;
		if (s->sig_task == sig_task && s->socket == socket)
			return s;
	}
	return NULL;
}

struct Socket *find_socket_by_stream_id(UBYTE stream_id)
{
	for (struct Node *node = active_sockets.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ)
	{
		struct Socket *s = (struct Socket *)node;
		if (s->stream_id == stream_id)
			return s;
	}
	return NULL;
}











struct Socket *sq_head = NULL;
struct Socket *sq_tail = NULL;

void add_to_send_queue(struct Socket *s)
{
	s->next_in_send_queue = NULL;

	if (sq_head == NULL)
		sq_head = s;
	else
		sq_tail->next_in_send_queue = s;
	sq_tail = s;

	s->flags |= SOCKET_IN_SEND_QUEUE;
}

void remove_from_send_queue(struct Socket *s)
{
	if (s->flags & SOCKET_IN_SEND_QUEUE)
	{
		if (sq_head == s)
		{
			sq_head = s->next_in_send_queue;
			if (sq_head == NULL)
				sq_tail = NULL;
		}
		else
		{
			struct Socket *curr = sq_head;
			while (curr->next_in_send_queue != s)
				curr = curr->next_in_send_queue;

			curr->next_in_send_queue = s->next_in_send_queue;
			if (sq_tail == s)
				sq_tail = curr;
		}

		s->next_in_send_queue = NULL;
		s->flags &= ~SOCKET_IN_SEND_QUEUE;
	}
}
















void reset_socket(struct Socket *s, BOOL should_send)
{
	debug_printf("Called reset socket\n");

	// Release pending connect.
	if (s->pending_connect != NULL)
	{
		struct A314_IORequest *ior = s->pending_connect;
		ior->a314_Request.io_Error = A314_CONNECT_RESET;
		ReplyMsg((struct Message *)ior);
		debug_printf("Reply request 1\n");
		s->pending_connect = NULL;
	}

	// Release pending read.
	if (s->pending_read != NULL)
	{
		struct A314_IORequest *ior = s->pending_read;
		ior->a314_Length = 0;
		ior->a314_Request.io_Error = A314_READ_RESET;
		ReplyMsg((struct Message *)ior);
		debug_printf("Reply request 2\n");
		s->pending_read = NULL;
	}

	// Release pending write/eos.
	if (s->pending_write != NULL)
	{
		struct A314_IORequest *ior = s->pending_write;
		ior->a314_Length = 0;
		ior->a314_Request.io_Error = A314_WRITE_RESET; // A314_EOS_RESET == A314_WRITE_RESET
		ReplyMsg((struct Message *)ior);
		debug_printf("Reply request 3\n");
		s->pending_write = NULL;
	}

	// Free recevied data on socket.
	if (s->rq_head != NULL)
	{
		struct QueuedData *qd = s->rq_head;
		while (qd != NULL)
		{
			struct QueuedData *next = qd->next;
			FreeMem(qd, sizeof(struct QueuedData) + qd->length);
			qd = next;
		}
		s->rq_head = NULL;
		s->rq_tail = NULL;
	}

	remove_from_send_queue(s);

	// När SOCKET_RESET sätts så vet man att allt det ovanför är uppfyllt, dvs ingen pending operation, mottagarkön är tömd och frigjord, och står inte i sändkön.
	// Efter att denna flagga har satts så får man inte göra operationer pending eller köa data på mottagarkön.
	// Däremot så kan man sätta socketen på sändkön, om det är så att man ska skicka en PKT_RESET.
	s->flags |= SOCKET_RESET;

	// Om man ska skicka reset så sätter man flaggan SOCKET_SHOULD_SEND_RESET och ställer socketen i send queue.
	// Man får inte remove'a den så länge som den står i send queue.
	// Om socket står i send-queue för att skicka reset, och en PKT_RESET tas emot för den strömmen, då ska man plocka bort
	// från send-queue, och frigöra socket-structen.

	if (should_send)
	{
		if (sq_head == NULL && room_in_a2r(0))
		{
			append_a2r_packet(PKT_RESET, s->stream_id, 0, NULL);

			// Remove from list of active sockets.
			Remove((struct Node *)s);
			FreeMem(s, sizeof(struct Socket));
		}
		else
		{
			s->flags |= SOCKET_SHOULD_SEND_RESET;
			s->send_queue_required_length = 0;
			add_to_send_queue(s);
		}
	}
	else
	{
		// Remove from list of active sockets.
		Remove((struct Node *)s);
		FreeMem(s, sizeof(struct Socket));
	}
}













// När jag tar emot ett meddelande från com-arean så skriver jag det hit, för då slipper jag hantera ringbuffer problem.
UBYTE received_packet[256];

void handle_received_packet_r2a(UBYTE type, UBYTE stream_id, UBYTE length)
{
	struct Socket *s = find_socket_by_stream_id(stream_id);

	if (s != NULL && type == PKT_RESET)
	{
		debug_printf("Received a RESET packet from rasp\n");
		reset_socket(s, FALSE);
		return;
	}

	if (s == NULL || (s->flags & SOCKET_RESET))
	{
		// Bara ignorera detta meddelande.
		// Det enda som kan fungera utan en existerande ström är CONNECT, och vi hanterar inte det tsv.
		return;
	}

	if (type == PKT_CONNECT_RESPONSE)
	{
		debug_printf("Received a CONNECT RESPONSE packet from rasp\n");

		if (s->pending_connect == NULL)
			debug_printf("SERIOUS ERROR: received a CONNECT RESPONSE even though no connect was pending\n");
		else if (length != 1)
			debug_printf("SERIOUS ERROR: received a CONNECT RESPONSE whose length was not 1\n");
		else
		{
			UBYTE result = received_packet[0];
			if (result == 0)
			{
				struct A314_IORequest *ior = s->pending_connect;
				ior->a314_Request.io_Error = A314_CONNECT_OK;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 4\n");
				s->pending_connect = NULL;
			}
			else
			{
				struct A314_IORequest *ior = s->pending_connect;
				ior->a314_Request.io_Error = A314_CONNECT_UNKNOWN_SERVICE;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 5\n");
				s->pending_connect = NULL;

				reset_socket(s, FALSE);
			}
		}
	}
	else if (type == PKT_DATA)
	{
		debug_printf("Received a DATA packet from rasp\n");

		if (s->pending_read != NULL)
		{
			struct A314_IORequest *ior = s->pending_read;

			if (ior->a314_Length < length)
				reset_socket(s, TRUE);
			else
			{
				memcpy(ior->a314_Buffer, received_packet, length);
				ior->a314_Length = length;
				ior->a314_Request.io_Error = A314_READ_OK;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 6\n");
				s->pending_read = NULL;
			}
		}
		else
		{
			struct QueuedData *qd = (struct QueuedData *)AllocMem(sizeof(struct QueuedData) + length, 0);
			qd->next = NULL,
			qd->length = length;
			memcpy(qd->data, received_packet, length);

			if (s->rq_head == NULL)
				s->rq_head = qd;
			else
				s->rq_tail->next = qd;
			s->rq_tail = qd;
		}
	}
	else if (type == PKT_EOS)
	{
		debug_printf("Received a EOS packet from rasp\n");

		s->flags |= SOCKET_RAS_EOS_RECV;

		if (s->pending_read != NULL)
		{
			// Om en read är pending så måste receive queue vara tom.

			struct A314_IORequest *ior = s->pending_read;
			ior->a314_Length = 0;
			ior->a314_Request.io_Error = A314_READ_EOS;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 7\n");
			s->pending_read = NULL;

			s->flags |= SOCKET_APP_READ_EOS;

			if ((s->flags & SOCKET_APP_EOS_RECV) && s->pending_write == NULL)
			{
				reset_socket(s, FALSE);
			}
		}
	}
}

void handle_packets_received_r2a()
{
	while (used_in_r2a() != 0)
	{
		UBYTE index = ca->r2a_head;

		UBYTE len = ca->r2a_buffer[index++];
		UBYTE type = ca->r2a_buffer[index++];
		UBYTE stream_id = ca->r2a_buffer[index++];

		for (int i = 0; i < len; i++)
			received_packet[i] = ca->r2a_buffer[index++];

		ca->r2a_head = index;

		handle_received_packet_r2a(type, stream_id, len);
	}
}









void handle_room_in_a2r()
{
	BOOL has_enough_space = TRUE;

	while (sq_head != NULL && has_enough_space)
	{
		struct Socket *s = sq_head;

		if (s->pending_connect != NULL)
		{
			struct A314_IORequest *ior = s->pending_connect;

			int len = ior->a314_Length;

			if (room_in_a2r(len))
			{
				remove_from_send_queue(s);

				append_a2r_packet(PKT_CONNECT, s->stream_id, (UBYTE)len, ior->a314_Buffer);
			}
			else
				has_enough_space = FALSE;
		}
		else if (s->pending_write != NULL)
		{
			struct A314_IORequest *ior = s->pending_write;

			int len = ior->a314_Length;

			if (room_in_a2r(len))
			{
				s->pending_write = NULL;

				remove_from_send_queue(s);

				if (ior->a314_Request.io_Command == A314_WRITE)
				{
					append_a2r_packet(PKT_DATA, s->stream_id, (UBYTE)len, ior->a314_Buffer);

					ior->a314_Request.io_Error = A314_WRITE_OK;
					ReplyMsg((struct Message *)ior);
					debug_printf("Reply request 8\n");
				}
				else // A314_EOS
				{
					append_a2r_packet(PKT_EOS, s->stream_id, 0, NULL);

					ior->a314_Request.io_Error = A314_EOS_OK;
					ReplyMsg((struct Message *)ior);
					debug_printf("Reply request 9\n");

					if (s->flags & SOCKET_APP_READ_EOS)
						reset_socket(s, FALSE);
				}
			}
			else
				has_enough_space = FALSE;
		}
		else if (s->flags & SOCKET_SHOULD_SEND_RESET)
		{
			if (room_in_a2r(0))
			{
				remove_from_send_queue(s);

				append_a2r_packet(PKT_RESET, s->stream_id, 0, NULL);
			}
			else
				has_enough_space = FALSE;
		}
	}
}

















UBYTE next_stream_id = 1;

// Bitmask för 128 strömmar => 16 bytes
// Markera varje plats med en etta om den är upptagen.
// Kan snabbt leta upp en ledig ström genom att kolla var det finns nollor.
// Ström 0 är alltid markerad som tagen.

UBYTE allocate_stream_id()
{

	// Hitta ett stream id för denna strömmen.
	// Gör en bitmap för att visa vilka ström-id'n som är tagna.
	// Just nu så loopar man bara, vilket kanske är okej, men det är inte bra.
	// Ett felmeddelande är att alla stream id'n är tagna.
	// TODO:

	UBYTE stream_id = next_stream_id;
	next_stream_id += 2;
	return stream_id;
}

void free_stream_id()
{
}

void handle_received_app_request(struct A314_IORequest *ior)
{
	struct Socket *s = find_socket(ior->a314_Request.io_Message.mn_ReplyPort->mp_SigTask, ior->a314_Socket);

	if (ior->a314_Request.io_Command == A314_CONNECT)
	{
		debug_printf("Received a CONNECT request from application\n");
		if (s != NULL)
		{
			ior->a314_Request.io_Error = A314_CONNECT_SOCKET_IN_USE;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 10\n");
		}
		else if (ior->a314_Length + 3 > 255)
		{
			ior->a314_Request.io_Error = A314_CONNECT_RESET;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 11\n");
		}
		else
		{
			s = (struct Socket *)AllocMem(sizeof(struct Socket), MEMF_CLEAR);
			s->sig_task = ior->a314_Request.io_Message.mn_ReplyPort->mp_SigTask;
			s->socket = ior->a314_Socket;

			AddTail(&active_sockets, (struct Node *)s);

			s->stream_id = allocate_stream_id();

			s->pending_connect = ior;
			s->flags = 0;

			int len = ior->a314_Length;
			if (sq_head == NULL && room_in_a2r(len))
			{
				append_a2r_packet(PKT_CONNECT, s->stream_id, (UBYTE)len, ior->a314_Buffer);
			}
			else
			{
				s->send_queue_required_length = len;
				add_to_send_queue(s);
			}
		}
	}
	else if (ior->a314_Request.io_Command == A314_READ)
	{
		debug_printf("Received a READ request from application\n");
		if (s == NULL || (s->flags &  SOCKET_RESET))
		{
			ior->a314_Length = 0;
			ior->a314_Request.io_Error = A314_READ_RESET;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 12\n");
		}
		else
		{
			if (s->pending_connect != NULL || s->pending_read != NULL)
			{
				reset_socket(s, TRUE);

				ior->a314_Length = 0;
				ior->a314_Request.io_Error = A314_READ_RESET;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 13\n");
			}
			else
			{
				struct QueuedData *qd = s->rq_head;

				if (qd != NULL)
				{
					int len = qd->length;

					if (ior->a314_Length < len)
					{
						reset_socket(s, TRUE);

						ior->a314_Length = 0;
						ior->a314_Request.io_Error = A314_READ_RESET;
						ReplyMsg((struct Message *)ior);
						debug_printf("Reply request 14\n");
					}
					else
					{
						s->rq_head = qd->next;
						if (s->rq_head == NULL)
							s->rq_tail = NULL;

						memcpy(ior->a314_Buffer, qd->data, len);
						FreeMem(qd, sizeof(struct QueuedData) + len);

						ior->a314_Length = len;
						ior->a314_Request.io_Error = A314_READ_OK;
						ReplyMsg((struct Message *)ior);
						debug_printf("Reply request 15\n");
					}
				}
				else
				{
					if (s->flags & SOCKET_RAS_EOS_RECV)
					{
						ior->a314_Length = 0;
						ior->a314_Request.io_Error = A314_READ_EOS;
						ReplyMsg((struct Message *)ior);
						debug_printf("Reply request 16\n");

						s->flags |= SOCKET_APP_READ_EOS;

						if ((s->flags & SOCKET_APP_EOS_RECV) && s->pending_write == NULL)
							reset_socket(s, FALSE);
					}
					else
						s->pending_read = ior;
				}
			}
		}
	}
	else if (ior->a314_Request.io_Command == A314_WRITE)
	{
		debug_printf("Received a WRITE request from application\n");
		if (s == NULL || (s->flags &  SOCKET_RESET))
		{
			ior->a314_Length = 0;
			ior->a314_Request.io_Error = A314_WRITE_RESET;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 17\n");
		}
		else
		{
			int len = ior->a314_Length;
			if (s->pending_connect != NULL || s->pending_write != NULL || (s->flags & SOCKET_APP_EOS_RECV) || len + 3 > 255)
			{
				reset_socket(s, TRUE);
				ior->a314_Length = 0;
				ior->a314_Request.io_Error = A314_WRITE_RESET;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 18\n");
			}
			else
			{
				if (sq_head == NULL && room_in_a2r(len))
				{
					append_a2r_packet(PKT_DATA, s->stream_id, (UBYTE)len, ior->a314_Buffer);

					ior->a314_Request.io_Error = A314_WRITE_OK;
					ReplyMsg((struct Message *)ior);
					debug_printf("Reply request 19\n");
				}
				else
				{
					s->pending_write = ior;
					s->send_queue_required_length = len;
					add_to_send_queue(s);
				}
			}
		}
	}
	else if (ior->a314_Request.io_Command == A314_EOS)
	{
		debug_printf("Received an EOS request from application\n");
		if (s == NULL || (s->flags &  SOCKET_RESET))
		{
			ior->a314_Request.io_Error = A314_EOS_RESET;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 20\n");
		}
		else
		{
			if (s->pending_connect != NULL || s->pending_write != NULL || (s->flags & SOCKET_APP_EOS_RECV))
			{
				reset_socket(s, TRUE);
				ior->a314_Length = 0;
				ior->a314_Request.io_Error = A314_EOS_RESET;
				ReplyMsg((struct Message *)ior);
				debug_printf("Reply request 21\n");
			}
			else
			{
				s->flags |= SOCKET_APP_EOS_RECV;

				if (sq_head == NULL && room_in_a2r(0))
				{
					append_a2r_packet(PKT_EOS, s->stream_id, 0, NULL);

					ior->a314_Request.io_Error = A314_EOS_OK;
					ReplyMsg((struct Message *)ior);
					debug_printf("Reply request 22\n");
				}
				else
				{
					s->pending_write = ior;
					s->send_queue_required_length = 0;
					add_to_send_queue(s);
				}
			}
		}
	}
	else if (ior->a314_Request.io_Command == A314_RESET)
	{
		debug_printf("Received a RESET request from application\n");
		if (s == NULL || (s->flags &  SOCKET_RESET))
		{
			ior->a314_Request.io_Error = A314_RESET_OK;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 23\n");
		}
		else
		{
			reset_socket(s, TRUE);
			ior->a314_Request.io_Error = A314_RESET_OK;
			ReplyMsg((struct Message *)ior);
			debug_printf("Reply request 24\n");
		}
	}
	else
	{
		ior->a314_Request.io_Error = IOERR_NOCMD;
		ReplyMsg((struct Message *)ior);
	}
}


#define SIGB_INT SIGBREAKB_CTRL_E
#define SIGB_MSGPORT SIGBREAKB_CTRL_F

#define SIGF_INT SIGBREAKF_CTRL_E
#define SIGF_MSGPORT SIGBREAKF_CTRL_F

struct MsgPort task_mp;
struct Task *task;

void task_main()
{
	write_a_enable(0);
	read_a_events();

	write_base_address(ca);

	set_rasp_irq(R_EVENT_BASE_ADDRESS);


	vertb_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	vertb_interrupt.is_Node.ln_Pri = -60;
	vertb_interrupt.is_Node.ln_Name = device_name;
	vertb_interrupt.is_Data = (APTR)task;
	vertb_interrupt.is_Code = IntServer;

	AddIntServer(INTB_VERTB, &vertb_interrupt);


	ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	ports_interrupt.is_Node.ln_Pri = 0;
	ports_interrupt.is_Node.ln_Name = device_name;
	ports_interrupt.is_Data = (APTR)task;
	ports_interrupt.is_Code = IntServer;

	AddIntServer(INTB_PORTS, &ports_interrupt);


	write_a_enable(A_EVENT_R2A_TAIL);

	while (1)
	{
		debug_printf("Waiting for signal\n");

		ULONG signal = Wait(SIGF_MSGPORT | SIGF_INT);

		UBYTE prev_a2r_tail = ca->a2r_tail;
		UBYTE prev_r2a_head = ca->r2a_head;

		if (signal & SIGF_MSGPORT)
		{
			write_a_enable(0);

			struct Message *msg;
			while (msg = GetMsg(&task_mp))
				handle_received_app_request((struct A314_IORequest *)msg);
		}

		UBYTE a_enable = 0;
		while (a_enable == 0)
		{
			handle_packets_received_r2a();
			handle_room_in_a2r();

			UBYTE r_events = 0;
			if (ca->a2r_tail != prev_a2r_tail)
				r_events |= R_EVENT_A2R_TAIL;
			if (ca->r2a_head != prev_r2a_head)
				r_events |= R_EVENT_R2A_HEAD;

			Disable();
			UBYTE prev_regd = read_cp_nibble(13);
			write_cp_nibble(13, prev_regd | 8);
			read_cp_nibble(A_EVENTS_ADDRESS);

			if (ca->r2a_head == ca->r2a_tail)
			{
				if (sq_head == NULL)
					a_enable = A_EVENT_R2A_TAIL;
				else if (!room_in_a2r(sq_head->send_queue_required_length))
					a_enable = A_EVENT_R2A_TAIL | A_EVENT_A2R_HEAD;

				if (a_enable != 0)
				{
					write_cp_nibble(A_ENABLE_ADDRESS, a_enable);
					if (r_events != 0)
						write_cp_nibble(R_EVENTS_ADDRESS, r_events);
				}
			}

			write_cp_nibble(13, prev_regd);
			Enable();
		}
	}

	debug_printf("Shutting down\n");

	RemIntServer(INTB_PORTS, &ports_interrupt);
	RemIntServer(INTB_VERTB, &vertb_interrupt);
	FreeMem(ca, sizeof(struct ComArea));

	// Stack and task structure should be reclaimed.
}

BOOL task_start()
{
	if (!fix_memory())
		return FALSE;

	fix_address_mapping();

	ca = (struct ComArea *)AllocMem(sizeof(struct ComArea), MEMF_A314 | MEMF_CLEAR);
	if (ca == NULL)
	{
		debug_printf("Unable to allocate A314 memory for com area\n");
		return FALSE;
	}

	task = CreateTask(device_name, 0, (void *)task_main, 1024);
	if (task == NULL)
	{
		debug_printf("Unable to create task\n");
		FreeMem(ca, sizeof(struct ComArea));
		return FALSE;
	}

	task_mp.mp_Node.ln_Name = device_name;
	task_mp.mp_Node.ln_Pri = 0;
	task_mp.mp_Node.ln_Type = NT_MSGPORT;
	task_mp.mp_Flags = PA_SIGNAL;
	task_mp.mp_SigBit = SIGB_MSGPORT;
	task_mp.mp_SigTask = task;

	NewList(&(task_mp.mp_MsgList));

	NewList(&active_sockets);

	return TRUE;
}

struct MyDevice *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct MyDevice *dev)
{
	SysBase = *(struct ExecBase **)4;

	// Vi anropas från InitResident i initializers.asm.
	// Innan vi har kommit hit så har MakeLibrary körts.

	dev->md_Lib.lib_Node.ln_Type = NT_DEVICE;
	dev->md_Lib.lib_Node.ln_Name = device_name;
	dev->md_Lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	dev->md_Lib.lib_Version = 1;
	dev->md_Lib.lib_Revision = 0;
	dev->md_Lib.lib_IdString = (APTR)id_string;

	dev->md_SegList = seg_list;

	// Efter vi returnerar så körs AddDevice.
	return dev;
}

BPTR expunge(__reg("a6") struct MyDevice *dev)
{
	// Det finns inget sätt att ladda ur a314.device för närvarande.
	if (TRUE) //dev->md_Lib.lib_OpenCnt != 0)
	{
		dev->md_Lib.lib_Flags |= LIBF_DELEXP;
		return 0;
	}

	/*
	BPTR seg_list = dev->md_SegList;
	Remove(&dev->md_Lib.lib_Node);
	FreeMem((char *)dev - dev->md_Lib.lib_NegSize, dev->md_Lib.lib_NegSize + dev->md_Lib.lib_PosSize);
	return seg_list;
	*/
}

BOOL running = FALSE;

void open(__reg("a6") struct MyDevice *dev, __reg("a1") struct A314_IORequest *ior, __reg("d0") ULONG unitnum, __reg("d1") ULONG flags)
{
	dev->md_Lib.lib_OpenCnt++;

	if (dev->md_Lib.lib_OpenCnt == 1 && !running)
	{
		if (!task_start())
		{
			ior->a314_Request.io_Error = IOERR_OPENFAIL;
			ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
			dev->md_Lib.lib_OpenCnt--;
			return;
		}
		running = TRUE;
	}

	ior->a314_Request.io_Error = 0;
	ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
}

BPTR close(__reg("a6") struct MyDevice *dev, __reg("a1") struct A314_IORequest *ior)
{
	ior->a314_Request.io_Device = NULL;
	ior->a314_Request.io_Unit = NULL;

	dev->md_Lib.lib_OpenCnt--;

	if (dev->md_Lib.lib_OpenCnt == 0 && (dev->md_Lib.lib_Flags & LIBF_DELEXP))
		return expunge(dev);

	return 0;
}

void begin_io(__reg("a6") struct MyDevice *dev, __reg("a1") struct A314_IORequest *ior)
{
	PutMsg(&task_mp, (struct Message *)ior);
	ior->a314_Request.io_Flags &= ~IOF_QUICK;
}

ULONG abort_io(__reg("a6") struct MyDevice *dev, __reg("a1") struct A314_IORequest *ior)
{
	// No-operation.
	return IOERR_NOCMD;
}

ULONG get_a314_membase(__reg("a6") struct MyDevice *dev)
{
	return a314_membase;
}

ULONG device_vectors[] =
{
	(ULONG)open,
	(ULONG)close,
	(ULONG)expunge,
	0,
	(ULONG)begin_io,
	(ULONG)abort_io,
	(ULONG)get_a314_membase,
	-1,
};

ULONG auto_init_tables[] =
{
	sizeof(struct MyDevice),
	(ULONG)device_vectors,
	0,
	(ULONG)init_device,
};
