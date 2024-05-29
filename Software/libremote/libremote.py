#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2024 Niklas Ekström

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)

from a314d import A314d
import queue
import select
import struct
import threading
import time
from typing import Dict

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

QITEM_SIGNALS = 1
QITEM_OP_REQ = 2
QITEM_ALLOC_MEM_RES = 3
QITEM_FREE_MEM_RES = 4
QITEM_COPY_FROM_BOUNCE_RES = 5
QITEM_COPY_TO_BOUNCE_RES = 6
QITEM_COPY_STR_TO_BOUNCE_RES = 7
QITEM_READ_MEM_COMPLETE = 8
QITEM_WRITE_MEM_COMPLETE = 9
QITEM_RESET = 10

SERVICE_NAME = 'example'

FN_EVALEXPR = 0
FN_SLEEP = 1

SIGBREAKB_CTRL_C = 12
SIGBREAKB_CTRL_D = 13
SIGBREAKB_CTRL_E = 14
SIGBREAKB_CTRL_F = 15
SIGBREAKF_CTRL_C = (1 << SIGBREAKB_CTRL_C)
SIGBREAKF_CTRL_D = (1 << SIGBREAKB_CTRL_D)
SIGBREAKF_CTRL_E = (1 << SIGBREAKB_CTRL_E)
SIGBREAKF_CTRL_F = (1 << SIGBREAKB_CTRL_F)

class LibInstance:
    def __init__(self, service: 'LibRemoteService', stream_id: int) -> None:
        self.service = service
        self.stream_id = stream_id
        self.queue = queue.Queue()

        self.wait_first_message = True
        self.bb_address = 0
        self.bb_size = 0

        threading.Thread(target=self.run).start()

    # Put items on queue.
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

    def process_read_mem_complete(self, address: int, data: bytes):
        self.queue.put((QITEM_READ_MEM_COMPLETE, data))

    def process_write_mem_complete(self, address: int):
        self.queue.put((QITEM_WRITE_MEM_COMPLETE, None))

    def process_stream_reset(self):
        self.queue.put((QITEM_RESET, None))

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

    # The implemented library functions.
    def handle_evalexpr_op(self, str_address: int):
        expr = self.read_str(str_address)
        try:
            result = eval(expr)
        except:
            result = 0
        self.service.send_op_res(self.stream_id, result, 0)

    def handle_sleep_op(self, secs: int):
        signals_consumed = self.wait_signals(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D, secs)
        result = signals_consumed
        self.service.send_op_res(self.stream_id, result, signals_consumed)

    # Dispatcher loop.
    def run(self):
        try:
            while True:
                op, args = self.wait_qitem(QITEM_OP_REQ)
                if op == FN_EVALEXPR:
                    (str_address,) = struct.unpack('>I', args)
                    self.handle_evalexpr_op(str_address)
                elif op == FN_SLEEP:
                    (secs,) = struct.unpack('>I', args)
                    self.handle_sleep_op(secs)
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
