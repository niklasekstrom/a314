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
import threading
import queue

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

AWS_CLIENT_REQ_OPEN_WINDOW = 1
AWS_CLIENT_RES_OPEN_WINDOW_FAIL = 2
AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS = 3
AWS_CLIENT_REQ_CLOSE_WINDOW = 4
AWS_CLIENT_REQ_COPY_FLIP_BUFFER = 5
AWS_CLIENT_REQ_WB_SCREEN_INFO = 6
AWS_CLIENT_RES_WB_SCREEN_INFO = 7
AWS_CLIENT_EVENT_CLOSE_WINDOW = 8
AWS_CLIENT_EVENT_FLIP_DONE = 9

class Window(object):
    def __init__(self, wid):
        self.wid = wid
        self.width = None
        self.height = None
        self.depth = None

class Connection(threading.Thread):
    def __init__(self, sock, callbacks):
        super().__init__()
        self.sock = sock
        self.callbacks = callbacks
        self.is_open = True
        self.next_wid = 0
        self.windows = {}
        self.rbuf = None
        self.sync_queue = queue.Queue()

    def run(self):
        while True:
            rl, _, _ = select.select([self.sock], [], [])
            if self.sock not in rl:
                continue
            if not self.handle_readable():
                self.is_open = False
                self.callbacks.connection_closed(self)
                self.sync_queue.put(None)
                self.sock.close()
                break

    def send(self, data):
        data = struct.pack('=I', len(data)) + data
        self.sock.sendall(data)

    def close(self):
        self.sock.shutdown(socket.SHUT_RDWR)
        self.join(2.0)

    def process_msg(self, msg):
        cmd = msg[0]
        if cmd in [AWS_CLIENT_RES_OPEN_WINDOW_FAIL, AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS, AWS_CLIENT_RES_WB_SCREEN_INFO]:
            self.sync_queue.put(msg)
        elif cmd == AWS_CLIENT_EVENT_CLOSE_WINDOW:
            wid = struct.unpack('=H', msg[1:3])[0]
            self.callbacks.event_close_window(self, wid)
        elif cmd == AWS_CLIENT_EVENT_FLIP_DONE:
            wid = struct.unpack('=H', msg[1:3])[0]
            self.callbacks.event_flip_done(self, wid)
        else:
            logger.error('Received unknown command %d, shutting down', cmd)
            exit(-1)

    def handle_readable(self):
        buf = self.sock.recv(128)
        if not buf:
            self.sock.shutdown(socket.SHUT_RDWR)
            return False

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

        return True

    def get_wb_screen_info(self):
        self.send(bytes([AWS_CLIENT_REQ_WB_SCREEN_INFO]))
        msg = self.sync_queue.get()
        if msg is None:
            raise RuntimeError('Connection failed')
        w, h, d = struct.unpack('=HHH', msg[1:7])
        pal = msg[7:]
        pal = [struct.unpack('=H', pal[2*i:2*(i+1)])[0] for i in range(len(pal) // 2)]
        return (w, h, d, pal)

    def open_window(self, left, top, width, height, title):
        if not self.is_open:
            return None, None

        wid = self.next_wid
        while wid in self.windows:
            wid = (wid + 1) % 65536
        self.next_wid = (wid + 1) % 65536
        self.windows[wid] = Window(wid)
        self.send(struct.pack('=BHHHHH', AWS_CLIENT_REQ_OPEN_WINDOW, wid, left, top, width, height) + title.encode('latin-1'))
        msg = self.sync_queue.get()
        if not msg:
            raise RuntimeError('Connection failed')

        cmd = msg[0]
        if cmd == AWS_CLIENT_RES_OPEN_WINDOW_FAIL:
            #wid = struct.unpack('=H', msg[1:3])[0]
            del self.windows[wid]
            return None, None
        elif cmd == AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS:
            w = self.windows[wid]
            _, w.width, w.height, w.depth = struct.unpack('=HHHH', msg[1:9])
            return w.wid, (w.width, w.height, w.depth)
        else:
            raise RuntimeError('Invalid response')

    def close_window(self, wid):
        if not self.is_open or wid not in self.windows:
            return
        self.send(struct.pack('=BH', AWS_CLIENT_REQ_CLOSE_WINDOW, wid))
        del self.windows[wid]

    def copy_flip_window(self, wid, buffer):
        if not self.is_open or wid not in self.windows:
            return
        self.send(struct.pack('=BH', AWS_CLIENT_REQ_COPY_FLIP_BUFFER, wid) + buffer)

def connect(callbacks):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 18377))
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    conn = Connection(sock, callbacks)
    conn.start()
    return conn
