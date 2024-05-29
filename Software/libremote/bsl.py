#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2024 Niklas Ekström

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.DEBUG)

from a314d import A314d
import queue
import select
import socket
import struct
import threading
import time
from typing import Dict, List, Tuple

logger = logging.getLogger(__name__)

# These must match constants in messages.h.
MSG_SIGNALS                 = 1
MSG_OP_REQ                  = 2
MSG_OP_RES                  = 3
MSG_ALLOC_MEM_REQ           = 4
MSG_ALLOC_MEM_RES           = 5
MSG_FREE_MEM_REQ            = 6
MSG_FREE_MEM_RES            = 7
MSG_COPY_FROM_BOUNCE_REQ    = 8
MSG_COPY_FROM_BOUNCE_RES    = 9
MSG_COPY_TO_BOUNCE_REQ      = 10
MSG_COPY_TO_BOUNCE_RES      = 11
MSG_COPY_STR_TO_BOUNCE_REQ  = 12
MSG_COPY_STR_TO_BOUNCE_RES  = 13
MSG_COPY_TAG_LIST_TO_BOUNCE_REQ = 14
MSG_COPY_TAG_LIST_TO_BOUNCE_RES = 15

QITEM_RESET = 1
QITEM_READ_MEM_COMPLETE = 2
QITEM_WRITE_MEM_COMPLETE = 3
QITEM_SIGNALS = 4
QITEM_OP_REQ = 5
QITEM_ALLOC_MEM_RES = 6
QITEM_FREE_MEM_RES = 7
QITEM_COPY_FROM_BOUNCE_RES = 8
QITEM_COPY_TO_BOUNCE_RES = 9
QITEM_COPY_STR_TO_BOUNCE_RES = 10
QITEM_COPY_TAG_LIST_TO_BOUNCE_RES = 11

SERVICE_NAME = 'bsl'

FN_SOCKET = 0
FN_BIND = 1
FN_LISTEN = 2
FN_ACCEPT = 3
FN_CONNECT = 4
FN_SENDTO = 5
FN_SEND = 6
FN_RECVFROM = 7
FN_RECV = 8
FN_SHUTDOWN = 9
FN_SETSOCKOPT = 10
FN_GETSOCKOPT = 11
FN_GETSOCKNAME = 12
FN_GETPEERNAME = 13
FN_IOCTLSOCKET = 14
FN_CLOSESOCKET = 15
FN_WAITSELECT = 16
FN_SETSOCKETSIGNALS = 17
FN_GETDTABLESIZE = 18
FN_OBTAINSOCKET = 19
FN_RELEASESOCKET = 20
FN_RELEASECOPYOFSOCKET = 21
FN_ERRNO = 22
FN_SETERRNOPTR = 23
FN_INET_NTOA = 24
FN_INET_ADDR = 25
FN_INET_LNAOF = 26
FN_INET_NETOF = 27
FN_INET_MAKEADDR = 28
FN_INET_NETWORK = 29
FN_GETHOSTBYNAME = 30
FN_GETHOSTBYADDR = 31
FN_GETNETBYNAME = 32
FN_GETNETBYADDR = 33
FN_GETSERVBYNAME = 34
FN_GETSERVBYPORT = 35
FN_GETPROTOBYNAME = 36
FN_GETPROTOBYNUMBER = 37
FN_VSYSLOG = 38
FN_DUP2SOCKET = 39
FN_SENDMSG = 40
FN_RECVMSG = 41
FN_GETHOSTNAME = 42
FN_GETHOSTID = 43
FN_SOCKETBASETAGLIST = 44
FN_GETSOCKETEVENTS = 45

TAG_DONE = 0
TAG_IGNORE = 1
TAG_MORE = 2
TAG_SKIP = 3
TAG_USER = 0x80000000
SBTF_REF = 0x8000

