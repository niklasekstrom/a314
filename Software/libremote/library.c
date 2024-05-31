#include <stdint.h>
#include <string.h>

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <utility/tagitem.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "../a314device/a314.h"
#include "../a314device/proto_a314.h"

#include "messages.h"
#include "stubs.h"

#define SysBase             (*(struct ExecBase **)4)

#define LIB_NEG_SIZE        (LVO_COUNT*6)

#define BOUNCE_BUFFER_SIZE  (16*1024)

// Must keep VERSION in romtag.asm in sync.
#define LIB_VERSION     3
#define LIB_REVISION    0
#define LIB_DATE        "28.5.2024"

#define VSTR(s) STR(s)
#define STR(s) #s

const char library_name[] = LIBRARY_NAME;
const char id_string[] = LIBRARY_NAME " " VSTR(LIB_VERSION) "." VSTR(LIB_REVISION) " (" LIB_DATE ")\n\r";
const char service_name[] = SERVICE_NAME;

static BPTR saved_seg_list;
static struct Library *global_lib;
struct Library *A314Base;

struct MemChunkHdr
{
    struct MemChunkHdr *next;
    uint32_t size;
};

struct LibRemote
{
    struct Library lib;
    struct MsgPort mp;
    struct A314_IORequest write_req;
    struct A314_IORequest read_req;
    uint32_t bounce_buffer_address;
    struct MemChunkHdr *chunks;
    uint8_t read_buf[256];
};

static uint32_t null_func()
{
    return 0;
}

