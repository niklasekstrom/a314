#ifndef PROTO_CLOCKPORT_H
#define PROTO_CLOCKPORT_H

#include "lib_clockport.h"

extern struct Library *ClockportBase;

struct ClockportDescriptor *__FindClockport(__reg("a6") void *, __reg("a0") struct ClockportDescriptor *, __reg("a1") char *)="\tjsr\t-30(a6)";
#define FindClockport(old, device) __FindClockport(ClockportBase, old, device)

int __AllocateClockport(__reg("a6") void *, __reg("a0") struct ClockportDescriptor *)="\tjsr\t-36(a6)";
#define AllocateClockport(cd) __AllocateClockport(ClockportBase, cd)

int __FreeClockport(__reg("a6") void *, __reg("a0") struct ClockportDescriptor *)="\tjsr\t-42(a6)";
#define FreeClockport(cd) __FreeClockport(ClockportBase, cd)

#endif /* PROTO_CLOCKPORT_H */