SBTC_BREAKMASK      = 1
SBTC_SIGIOMASK      = 2
SBTC_SIGURGMASK     = 3
SBTC_SIGEVENTMASK   = 4
SBTC_ERRNO          = 6
SBTC_HERRNO         = 7
SBTC_DTABLESIZE     = 8
SBTC_FDCALLBACK     = 9
SBTC_LOGSTAT        = 10
SBTC_LOGTAGPTR      = 11
SBTC_LOGFACILITY    = 12
SBTC_LOGMASK        = 13
SBTC_ERRNOSTRPTR    = 14
SBTC_HERRNOSTRPTR   = 15
SBTC_IOERRNOSTRPTR  = 16
SBTC_S2ERRNOSTRPTR  = 17
SBTC_S2WERRNOSTRPTR = 18
SBTC_ERRNOBYTEPTR   = 21
SBTC_ERRNOWORDPTR   = 22
SBTC_ERRNOLONGPTR   = 24
SBTC_HERRNOLONGPTR  = 25
SBTC_RELEASESTRPTR  = 29

class AmSocket:
    def __init__(self, slot: int, sock: socket.socket) -> None:
        self.slot = slot
        self.sock = sock

class LibInstance:
    def __init__(self, service: 'LibRemoteService', stream_id: int) -> None:
        self.service = service
        self.stream_id = stream_id
        self.queue = queue.Queue()

        self.wait_first_message = True
        self.bb_address = 0
        self.bb_size = 0

        self.break_mask = 4096 # SIGBREAKF_CTRL_C
        self.log_tag_ptr = 0
        self.errno_ptr = 0
        self.errno_size = 0
        self.herrno_ptr = None

        self.return_mem_address = 0
        self.sockets: Dict[int, AmSocket] = {}

        threading.Thread(target=self.run).start()

    # Put items on queue.
    def process_stream_reset(self):
        self.queue.put((QITEM_RESET, None))

    def process_read_mem_complete(self, address: int, data: bytes):
        self.queue.put((QITEM_READ_MEM_COMPLETE, data))

    def process_write_mem_complete(self, address: int):
        self.queue.put((QITEM_WRITE_MEM_COMPLETE, None))

    def process_signals(self, signals: int):
        self.queue.put((QITEM_SIGNALS, signals))

    def process_op_req(self, op: int, args: bytes):
        self.queue.put((QITEM_OP_REQ, (op, args)))

    def process_alloc_mem_res(self, address: int):
        self.queue.put((QITEM_ALLOC_MEM_RES, address))

    def process_free_mem_res(self):
        self.queue.put((QITEM_FREE_MEM_RES, None))

    def process_copy_from_bounce_res(self):
        self.queue.put((QITEM_COPY_FROM_BOUNCE_RES, None))

    def process_copy_to_bounce_res(self):
        self.queue.put((QITEM_COPY_TO_BOUNCE_RES, None))

    def process_copy_str_to_bounce_res(self, length: int):
        self.queue.put((QITEM_COPY_STR_TO_BOUNCE_RES, length))

    def process_copy_tag_list_to_bounce_res(self, length: int):
        self.queue.put((QITEM_COPY_TAG_LIST_TO_BOUNCE_RES, length))

    # Wait for specific queue item.
    def wait_qitem(self, match_qitem: int):
        while True:
            qitem, arg = self.queue.get()
            if qitem == QITEM_RESET:
                raise InterruptedError()
            elif qitem == match_qitem:
                return arg

    def wait_signals(self, signals_mask: int, timeout_secs: int) -> int:
        end_time = time.time() + timeout_secs
        while True:
            try:
                time_left = max(end_time - time.time(), 0)
                qitem, arg = self.queue.get(timeout=time_left)
                if qitem == QITEM_RESET:
                    raise InterruptedError()
                elif qitem == QITEM_SIGNALS:
                    arg &= signals_mask
                    if arg:
                        return arg
            except queue.Empty:
                return 0

    # Blocking functions to run commands on Amiga.
    def alloc_mem(self, length: int) -> int:
        self.service.send_alloc_mem_req(self.stream_id, length)
        return self.wait_qitem(QITEM_ALLOC_MEM_RES)

    def free_mem(self, address: int) -> int:
        self.service.send_free_mem_req(self.stream_id, address)
        return self.wait_qitem(QITEM_FREE_MEM_RES)

    def copy_from_bounce(self, mem_address: int, bounce_address: int, length: int) -> None:
        self.service.send_copy_from_bounce_req(self.stream_id, mem_address, bounce_address, length)
        self.wait_qitem(QITEM_COPY_FROM_BOUNCE_RES)

    def copy_to_bounce(self, bounce_address: int, mem_address: int, length: int) -> None:
        self.service.send_copy_to_bounce_req(self.stream_id, bounce_address, mem_address, length)
        self.wait_qitem(QITEM_COPY_TO_BOUNCE_RES)

    def copy_str_to_bounce(self, bounce_address: int, str_address: int) -> int:
        self.service.send_copy_str_to_bounce_req(self.stream_id, bounce_address, str_address)
        return self.wait_qitem(QITEM_COPY_STR_TO_BOUNCE_RES)

    def copy_tag_list_to_bounce(self, bounce_address: int, tag_list_address: int) -> int:
        self.service.send_copy_tag_list_to_bounce_req(self.stream_id, bounce_address, tag_list_address)
        return self.wait_qitem(QITEM_COPY_TAG_LIST_TO_BOUNCE_RES)

    # Read from/write to bounce buffer.
    def read_mem(self, address: int, length: int) -> bytes:
        self.service.start_read_mem(self.stream_id, address, length)
        return self.wait_qitem(QITEM_READ_MEM_COMPLETE)

    def write_mem(self, address: int, data: bytes) -> None:
        self.service.start_write_mem(self.stream_id, address, data)
        self.wait_qitem(QITEM_WRITE_MEM_COMPLETE)

    def read_str(self, address: int) -> str:
        length = self.copy_str_to_bounce(self.bb_address, address)
        data = self.read_mem(self.bb_address, length)
        return data.decode('latin-1')

    def read_tag_list(self, address: int) -> List[Tuple[int, int]]:
        length = self.copy_tag_list_to_bounce(self.bb_address, address)
        data = self.read_mem(self.bb_address, length)
        arr = struct.unpack(f'>{len(data) // 4}I', data)
        return list(zip(arr[0::2], arr[1::2]))

    # The implemented library functions.
    def handle_socket_op(self, domain: int, type_: int, protocol: int):
        logger.debug('handle_socket_op(domain=%s, type_=%s, protocol=%s)', domain, type_, protocol)
        AF_INET         = 2

        SOCK_STREAM     = 1
        SOCK_DGRAM      = 2
        SOCK_RAW        = 3
        SOCK_RDM        = 4
        SOCK_SEQPACKET  = 5

        result = 2**32 - 1

        slot = min(i for i in range(64) if i not in self.sockets)

        if domain == AF_INET:
            if type_ == SOCK_STREAM:
                self.sockets[slot] = AmSocket(slot, socket.socket())
                result = slot
            elif type_ == SOCK_DGRAM:
                self.sockets[slot] = AmSocket(slot, socket.socket(type=socket.SOCK_DGRAM))
                result = slot

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_bind_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_bind_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_listen_op(self, sock: int, backlog: int):
        logger.debug('handle_listen_op(sock=%s, backlog=%s)', sock, backlog)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_accept_op(self, sock: int, addr: int, addrlen: int):
        logger.debug('handle_accept_op(sock=%s, addr=%s, addrlen=%s)', sock, addr, addrlen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_connect_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_connect_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        self.copy_to_bounce(self.bb_address, name, namelen)
        data = self.read_mem(self.bb_address, namelen)

        (port,) = struct.unpack('>H', data[2:4])
        host = '.'.join(map(str, data[4:8]))
        addr = (host, port)
        logger.debug('addr=%s', addr)

        # TODO: Handle break signal.
        # TODO: Handle non blocking operation.

        s = self.sockets.get(sock)
        if not s:
            # TODO: Set/write errno.
            result = 2**32 - 1
        else:
            try:
                s.sock.connect(addr)
                result = 0
            except:
                # TODO: Set/write errno.
                result = 2**32 - 1

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_sendto_op(self, sock: int, buf: int, len_: int, flags: int, to: int, tolen: int):
        logger.debug('handle_sendto_op(sock=%s, buf=%s, len_=%s, flags=%s, to=%s, tolen=%s)', sock, buf, len_, flags, to, tolen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_send_op(self, sock: int, buf: int, len_: int, flags: int):
        logger.debug('handle_send_op(sock=%s, buf=%s, len_=%s, flags=%s)', sock, buf, len_, flags)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_recvfrom_op(self, sock: int, buf: int, len_: int, flags: int, addr: int, addrlen: int):
        logger.debug('handle_recvfrom_op(sock=%s, buf=%s, len_=%s, flags=%s, addr=%s, addrlen=%s)', sock, buf, len_, flags, addr, addrlen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_recv_op(self, sock: int, buf: int, len_: int, flags: int):
        logger.debug('handle_recv_op(sock=%s, buf=%s, len_=%s, flags=%s)', sock, buf, len_, flags)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_shutdown_op(self, sock: int, how: int):
        logger.debug('handle_shutdown_op(sock=%s, how=%s)', sock, how)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_setsockopt_op(self, sock: int, level: int, optname: int, optval: int, optlen: int):
        logger.debug('handle_setsockopt_op(sock=%s, level=%s, optname=%s, optval=%s, optlen=%s)', sock, level, optname, optval, optlen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getsockopt_op(self, sock: int, level: int, optname: int, optval: int, optlen: int):
        logger.debug('handle_getsockopt_op(sock=%s, level=%s, optname=%s, optval=%s, optlen=%s)', sock, level, optname, optval, optlen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getsockname_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_getsockname_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getpeername_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_getpeername_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_ioctlsocket_op(self, sock: int, req: int, argp: int):
        logger.debug('handle_ioctlsocket_op(sock=%s, req=%s, argp=%s)', sock, req, argp)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_closesocket_op(self, sock: int):
        logger.debug('handle_closesocket_op(sock=%s)', sock)

        s = self.sockets.get(sock)
        if not s:
            # TODO: Set/write errno.
            result = 2**32 - 1
        else:
            try:
                s.sock.close()
            except:
                pass
            del self.sockets[sock]
            result = 0

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_waitselect_op(self, nfds: int, read_fds: int, write_fds: int, except_fds: int, _timeout: int, signals: int):
        logger.debug('handle_waitselect_op(nfds=%s, read_fds=%s, write_fds=%s, except_fds=%s, _timeout=%s, signals=%s)', nfds, read_fds, write_fds, except_fds, _timeout, signals)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_setsocketsignals_op(self, int_mask: int, io_mask: int, urgent_mask: int):
        logger.debug('handle_setsocketsignals_op(int_mask=%s, io_mask=%s, urgent_mask=%s)', int_mask, io_mask, urgent_mask)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getdtablesize_op(self):
        logger.debug('handle_getdtablesize_op()')
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_obtainsocket_op(self, id_: int, domain: int, type_: int, protocol: int):
        logger.debug('handle_obtainsocket_op(id_=%s, domain=%s, type_=%s, protocol=%s)', id_, domain, type_, protocol)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_releasesocket_op(self, sock: int, id_: int):
        logger.debug('handle_releasesocket_op(sock=%s, id_=%s)', sock, id_)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_releasecopyofsocket_op(self, sock: int, id_: int):
        logger.debug('handle_releasecopyofsocket_op(sock=%s, id_=%s)', sock, id_)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_errno_op(self):
        logger.debug('handle_errno_op()')
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_seterrnoptr_op(self, errno_ptr: int, size: int):
        logger.debug('handle_seterrnoptr_op(errno_ptr=%s, size=%s)', errno_ptr, size)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_ntoa_op(self, ip: int):
        logger.debug('handle_inet_ntoa_op(ip=%s)', ip)

        if not self.return_mem_address:
            self.return_mem_address = self.alloc_mem(512)

        if self.return_mem_address:
            data = struct.pack('>I', ip)
            data = '.'.join(str(x) for x in data)
            data = data.encode('latin-1') + b'\x00'
            #logger.debug('data=%s', data)
            self.write_mem(self.bb_address, data)
            self.copy_from_bounce(self.return_mem_address, self.bb_address, len(data))

        result = self.return_mem_address
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_addr_op(self, cp: int):
        logger.debug('handle_inet_addr_op(cp=%s)', cp)
        s = self.read_str(cp)
        #logger.debug('cp=%s', s)
        try:
            (result,) = struct.unpack('>I', bytes(map(int, s.split('.'))))
        except:
            result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_lnaof_op(self, in_: int):
        logger.debug('handle_inet_lnaof_op(in_=%s)', in_)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_netof_op(self, in_: int):
        logger.debug('handle_inet_netof_op(in_=%s)', in_)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_makeaddr_op(self, net: int, host: int):
        logger.debug('handle_inet_makeaddr_op(net=%s, host=%s)', net, host)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_inet_network_op(self, cp: int):
        logger.debug('handle_inet_network_op(cp=%s)', cp)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_gethostbyname_op(self, name: int):
        logger.debug('handle_gethostbyname_op(name=%s)', name)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_gethostbyaddr_op(self, addr: int, len_: int, type_: int):
        logger.debug('handle_gethostbyaddr_op(addr=%s, len_=%s, type_=%s)', addr, len_, type_)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getnetbyname_op(self, name: int):
        logger.debug('handle_getnetbyname_op(name=%s)', name)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getnetbyaddr_op(self, net: int, type_: int):
        logger.debug('handle_getnetbyaddr_op(net=%s, type_=%s)', net, type_)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getservbyname_op(self, name: int, proto: int):
        logger.debug('handle_getservbyname_op(name=%s, proto=%s)', name, proto)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getservbyport_op(self, port: int, proto: int):
        logger.debug('handle_getservbyport_op(port=%s, proto=%s)', port, proto)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getprotobyname_op(self, name: int):
        logger.debug('handle_getprotobyname_op(name=%s)', name)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getprotobynumber_op(self, proto: int):
        logger.debug('handle_getprotobynumber_op(proto=%s)', proto)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_vsyslog_op(self, pri: int, msg: int, args_: int):
        logger.debug('handle_vsyslog_op(pri=%s, msg=%s, args_=%s)', pri, msg, args_)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_dup2socket_op(self, old_socket: int, new_socket: int):
        logger.debug('handle_dup2socket_op(old_socket=%s, new_socket=%s)', old_socket, new_socket)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_sendmsg_op(self, sock: int, msg: int, flags: int):
        logger.debug('handle_sendmsg_op(sock=%s, msg=%s, flags=%s)', sock, msg, flags)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_recvmsg_op(self, sock: int, msg: int, flags: int):
        logger.debug('handle_recvmsg_op(sock=%s, msg=%s, flags=%s)', sock, msg, flags)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_gethostname_op(self, name: int, namelen: int):
        logger.debug('handle_gethostname_op(name=%s, namelen=%s)', name, namelen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_gethostid_op(self):
        logger.debug('handle_gethostid_op()')
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_socketbasetaglist_op(self, tags: int):
        logger.debug('handle_socketbasetaglist_op(tags=%s)', tags)
        tl = self.read_tag_list(tags)

        #logger.debug('Tag list: %s', tl)

        result = 0

        for i, (key, value) in enumerate(tl):
            if key & TAG_USER:
                key &= 0xffff
                is_ref = (key & SBTF_REF) != 0
                key &= 0x7fff
                is_set = (key & 1) != 0
                key >>= 1

                #logger.debug('Tag: key=%s, is_ref=%s, is_set=%s, value=%s', key, is_ref, is_set, value)

                if not is_set or is_ref:
                    result = i + 1
                    break

                if key == SBTC_BREAKMASK:
                    self.break_mask = value
                elif key == SBTC_LOGTAGPTR:
                    self.log_tag_ptr = value
                elif key == SBTC_ERRNOLONGPTR:
                    self.errno_ptr = value
                    self.errno_size = 4
                elif key == SBTC_HERRNOLONGPTR:
                    self.herrno_ptr = value
                else:
                    result = i + 1
                    break
            elif key not in (TAG_DONE, TAG_MORE):
                logger.debug('Unknown tag: key=%s, value=%s', key, value)
                result = i + 1
                break

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getsocketevents_op(self, event_ptr: int):
        logger.debug('handle_getsocketevents_op(event_ptr=%s)', event_ptr)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    # Dispatcher loop.
    def run(self):
        try:
            while True:
                op, args = self.wait_qitem(QITEM_OP_REQ)
                if op == FN_SOCKET:
                    self.handle_socket_op(*struct.unpack(">3I", args))
                elif op == FN_BIND:
                    self.handle_bind_op(*struct.unpack(">3I", args))
                elif op == FN_LISTEN:
                    self.handle_listen_op(*struct.unpack(">2I", args))
                elif op == FN_ACCEPT:
                    self.handle_accept_op(*struct.unpack(">3I", args))
                elif op == FN_CONNECT:
                    self.handle_connect_op(*struct.unpack(">3I", args))
                elif op == FN_SENDTO:
                    self.handle_sendto_op(*struct.unpack(">6I", args))
                elif op == FN_SEND:
                    self.handle_send_op(*struct.unpack(">4I", args))
                elif op == FN_RECVFROM:
                    self.handle_recvfrom_op(*struct.unpack(">6I", args))
                elif op == FN_RECV:
                    self.handle_recv_op(*struct.unpack(">4I", args))
                elif op == FN_SHUTDOWN:
                    self.handle_shutdown_op(*struct.unpack(">2I", args))
                elif op == FN_SETSOCKOPT:
                    self.handle_setsockopt_op(*struct.unpack(">5I", args))
                elif op == FN_GETSOCKOPT:
                    self.handle_getsockopt_op(*struct.unpack(">5I", args))
                elif op == FN_GETSOCKNAME:
                    self.handle_getsockname_op(*struct.unpack(">3I", args))
                elif op == FN_GETPEERNAME:
                    self.handle_getpeername_op(*struct.unpack(">3I", args))
                elif op == FN_IOCTLSOCKET:
                    self.handle_ioctlsocket_op(*struct.unpack(">3I", args))
                elif op == FN_CLOSESOCKET:
                    self.handle_closesocket_op(*struct.unpack(">1I", args))
                elif op == FN_WAITSELECT:
                    self.handle_waitselect_op(*struct.unpack(">6I", args))
                elif op == FN_SETSOCKETSIGNALS:
                    self.handle_setsocketsignals_op(*struct.unpack(">3I", args))
                elif op == FN_GETDTABLESIZE:
                    self.handle_getdtablesize_op()
                elif op == FN_OBTAINSOCKET:
                    self.handle_obtainsocket_op(*struct.unpack(">4I", args))
                elif op == FN_RELEASESOCKET:
                    self.handle_releasesocket_op(*struct.unpack(">2I", args))
                elif op == FN_RELEASECOPYOFSOCKET:
                    self.handle_releasecopyofsocket_op(*struct.unpack(">2I", args))
                elif op == FN_ERRNO:
                    self.handle_errno_op()
                elif op == FN_SETERRNOPTR:
                    self.handle_seterrnoptr_op(*struct.unpack(">2I", args))
                elif op == FN_INET_NTOA:
                    self.handle_inet_ntoa_op(*struct.unpack(">1I", args))
                elif op == FN_INET_ADDR:
                    self.handle_inet_addr_op(*struct.unpack(">1I", args))
                elif op == FN_INET_LNAOF:
                    self.handle_inet_lnaof_op(*struct.unpack(">1I", args))
                elif op == FN_INET_NETOF:
                    self.handle_inet_netof_op(*struct.unpack(">1I", args))
                elif op == FN_INET_MAKEADDR:
                    self.handle_inet_makeaddr_op(*struct.unpack(">2I", args))
                elif op == FN_INET_NETWORK:
                    self.handle_inet_network_op(*struct.unpack(">1I", args))
                elif op == FN_GETHOSTBYNAME:
                    self.handle_gethostbyname_op(*struct.unpack(">1I", args))
                elif op == FN_GETHOSTBYADDR:
                    self.handle_gethostbyaddr_op(*struct.unpack(">3I", args))
                elif op == FN_GETNETBYNAME:
                    self.handle_getnetbyname_op(*struct.unpack(">1I", args))
                elif op == FN_GETNETBYADDR:
                    self.handle_getnetbyaddr_op(*struct.unpack(">2I", args))
                elif op == FN_GETSERVBYNAME:
                    self.handle_getservbyname_op(*struct.unpack(">2I", args))
                elif op == FN_GETSERVBYPORT:
                    self.handle_getservbyport_op(*struct.unpack(">2I", args))
                elif op == FN_GETPROTOBYNAME:
                    self.handle_getprotobyname_op(*struct.unpack(">1I", args))
                elif op == FN_GETPROTOBYNUMBER:
                    self.handle_getprotobynumber_op(*struct.unpack(">1I", args))
                elif op == FN_VSYSLOG:
                    self.handle_vsyslog_op(*struct.unpack(">3I", args))
                elif op == FN_DUP2SOCKET:
                    self.handle_dup2socket_op(*struct.unpack(">2I", args))
                elif op == FN_SENDMSG:
                    self.handle_sendmsg_op(*struct.unpack(">3I", args))
                elif op == FN_RECVMSG:
                    self.handle_recvmsg_op(*struct.unpack(">3I", args))
                elif op == FN_GETHOSTNAME:
                    self.handle_gethostname_op(*struct.unpack(">2I", args))
                elif op == FN_GETHOSTID:
                    self.handle_gethostid_op()
                elif op == FN_SOCKETBASETAGLIST:
                    self.handle_socketbasetaglist_op(*struct.unpack(">1I", args))
                elif op == FN_GETSOCKETEVENTS:
                    self.handle_getsocketevents_op(*struct.unpack(">1I", args))
        except InterruptedError:
            logger.info('LibInstance thread was RESET')
        except:
            logger.exception('LibInstance thread crashed')

class LibRemoteService:
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)
        self.instances: Dict[int, LibInstance] = {}
        self.read_mem_queue = []
        self.write_mem_queue = []
        self.send_lock = threading.Lock()
        self.rbuf = b''

    def start_read_mem(self, stream_id: int, address: int, length: int):
        with self.send_lock:
            self.read_mem_queue.append((stream_id, address))
            self.a314d.send_read_mem_req(address, length)

    def start_write_mem(self, stream_id: int, address: int, data: bytes):
        with self.send_lock:
            self.write_mem_queue.append((stream_id, address))
            self.a314d.send_write_mem_req(address, data)

    def send_op_res(self, stream_id: int, result: int, signals_consumed: int):
        with self.send_lock:
            data = struct.pack('>BBII', MSG_OP_RES, 0, result, signals_consumed)
            self.a314d.send_data(stream_id, data)

    def send_alloc_mem_req(self, stream_id: int, length: int):
        with self.send_lock:
            data = struct.pack('>BBI', MSG_ALLOC_MEM_REQ, 0, length)
            self.a314d.send_data(stream_id, data)

    def send_free_mem_req(self, stream_id: int, address: int):
        with self.send_lock:
            data = struct.pack('>BBI', MSG_FREE_MEM_REQ, 0, address)
            self.a314d.send_data(stream_id, data)

    def send_copy_from_bounce_req(self, stream_id: int, dst: int, src: int, length: int):
        with self.send_lock:
            data = struct.pack('>BBIII', MSG_COPY_FROM_BOUNCE_REQ, 1, dst, src, length)
            self.a314d.send_data(stream_id, data)

    def send_copy_to_bounce_req(self, stream_id: int, dst: int, src: int, length: int):
        with self.send_lock:
            data = struct.pack('>BBIII', MSG_COPY_TO_BOUNCE_REQ, 1, dst, src, length)
            self.a314d.send_data(stream_id, data)

    def send_copy_str_to_bounce_req(self, stream_id: int, bounce_address: int, str_address: int):
        with self.send_lock:
            data = struct.pack('>BBII', MSG_COPY_STR_TO_BOUNCE_REQ, 0, bounce_address, str_address)
            self.a314d.send_data(stream_id, data)

    def send_copy_tag_list_to_bounce_req(self, stream_id: int, bounce_address: int, tag_list_address: int):
        with self.send_lock:
            data = struct.pack('>BBII', MSG_COPY_TAG_LIST_TO_BOUNCE_REQ, 0, bounce_address, tag_list_address)
            self.a314d.send_data(stream_id, data)

    def process_stream_connect(self, stream_id: int, name: bytes):
        if name == SERVICE_NAME.encode():
            logger.info('Amiga connected to LibRemote service')
            self.instances[stream_id] = LibInstance(self, stream_id)
            self.a314d.send_connect_response(stream_id, 0)
        else:
            self.a314d.send_connect_response(stream_id, 3)

    def process_stream_data(self, stream_id: int, payload: bytes):
        inst = self.instances.get(stream_id)
        if not inst:
            return

        if inst.wait_first_message:
            inst.bb_address, inst.bb_size = struct.unpack('>II', payload)
            inst.wait_first_message = False
            return

        kind = payload[0]

        if kind == MSG_SIGNALS:
            (signals,) = struct.unpack('>I', payload[2:])
            inst.process_signals(signals)
        elif kind == MSG_OP_REQ:
            inst.process_op_req(payload[1], payload[2:])
        elif kind == MSG_ALLOC_MEM_RES:
            (address,) = struct.unpack('>I', payload[2:])
            inst.process_alloc_mem_res(address)
        elif kind == MSG_FREE_MEM_RES:
            inst.process_free_mem_res()
        elif kind == MSG_COPY_FROM_BOUNCE_RES:
            inst.process_copy_from_bounce_res()
        elif kind == MSG_COPY_TO_BOUNCE_RES:
            inst.process_copy_to_bounce_res()
        elif kind == MSG_COPY_STR_TO_BOUNCE_RES:
            (length,) = struct.unpack('>I', payload[2:])
            inst.process_copy_str_to_bounce_res(length)
        elif kind == MSG_COPY_TAG_LIST_TO_BOUNCE_RES:
            (length,) = struct.unpack('>I', payload[2:])
            inst.process_copy_tag_list_to_bounce_res(length)

    def process_stream_eos(self, stream_id: int):
        raise AssertionError('EOS is not supposed to be used')

    def process_stream_reset(self, stream_id: int):
        inst = self.instances.get(stream_id)
        if inst:
            inst.process_stream_reset()
            del self.instances[stream_id]
            logger.info('Amiga disconnected from LibRemote service')

    def process_read_mem_res(self, payload: bytes):
        stream_id, address = self.read_mem_queue.pop(0)
        inst = self.instances.get(stream_id)
        if inst:
            inst.process_read_mem_complete(address, payload)

    def process_write_mem_res(self):
        stream_id, address = self.write_mem_queue.pop(0)
        inst = self.instances.get(stream_id)
        if inst:
            inst.process_write_mem_complete(address)

    def process_drv_msg(self, stream_id: int, ptype: int, payload: bytes):
        if ptype == self.a314d.MSG_CONNECT:
            self.process_stream_connect(stream_id, payload)
        elif ptype == self.a314d.MSG_DATA:
            self.process_stream_data(stream_id, payload)
        elif ptype == self.a314d.MSG_EOS:
            self.process_stream_eos(stream_id)
        elif ptype == self.a314d.MSG_RESET:
            self.process_stream_reset(stream_id)
        elif ptype == self.a314d.MSG_READ_MEM_RES:
            self.process_read_mem_res(payload)
        elif ptype == self.a314d.MSG_WRITE_MEM_RES:
            self.process_write_mem_res()

    def handle_a314d_readable(self):
        buf = self.a314d.drv.recv(8192)
        if not buf:
            logger.error('Connection to a314d was closed, shutting down')
            exit(-1)

        self.rbuf += buf
        while True:
            if len(self.rbuf) < 9:
                break

            (plen, stream_id, ptype) = struct.unpack('=IIB', self.rbuf[:9])
            if len(self.rbuf) < 9 + plen:
                break

            payload = self.rbuf[9:9+plen]
            self.rbuf = self.rbuf[9+plen:]

            self.process_drv_msg(stream_id, ptype, payload)

    def run(self):
        logger.info('LibRemote service started')

        while True:
            try:
                rl, _, _ = select.select([self.a314d], [], [], 10.0)
            except KeyboardInterrupt:
                break

            if self.a314d in rl:
                self.handle_a314d_readable()

        self.a314d.close()

        logger.info('LibRemote service stopped')

if __name__ == '__main__':
    service = LibRemoteService()
    service.run()
