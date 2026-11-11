#ifndef ALIB_H
#define ALIB_H

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/ports.h>

void NewList(struct List* list);
struct MsgPort* CreatePort(STRPTR name, LONG pri);
void DeletePort(struct MsgPort* port);

#endif
