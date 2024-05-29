#ifndef __LIBREMOTE_MESSAGES_H
#define __LIBREMOTE_MESSAGES_H

#include <stdint.h>

#define MSG_SIGNALS                 1
#define MSG_OP_REQ                  2
#define MSG_OP_RES                  3
#define MSG_ALLOC_MEM_REQ           4
#define MSG_ALLOC_MEM_RES           5
#define MSG_FREE_MEM_REQ            6
#define MSG_FREE_MEM_RES            7
#define MSG_COPY_FROM_BOUNCE_REQ    8
#define MSG_COPY_FROM_BOUNCE_RES    9
#define MSG_COPY_TO_BOUNCE_REQ      10
#define MSG_COPY_TO_BOUNCE_RES      11
#define MSG_COPY_STR_TO_BOUNCE_REQ  12
#define MSG_COPY_STR_TO_BOUNCE_RES  13
#define MSG_COPY_TAG_LIST_TO_BOUNCE_REQ 14
#define MSG_COPY_TAG_LIST_TO_BOUNCE_RES 15

struct CopyDesc
{
    uint32_t dst;
    uint32_t src;
    uint32_t len;
};

struct SignalsMsg // Amiga -> Pi
{
    uint8_t kind;
    uint8_t pad;
    uint32_t signals;
};

struct OpReqMsg // Amiga -> Pi
{
    uint8_t kind;
    uint8_t op;
};

struct OpResMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t copy_count;
    uint32_t result;
    uint32_t signals_consumed;
};

struct AllocMemReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t pad;
    uint32_t length;
};

struct AllocMemResMsg // Amiga -> Pi
{
    uint8_t kind;
    uint8_t pad;
    uint32_t address;
};

struct FreeMemReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t pad;
    uint32_t address;
};

struct FreeMemResMsg // Amiga -> Pi
{
    uint8_t kind;
};

struct CopyFromBounceReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t copy_count;
};

struct CopyFromBounceResMsg // Amiga -> Pi
{
    uint8_t kind;
};

struct CopyToBounceReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t copy_count;
};

struct CopyToBounceResMsg // Amiga -> Pi
{
    uint8_t kind;
};

struct CopyStrToBounceReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t pad;
    uint32_t bounce_address;
    uint32_t str_address;
};

struct CopyStrToBounceResMsg // Amiga -> Pi
{
    uint8_t kind;
    uint8_t pad;
    uint32_t length;
};

struct CopyTagListToBounceReqMsg // Pi -> Amiga
{
    uint8_t kind;
    uint8_t pad;
    uint32_t bounce_address;
    uint32_t tag_list_address;
};

struct CopyTagListToBounceResMsg // Amiga -> Pi
{
    uint8_t kind;
    uint8_t pad;
    uint32_t length;
};

#endif /* __LIBREMOTE_MESSAGES_H */
