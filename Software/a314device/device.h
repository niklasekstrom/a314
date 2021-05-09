#ifndef __A314_DEVICE_H
#define __A314_DEVICE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <libraries/dos.h>

struct A314Device
{
    struct Library lib;

    BPTR saved_seg_list;
    BOOL running;

    ULONG bank_address[4];
    UWORD is_a600;

};

extern char device_name[];
extern char id_string[];

#endif /* __A314_DEVICE_H */
