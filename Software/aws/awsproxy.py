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

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

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

def wait_for_msg(drv):
    header = b''
    while len(header) < 9:
        data = drv.recv(9 - len(header))
        if not data:
            logger.error('Connection to a314d was closed, terminating.')
            exit(-1)
        header += data
    (plen, stream_id, ptype) = struct.unpack('=IIB', header)
    payload = b''
    while len(payload) < plen:
        data = drv.recv(plen - len(payload))
        if not data:
            logger.error('Connection to a314d was closed, terminating.')
            exit(-1)
        payload += data
    return (stream_id, ptype, payload)

def send_register_req(drv, name):
    m = struct.pack('=IIB', len(name), 0, MSG_REGISTER_REQ) + name
    drv.sendall(m)

def send_read_mem_req(drv, address, length):
    m = struct.pack('=IIBII', 8, 0, MSG_READ_MEM_REQ, address, length)
    drv.sendall(m)

def read_mem(drv, address, length):
    send_read_mem_req(drv, address, length)
    _, ptype, payload = wait_for_msg(drv)
    if ptype != MSG_READ_MEM_RES:
        logger.error('Expected MSG_READ_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)
    return payload

def send_write_mem_req(drv, address, data):
    m = struct.pack('=IIBI', 4 + len(data), 0, MSG_WRITE_MEM_REQ, address) + data
    drv.sendall(m)

def write_mem(drv, address, data):
    send_write_mem_req(drv, address, data)
    _, ptype, _ = wait_for_msg(drv)
    if ptype != MSG_WRITE_MEM_RES:
        logger.error('Expected MSG_WRITE_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)

def send_connect_response(drv, stream_id, result):
    m = struct.pack('=IIBB', 1, stream_id, MSG_CONNECT_RESPONSE, result)
    drv.sendall(m)

def send_data(drv, stream_id, data):
    m = struct.pack('=IIB', len(data), stream_id, MSG_DATA) + data
    drv.sendall(m)

def send_eos(drv, stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_EOS)
    drv.sendall(m)

def send_reset(drv, stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_RESET)
    drv.sendall(m)

### A314 communication routines above. Actual driver below.

class Window(object):
    def __init__(self, wid, cwid, c):
        self.wid = wid
        self.cwid = cwid
        self.client = c
        self.buffer_address = None

class Client(object):
    def __init__(self, sock):
        self.sock = sock
        self.rbuf = None
        self.windows = {} # cwid -> window object.
        self.waiting_for_wb_screen_info = False

    def send(self, data):
        data = struct.pack('=I', len(data)) + data
        self.sock.sendall(data)

    def fileno(self):
        return self.sock.fileno()

clients = []

# wid -> window object.
windows = {}

next_window_id = 0

def get_next_wid():
    global next_window_id
    wid = next_window_id
    while wid in windows:
        wid = (wid + 1) % 256
    next_window_id = (wid + 1) % 256
    return wid

current_stream_id = None
done = False
rbuf = b''

write_mem_reqs = []

SERVICE_NAME = b'awsproxy'

AWS_REQ_OPEN_WINDOW = 1
AWS_RES_OPEN_WINDOW_FAIL = 2
AWS_RES_OPEN_WINDOW_SUCCESS = 3
AWS_REQ_CLOSE_WINDOW = 4
AWS_REQ_FLIP_BUFFER = 5
AWS_REQ_WB_SCREEN_INFO = 6
AWS_RES_WB_SCREEN_INFO = 7
AWS_EVENT_CLOSE_WINDOW = 8
AWS_EVENT_FLIP_DONE = 9

AWS_CLIENT_REQ_OPEN_WINDOW = 1
AWS_CLIENT_RES_OPEN_WINDOW_FAIL = 2
AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS = 3
AWS_CLIENT_REQ_CLOSE_WINDOW = 4
AWS_CLIENT_REQ_COPY_FLIP_BUFFER = 5
AWS_CLIENT_REQ_WB_SCREEN_INFO = 6
AWS_CLIENT_RES_WB_SCREEN_INFO = 7
AWS_CLIENT_EVENT_CLOSE_WINDOW = 8
AWS_CLIENT_EVENT_FLIP_DONE = 9

def process_drv_data(msg):
    cmd = msg[0]
    if cmd == AWS_RES_OPEN_WINDOW_FAIL:
        wid = msg[1]
        if wid in windows:
            w = windows[wid]
            c = w.client
            c.send(struct.pack('=BH', AWS_CLIENT_RES_OPEN_WINDOW_FAIL, w.cwid))
            del c.windows[w.cwid]
            del windows[w.wid]
    elif cmd == AWS_RES_OPEN_WINDOW_SUCCESS:
        wid = msg[1]
        if wid in windows:
            w = windows[wid]
            ba, width, height, depth = struct.unpack('>IHHH', msg[2:12])
            w.buffer_address = ba
            c = w.client
            c.send(struct.pack('=BHHHH', AWS_CLIENT_RES_OPEN_WINDOW_SUCCESS, w.cwid, width, height, depth))
    elif cmd == AWS_EVENT_CLOSE_WINDOW:
        wid = msg[1]
        if wid in windows:
            w = windows[wid]
            c = w.client
            c.send(struct.pack('=BH', AWS_CLIENT_EVENT_CLOSE_WINDOW, w.cwid))
    elif cmd == AWS_EVENT_FLIP_DONE:
        wid = msg[1]
        if wid in windows:
            w = windows[wid]
            c = w.client
            c.send(struct.pack('=BH', AWS_CLIENT_EVENT_FLIP_DONE, w.cwid))
    elif cmd == AWS_RES_WB_SCREEN_INFO:
        width, height, depth = struct.unpack('>HHH', msg[2:8])
        pal = list(msg[8:])
        for i in range(len(pal) // 2):
            pal[2*i], pal[2*i + 1] = pal[2*i + 1], pal[2*i]
        pal = bytes(pal)
        for c in clients:
            if c.waiting_for_wb_screen_info:
                c.send(struct.pack('=BHHH', AWS_CLIENT_RES_WB_SCREEN_INFO, width, height, depth) + pal)
                c.waiting_for_wb_screen_info = False

def process_write_mem_res():
    wid = write_mem_reqs.pop(0)
    if wid in windows:
        send_data(drv, current_stream_id, struct.pack('>BB', AWS_REQ_FLIP_BUFFER, wid))

def process_client_msg(c, msg):
    cmd = msg[0]
    if cmd == AWS_CLIENT_REQ_OPEN_WINDOW:
        cwid, left, top, width, height = struct.unpack('=HHHHH', msg[1:11])
        title = msg[11:]
        w = Window(get_next_wid(), cwid, c)
        c.windows[w.cwid] = w
        windows[w.wid] = w
        send_data(drv, current_stream_id, struct.pack('>BBHHHH', AWS_REQ_OPEN_WINDOW, w.wid, left, top, width, height) + title)
    elif cmd == AWS_CLIENT_REQ_CLOSE_WINDOW:
        cwid = struct.unpack('=H', msg[1:3])[0]
        # TODO: Probably close connection to client if receives an invalid request?
        if cwid in c.windows:
            w = c.windows[cwid]
            send_data(drv, current_stream_id, struct.pack('>BB', AWS_REQ_CLOSE_WINDOW, w.wid))
            del c.windows[w.cwid]
            del windows[w.wid]
    elif cmd == AWS_CLIENT_REQ_COPY_FLIP_BUFFER:
        cwid = struct.unpack('=H', msg[1:3])[0]
        if cwid in c.windows:
            w = c.windows[cwid]
            write_mem_reqs.append(w.wid)
            send_write_mem_req(drv, w.buffer_address, msg[3:])
    elif cmd == AWS_CLIENT_REQ_WB_SCREEN_INFO:
        c.waiting_for_wb_screen_info = True
        send_data(drv, current_stream_id, bytes([AWS_REQ_WB_SCREEN_INFO]))

def process_client_readable(c):
    buf = c.sock.recv(128*1024)
    if not buf:
        logger.info('Client disconnected')
        for w in c.windows.values():
            send_data(drv, current_stream_id, struct.pack('>BB', AWS_REQ_CLOSE_WINDOW, w.wid))
            del windows[w.wid]

        c.sock.shutdown(socket.SHUT_RDWR)
        c.sock.close()
        clients.remove(c)
        return

    c.rbuf = c.rbuf + buf if c.rbuf else buf

    while c.rbuf:
        blen = len(c.rbuf)
        if blen < 4:
            break

        plen = struct.unpack('=I', c.rbuf[:4])[0]
        if blen < 4 + plen:
            break

        payload = c.rbuf[4:4+plen]
        c.rbuf = None if blen == 4+plen else c.rbuf[4+plen:]

        process_client_msg(c, payload)

def process_incoming_connection():
    sock, _ = server_socket.accept()
    if current_stream_id is None:
        logger.info('Rejected client connection since Amiga is not connected')
        sock.close()
    else:
        logger.info('Accepted client connection')
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        clients.append(Client(sock))

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id

    if ptype == MSG_CONNECT:
        if payload == SERVICE_NAME and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            send_connect_response(drv, stream_id, 0)
        else:
            send_connect_response(drv, stream_id, 3)
    elif ptype == MSG_WRITE_MEM_RES:
        process_write_mem_res()
    elif current_stream_id == stream_id:
        if ptype == MSG_DATA:
            process_drv_data(payload)
        elif ptype == MSG_RESET:
            current_stream_id = None
            logger.info('Amiga disconnected')

def process_drv_readable():
    global done, rbuf

    buf = drv.recv(1024)
    if not buf:
        done = True
        return

    rbuf += buf
    while True:
        if len(rbuf) < 9:
            break

        (plen, stream_id, ptype) = struct.unpack('=IIB', rbuf[:9])
        if len(rbuf) < 9 + plen:
            break

        payload = rbuf[9:9+plen]
        rbuf = rbuf[9+plen:]

        process_drv_msg(stream_id, ptype, payload)

try:
    idx = sys.argv.index('-ondemand')
except ValueError:
    idx = -1

if idx != -1:
    fd = int(sys.argv[idx + 1])
    drv = socket.socket(fileno=fd)
else:
    drv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    drv.connect(('localhost', 7110))
    drv.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    send_register_req(drv, SERVICE_NAME)
    _, _, payload = wait_for_msg(drv)
    if payload[0] != 1:
        logger.error("Unable to register 'aws' with a314d, shutting down")
        done = True

if not done:
    try:
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind(('localhost', 18377))
        server_socket.listen()
    except:
        logger.error('Unable to bind server socket to TCP port localhost:18377')
        done = True

if not done:
    logger.info('awsproxy is running')

while not done:
    try:
        rl = [drv, server_socket] + clients
        rl, _, _ = select.select(rl, [], [], 10.0)
    except KeyboardInterrupt:
        rl = []
        done = True

    for sock in rl:
        if sock == drv:
            process_drv_readable()
        elif sock == server_socket:
            process_incoming_connection()
        else:
            process_client_readable(sock)

for c in clients:
    # No need to close windows, as they will all be closed when stream is reset.
    c.sock.shutdown(socket.SHUT_RDWR)
    c.sock.close()

if server_socket:
    server_socket.close()
    server_socket = None

if drv:
    if current_stream_id is not None:
        send_reset(drv, current_stream_id)
    drv.close()
    drv = None

logger.info('awsproxy stopped')
