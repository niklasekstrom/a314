#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Tests for bsdsocket.py that run without Amiga hardware.
#
# FakeAmiga stands in for a314d and for library.c on the Amiga side. It keeps
# the Amiga memory in a bytearray, and answers the requests from the Pi side
# in the same way as send_request() in library.c does. The tests then use
# real Linux sockets on localhost.
#
# Run with: python3 -m unittest test_bsdsocket.py

import os
import queue
import socket
import struct
import sys
import threading
import time
import unittest
import logging

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'a314d'))

import bsdsocket as bsl
from a314d import A314d

logging.getLogger().setLevel(logging.WARNING)

STREAM_ID = 1
MEM_SIZE = 256 * 1024
BOUNCE_SIZE = 16 * 1024

SIGBREAKF_CTRL_C = 1 << 12
SIG_USER = 1 << 20

TAG_USER = 0x80000000

def sbtm_setval(code):
    return TAG_USER | (code << 1) | 1

def sbtm_getval(code):
    return TAG_USER | (code << 1)

class FakeA314d:
    MSG_CONNECT = A314d.MSG_CONNECT
    MSG_DATA = A314d.MSG_DATA
    MSG_EOS = A314d.MSG_EOS
    MSG_RESET = A314d.MSG_RESET
    MSG_READ_MEM_RES = A314d.MSG_READ_MEM_RES
    MSG_WRITE_MEM_RES = A314d.MSG_WRITE_MEM_RES

    def __init__(self, inbox: queue.Queue):
        self.inbox = inbox

    def send_data(self, stream_id, data):
        self.inbox.put(('data', bytes(data)))

    def send_read_mem_req(self, address, length):
        self.inbox.put(('read', address, length))

    def send_write_mem_req(self, address, data):
        self.inbox.put(('write', address, bytes(data)))

    def send_connect_response(self, stream_id, result):
        pass

    def close(self):
        pass

class FakeService(bsl.LibRemoteService):
    def __init__(self, inbox: queue.Queue):
        self.a314d = FakeA314d(inbox)
        self.instances = {}
        self.read_mem_queue = []
        self.write_mem_queue = []
        self.send_lock = threading.Lock()
        self.rbuf = b''