static BPTR expunge()
{
    if (global_lib->lib_OpenCnt != 0)
    {
        global_lib->lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    Remove(&global_lib->lib_Node);
    FreeMem((char *)global_lib - global_lib->lib_NegSize, global_lib->lib_NegSize + global_lib->lib_PosSize);
    global_lib = NULL;

    BPTR seg_list = saved_seg_list;
    saved_seg_list = 0;
    return seg_list;
}

static BPTR close(__reg("a6") struct LibRemote *lib)
{
    struct MemChunkHdr *chunk = lib->chunks;
    while (chunk)
    {
        struct MemChunkHdr *next = chunk->next;
        FreeMem(chunk, chunk->size);
        chunk = next;
    }

    // Reset connection.
    lib->write_req.a314_Buffer = NULL;
    lib->write_req.a314_Length = 0;
    lib->write_req.a314_Request.io_Command = A314_RESET;
    DoIO(&lib->write_req.a314_Request);

    FreeMemA314(lib->bounce_buffer_address, BOUNCE_BUFFER_SIZE);

    CloseDevice(&lib->write_req.a314_Request);

    FreeMem((char *)lib - lib->lib.lib_NegSize, lib->lib.lib_NegSize + lib->lib.lib_PosSize);

    global_lib->lib_OpenCnt--;

    if (global_lib->lib_OpenCnt == 0 && (global_lib->lib_Flags & LIBF_DELEXP) != 0)
        return expunge();
    else
        return 0;
}

static uint32_t alloc_chunk(__reg("a6") struct LibRemote *lib, uint32_t length)
{
    struct MemChunkHdr *chunk = AllocMem(length + 8, MEMF_ANY);

    if (!chunk)
        return 0;

    chunk->next = lib->chunks;
    chunk->size = length + 8;
    lib->chunks = chunk;
    return (uint32_t)chunk + 8;
}

static void free_chunk(__reg("a6") struct LibRemote *lib, uint32_t address)
{
    struct MemChunkHdr *chunk = (void *)(address - 8);

    if (lib->chunks == chunk)
        lib->chunks = chunk->next;
    else
    {
        struct MemChunkHdr *prev_chunk = lib->chunks;

        while (prev_chunk)
        {
            if (prev_chunk->next == chunk)
            {
                prev_chunk->next = chunk->next;
                break;
            }
            prev_chunk = prev_chunk->next;
        }
    }

    FreeMem(chunk, chunk->size);
}

static int get_tag_list_length(struct TagItem *tag_list)
{
    int count = 1;
    while (tag_list->ti_Tag != TAG_DONE && tag_list->ti_Tag != TAG_MORE)
    {
        tag_list++;
        count++;
    }
    return count * sizeof(struct TagItem);
}

static uint32_t send_request(__reg("a6") struct LibRemote *lib, uint8_t *write_buf, uint32_t write_length)
{
    uint8_t *read_buf = lib->read_buf;

    lib->read_req.a314_Buffer = read_buf;
    lib->read_req.a314_Length = sizeof(lib->read_buf);
    lib->read_req.a314_Request.io_Command = A314_READ;
    SendIO(&lib->read_req.a314_Request);

    lib->write_req.a314_Buffer = write_buf;
    lib->write_req.a314_Length = write_length;
    lib->write_req.a314_Request.io_Command = A314_WRITE;
    SendIO(&lib->write_req.a314_Request);

    BOOL pending_read = TRUE;
    BOOL pending_write = TRUE;

    uint32_t signals_received = 0;
    uint32_t signals_to_send = 0;

    while (TRUE)
    {
        uint32_t new_signals = Wait(-1);

        signals_received |= new_signals & ~SIGF_SINGLE;
        signals_to_send |= new_signals & ~SIGF_SINGLE;

        if (new_signals & SIGF_SINGLE)
        {
            struct A314_IORequest *req;
            while (req = (struct A314_IORequest *)GetMsg(&lib->mp))
            {
                if (req == &lib->write_req)
                    pending_write = FALSE;
                else // if (req == &lib->read_req)
                    pending_read = FALSE;
            }
        }

        if (!pending_write && !pending_read)
        {
            uint8_t kind = read_buf[0];

            if (kind == MSG_OP_RES)
                break;

            if (kind == MSG_ALLOC_MEM_REQ)
            {
                struct AllocMemReqMsg *req = (struct AllocMemReqMsg *)read_buf;
                uint32_t addr = alloc_chunk(lib, req->length);

                write_buf[0] = MSG_ALLOC_MEM_RES;
                *(uint32_t *)&write_buf[2] = addr;
                write_length = 6;
            }
            else if (kind == MSG_FREE_MEM_REQ)
            {
                struct FreeMemReqMsg *req = (struct FreeMemReqMsg *)read_buf;
                free_chunk(lib, req->address);

                write_buf[0] = MSG_FREE_MEM_RES;
                write_length = 1;
            }
            else if (kind == MSG_READ_MEM_REQ)
            {
                uint8_t *read_mem_desc = &read_buf[2];

                write_buf[0] = MSG_READ_MEM_RES;
                write_length = 2;
                uint8_t *write_to = &write_buf[2];

                while (read_mem_desc - read_buf < lib->read_req.a314_Length)
                {
                    uint32_t src = *(uint32_t *)read_mem_desc;
                    read_mem_desc += 4;

                    uint8_t len = *read_mem_desc++;
                    read_mem_desc++;

                    memcpy(write_to, (void *)src, len);

                    write_to += len;
                    write_length += len;
                }
            }
            else if (kind == MSG_WRITE_MEM_REQ)
            {
                uint8_t *write_mem_desc = &read_buf[2];

                while (write_mem_desc - read_buf < lib->read_req.a314_Length)
                {
                    uint32_t dst = *(uint32_t *)write_mem_desc;
                    write_mem_desc += 4;

                    uint8_t len = *write_mem_desc++;
                    if ((len & 1) == 0)
                        write_mem_desc++;

                    memcpy((void *)dst, write_mem_desc, len);

                    write_mem_desc += len;
                }

                write_buf[0] = MSG_WRITE_MEM_RES;
                write_length = 1;
            }
            else if (kind == MSG_COPY_FROM_BOUNCE_REQ)
            {
                struct CopyFromBounceReqMsg *req = (struct CopyFromBounceReqMsg *)read_buf;
                struct CopyDesc *cd = (struct CopyDesc *)&req[1];

                for (int i = 0; i < req->copy_count; i++)
                {
                    ReadMemA314((void *)cd->dst, cd->src, cd->len);
                    cd++;
                }

                write_buf[0] = MSG_COPY_FROM_BOUNCE_RES;
                write_length = 1;
            }
            else if (kind == MSG_COPY_TO_BOUNCE_REQ)
            {
                struct CopyToBounceReqMsg *req = (struct CopyToBounceReqMsg *)read_buf;
                struct CopyDesc *cd = (struct CopyDesc *)&req[1];

                for (int i = 0; i < req->copy_count; i++)
                {
                    WriteMemA314(cd->dst, (void *)cd->src, cd->len);
                    cd++;
                }

                write_buf[0] = MSG_COPY_TO_BOUNCE_RES;
                write_length = 1;
            }
            else if (kind == MSG_COPY_STR_TO_BOUNCE_REQ)
            {
                struct CopyStrToBounceReqMsg *req = (struct CopyStrToBounceReqMsg *)read_buf;
                uint32_t len = strlen((void *)req->str_address);

                WriteMemA314(req->bounce_address, (void *)req->str_address, len);

                write_buf[0] = MSG_COPY_STR_TO_BOUNCE_RES;
                *(uint32_t *)&write_buf[2] = len;
                write_length = 6;
            }
            else if (kind == MSG_COPY_TAG_LIST_TO_BOUNCE_REQ)
            {
                struct CopyTagListToBounceReqMsg *req = (struct CopyTagListToBounceReqMsg *)read_buf;
                uint32_t len = get_tag_list_length((void *)req->tag_list_address);

                WriteMemA314(req->bounce_address, (void *)req->tag_list_address, len);

                write_buf[0] = MSG_COPY_TAG_LIST_TO_BOUNCE_RES;
                *(uint32_t *)&write_buf[2] = len;
                write_length = 6;
            }
            else
            {
                // TODO: Should handle this severe error in some other way.
                // This means that received an unknown message.
                write_buf[0] = kind + 1;
                write_length = 1;
            }

            lib->read_req.a314_Buffer = read_buf;
            lib->read_req.a314_Length = sizeof(lib->read_buf);
            lib->read_req.a314_Request.io_Command = A314_READ;
            SendIO(&lib->read_req.a314_Request);

            lib->write_req.a314_Buffer = write_buf;
            lib->write_req.a314_Length = write_length;
            lib->write_req.a314_Request.io_Command = A314_WRITE;
            SendIO(&lib->write_req.a314_Request);

            pending_read = TRUE;
            pending_write = TRUE;
        }

        if (!pending_write && signals_to_send)
        {
            write_buf[0] = MSG_SIGNALS;
            *(uint32_t *)&write_buf[2] = signals_to_send;
            write_length = 6;

            signals_to_send = 0;

            lib->write_req.a314_Buffer = write_buf;
            lib->write_req.a314_Length = write_length;
            lib->write_req.a314_Request.io_Command = A314_WRITE;
            SendIO(&lib->write_req.a314_Request);

            pending_write = TRUE;
        }
    }

    struct OpResMsg *op_msg = (struct OpResMsg *)read_buf;
    struct CopyDesc *cd = (struct CopyDesc *)&op_msg[1];

    for (int i = 0; i < op_msg->copy_count; i++)
    {
        ReadMemA314((void *)cd->dst, cd->src, cd->len);
        cd++;
    }

    uint8_t *write_mem_desc = (uint8_t *)cd;

    while (write_mem_desc - read_buf < lib->read_req.a314_Length)
    {
        uint32_t dst = *(uint32_t *)write_mem_desc;
        write_mem_desc += 4;

        uint8_t len = *write_mem_desc++;
        if ((len & 1) == 0)
            write_mem_desc++;

        memcpy((void *)dst, write_mem_desc, len);

        write_mem_desc += len;
    }

    Signal(FindTask(NULL), signals_received & ~(op_msg->signals_consumed));

    return op_msg->result;
}

static struct Library *open(__reg("a6") struct Library *glib, __reg("d0") uint32_t version)
{
    global_lib->lib_OpenCnt++;

    BYTE *p = AllocMem(sizeof(struct LibRemote) + LIB_NEG_SIZE, MEMF_ANY | MEMF_CLEAR);
    if (!p)
        goto err_out0;

    // Initialize library struct.
    struct LibRemote *lib = (struct LibRemote *)(p + LIB_NEG_SIZE);
    lib->lib.lib_Node.ln_Type = NT_LIBRARY;
    lib->lib.lib_Node.ln_Name = (char *)library_name;
    lib->lib.lib_NegSize = LIB_NEG_SIZE;
    lib->lib.lib_PosSize = sizeof(struct LibRemote);
    lib->lib.lib_Version = LIB_VERSION;
    lib->lib.lib_Revision = LIB_REVISION;
    lib->lib.lib_IdString = id_string;
    lib->lib.lib_OpenCnt = 1;

    fill_lvos(lib);

    // Create message port.
    lib->mp.mp_Node.ln_Type = NT_MSGPORT;
    lib->mp.mp_Node.ln_Name = (void *)library_name;
    lib->mp.mp_Flags = PA_SIGNAL;
    lib->mp.mp_SigTask = FindTask(NULL);
    lib->mp.mp_SigBit = SIGB_SINGLE;

    // Open a314.device.
    lib->write_req.a314_Request.io_Message.mn_Length = sizeof(struct A314_IORequest);
    lib->write_req.a314_Request.io_Message.mn_ReplyPort = &lib->mp;
    if (OpenDevice(A314_NAME, 0, &lib->write_req.a314_Request, 0))
        goto err_out1;

    // Get unique socket identifier.
    struct DosLibrary *DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 0);
    struct DateStamp ds;
    DateStamp(&ds);
    lib->write_req.a314_Socket = (ds.ds_Minute * 60 * TICKS_PER_SECOND) + ds.ds_Tick;
    CloseLibrary((struct Library *)DOSBase);

    memcpy(&lib->read_req, &lib->write_req, sizeof(struct A314_IORequest));

    // Allocate bounce buffer.
    A314Base = (struct Library *)lib->read_req.a314_Request.io_Device;

    lib->bounce_buffer_address = AllocMemA314(BOUNCE_BUFFER_SIZE);
    if (lib->bounce_buffer_address == INVALID_A314_ADDRESS)
        goto err_out2;

    // Connect to service.
    lib->write_req.a314_Buffer = (char *)service_name;
    lib->write_req.a314_Length = sizeof(service_name) - 1;
    lib->write_req.a314_Request.io_Command = A314_CONNECT;

    if (DoIO(&lib->write_req.a314_Request) != A314_CONNECT_OK)
        goto err_out3;

    // Send open message.
    uint32_t open_msg[2] = {lib->bounce_buffer_address, BOUNCE_BUFFER_SIZE};

    lib->write_req.a314_Buffer = (void *)&open_msg[0];
    lib->write_req.a314_Length = sizeof(open_msg);
    lib->write_req.a314_Request.io_Command = A314_WRITE;
    DoIO(&lib->write_req.a314_Request);

    global_lib->lib_Flags &= ~LIBF_DELEXP;
    return &lib->lib;

err_out3:
    FreeMemA314(lib->bounce_buffer_address, BOUNCE_BUFFER_SIZE);

err_out2:
    CloseDevice(&lib->write_req.a314_Request);

err_out1:
    FreeMem((char *)lib - lib->lib.lib_NegSize, lib->lib.lib_NegSize + lib->lib.lib_PosSize);

err_out0:
    global_lib->lib_OpenCnt--;
    return NULL;
}

static uint32_t library_vectors[] =
{
    (uint32_t)open,
    (uint32_t)null_func,
    (uint32_t)expunge,
    (uint32_t)null_func,
    -1,
};

static struct Library *init_library(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct Library *lib)
{
    saved_seg_list = seg_list;
    global_lib = lib;

    lib->lib_Version = LIB_VERSION;
    lib->lib_Revision = LIB_REVISION;

    return lib;
}

uint32_t auto_init_tables[] =
{
    sizeof(struct Library),
    (uint32_t)library_vectors,
    0,
    (uint32_t)init_library,
};
