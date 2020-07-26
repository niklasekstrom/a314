#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2020 Niklas Ekström

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

def wait_for_msg():
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

def send_register_req(name):
    m = struct.pack('=IIB', len(name), 0, MSG_REGISTER_REQ) + name
    drv.sendall(m)

def send_read_mem_req(address, length):
    m = struct.pack('=IIBII', 8, 0, MSG_READ_MEM_REQ, address, length)
    drv.sendall(m)

def read_mem(address, length):
    send_read_mem_req(address, length)
    stream_id, ptype, payload = wait_for_msg()
    if ptype != MSG_READ_MEM_RES:
        logger.error('Expected MSG_READ_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)
    return payload

def send_write_mem_req(address, data):
    m = struct.pack('=IIBI', 4 + len(data), 0, MSG_WRITE_MEM_REQ, address) + data
    drv.sendall(m)

def write_mem(address, data):
    send_write_mem_req(address, data)
    stream_id, ptype, payload = wait_for_msg()
    if ptype != MSG_WRITE_MEM_RES:
        logger.error('Expected MSG_WRITE_MEM_RES but got %s. Shutting down.', ptype)
        exit(-1)

def send_connect_response(stream_id, result):
    m = struct.pack('=IIBB', 1, stream_id, MSG_CONNECT_RESPONSE, result)
    drv.sendall(m)

def send_data(stream_id, data):
    m = struct.pack('=IIB', len(data), stream_id, MSG_DATA) + data
    drv.sendall(m)

def send_eos(stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_EOS)
    drv.sendall(m)

def send_reset(stream_id):
    m = struct.pack('=IIB', 0, stream_id, MSG_RESET)
    drv.sendall(m)

current_stream_id = None
done = False
rbuf = b''
DEV_NAME = '/dev/input/mice'

last_buttons = 0
next_send = time.time()
buffered_movement = None
MIN_SEND_INTERVAL = 0.02 # 20 ms.

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id, buffered_movement

    if ptype == MSG_CONNECT:
        if payload == b'remote-mouse' and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif current_stream_id == stream_id:
        if ptype == MSG_DATA:
            pass
        elif ptype == MSG_EOS:
            pass
        elif ptype == MSG_RESET:
            current_stream_id = None
            buffered_movement = None
            logger.info('Amiga disconnected')

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

    send_register_req(b'remote-mouse')
    _, _, payload = wait_for_msg()
    if payload[0] != 1:
        logger.error('Unable to register remote-mouse with driver, shutting down')
        drv.close()
        done = True

if not done:
    try:
        dev_fd = os.open(DEV_NAME, os.O_RDONLY | os.O_NONBLOCK)
    except:
        logger.error('Unable to open mouse device at ' + DEV_NAME)
        done = True

if not done:
    logger.info('remote-mouse service is running')

while not done:
    sel_fds = [drv, dev_fd]

    if idx == -1:
        sel_fds.append(sys.stdin)

    timeout = 5.0

    if buffered_movement:
        now = time.time()
        if next_send <= now:
            send_data(current_stream_id, struct.pack('>hhb', *buffered_movement, last_buttons))
            buffered_movement = None
            next_send = now + MIN_SEND_INTERVAL
        else:
            timeout = next_send - now

    rfd, wfd, xfd = select.select(sel_fds, [], [], timeout)

    for fd in rfd:
        if fd == sys.stdin:
            line = sys.stdin.readline()
            if not line or line.startswith('quit'):
                if current_stream_id is not None:
                    send_reset(current_stream_id)
                drv.close()
                done = True
        elif fd == drv:
            buf = drv.recv(1024)
            if not buf:
                if current_stream_id is not None:
                    send_reset(current_stream_id)
                drv.close()
                done = True
            else:
                rbuf += buf
                while True:
                    if len(rbuf) < 9:
                        break

                    (plen, stream_id, ptype) = struct.unpack('=IIB', rbuf[:9])
                    if len(rbuf) < 9 + plen:
                        break

                    rbuf = rbuf[9:]
                    payload = rbuf[:plen]
                    rbuf = rbuf[plen:]

                    process_drv_msg(stream_id, ptype, payload)
        elif fd == dev_fd:
            data = os.read(dev_fd, 32)

            if len(data) == 0:
                os.close(dev_fd)
                if current_stream_id is not None:
                    send_reset(current_stream_id)
                drv.close()
                done = True
            elif len(data) == 3:
                flags, dx, dy = data
                buttons = flags & 3

                if current_stream_id is not None:
                    if flags & 0x10:
                        dx = dx - 256
                    if flags & 0x20:
                        dy = dy - 256
                    dy = -dy

                    if buffered_movement is None:
                        buffered_movement = (dx, dy)
                    else:
                        bdx, bdy = buffered_movement
                        buffered_movement = (bdx + dx, bdy + dy)

                    now = time.time()

                    if buttons != last_buttons or next_send <= now:
                        send_data(current_stream_id, struct.pack('>hhb', *buffered_movement, buttons))
                        buffered_movement = None
                        next_send = now + MIN_SEND_INTERVAL

                last_buttons = buttons
