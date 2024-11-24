#ifndef LIB_CLOCKPORT_H
#define LIB_CLOCKPORT_H

#include <exec/nodes.h>

#define CLOCKPORT_NAME      "clockport.library"

#define CDF_ALLOCATED       (1 << 0)

struct ClockportDescriptor
{
    struct Node cd_Node;
    USHORT      cd_Flags;
    ULONG       cd_Address;
    char       *cd_Device;
    UBYTE       cd_LowAddressBit;
    UBYTE       cd_Interrupt;
};

#endif /* LIB_CLOCKPORT_H */
