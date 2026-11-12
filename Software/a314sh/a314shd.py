#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2026 Jeremy Bauer

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)

import select
import socket
import struct
import sys
from collections import deque

from a314d import A314d

logger = logging.getLogger(__name__)

SERVICE_NAME = 'a314sh'  # Service name for a314d
SERVICE_HOST = 'localhost'  # Host for a314d service
CONTROL_PORT = 23891     # Port for a314sh control service.  Must match a314sh.py.

REQ_EXEC = 1             # Request type for executing commands
SERVICE_ERROR_RC = -1    # Return code sent when the command could not be run at all

CONNECT_OK = 0               # Connect response: access granted
CONNECT_UNKNOWN_SERVICE = 3  # Connect response: service name not recognized

MAX_PKT = 224            # Maximum packet size for a314d
MAX_REQUEST = 1 << 20    # Maximum request size
MAX_RESPONSE = 16 << 20  # Maximum response size

DRV_HEADER_SIZE = struct.calcsize('=IIB')  # a314d message header (plen, stream_id, ptype)
LENGTH_FIELD_SIZE = 4    # Size of the 32-bit length fields in the wire protocol
CLIENT_RECV_SIZE = 65536 # Read chunk size for client connections
DRV_RECV_SIZE = 8192     # Read chunk size for the a314d connection
LISTEN_BACKLOG = 10      # Maximum number of pending client connections
SELECT_TIMEOUT = 10.0    # Timeout for select, in seconds


