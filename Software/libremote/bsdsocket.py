#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2024 Niklas Ekström

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.DEBUG)

from a314d import A314d
import errno
import fcntl
import queue
import select
import socket
import struct
import termios
import threading
import time
from typing import Dict, List, Tuple, Optional, Any

logger = logging.getLogger(__name__)

# These must match constants in messages.h.
MSG_SIGNALS                 = 1
MSG_OP_REQ                  = 2
MSG_OP_RES                  = 3
MSG_ALLOC_MEM_REQ           = 4
MSG_ALLOC_MEM_RES           = 5
MSG_FREE_MEM_REQ            = 6
MSG_FREE_MEM_RES            = 7
MSG_READ_MEM_REQ            = 8
MSG_READ_MEM_RES            = 9
MSG_WRITE_MEM_REQ           = 10
MSG_WRITE_MEM_RES           = 11
MSG_COPY_FROM_BOUNCE_REQ    = 12
MSG_COPY_FROM_BOUNCE_RES    = 13
MSG_COPY_TO_BOUNCE_REQ      = 14
MSG_COPY_TO_BOUNCE_RES      = 15
MSG_COPY_STR_TO_BOUNCE_REQ  = 16
MSG_COPY_STR_TO_BOUNCE_RES  = 17
MSG_COPY_TAG_LIST_TO_BOUNCE_REQ = 18
MSG_COPY_TAG_LIST_TO_BOUNCE_RES = 19

QITEM_RESET = 1
QITEM_READ_BOUNCE_COMPLETE = 2
QITEM_WRITE_BOUNCE_COMPLETE = 3
QITEM_SIGNALS = 4
QITEM_OP_REQ = 5
QITEM_ALLOC_MEM_RES = 6
QITEM_FREE_MEM_RES = 7
QITEM_READ_MEM_RES = 8
QITEM_WRITE_MEM_RES = 9
QITEM_COPY_FROM_BOUNCE_RES = 10
QITEM_COPY_TO_BOUNCE_RES = 11
QITEM_COPY_STR_TO_BOUNCE_RES = 12
QITEM_COPY_TAG_LIST_TO_BOUNCE_RES = 13

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

# Functions that return a pointer, and so return NULL on failure.
POINTER_OPS = {
    FN_INET_NTOA, FN_GETHOSTBYNAME, FN_GETHOSTBYADDR, FN_GETNETBYNAME,
    FN_GETNETBYADDR, FN_GETSERVBYNAME, FN_GETSERVBYPORT, FN_GETPROTOBYNAME,
    FN_GETPROTOBYNUMBER,
}

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

# Errno values as seen by the Amiga (BSD numbering, as in Roadshow's
# sys/errno.h). Linux uses different numbers for most of these.
BSD_ERRNO = {
    'EPERM': 1, 'ENOENT': 2, 'EINTR': 4, 'EIO': 5, 'EBADF': 9,
    'ENOMEM': 12, 'EACCES': 13, 'EFAULT': 14, 'EINVAL': 22, 'ENFILE': 23,
    'EMFILE': 24, 'EPIPE': 32, 'EAGAIN': 35, 'EWOULDBLOCK': 35,
    'EINPROGRESS': 36, 'EALREADY': 37, 'ENOTSOCK': 38, 'EDESTADDRREQ': 39,
    'EMSGSIZE': 40, 'EPROTOTYPE': 41, 'ENOPROTOOPT': 42,
    'EPROTONOSUPPORT': 43, 'ESOCKTNOSUPPORT': 44, 'EOPNOTSUPP': 45,
    'ENOTSUP': 45, 'EPFNOSUPPORT': 46, 'EAFNOSUPPORT': 47, 'EADDRINUSE': 48,
    'EADDRNOTAVAIL': 49, 'ENETDOWN': 50, 'ENETUNREACH': 51, 'ENETRESET': 52,
    'ECONNABORTED': 53, 'ECONNRESET': 54, 'ENOBUFS': 55, 'EISCONN': 56,
    'ENOTCONN': 57, 'ESHUTDOWN': 58, 'ETIMEDOUT': 60, 'ECONNREFUSED': 61,
    'EHOSTDOWN': 64, 'EHOSTUNREACH': 65,
}

EINTR = BSD_ERRNO['EINTR']
EBADF = BSD_ERRNO['EBADF']
EINVAL = BSD_ERRNO['EINVAL']
EMFILE = BSD_ERRNO['EMFILE']
EWOULDBLOCK = BSD_ERRNO['EWOULDBLOCK']
EINPROGRESS = BSD_ERRNO['EINPROGRESS']
EALREADY = BSD_ERRNO['EALREADY']
ENOPROTOOPT = BSD_ERRNO['ENOPROTOOPT']
EPROTONOSUPPORT = BSD_ERRNO['EPROTONOSUPPORT']
EAFNOSUPPORT = BSD_ERRNO['EAFNOSUPPORT']
ETIMEDOUT = BSD_ERRNO['ETIMEDOUT']
ECONNREFUSED = BSD_ERRNO['ECONNREFUSED']

def bsd_errno(linux_errno: Optional[int]) -> int:
    name = errno.errorcode.get(linux_errno) if linux_errno is not None else None
    return BSD_ERRNO.get(name, EINVAL)

# h_errno values.
HOST_NOT_FOUND = 1
TRY_AGAIN = 2
NO_RECOVERY = 3