class FakeAmiga:
    def __init__(self):
        self.inbox = queue.Queue()
        self.mem = bytearray(MEM_SIZE)
        self.next_free = 0x1000
        self.task_signals = 0
        self.reset_on_mem_req = False
        self.was_reset = False

        self.service = FakeService(self.inbox)
        self.bb = self.alloc(BOUNCE_SIZE)
        self.service.process_stream_connect(STREAM_ID, b'bsl')
        self.inst = self.service.instances[STREAM_ID]
        self.service.process_stream_data(STREAM_ID, struct.pack('>II', self.bb, BOUNCE_SIZE))

    # Memory helpers.
    def alloc(self, length):
        address = self.next_free
        self.next_free += (length + 3) & ~3
        assert self.next_free <= MEM_SIZE
        return address

    def put(self, data):
        address = self.alloc(len(data))
        self.mem[address:address + len(data)] = data
        return address

    def put_long(self, value):
        return self.put(struct.pack('>I', value & 0xffffffff))

    def get(self, address, length):
        return bytes(self.mem[address:address + length])

    def get_long(self, address):
        return struct.unpack('>I', self.get(address, 4))[0]

    def put_str(self, s):
        return self.put(s.encode('latin-1') + b'\0')

    def get_str(self, address):
        end = self.mem.index(0, address)
        return self.mem[address:end].decode('latin-1')

    def sockaddr(self, host, port, sin_len=16):
        return self.put(struct.pack('>BBH4s8x', sin_len, bsl.AF_INET, port, socket.inet_aton(host)))

    # Signals sent from another thread while a call is in progress.
    def signal_later(self, signals, delay):
        threading.Timer(delay, lambda: self.inbox.put(('signal', signals))).start()

    def reset_later(self, delay):
        threading.Timer(delay, lambda: self.inbox.put(('reset',))).start()

    def reset(self):
        self.service.process_stream_reset(STREAM_ID)
        self.was_reset = True

    # Corresponds to send_request() in library.c.
    def call(self, op, *args, pre_signals=0, timeout=10.0):
        assert not self.was_reset

        self.task_signals &= ~pre_signals
        signals_received = pre_signals

        payload = struct.pack('>BB', bsl.MSG_OP_REQ, op)
        payload += struct.pack(f'>{len(args)}I', *(a & 0xffffffff for a in args))
        payload += struct.pack('>I', pre_signals)
        assert len(payload) <= bsl.MAX_MSG_LENGTH
        self.service.process_stream_data(STREAM_ID, payload)

        # library.c also forwards the already set signals in MSG_SIGNALS.
        if pre_signals:
            self.service.process_stream_data(STREAM_ID, struct.pack('>BBI', bsl.MSG_SIGNALS, 0, pre_signals))

        while True:
            item = self.inbox.get(timeout=timeout)
            kind = item[0]

            if kind == 'signal':
                signals_received |= item[1]
                self.service.process_stream_data(STREAM_ID, struct.pack('>BBI', bsl.MSG_SIGNALS, 0, item[1]))
            elif kind == 'reset':
                self.reset()
                return None
            elif kind == 'read':
                _, address, length = item
                self.service.process_read_mem_res(self.get(address, length))
            elif kind == 'write':
                _, address, data = item
                self.mem[address:address + len(data)] = data
                self.service.process_write_mem_res()
            elif kind == 'data':
                msg = item[1]
                assert len(msg) <= bsl.MAX_MSG_LENGTH, len(msg)
                if msg[0] == bsl.MSG_OP_RES:
                    result, consumed = struct.unpack('>II', msg[2:10])
                    assert msg[1] == 0 and len(msg) == 10
                    self.task_signals |= signals_received & ~consumed
                    self.consumed = consumed
                    return result
                if self.reset_on_mem_req:
                    self.reset()
                    return None
                self.handle_mem_req(msg)

    def handle_mem_req(self, msg):
        kind = msg[0]
        if kind == bsl.MSG_READ_MEM_REQ:
            res = bytes([bsl.MSG_READ_MEM_RES, 0])
            pos = 2
            while pos < len(msg):
                src, length = struct.unpack('>IB', msg[pos:pos + 5])
                pos += 6
                res += self.get(src, length)
            assert len(res) <= bsl.MAX_MSG_LENGTH
        elif kind == bsl.MSG_WRITE_MEM_REQ:
            pos = 2
            while pos < len(msg):
                dst, length = struct.unpack('>IB', msg[pos:pos + 5])
                pos += 5
                if (length & 1) == 0:
                    pos += 1
                self.mem[dst:dst + length] = msg[pos:pos + length]
                pos += length
            res = bytes([bsl.MSG_WRITE_MEM_RES])
        elif kind == bsl.MSG_COPY_FROM_BOUNCE_REQ or kind == bsl.MSG_COPY_TO_BOUNCE_REQ:
            for i in range(msg[1]):
                dst, src, length = struct.unpack('>III', msg[2 + i * 12:14 + i * 12])
                self.mem[dst:dst + length] = self.mem[src:src + length]
            res = bytes([kind + 1])
        elif kind == bsl.MSG_COPY_STR_TO_BOUNCE_REQ:
            bounce, src = struct.unpack('>II', msg[2:10])
            length = self.mem.index(0, src) - src
            self.mem[bounce:bounce + length] = self.mem[src:src + length]
            res = struct.pack('>BBI', kind + 1, 0, length)
        elif kind == bsl.MSG_COPY_TAG_LIST_TO_BOUNCE_REQ:
            bounce, src = struct.unpack('>II', msg[2:10])
            pos = src
            while self.get_long(pos) not in (0, 2): # TAG_DONE, TAG_MORE
                pos += 8
            length = pos + 8 - src
            self.mem[bounce:bounce + length] = self.mem[src:src + length]
            res = struct.pack('>BBI', kind + 1, 0, length)
        elif kind == bsl.MSG_ALLOC_MEM_REQ:
            (length,) = struct.unpack('>I', msg[2:6])
            res = struct.pack('>BBI', bsl.MSG_ALLOC_MEM_RES, 0, self.alloc(length))
        elif kind == bsl.MSG_FREE_MEM_REQ:
            res = bytes([bsl.MSG_FREE_MEM_RES])
        else:
            raise AssertionError(f'Unexpected message kind {kind}')
        self.service.process_stream_data(STREAM_ID, res)

    def close(self):
        if not self.was_reset:
            self.reset()
        self.inst.thread.join(5.0)
        assert not self.inst.thread.is_alive()

    # Wrappers for some library functions.
    def errno(self):
        return self.call(bsl.FN_ERRNO)

    def socket(self):
        fd = self.call(bsl.FN_SOCKET, bsl.AF_INET, bsl.SOCK_STREAM, 0)
        assert fd != bsl.FAIL
        return fd

    def listener(self, host='127.0.0.1', port=0, backlog=5):
        fd = self.socket()
        assert self.call(bsl.FN_BIND, fd, self.sockaddr(host, port), 16) == 0
        assert self.call(bsl.FN_LISTEN, fd, backlog) == 0
        return fd, self.getsockname(fd)[1]

    def getsockname(self, fd):
        name = self.alloc(16)
        namelen = self.put_long(16)
        assert self.call(bsl.FN_GETSOCKNAME, fd, name, namelen) == 0
        data = self.get(name, 16)
        return socket.inet_ntoa(data[4:8]), struct.unpack('>H', data[2:4])[0]

    def ioctl_nbio(self, fd, on=True):
        assert self.call(bsl.FN_IOCTLSOCKET, fd, bsl.FIONBIO, self.put_long(1 if on else 0)) == 0

    def waitselect(self, nfds, rfds=None, wfds=None, timeout=None, sigmask=None, pre_signals=0, nwords=2):
        def fdset(fds):
            if fds is None:
                return 0
            mask = 0
            for fd in fds:
                mask |= 1 << fd
            return self.put(struct.pack(f'>{nwords}I', *((mask >> (32 * i)) & 0xffffffff for i in range(nwords))))

        def fds_of(ptr):
            if not ptr:
                return None
            words = struct.unpack(f'>{nwords}I', self.get(ptr, nwords * 4))
            mask = sum(w << (32 * i) for i, w in enumerate(words))
            return {fd for fd in range(nwords * 32) if mask & (1 << fd)}

        rptr = fdset(rfds)
        wptr = fdset(wfds)
        tptr = 0
        if timeout is not None:
            sec = int(timeout)
            tptr = self.put(struct.pack('>II', sec, int((timeout - sec) * 1e6)))
        sptr = self.put_long(sigmask) if sigmask is not None else 0

        result = self.call(bsl.FN_WAITSELECT, nfds, rptr, wptr, 0, tptr, sptr, pre_signals=pre_signals)
        sigs = self.get_long(sptr) if sptr else None
        return bsl.to_signed(result), fds_of(rptr), fds_of(wptr), sigs

    def recv_all(self, fd, length):
        buf = self.alloc(length)
        got = 0
        while got < length:
            n = self.call(bsl.FN_RECV, fd, buf + got, length - got, 0)
            assert n not in (0, bsl.FAIL), n
            got += n
        return self.get(buf, length)