class ShellService(object):
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)

        self.control_server_socket = socket.socket()
        self.control_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.control_server_socket.bind((SERVICE_HOST, CONTROL_PORT))
        self.control_server_socket.listen(LISTEN_BACKLOG)

        self.clients = {}       # socket -> bytearray with buffered request bytes
        self.pending = deque()  # (socket, command) tuples waiting to be served
        self.current = None     # socket waiting for the in-flight Amiga response

        self.stream_id = None
        self.stream_rbuf = b''  # buffered response bytes from the Amiga
        self.rbuf = b''

    def send_stream_frame(self, payload):
        frame = struct.pack('>I', len(payload)) + payload
        for i in range(0, len(frame), MAX_PKT):
            self.a314d.send_data(self.stream_id, frame[i:i + MAX_PKT])

    def drop_client(self, s):
        if s in self.clients:
            del self.clients[s]
        try:
            s.close()
        except OSError:
            pass

    def respond(self, s, rc, out):
        try:
            s.sendall(struct.pack('>iI', rc, len(out)) + out)
        except OSError:
            pass
        self.drop_client(s)

    def maybe_serve_next(self):
        if self.current is not None:
            return

        while self.pending:
            s, cmd = self.pending.popleft()

            if s not in self.clients:
                continue

            if self.stream_id is None:
                self.respond(s, SERVICE_ERROR_RC, b'a314sh: the Amiga-side daemon is not connected\n')
                continue

            self.current = s
            self.stream_rbuf = b''
            self.send_stream_frame(struct.pack('>B', REQ_EXEC) + cmd)
            return

    def handle_client_readable(self, s):
        try:
            buf = s.recv(CLIENT_RECV_SIZE)
        except OSError:
            buf = b''

        if not buf:
            self.drop_client(s)
            return

        self.clients[s] += buf

        while len(self.clients[s]) >= LENGTH_FIELD_SIZE:
            (length,) = struct.unpack_from('>I', self.clients[s])
            if length < 1 or length > MAX_REQUEST:
                self.drop_client(s)
                return
            if len(self.clients[s]) < LENGTH_FIELD_SIZE + length:
                break
            cmd = bytes(self.clients[s][LENGTH_FIELD_SIZE:LENGTH_FIELD_SIZE + length])
            del self.clients[s][:LENGTH_FIELD_SIZE + length]
            self.pending.append((s, cmd))

        self.maybe_serve_next()

    def handle_stream_data(self, payload):
        if self.current is None:
            return

        self.stream_rbuf += payload

        if len(self.stream_rbuf) < LENGTH_FIELD_SIZE:
            return

        (length,) = struct.unpack_from('>I', self.stream_rbuf)

        if length > MAX_RESPONSE:
            logger.error('Protocol error: response of %d bytes exceeds limit', length)
            self.a314d.send_reset(self.stream_id)
            self.stream_id = None
            self.stream_rbuf = b''
            s = self.current
            self.current = None
            self.respond(s, SERVICE_ERROR_RC, b'a314sh: protocol error\n')
            return

        if len(self.stream_rbuf) < LENGTH_FIELD_SIZE + length:
            return

        res = self.stream_rbuf[LENGTH_FIELD_SIZE:LENGTH_FIELD_SIZE + length]
        self.stream_rbuf = b''

        s = self.current
        self.current = None

        if s is not None and s in self.clients:
            (rc,) = struct.unpack_from('>i', res)
            self.respond(s, rc, res[LENGTH_FIELD_SIZE:])

        self.maybe_serve_next()

    def amiga_disconnected(self):
        self.stream_id = None
        self.stream_rbuf = b''

        if self.current is not None:
            self.respond(self.current, SERVICE_ERROR_RC, b'a314sh: the connection to the Amiga was lost\n')
            self.current = None

        while self.pending:
            s, _ = self.pending.popleft()
            if s in self.clients:
                self.respond(s, SERVICE_ERROR_RC, b'a314sh: the Amiga daemon is not connected\n')

    def process_drv_msg(self, stream_id, ptype, payload):
        if ptype == self.a314d.MSG_CONNECT:
            if payload == SERVICE_NAME.encode():
                if self.stream_id is not None:
                    self.a314d.send_reset(self.stream_id)
                    self.amiga_disconnected()
                logger.info('Amiga connected to a314sh service')
                self.stream_id = stream_id
                self.a314d.send_connect_response(stream_id, CONNECT_OK)
                self.maybe_serve_next()
            else:
                self.a314d.send_connect_response(stream_id, CONNECT_UNKNOWN_SERVICE)
        elif self.stream_id == stream_id:
            if ptype == self.a314d.MSG_DATA:
                self.handle_stream_data(payload)
            elif ptype == self.a314d.MSG_EOS:
                logger.info('Amiga disconnected from a314sh service')
                self.a314d.send_eos(stream_id)
                self.amiga_disconnected()
            elif ptype == self.a314d.MSG_RESET:
                logger.info('Amiga connection was reset')
                self.amiga_disconnected()

    def handle_a314d_readable(self):
        buf = self.a314d.drv.recv(DRV_RECV_SIZE)
        if not buf:
            logger.error('Connection to a314d was closed, shutting down')
            exit(-1)

        self.rbuf += buf
        while True:
            if len(self.rbuf) < DRV_HEADER_SIZE:
                break

            (plen, stream_id, ptype) = struct.unpack('=IIB', self.rbuf[:DRV_HEADER_SIZE])
            if len(self.rbuf) < DRV_HEADER_SIZE + plen:
                break

            payload = self.rbuf[DRV_HEADER_SIZE:DRV_HEADER_SIZE + plen]
            self.rbuf = self.rbuf[DRV_HEADER_SIZE + plen:]

            self.process_drv_msg(stream_id, ptype, payload)

    def run(self):
        logger.info('a314sh service is running')

        while True:
            try:
                rl = [self.a314d, self.control_server_socket] + list(self.clients)
                rl, _, _ = select.select(rl, [], [], SELECT_TIMEOUT)
            except KeyboardInterrupt:
                break

            for s in rl:
                if s == self.a314d:
                    self.handle_a314d_readable()
                elif s == self.control_server_socket:
                    cs, _ = self.control_server_socket.accept()
                    self.clients[cs] = bytearray()
                else:
                    self.handle_client_readable(s)

        for s in list(self.clients):
            s.close()
        self.control_server_socket.close()
        self.a314d.close()

        logger.info('a314sh service stopped')


if __name__ == '__main__':
    service = ShellService()
    service.run()