AF_INET = 2
SOCK_STREAM = 1
SOCK_DGRAM = 2

MSG_OOB = 0x1
MSG_PEEK = 0x2
MSG_DONTWAIT = 0x80

FIONBIO = 0x8004667e
FIONREAD = 0x4004667f

# (Amiga level, Amiga optname) -> (Linux level, Linux optname, kind)
SOCKOPTS = {
    (0xffff, 0x0004): (socket.SOL_SOCKET, socket.SO_REUSEADDR, 'int'),
    (0xffff, 0x0008): (socket.SOL_SOCKET, socket.SO_KEEPALIVE, 'int'),
    (0xffff, 0x0020): (socket.SOL_SOCKET, socket.SO_BROADCAST, 'int'),
    (0xffff, 0x0100): (socket.SOL_SOCKET, socket.SO_OOBINLINE, 'int'),
    (0xffff, 0x1001): (socket.SOL_SOCKET, socket.SO_SNDBUF, 'int'),
    (0xffff, 0x1002): (socket.SOL_SOCKET, socket.SO_RCVBUF, 'int'),
    (0xffff, 0x1007): (socket.SOL_SOCKET, socket.SO_ERROR, 'errno'),
    (0xffff, 0x1008): (socket.SOL_SOCKET, socket.SO_TYPE, 'int'),
    (6, 0x0001): (socket.IPPROTO_TCP, socket.TCP_NODELAY, 'int'),
}

MAX_SOCKETS = 64
MAX_RECV_LENGTH = 256 * 1024

# Returned in D0 to signal failure.
FAIL = 2**32 - 1

# An A314 stream message carries at most 252 bytes of payload (see
# a314device/driver_task.c), which limits the in-message read/write requests.
MAX_MSG_LENGTH = 252

def to_signed(value: int) -> int:
    return value - 2**32 if value & 0x80000000 else value

class StreamReset(Exception):
    '''The Amiga closed the library; the instance thread should exit.'''

class SockError(Exception):
    '''An error that is reported to the Amiga with a BSD errno value.'''
    def __init__(self, errno: int) -> None:
        super().__init__(errno)
        self.errno = errno

class AmSocket:
    def __init__(self, slot: int, sock: socket.socket) -> None:
        self.slot = slot
        self.sock = sock
        # Pi side sockets are always non-blocking, and blocking calls are
        # emulated using wait_ready(). This flag is the Amiga's view (FIONBIO).
        self.nonblocking = False
        sock.setblocking(False)