class BsdSocketTest(unittest.TestCase):
    def setUp(self):
        self.amiga = FakeAmiga()
        self.clients = []

    def tearDown(self):
        self.amiga.close()
        for c in self.clients:
            c.close()

    def client(self, port):
        c = socket.create_connection(('127.0.0.1', port), timeout=5)
        self.clients.append(c)
        return c

    def port_is_free(self, port):
        # With SO_REUSEADDR, as amiagent does, so that TIME_WAIT is no problem
        # but a listening socket is.
        s = socket.socket()
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(('127.0.0.1', port))
            return True
        except OSError:
            return False
        finally:
            s.close()

    def test_server_round_trip(self):
        a = self.amiga
        fd = a.socket()
        one = a.put_long(1)
        self.assertEqual(a.call(bsl.FN_SETSOCKOPT, fd, 0xffff, 0x4, one, 4), 0)
        # amiagent leaves sin_len as zero.
        self.assertEqual(a.call(bsl.FN_BIND, fd, a.sockaddr('127.0.0.1', 0, sin_len=0), 16), 0)
        self.assertEqual(a.call(bsl.FN_LISTEN, fd, 2), 0)
        _, port = a.getsockname(fd)

        c = self.client(port)

        n, rfds, _, sigs = a.waitselect(fd + 1, rfds=[fd], timeout=2.0, sigmask=SIGBREAKF_CTRL_C | SIG_USER)
        self.assertEqual((n, rfds, sigs), (1, {fd}, 0))

        addr = a.alloc(16)
        addrlen = a.put_long(16)
        cfd = a.call(bsl.FN_ACCEPT, fd, addr, addrlen)
        self.assertNotEqual(cfd, bsl.FAIL)
        self.assertEqual(a.get_long(addrlen), 16)
        data = a.get(addr, 16)
        self.assertEqual(data[0:2], bytes([16, bsl.AF_INET]))
        self.assertEqual(struct.unpack('>H', data[2:4])[0], c.getsockname()[1])

        (ip,) = struct.unpack('>I', data[4:8])
        self.assertEqual(a.get_str(a.call(bsl.FN_INET_NTOA, ip)), '127.0.0.1')

        payload = os.urandom(40000)
        c.sendall(payload)
        self.assertEqual(a.recv_all(cfd, len(payload)), payload)

        reply = os.urandom(40000)
        self.assertEqual(a.call(bsl.FN_SEND, cfd, a.put(reply), len(reply), 0), len(reply))
        got = b''
        while len(got) < len(reply):
            got += c.recv(65536)
        self.assertEqual(got, reply)

        self.assertEqual(a.call(bsl.FN_CLOSESOCKET, cfd), 0)
        self.assertEqual(c.recv(10), b'')
        self.assertEqual(a.call(bsl.FN_CLOSESOCKET, fd), 0)
        self.assertTrue(self.port_is_free(port))

    def test_accept_null_addr(self):
        a = self.amiga
        fd, port = a.listener()
        self.client(port)
        cfd = a.call(bsl.FN_ACCEPT, fd, 0, 0)
        self.assertNotIn(cfd, (bsl.FAIL, fd))

    def test_accept_short_addrlen(self):
        a = self.amiga
        fd, port = a.listener()
        self.client(port)
        addr = a.put(b'\xee' * 16)
        addrlen = a.put_long(4)
        a.call(bsl.FN_ACCEPT, fd, addr, addrlen)
        self.assertEqual(a.get(addr, 16)[4:], b'\xee' * 12)
        self.assertEqual(a.get_long(addrlen), 16)

    def test_bind_in_use(self):
        a = self.amiga
        _, port = a.listener()
        fd = a.socket()
        self.assertEqual(a.call(bsl.FN_BIND, fd, a.sockaddr('127.0.0.1', port), 16), bsl.FAIL)
        self.assertEqual(a.errno(), 48) # EADDRINUSE

    def test_waitselect_timeout(self):
        a = self.amiga
        fd, _ = a.listener()
        start = time.monotonic()
        n, rfds, _, _ = a.waitselect(fd + 1, rfds=[fd], timeout=0.3)
        self.assertGreaterEqual(time.monotonic() - start, 0.3)
        self.assertEqual((n, rfds), (0, set()))

    def test_waitselect_poll(self):
        a = self.amiga
        fd, _ = a.listener()
        n, rfds, _, _ = a.waitselect(fd + 1, rfds=[fd], timeout=0)
        self.assertEqual((n, rfds), (0, set()))

    def test_waitselect_preset_user_signal(self):
        a = self.amiga
        fd, _ = a.listener()
        n, _, _, sigs = a.waitselect(fd + 1, rfds=[fd], timeout=0, sigmask=SIG_USER, pre_signals=SIG_USER)
        self.assertEqual((n, sigs), (0, SIG_USER))
        self.assertEqual(a.consumed, SIG_USER)
        self.assertEqual(a.task_signals & SIG_USER, 0)

    def test_waitselect_late_user_signal(self):
        a = self.amiga
        fd, _ = a.listener()
        a.signal_later(SIG_USER, 0.2)
        start = time.monotonic()
        n, _, _, sigs = a.waitselect(fd + 1, rfds=[fd], timeout=5, sigmask=SIG_USER)
        self.assertLess(time.monotonic() - start, 4)
        self.assertEqual((n, sigs), (0, SIG_USER))

    def test_waitselect_other_signals_are_reposted(self):
        a = self.amiga
        fd, _ = a.listener()
        n, _, _, sigs = a.waitselect(fd + 1, rfds=[fd], timeout=0, sigmask=SIG_USER, pre_signals=1 << 8)
        self.assertEqual((n, sigs), (0, 0))
        self.assertEqual(a.task_signals, 1 << 8)

    def test_waitselect_break(self):
        a = self.amiga
        fd, _ = a.listener()
        n, _, _, _ = a.waitselect(fd + 1, rfds=[fd], timeout=5, sigmask=SIG_USER, pre_signals=SIGBREAKF_CTRL_C)
        self.assertEqual(n, -1)
        self.assertEqual(a.consumed, 0)
        self.assertEqual(a.task_signals, SIGBREAKF_CTRL_C)
        self.assertEqual(a.errno(), bsl.EINTR)

    def test_waitselect_break_in_user_mask(self):
        # As amiagent does: Ctrl-C in the user mask is returned, not EINTR.
        a = self.amiga
        fd, _ = a.listener()
        a.signal_later(SIGBREAKF_CTRL_C, 0.2)
        n, _, _, sigs = a.waitselect(fd + 1, rfds=[fd], timeout=5, sigmask=SIGBREAKF_CTRL_C | SIG_USER)
        self.assertEqual((n, sigs), (0, SIGBREAKF_CTRL_C))
        self.assertEqual(a.consumed, SIGBREAKF_CTRL_C)

    def test_waitselect_high_fds(self):
        a = self.amiga
        fds = [a.socket() for _ in range(bsl.MAX_SOCKETS)]
        self.assertEqual(fds, list(range(bsl.MAX_SOCKETS)))
        self.assertEqual(a.call(bsl.FN_SOCKET, bsl.AF_INET, bsl.SOCK_STREAM, 0), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.EMFILE)

        for fd in (32, 63):
            self.assertEqual(a.call(bsl.FN_BIND, fd, a.sockaddr('127.0.0.1', 0), 16), 0)
            self.assertEqual(a.call(bsl.FN_LISTEN, fd, 1), 0)
        self.client(a.getsockname(63)[1])

        n, rfds, _, _ = a.waitselect(64, rfds=[32, 63], timeout=2)
        self.assertEqual((n, rfds), (1, {63}))

    def test_waitselect_bad_fd(self):
        a = self.amiga
        n, rfds, _, _ = a.waitselect(10, rfds=[7], timeout=0)
        self.assertEqual(n, -1)
        self.assertEqual(rfds, {7})
        self.assertEqual(a.errno(), bsl.EBADF)

    def test_waitselect_counts_bits(self):
        a = self.amiga
        fd, port = a.listener()
        self.client(port)
        cfd = a.call(bsl.FN_ACCEPT, fd, 0, 0)
        self.clients[0].sendall(b'x')
        time.sleep(0.1)
        n, rfds, wfds, _ = a.waitselect(cfd + 1, rfds=[cfd], wfds=[cfd], timeout=1)
        self.assertEqual((n, rfds, wfds), (2, {cfd}, {cfd}))

    def test_blocking_recv_break(self):
        a = self.amiga
        fd, port = a.listener()
        self.client(port)
        cfd = a.call(bsl.FN_ACCEPT, fd, 0, 0)
        a.signal_later(SIGBREAKF_CTRL_C, 0.2)
        self.assertEqual(a.call(bsl.FN_RECV, cfd, a.alloc(100), 100, 0), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.EINTR)
        self.assertEqual(a.task_signals, SIGBREAKF_CTRL_C)

    def test_nonblocking_recv_and_accept(self):
        a = self.amiga
        fd, port = a.listener()
        a.ioctl_nbio(fd)
        self.assertEqual(a.call(bsl.FN_ACCEPT, fd, 0, 0), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.EWOULDBLOCK)

        self.client(port)
        time.sleep(0.1)
        cfd = a.call(bsl.FN_ACCEPT, fd, 0, 0)
        self.assertNotEqual(cfd, bsl.FAIL)
        self.assertEqual(a.call(bsl.FN_RECV, cfd, a.alloc(100), 100, 0), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.EWOULDBLOCK)

        nread = a.put_long(0)
        self.clients[0].sendall(b'hello')
        time.sleep(0.1)
        self.assertEqual(a.call(bsl.FN_IOCTLSOCKET, cfd, bsl.FIONREAD, nread), 0)
        self.assertEqual(a.get_long(nread), 5)

    def test_nonblocking_connect(self):
        a = self.amiga
        server = socket.socket()
        self.clients.append(server)
        server.bind(('127.0.0.1', 0))
        server.listen(1)

        fd = a.socket()
        a.ioctl_nbio(fd)
        result = a.call(bsl.FN_CONNECT, fd, a.sockaddr('127.0.0.1', server.getsockname()[1]), 16)
        if result == bsl.FAIL:
            self.assertEqual(a.errno(), bsl.EINPROGRESS)

        n, _, wfds, _ = a.waitselect(fd + 1, wfds=[fd], timeout=2)
        self.assertEqual((n, wfds), (1, {fd}))

        optval = a.put_long(12345)
        optlen = a.put_long(4)
        self.assertEqual(a.call(bsl.FN_GETSOCKOPT, fd, 0xffff, 0x1007, optval, optlen), 0)
        self.assertEqual(a.get_long(optval), 0)

    def test_connect_refused(self):
        a = self.amiga
        s = socket.socket()
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
        s.close()

        fd = a.socket()
        self.assertEqual(a.call(bsl.FN_CONNECT, fd, a.sockaddr('127.0.0.1', port), 16), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.ECONNREFUSED)

    def test_bad_fd_keeps_thread_alive(self):
        a = self.amiga
        self.assertEqual(a.call(bsl.FN_RECV, 5, a.alloc(10), 10, 0), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.EBADF)
        self.assertEqual(a.call(bsl.FN_GETDTABLESIZE), bsl.MAX_SOCKETS)

    def test_unknown_sockopt(self):
        a = self.amiga
        fd = a.socket()
        self.assertEqual(a.call(bsl.FN_SETSOCKOPT, fd, 0xffff, 0x80, a.put_long(1), 8), bsl.FAIL)
        self.assertEqual(a.errno(), bsl.ENOPROTOOPT)

    def test_errno_pointers(self):
        a = self.amiga
        errno_long = a.put_long(0)
        tags = a.put(struct.pack('>IIIIII',
            sbtm_setval(bsl.SBTC_ERRNOLONGPTR), errno_long,
            sbtm_getval(bsl.SBTC_DTABLESIZE), 0,
            0, 0))
        self.assertEqual(a.call(bsl.FN_SOCKETBASETAGLIST, tags), 0)
        self.assertEqual(a.get_long(tags + 12), bsl.MAX_SOCKETS)

        a.call(bsl.FN_CLOSESOCKET, 9)
        self.assertEqual(a.get_long(errno_long), bsl.EBADF)

        errno_byte = a.put(b'\0')
        self.assertEqual(a.call(bsl.FN_SETERRNOPTR, errno_byte, 1), 0)
        fd = a.socket()
        a.call(bsl.FN_CONNECT, fd, a.sockaddr('127.0.0.1', 1), 16)
        self.assertEqual(a.get(errno_byte, 1)[0], bsl.ECONNREFUSED)

    def test_gethostbyname_failure(self):
        a = self.amiga
        herrno = a.put_long(0)
        tags = a.put(struct.pack('>IIII', sbtm_setval(bsl.SBTC_HERRNOLONGPTR), herrno, 0, 0))
        self.assertEqual(a.call(bsl.FN_SOCKETBASETAGLIST, tags), 0)
        self.assertEqual(a.call(bsl.FN_GETHOSTBYNAME, a.put_str('no-such-host.invalid')), 0)
        self.assertNotEqual(a.get_long(herrno), 0)

    def test_reset_during_accept(self):
        a = self.amiga
        fd, port = a.listener()
        a.reset_later(0.2)
        self.assertIsNone(a.call(bsl.FN_ACCEPT, fd, 0, 0))
        a.inst.thread.join(5.0)
        self.assertFalse(a.inst.thread.is_alive())
        self.assertTrue(self.port_is_free(port))

    def test_reset_during_memory_io(self):
        a = self.amiga
        fd = a.socket()
        a.reset_on_mem_req = True
        self.assertIsNone(a.call(bsl.FN_BIND, fd, a.sockaddr('127.0.0.1', 0), 16))
        a.inst.thread.join(5.0)
        self.assertFalse(a.inst.thread.is_alive())

if __name__ == '__main__':
    unittest.main()
