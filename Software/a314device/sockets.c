#include <proto/exec.h>

#include "sockets.h"

struct List active_sockets;

struct Socket *sq_head = NULL;
struct Socket *sq_tail = NULL;

static UBYTE next_stream_id = 1;

extern void NewList(struct List *l);

void init_sockets()
{
	NewList(&active_sockets);
}

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

static UBYTE allocate_stream_id()
{
	// Bug: If all stream ids are allocated then this loop won't terminate.

	while (1)
	{
		UBYTE stream_id = next_stream_id;
		next_stream_id += 2;
		if (find_socket_by_stream_id(stream_id) == NULL)
			return stream_id;
	}
}

static void free_stream_id(UBYTE stream_id)
{
	// Currently do nothing.
	// Could speed up allocate_stream_id using a bitmap?
}

struct Socket *create_socket(struct Task *task, ULONG id)
{
	struct Socket *s = (struct Socket *)AllocMem(sizeof(struct Socket), MEMF_CLEAR);
	s->sig_task = task;
	s->socket = id;
	s->stream_id = allocate_stream_id();
	AddTail(&active_sockets, (struct Node *)s);
	return s;
}

void delete_socket(struct Socket *s)
{
	Remove((struct Node *)s);
	free_stream_id(s->stream_id);
	FreeMem(s, sizeof(struct Socket));
}

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
