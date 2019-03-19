#ifndef PROTO_A314_H
#define PROTO_A314_H

extern struct Library *A314Base;

ULONG __GetA314MemBase(__reg("a6") void *)="\tjsr\t-42(a6)";
#define GetA314MemBase() __GetA314MemBase(A314Base)

/*
void *__AllocA314Mem(__reg("a6") void *, __reg("d0") ULONG byteSize)="\tjsr\t-42(a6)";
#define AllocA314Mem(byteSize) __AllocA314(A314Base, (byteSize))

void __FreeA314Mem(__reg("a6") void *, __reg("a1") void *memoryBlock, __reg("d0") ULONG byteSize)="\tjsr\t-48(a6)";
#define FreeA314Mem(byteSize) __FreeA314(A314Base, (memoryBlock), (byteSize))
*/

#endif
