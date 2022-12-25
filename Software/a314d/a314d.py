# -*- coding: utf-8 -*-

# Copyright (c) 2022 Niklas Ekström

import logging
import socket
import struct
import sys

logger = logging.getLogger(__name__)

class A314d(object):
    MSG_REGISTER_REQ        = 1
    MSG_REGISTER_RES        = 2
    MSG_DEREGISTER_REQ      = 3
    MSG_DEREGISTER_RES      = 4
    MSG_READ_MEM_REQ        = 5
    MSG_READ_MEM_RES        = 6
    MSG_WRITE_MEM_REQ       = 7
    MSG_WRITE_MEM_RES       = 8
    MSG_CONNECT             = 9
    MSG_CONNECT_RESPONSE    = 10
    MSG_DATA                = 11
    MSG_EOS                 = 12
    MSG_RESET               = 13

    def __init__(self, service_name):
        if '-ondemand' in sys.argv:
            index = sys.argv.index('-ondemand')
            fd = int(sys.argv[index + 1])
            self.drv = socket.socket(fileno=fd)
        else:
            self.drv = socket.socket()
            self.drv.connect(('localhost', 7110))
            self.drv.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            self.send_register_req(service_name.encode())
            _, _, payload = self.wait_for_msg()
            success = payload[0] == 1
            if not success:
                raise RuntimeError(f'Unable to register service "{service_name}" with a314d')

    def close(self):
        self.drv.close()

    def fileno(self):
        return self.drv.fileno()

    def wait_for_msg(self):
        header = b''
        while len(header) < 9:
            data = self.drv.recv(9 - len(header))
            if not data:
                logger.error('Connection to a314d was closed, terminating.')
                exit(-1)
            header += data
        (plen, stream_id, ptype) = struct.unpack('=IIB', header)
        payload = b''
        while len(payload) < plen:
            data = self.drv.recv(plen - len(payload))
            if not data:
                logger.error('Connection to a314d was closed, terminating.')
                exit(-1)
            payload += data
        return (stream_id, ptype, payload)

    def send_register_req(self, name):
        m = struct.pack('=IIB', len(name), 0, self.MSG_REGISTER_REQ) + name
        self.drv.sendall(m)

    def send_read_mem_req(self, address, length):
        m = struct.pack('=IIBII', 8, 0, self.MSG_READ_MEM_REQ, address, length)
        self.drv.sendall(m)

    def read_mem(self, address, length):
        self.send_read_mem_req(address, length)
        _, ptype, payload = self.wait_for_msg()
        if ptype != self.MSG_READ_MEM_RES:
            logger.error('Expected MSG_READ_MEM_RES but got %s. Shutting down.', ptype)
            exit(-1)
        return payload

    def send_write_mem_req(self, address, data):
        m = struct.pack('=IIBI', 4 + len(data), 0, self.MSG_WRITE_MEM_REQ, address) + data
        self.drv.sendall(m)

    def write_mem(self, address, data):
        self.send_write_mem_req(address, data)
        _, ptype, _ = self.wait_for_msg()
        if ptype != self.MSG_WRITE_MEM_RES:
            logger.error('Expected MSG_WRITE_MEM_RES but got %s. Shutting down.', ptype)
            exit(-1)

    def send_connect_response(self, stream_id, result):
        m = struct.pack('=IIBB', 1, stream_id, self.MSG_CONNECT_RESPONSE, result)
        self.drv.sendall(m)

    def send_data(self, stream_id, data):
        m = struct.pack('=IIB', len(data), stream_id, self.MSG_DATA) + data
        self.drv.sendall(m)

    def send_eos(self, stream_id):
        m = struct.pack('=IIB', 0, stream_id, self.MSG_EOS)
        self.drv.sendall(m)

    def send_reset(self, stream_id):
        m = struct.pack('=IIB', 0, stream_id, self.MSG_RESET)
        self.drv.sendall(m)