class LibInstance:
    def __init__(self, service: 'LibRemoteService', stream_id: int) -> None:
        self.service = service
        self.stream_id = stream_id
        self.queue = queue.Queue()
        self.notify_sock, self.wait_sock = socket.socketpair()

        self.wait_first_message = True
        self.bb_address = 0
        self.bb_size = 0

        self.break_mask = 4096 # SIGBREAKF_CTRL_C
        self.log_tag_ptr = 0
        self.errno_ptr = 0
        self.errno_size = 0
        self.herrno_ptr = 0
        self.sigio_mask = 0
        self.sigurg_mask = 0
        self.sigevent_mask = 0

        # Store the latest errno value locally, as it may be asked for.
        self.errno = 0
        self.herrno = 0

        # Signals received by the Amiga task during the current operation,
        # including those already set when the operation was called.
        self.pending_signals = 0

        self.return_mem_address = 0
        self.sockets: Dict[int, AmSocket] = {}

        self.thread = threading.Thread(target=self.run)
        self.thread.start()

    # Put items on queue.
    def process_stream_reset(self):
        self.queue.put((QITEM_RESET, None))
        self.notify_sock.send(b'#')

    def process_read_mem_complete(self, address: int, data: bytes):
        self.queue.put((QITEM_READ_BOUNCE_COMPLETE, data))
        self.notify_sock.send(b'#')

    def process_write_mem_complete(self, address: int):
        self.queue.put((QITEM_WRITE_BOUNCE_COMPLETE, None))
        self.notify_sock.send(b'#')

    def process_signals(self, signals: int):
        self.queue.put((QITEM_SIGNALS, signals))
        self.notify_sock.send(b'#')

    def process_op_req(self, op: int, args: bytes):
        self.queue.put((QITEM_OP_REQ, (op, args)))
        self.notify_sock.send(b'#')

    def process_alloc_mem_res(self, address: int):
        self.queue.put((QITEM_ALLOC_MEM_RES, address))
        self.notify_sock.send(b'#')

    def process_free_mem_res(self):
        self.queue.put((QITEM_FREE_MEM_RES, None))
        self.notify_sock.send(b'#')

    def process_read_mem_res(self, data: bytes):
        self.queue.put((QITEM_READ_MEM_RES, data))
        self.notify_sock.send(b'#')

    def process_write_mem_res(self):
        self.queue.put((QITEM_WRITE_MEM_RES, None))
        self.notify_sock.send(b'#')

    def process_copy_from_bounce_res(self):
        self.queue.put((QITEM_COPY_FROM_BOUNCE_RES, None))
        self.notify_sock.send(b'#')

    def process_copy_to_bounce_res(self):
        self.queue.put((QITEM_COPY_TO_BOUNCE_RES, None))
        self.notify_sock.send(b'#')

    def process_copy_str_to_bounce_res(self, length: int):
        self.queue.put((QITEM_COPY_STR_TO_BOUNCE_RES, length))
        self.notify_sock.send(b'#')

    def process_copy_tag_list_to_bounce_res(self, length: int):
        self.queue.put((QITEM_COPY_TAG_LIST_TO_BOUNCE_RES, length))
        self.notify_sock.send(b'#')

    # Wait for queue item.
    def wait_any_qitem(self, timeout: Optional[float] = None) -> Tuple[int, Any]:
        rl, _, _ = select.select([self.wait_sock], [], [], timeout)
        if self.wait_sock not in rl:
            raise queue.Empty()
        hash = self.wait_sock.recv(1)
        assert hash == b'#'
        return self.queue.get(block=False)

    def wait_qitem(self, match_qitem: int):
        while True:
            qitem, arg = self.wait_any_qitem()
            if qitem == QITEM_RESET:
                raise StreamReset()
            elif qitem == match_qitem:
                return arg
            elif qitem == QITEM_SIGNALS:
                self.pending_signals |= arg
            else:
                logger.warning('Unexpected queue item %s while waiting for %s', qitem, match_qitem)

    def wait_ready(self, rlist: List[socket.socket], wlist: List[socket.socket], xlist: List[socket.socket],
                   deadline: Optional[float], user_mask: int) -> Tuple[List[socket.socket], List[socket.socket], List[socket.socket], int]:
        '''Wait until a socket is ready, the deadline (time.monotonic()) passes,
        or a break signal or a signal in user_mask is received. Returns the
        ready sockets and the received signals that are in the masks.'''
        mask = self.break_mask | user_mask

        while True:
            if self.pending_signals & mask:
                timeout = 0.0
            elif deadline is None:
                timeout = None
            else:
                timeout = max(0.0, deadline - time.monotonic())

            rl, wl, xl = select.select(rlist + [self.wait_sock], wlist, xlist, timeout)

            if self.wait_sock in rl:
                rl.remove(self.wait_sock)
                qitem, arg = self.wait_any_qitem(0)
                if qitem == QITEM_RESET:
                    raise StreamReset()
                elif qitem == QITEM_SIGNALS:
                    self.pending_signals |= arg
                else:
                    logger.warning('Unexpected queue item %s while waiting for sockets', qitem)

            signals = self.pending_signals & mask
            if rl or wl or xl or signals or timeout == 0.0:
                return rl, wl, xl, signals

    def wait_socket(self, s: AmSocket, readable: bool) -> None:
        '''Block until s is readable/writable. Raises EINTR on a break signal.
        The break signal is not consumed, so it is still set on the Amiga.'''
        sl = [s.sock]
        rl, wl, _, signals = self.wait_ready(sl if readable else [], [] if readable else sl, [], None, 0)
        if not rl and not wl and signals:
            raise SockError(EINTR)

    def get_socket(self, sock: int) -> AmSocket:
        s = self.sockets.get(sock)
        if not s:
            raise SockError(EBADF)
        return s

    def alloc_slot(self) -> int:
        for slot in range(MAX_SOCKETS):
            if slot not in self.sockets:
                return slot
        raise SockError(EMFILE)

    def read_int_arg(self, address: int, length: int) -> int:
        '''Read an int option value of 1, 2 or 4 bytes.'''
        if length >= 4:
            (value,) = struct.unpack('>i', self.read_mem(address, 4))
        elif length == 2:
            (value,) = struct.unpack('>h', self.read_mem(address, 2))
        elif length == 1:
            (value,) = struct.unpack('>b', self.read_mem(address, 1))
        else:
            raise SockError(EINVAL)
        return value

    def read_sockaddr(self, address: int, length: int) -> Tuple[str, int]:
        '''Read a struct sockaddr_in. sin_len is ignored, as it is often 0.'''
        length = to_signed(length)
        if not address or length < 8:
            raise SockError(EINVAL)
        data = self.read_mem(address, min(length, 16))
        if data[1] != AF_INET:
            raise SockError(EAFNOSUPPORT)
        (port,) = struct.unpack('>H', data[2:4])
        return (socket.inet_ntoa(data[4:8]), port)

    def write_sockaddr(self, address: int, length_ptr: int, addr: Tuple[str, int]) -> None:
        '''Write a struct sockaddr_in, and its length to *length_ptr.'''
        if not address or not length_ptr:
            return
        (capacity,) = struct.unpack('>i', self.read_mem(length_ptr, 4))
        if capacity < 0:
            raise SockError(EINVAL)
        host, port = addr[0], addr[1]
        data = struct.pack('>BBH4s8x', 16, AF_INET, port, socket.inet_aton(host))
        writes = [(length_ptr, struct.pack('>I', len(data)))]
        if capacity:
            writes.append((address, data[:capacity]))
        self.multi_write_mem(writes)

    # Blocking functions to run commands on Amiga.
    def alloc_mem(self, length: int) -> int:
        self.service.send_alloc_mem_req(self.stream_id, length)
        return self.wait_qitem(QITEM_ALLOC_MEM_RES)

    def free_mem(self, address: int) -> int:
        self.service.send_free_mem_req(self.stream_id, address)
        return self.wait_qitem(QITEM_FREE_MEM_RES)

    def read_mem(self, address: int, length: int) -> bytes:
        self.service.send_read_mem_req(self.stream_id, [(address, length)])
        return self.wait_qitem(QITEM_READ_MEM_RES)

    def multi_read_mem(self, descs: List[Tuple[int, int]]) -> List[bytes]:
        self.service.send_read_mem_req(self.stream_id, descs)
        combined: bytes = self.wait_qitem(QITEM_READ_MEM_RES)
        bufs = []
        offset = 0
        for _, length in descs:
            bufs.append(combined[offset:offset + length])
            offset += length
        return bufs

    def write_mem(self, address: int, buf: bytes) -> None:
        self.multi_write_mem([(address, buf)])

    def multi_write_mem(self, descs: List[Tuple[int, bytes]]) -> None:
        self.service.send_write_mem_req(self.stream_id, descs)
        self.wait_qitem(QITEM_WRITE_MEM_RES)

    def copy_from_bounce(self, mem_address: int, bounce_address: int, length: int) -> None:
        self.multi_copy_from_bounce([(mem_address, bounce_address, length)])

    def multi_copy_from_bounce(self, copies: List[Tuple[int, int, int]]) -> None:
        self.service.send_copy_from_bounce_req(self.stream_id, copies)
        self.wait_qitem(QITEM_COPY_FROM_BOUNCE_RES)

    def copy_to_bounce(self, bounce_address: int, mem_address: int, length: int) -> None:
        self.multi_copy_to_bounce([(bounce_address, mem_address, length)])

    def multi_copy_to_bounce(self, copies: List[Tuple[int, int, int]]) -> None:
        self.service.send_copy_to_bounce_req(self.stream_id, copies)
        self.wait_qitem(QITEM_COPY_TO_BOUNCE_RES)

    def copy_str_to_bounce(self, bounce_address: int, str_address: int) -> int:
        self.service.send_copy_str_to_bounce_req(self.stream_id, bounce_address, str_address)
        return self.wait_qitem(QITEM_COPY_STR_TO_BOUNCE_RES)

    def copy_tag_list_to_bounce(self, bounce_address: int, tag_list_address: int) -> int:
        self.service.send_copy_tag_list_to_bounce_req(self.stream_id, bounce_address, tag_list_address)
        return self.wait_qitem(QITEM_COPY_TAG_LIST_TO_BOUNCE_RES)

    # Read from/write to bounce buffer.
    def read_bounce(self, address: int, length: int) -> bytes:
        self.service.start_read_mem(self.stream_id, address, length)
        return self.wait_qitem(QITEM_READ_BOUNCE_COMPLETE)

    def write_bounce(self, address: int, data: bytes) -> None:
        self.service.start_write_mem(self.stream_id, address, data)
        self.wait_qitem(QITEM_WRITE_BOUNCE_COMPLETE)

    def read_str(self, address: int) -> str:
        length = self.copy_str_to_bounce(self.bb_address, address)
        data = self.read_bounce(self.bb_address, length)
        return data.decode('latin-1')

    def read_tag_list(self, address: int) -> List[Tuple[int, int]]:
        length = self.copy_tag_list_to_bounce(self.bb_address, address)
        data = self.read_bounce(self.bb_address, length)
        arr = struct.unpack(f'>{len(data) // 4}I', data)
        return list(zip(arr[0::2], arr[1::2]))

    def set_errno(self, errno: Optional[int]):
        if errno is not None:
            self.errno = errno
            if self.errno_ptr:
                size = self.errno_size
                fmt = '>B' if size == 1 else ('>H' if size == 2 else '>I')
                data = struct.pack(fmt, errno)
                self.write_mem(self.errno_ptr, data)

    def set_herrno(self, herrno: int):
        self.herrno = herrno
        if self.herrno_ptr:
            self.write_mem(self.herrno_ptr, struct.pack('>I', herrno))

    def fail_op(self, op: int, errno: int):
        '''Report a failed operation to the Amiga.'''
        self.set_errno(errno)
        result = 0 if op in POINTER_OPS else FAIL
        self.service.send_op_res(self.stream_id, result, 0)

    # The implemented library functions.
    def handle_socket_op(self, domain: int, type_: int, protocol: int):
        logger.debug('handle_socket_op(domain=%s, type_=%s, protocol=%s)', domain, type_, protocol)

        if domain != AF_INET:
            raise SockError(EAFNOSUPPORT)

        if type_ == SOCK_STREAM:
            sock_type = socket.SOCK_STREAM
        elif type_ == SOCK_DGRAM:
            sock_type = socket.SOCK_DGRAM
        else:
            raise SockError(EPROTONOSUPPORT)

        slot = self.alloc_slot()
        self.sockets[slot] = AmSocket(slot, socket.socket(socket.AF_INET, sock_type))
        result = slot

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_bind_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_bind_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        s = self.get_socket(sock)
        addr = self.read_sockaddr(name, namelen)
        s.sock.bind(addr)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_listen_op(self, sock: int, backlog: int):
        logger.debug('handle_listen_op(sock=%s, backlog=%s)', sock, backlog)
        s = self.get_socket(sock)
        s.sock.listen(max(0, to_signed(backlog)))
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_accept_op(self, sock: int, addr: int, addrlen: int):
        logger.debug('handle_accept_op(sock=%s, addr=%s, addrlen=%s)', sock, addr, addrlen)
        s = self.get_socket(sock)

        while True:
            try:
                conn, peer = s.sock.accept()
                break
            except BlockingIOError:
                if s.nonblocking:
                    raise SockError(EWOULDBLOCK)
                self.wait_socket(s, True)

        try:
            slot = self.alloc_slot()
            self.write_sockaddr(addr, addrlen, peer)
        except BaseException:
            conn.close()
            raise

        new_s = AmSocket(slot, conn)
        # As in BSD, the accepted socket inherits the non-blocking mode.
        new_s.nonblocking = s.nonblocking
        self.sockets[slot] = new_s
        result = slot

        self.service.send_op_res(self.stream_id, result, 0)

    def handle_connect_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_connect_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        s = self.get_socket(sock)
        addr = self.read_sockaddr(name, namelen)

        err = s.sock.connect_ex(addr)

        if err == errno.EINPROGRESS:
            if s.nonblocking:
                raise SockError(EINPROGRESS)

            # Raises EINTR on a break signal; the connection attempt then
            # continues in the background, as in BSD.
            self.wait_socket(s, False)
            err = s.sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)

        if err:
            raise SockError(bsd_errno(err))

        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_sendto_op(self, sock: int, buf: int, len_: int, flags: int, to: int, tolen: int):
        logger.debug('handle_sendto_op(sock=%s, buf=%s, len_=%s, flags=%s, to=%s, tolen=%s)', sock, buf, len_, flags, to, tolen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_send_op(self, sock: int, buf: int, len_: int, flags: int):
        logger.debug('handle_send_op(sock=%s, buf=%s, len_=%s, flags=%s)', sock, buf, len_, flags)
        s = self.get_socket(sock)

        len_ = to_signed(len_)
        if len_ < 0:
            raise SockError(EINVAL)

        send_flags = socket.MSG_OOB if flags & MSG_OOB else 0
        nonblocking = s.nonblocking or (flags & MSG_DONTWAIT) != 0

        data = b''
        addr = buf

        while len(data) < len_:
            take = min(len_ - len(data), self.bb_size)
            self.copy_to_bounce(self.bb_address, addr, take)
            data += self.read_bounce(self.bb_address, take)
            addr += take

        # A blocking send sends all data, unless interrupted. If some data was
        # sent before an interruption then the partial count is returned.
        sent = 0
        while True:
            try:
                sent += s.sock.send(data[sent:], send_flags)
                if sent == len(data) or nonblocking:
                    break
            except BlockingIOError:
                if nonblocking:
                    if sent:
                        break
                    raise SockError(EWOULDBLOCK)
                try:
                    self.wait_socket(s, False)
                except SockError:
                    if sent:
                        break
                    raise
            except OSError:
                if sent:
                    break
                raise

        result = sent
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_recvfrom_op(self, sock: int, buf: int, len_: int, flags: int, addr: int, addrlen: int):
        logger.debug('handle_recvfrom_op(sock=%s, buf=%s, len_=%s, flags=%s, addr=%s, addrlen=%s)', sock, buf, len_, flags, addr, addrlen)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_recv_op(self, sock: int, buf: int, len_: int, flags: int):
        logger.debug('handle_recv_op(sock=%s, buf=%s, len_=%s, flags=%s)', sock, buf, len_, flags)
        s = self.get_socket(sock)

        len_ = to_signed(len_)
        if len_ < 0:
            raise SockError(EINVAL)

        recv_flags = 0
        if flags & MSG_PEEK:
            recv_flags |= socket.MSG_PEEK
        if flags & MSG_OOB:
            recv_flags |= socket.MSG_OOB
        nonblocking = s.nonblocking or (flags & MSG_DONTWAIT) != 0

        data = b''
        if len_:
            while True:
                try:
                    data = s.sock.recv(min(len_, MAX_RECV_LENGTH), recv_flags)
                    break
                except BlockingIOError:
                    if nonblocking:
                        raise SockError(EWOULDBLOCK)
                    self.wait_socket(s, True)

        addr = buf
        offset = 0

        while offset < len(data):
            take = min(len(data) - offset, self.bb_size)

            self.write_bounce(self.bb_address, data[offset:offset + take])
            self.copy_from_bounce(addr, self.bb_address, take)

            offset += take
            addr += take

        result = len(data)
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_shutdown_op(self, sock: int, how: int):
        logger.debug('handle_shutdown_op(sock=%s, how=%s)', sock, how)
        s = self.get_socket(sock)
        if how > 2:
            raise SockError(EINVAL)
        s.sock.shutdown(how)
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_setsockopt_op(self, sock: int, level: int, optname: int, optval: int, optlen: int):
        logger.debug('handle_setsockopt_op(sock=%s, level=%s, optname=%s, optval=%s, optlen=%s)', sock, level, optname, optval, optlen)
        s = self.get_socket(sock)

        opt = SOCKOPTS.get((level, optname))
        if not opt or opt[2] != 'int':
            logger.info('Unsupported socket option level=%s optname=%s', level, optname)
            raise SockError(ENOPROTOOPT)

        if not optval:
            raise SockError(EINVAL)
        value = self.read_int_arg(optval, to_signed(optlen))
        s.sock.setsockopt(opt[0], opt[1], value)

        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getsockopt_op(self, sock: int, level: int, optname: int, optval: int, optlen: int):
        logger.debug('handle_getsockopt_op(sock=%s, level=%s, optname=%s, optval=%s, optlen=%s)', sock, level, optname, optval, optlen)
        s = self.get_socket(sock)

        opt = SOCKOPTS.get((level, optname))
        if not opt:
            logger.info('Unsupported socket option level=%s optname=%s', level, optname)
            raise SockError(ENOPROTOOPT)

        if not optval or not optlen:
            raise SockError(EINVAL)
        (capacity,) = struct.unpack('>i', self.read_mem(optlen, 4))
        if capacity < 4:
            raise SockError(EINVAL)

        value = s.sock.getsockopt(opt[0], opt[1])
        if opt[2] == 'errno' and value:
            value = bsd_errno(value)

        self.multi_write_mem([(optval, struct.pack('>i', value)), (optlen, struct.pack('>I', 4))])

        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getsockname_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_getsockname_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        s = self.get_socket(sock)
        if not name or not namelen:
            raise SockError(EINVAL)
        self.write_sockaddr(name, namelen, s.sock.getsockname())
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getpeername_op(self, sock: int, name: int, namelen: int):
        logger.debug('handle_getpeername_op(sock=%s, name=%s, namelen=%s)', sock, name, namelen)
        s = self.get_socket(sock)
        if not name or not namelen:
            raise SockError(EINVAL)
        self.write_sockaddr(name, namelen, s.sock.getpeername())
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_ioctlsocket_op(self, sock: int, req: int, argp: int):
        logger.debug('handle_ioctlsocket_op(sock=%s, req=%s, argp=%s)', sock, req, argp)
        s = self.get_socket(sock)

        if not argp:
            raise SockError(EINVAL)

        if req == FIONBIO:
            (value,) = struct.unpack('>i', self.read_mem(argp, 4))
            s.nonblocking = value != 0
        elif req == FIONREAD:
            buf = bytearray(4)
            fcntl.ioctl(s.sock.fileno(), termios.FIONREAD, buf)
            (count,) = struct.unpack('=i', buf)
            self.write_mem(argp, struct.pack('>i', count))
        else:
            logger.info('Unsupported IoctlSocket request %#x', req)
            raise SockError(EINVAL)

        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_closesocket_op(self, sock: int):
        logger.debug('handle_closesocket_op(sock=%s)', sock)
        s = self.get_socket(sock)
        del self.sockets[sock]
        try:
            s.sock.close()
        except OSError:
            pass
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_waitselect_op(self, nfds: int, read_fds: int, write_fds: int, except_fds: int, _timeout: int, signals: int):
        logger.debug('handle_waitselect_op(nfds=%s, read_fds=%s, write_fds=%s, except_fds=%s, _timeout=%s, signals=%s)', nfds, read_fds, write_fds, except_fds, _timeout, signals)

        nfds = to_signed(nfds)
        if nfds < 0:
            raise SockError(EINVAL)

        # As allowed by the autodoc, nfds is truncated to the sockets supported.
        nfds = min(nfds, MAX_SOCKETS)
        nwords = (nfds + 31) // 32

        # fd_set is an array of 32-bit words, fd 0 is bit 0 of the first word.
        fd_set_ptrs = [read_fds, write_fds, except_fds]

        read_reqs = []
        for ptr in fd_set_ptrs:
            if ptr and nwords:
                read_reqs.append((ptr, nwords * 4))
        if _timeout:
            read_reqs.append((_timeout, 8))
        if signals:
            read_reqs.append((signals, 4))

        bufs = self.multi_read_mem(read_reqs) if read_reqs else []

        fd_masks = []
        for ptr in fd_set_ptrs:
            mask = 0
            if ptr and nwords:
                words = struct.unpack(f'>{nwords}I', bufs.pop(0))
                for i, word in enumerate(words):
                    mask |= word << (32 * i)
                mask &= (1 << nfds) - 1
            fd_masks.append(mask)

        deadline = None
        if _timeout:
            sec, usec = struct.unpack('>ii', bufs.pop(0))
            if sec < 0 or usec < 0 or usec >= 1000000:
                raise SockError(EINVAL)
            deadline = time.monotonic() + sec + usec / 1e6

        user_mask = 0
        if signals:
            (user_mask,) = struct.unpack('>I', bufs.pop(0))

        logger.debug('fd_masks=%s, deadline=%s, user_mask=%s', fd_masks, deadline, user_mask)

        all_fds = fd_masks[0] | fd_masks[1] | fd_masks[2]
        for fd in range(nfds):
            if all_fds & (1 << fd) and fd not in self.sockets:
                raise SockError(EBADF)

        lists = [[self.sockets[fd].sock for fd in range(nfds) if mask & (1 << fd)] for mask in fd_masks]

        rl, wl, xl, received = self.wait_ready(lists[0], lists[1], lists[2], deadline, user_mask)

        # Bits in both masks are attributed to the user signals.
        user_signals = received & user_mask
        if received & self.break_mask & ~user_mask:
            # The break signal is not consumed, so it is still set on the Amiga.
            raise SockError(EINTR)

        ready_masks = []
        for ready in (rl, wl, xl):
            ready_ids = set(id(sock) for sock in ready)
            mask = 0
            for fd in range(nfds):
                s = self.sockets.get(fd)
                if s and id(s.sock) in ready_ids:
                    mask |= 1 << fd
            ready_masks.append(mask)

        write_reqs = []
        for ptr, mask in zip(fd_set_ptrs, ready_masks):
            if ptr and nwords:
                words = [(mask >> (32 * i)) & 0xffffffff for i in range(nwords)]
                write_reqs.append((ptr, struct.pack(f'>{nwords}I', *words)))
        if signals:
            write_reqs.append((signals, struct.pack('>I', user_signals)))

        if write_reqs:
            self.multi_write_mem(write_reqs)

        result = sum(bin(mask).count('1') for mask in ready_masks)
        self.service.send_op_res(self.stream_id, result, user_signals)

    def handle_setsocketsignals_op(self, int_mask: int, io_mask: int, urgent_mask: int):
        logger.debug('handle_setsocketsignals_op(int_mask=%s, io_mask=%s, urgent_mask=%s)', int_mask, io_mask, urgent_mask)
        self.break_mask = int_mask
        self.sigio_mask = io_mask
        self.sigurg_mask = urgent_mask
        result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_getdtablesize_op(self):
        logger.debug('handle_getdtablesize_op()')
        result = MAX_SOCKETS
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
        result = self.errno
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_seterrnoptr_op(self, errno_ptr: int, size: int):
        logger.debug('handle_seterrnoptr_op(errno_ptr=%s, size=%s)', errno_ptr, size)
        if size not in (1, 2, 4):
            raise SockError(EINVAL)
        self.errno_ptr = errno_ptr
        self.errno_size = size
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
            self.write_mem(self.return_mem_address, data)

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
        data = self.read_str(name)

        # NOTE: The lookup blocks, and cannot be interrupted by a break signal.
        try:
            host_addr = socket.gethostbyname(data)
        except socket.gaierror as e:
            logger.debug('gethostbyname(%s) failed: %s', data, e)
            if e.errno in (socket.EAI_AGAIN,):
                self.set_herrno(TRY_AGAIN)
            elif e.errno in (socket.EAI_FAIL,):
                self.set_herrno(NO_RECOVERY)
            else:
                self.set_herrno(HOST_NOT_FOUND)
            self.service.send_op_res(self.stream_id, 0, 0)
            return
        logger.debug('host_addr=%s', host_addr)

        result = 0

        if not self.return_mem_address:
            self.return_mem_address = self.alloc_mem(512)

        if self.return_mem_address:
            # Create hostent struct.
            data = struct.pack('>IIIII', name, 0, 2, 4, self.return_mem_address + 20)
            data += struct.pack('>II', self.return_mem_address + 28, 0)
            data += bytes(map(int, host_addr.split('.')))

            self.write_mem(self.return_mem_address, data)

            result = self.return_mem_address

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

                if is_set:
                    if is_ref:
                        (value,) = struct.unpack('>I', self.read_mem(value, 4))
                    if not self.set_socketbase_value(key, value):
                        result = i + 1
                        break
                else:
                    current = self.get_socketbase_value(key)
                    if current is None:
                        result = i + 1
                        break
                    # A GETVAL replaces ti_Data, a GETREF writes to *ti_Data.
                    dst = value if is_ref else tags + i * 8 + 4
                    self.write_mem(dst, struct.pack('>I', current & 0xffffffff))
            elif key not in (TAG_DONE, TAG_MORE, TAG_IGNORE):
                logger.debug('Unknown tag: key=%s, value=%s', key, value)
                result = i + 1
                break

        self.service.send_op_res(self.stream_id, result, 0)

    def set_socketbase_value(self, code: int, value: int) -> bool:
        if code == SBTC_BREAKMASK:
            self.break_mask = value
        elif code == SBTC_SIGIOMASK:
            self.sigio_mask = value
        elif code == SBTC_SIGURGMASK:
            self.sigurg_mask = value
        elif code == SBTC_SIGEVENTMASK:
            self.sigevent_mask = value
        elif code == SBTC_ERRNO:
            self.errno = value
        elif code == SBTC_HERRNO:
            self.herrno = value
        elif code == SBTC_DTABLESIZE:
            # The table has a fixed size, larger requests are refused.
            return value <= MAX_SOCKETS
        elif code == SBTC_LOGTAGPTR:
            self.log_tag_ptr = value
        elif code == SBTC_ERRNOBYTEPTR:
            self.errno_ptr, self.errno_size = value, 1
        elif code == SBTC_ERRNOWORDPTR:
            self.errno_ptr, self.errno_size = value, 2
        elif code == SBTC_ERRNOLONGPTR:
            self.errno_ptr, self.errno_size = value, 4
        elif code == SBTC_HERRNOLONGPTR:
            self.herrno_ptr = value
        else:
            logger.info('Unsupported SocketBaseTagList set code %s', code)
            return False
        return True

    def get_socketbase_value(self, code: int) -> Optional[int]:
        if code == SBTC_BREAKMASK:
            return self.break_mask
        elif code == SBTC_SIGIOMASK:
            return self.sigio_mask
        elif code == SBTC_SIGURGMASK:
            return self.sigurg_mask
        elif code == SBTC_SIGEVENTMASK:
            return self.sigevent_mask
        elif code == SBTC_ERRNO:
            return self.errno
        elif code == SBTC_HERRNO:
            return self.herrno
        elif code == SBTC_DTABLESIZE:
            return MAX_SOCKETS
        elif code == SBTC_LOGTAGPTR:
            return self.log_tag_ptr
        logger.info('Unsupported SocketBaseTagList get code %s', code)
        return None

    def handle_getsocketevents_op(self, event_ptr: int):
        logger.debug('handle_getsocketevents_op(event_ptr=%s)', event_ptr)
        result = 2**32 - 1
        self.service.send_op_res(self.stream_id, result, 0)

    def dispatch_op(self, op: int, args: bytes):
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
        else:
            logger.warning('Unknown operation %s', op)
            raise SockError(EINVAL)

    # Dispatcher loop.
    def run(self):
        try:
            while True:
                op, args = self.wait_qitem(QITEM_OP_REQ)

                # The last ULONG is the signals that were set when the
                # operation was called. Older signals are not relevant.
                (self.pending_signals,) = struct.unpack('>I', args[-4:])
                args = args[:-4]

                try:
                    self.dispatch_op(op, args)
                except StreamReset:
                    raise
                except SockError as e:
                    logger.debug('Operation %s failed with errno %s', op, e.errno)
                    self.fail_op(op, e.errno)
                except OSError as e:
                    logger.debug('Operation %s failed: %s', op, e)
                    self.fail_op(op, bsd_errno(e.errno))
                except Exception:
                    logger.exception('Operation %s raised an exception', op)
                    self.fail_op(op, EINVAL)
        except StreamReset:
            logger.info('LibInstance thread was RESET')
        except:
            logger.exception('LibInstance thread crashed')
        finally:
            for s in self.sockets.values():
                try:
                    s.sock.close()
                except OSError:
                    pass
            self.sockets.clear()

        self.wait_sock.close()
        self.notify_sock.close()

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

    def send_op_res(self, stream_id: int, result: int, signals_consumed: int, copy_reqs: List[Tuple[int, int, int]] = None, write_reqs: List[Tuple[int, bytes]] = None):
        copy_reqs_count = 0 if copy_reqs is None else len(copy_reqs)
        data = struct.pack('>BBII', MSG_OP_RES, copy_reqs_count, result & 0xffffffff, signals_consumed)

        if copy_reqs:
            for dst, src, length in copy_reqs:
                data += struct.pack('>III', dst, src, length)

        if write_reqs:
            for dst, buf in write_reqs:
                data += struct.pack('>IB', dst, len(buf))
                if (len(buf) & 1) == 0:
                    data += b'\x00'
                data += buf

        assert len(data) <= MAX_MSG_LENGTH
        with self.send_lock:
            self.a314d.send_data(stream_id, data)

    def send_alloc_mem_req(self, stream_id: int, length: int):
        with self.send_lock:
            data = struct.pack('>BBI', MSG_ALLOC_MEM_REQ, 0, length)
            self.a314d.send_data(stream_id, data)

    def send_free_mem_req(self, stream_id: int, address: int):
        with self.send_lock:
            data = struct.pack('>BBI', MSG_FREE_MEM_REQ, 0, address)
            self.a314d.send_data(stream_id, data)

    def send_read_mem_req(self, stream_id: int, descs: List[Tuple[int, int]]):
        # The response carries the data after a two byte header.
        assert sum(length for _, length in descs) <= MAX_MSG_LENGTH - 2
        data = struct.pack('>BB', MSG_READ_MEM_REQ, 0)
        for src, length in descs:
            data += struct.pack('>IBB', src, length, 0)
        assert len(data) <= MAX_MSG_LENGTH

        with self.send_lock:
            self.a314d.send_data(stream_id, data)

    def send_write_mem_req(self, stream_id: int, descs: List[Tuple[int, bytes]]):
        data = struct.pack('>BB', MSG_WRITE_MEM_REQ, 0)
        for dst, buf in descs:
            data += struct.pack('>IB', dst, len(buf))
            if (len(buf) & 1) == 0:
                data += b'\x00'
            data += buf

        assert len(data) <= MAX_MSG_LENGTH
        with self.send_lock:
            self.a314d.send_data(stream_id, data)

    def send_copy_from_bounce_req(self, stream_id: int, copies: List[Tuple[int, int, int]]):
        data = struct.pack('>BB', MSG_COPY_FROM_BOUNCE_REQ, len(copies))
        for dst, src, length in copies:
            data += struct.pack('>III', dst, src, length)

        with self.send_lock:
            self.a314d.send_data(stream_id, data)

    def send_copy_to_bounce_req(self, stream_id: int, copies: List[Tuple[int, int, int]]):
        data = struct.pack('>BB', MSG_COPY_TO_BOUNCE_REQ, len(copies))
        for dst, src, length in copies:
            data += struct.pack('>III', dst, src, length)

        with self.send_lock:
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
        elif kind == MSG_READ_MEM_RES:
            inst.process_read_mem_res(payload[2:])
        elif kind == MSG_WRITE_MEM_RES:
            inst.process_write_mem_res()
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
            return False

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

        return True

    def run(self):
        logger.info('LibRemote service started')

        while True:
            try:
                rl, _, _ = select.select([self.a314d], [], [], 10.0)
            except KeyboardInterrupt:
                break

            if self.a314d in rl:
                if not self.handle_a314d_readable():
                    break

        # Stop the instance threads, so that their sockets are closed.
        instances = list(self.instances.values())
        self.instances.clear()
        for inst in instances:
            inst.process_stream_reset()
        for inst in instances:
            inst.thread.join(5.0)

        self.a314d.close()

        logger.info('LibRemote service stopped')

if __name__ == '__main__':
    service = LibRemoteService()
    service.run()
