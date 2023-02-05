#ifndef PROTO_A314_H
#define PROTO_A314_H

extern struct Library *A314Base;

ULONG __TranslateAddressA314(__reg("a6") void *, __reg("a0") void *)="\tjsr\t-42(a6)";
#define TranslateAddressA314(address) __TranslateAddressA314(A314Base, address)

ULONG __AllocMemA314(__reg("a6") void *, __reg("d0") ULONG)="\tjsr\t-48(a6)";
#define AllocMemA314(length) __AllocMemA314(A314Base, length)

void __FreeMemA314(__reg("a6") void *, __reg("d0") ULONG, __reg("d1") ULONG)="\tjsr\t-54(a6)";
#define FreeMemA314(address, length) __FreeMemA314(A314Base, address, length)

void __WriteMemA314(__reg("a6") void *, __reg("d0") ULONG, __reg("a0") void *, __reg("d1") ULONG)="\tjsr\t-60(a6)";
#define WriteMemA314(address, src, length) __WriteMemA314(A314Base, address, src, length)

void __ReadMemA314(__reg("a6") void *, __reg("a0") void *, __reg("d0") ULONG, __reg("d1") ULONG)="\tjsr\t-66(a6)";
#define ReadMemA314(dst, address, length) __ReadMemA314(A314Base, dst, address, length)

#endif
