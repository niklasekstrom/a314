#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/ports.h>

#include <proto/exec.h>

#include <string.h>

#include "alib.h"

#define SysBase (*(struct ExecBase**)4)

void NewList(struct List* list)
{
    list->lh_Head = (struct Node*)&list->lh_Tail;
    list->lh_Tail = NULL;
    list->lh_TailPred = (struct Node*)&list->lh_Head;
}

struct MsgPort* CreatePort(STRPTR name, LONG pri)
{
    BYTE sig = AllocSignal(-1);
    if (sig == -1)
        return NULL;

    struct MsgPort* port = (struct MsgPort*)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port)
    {
        FreeSignal(sig);
        return NULL;
    }

    port->mp_Node.ln_Name = (char*)name;
    port->mp_Node.ln_Pri = (BYTE)pri;
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_Flags = PA_SIGNAL;
    port->mp_SigBit = sig;
    port->mp_SigTask = FindTask(NULL);
    NewList(&port->mp_MsgList);

    if (name)
        AddPort(port);

    return port;
}

void DeletePort(struct MsgPort* port)
{
    if (!port)
        return;

    if (port->mp_Node.ln_Name)
        RemPort(port);

    FreeSignal(port->mp_SigBit);
    FreeMem(port, sizeof(struct MsgPort));
}
