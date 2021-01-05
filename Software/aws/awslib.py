#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Niklas Ekström

import logging
import os
import select
import socket
import struct
import sys
import time
import random
import string

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

AWS_CLIENT_REQ_OPEN_WINDOW = 1
AWS_CLIENT_REQ_CLOSE_WINDOW = 2
AWS_CLIENT_REQ_COPY_FLIP_BUFFER = 3
AWS_CLIENT_RES_OPEN_WINDOW_FAIL = 4
AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS = 5
AWS_CLIENT_EVENT_CLOSE_WINDOW = 6

class Window(object):
    def __init__(self, wid):
        self.wid = wid
        self.width = None
        self.height = None
        self.depth = None

class Connection(object):
    def __init__(self, sock, callbacks):
        self.sock = sock
        self.callbacks = callbacks
        self.next_wid = 0
        self.windows = {}
        self.rbuf = None

    def fileno(self):
        return self.sock.fileno()

    def send(self, data):
        data = struct.pack('=I', len(data)) + data
        self.sock.sendall(data)

    def close(self):
        self.sock.shutdown(socket.SHUT_RDWR)
        self.sock.close()

    def open_window(self, left, top, width, height, title):
        wid = self.next_wid
        while wid in self.windows:
            wid = (wid + 1) % 65536
        self.next_wid = (wid + 1) % 65536
        self.windows[wid] = Window(wid)
        self.send(struct.pack('=BHHHHH', AWS_CLIENT_REQ_OPEN_WINDOW, wid, left, top, width, height) + title.encode('latin-1'))
        return wid

    def close_window(self, wid):
        if wid in self.windows:
            self.send(struct.pack('=BH', AWS_CLIENT_REQ_CLOSE_WINDOW, wid))
            del self.windows[wid]

    def copy_flip_window(self, wid, buffer):
        if wid in self.windows:
            self.send(struct.pack('=BH', AWS_CLIENT_REQ_COPY_FLIP_BUFFER, wid) + buffer)

    def process_msg(self, msg):
        cmd = msg[0]
        if cmd == AWS_CLIENT_RES_OPEN_WINDOW_FAIL:
            wid = struct.unpack('=H', msg[1:3])[0]
            if wid in self.windows:
                self.callbacks.open_window_fail(self, wid)
                del self.windows[wid]
        elif cmd == AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS:
            wid, width, height, depth = struct.unpack('=HHHH', msg[1:9])
            if wid in self.windows:
                w = self.windows[wid]
                w.width = width
                w.height = height
                w.depth = depth
                self.callbacks.open_window_success(self, wid, width, height, depth)
        elif cmd == AWS_CLIENT_EVENT_CLOSE_WINDOW:
            wid = struct.unpack('=H', msg[1:3])[0]
            if wid in self.windows:
                self.callbacks.event_close_window(self, wid)

    def handle_readable(self):
        buf = self.sock.recv(128)
        if not buf:
            self.sock.shutdown(socket.SHUT_RDWR)
            self.sock.close()
            self.sock = None
            self.callbacks.connection_closed(self)
            return

        self.rbuf = self.rbuf + buf if self.rbuf else buf

        while self.rbuf:
            blen = len(self.rbuf)
            if blen < 4:
                break
            plen = struct.unpack('=I', self.rbuf[:4])[0]
            if blen < plen + 4:
                break
            msg = self.rbuf[4:plen + 4]
            self.rbuf = None if blen == plen + 4 else self.rbuf[plen + 4:]
            self.process_msg(msg)

def connect(callbacks):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 18377))
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return Connection(sock, callbacks)
