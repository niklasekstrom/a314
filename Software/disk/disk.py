#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Niklas Ekström

import json
import logging
import select
import socket
import struct
import sys

import a314d

logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)
logger = logging.getLogger(__name__)

SERVICE_NAME = b'disk'

READ_TRACK_REQ = 1
WRITE_TRACK_REQ = 2
READ_TRACK_RES = 3
WRITE_TRACK_RES = 4
INSERT_NOTIFY = 5
EJECT_NOTIFY = 6

OP_RES_OK = 0
OP_RES_NOT_PRESENT = 1
OP_RES_WRITE_PROTECTED = 2

NUM_DRIVES = 4

CONF_FILE = "/etc/opt/a314/disk.conf"

current_stream_id = None
done = False
rbuf = b''

mem_read_queue = []
mem_write_queue = []

drives = [None] * NUM_DRIVES
drives_writable = [False] * NUM_DRIVES

auto_insert = {}

with open(CONF_FILE, 'rt') as f:
    j = json.load(f)
    for e in j.get('auto-insert', []):
        unit = e['unit']
        filename = e['filename']
        rw = e.get('rw', False)
        auto_insert[unit] = (filename, rw)

control_sockets = []

def eject_adf(unit):
    f = drives[unit]
    if not f:
        return

    f.close()
    drives[unit] = None

    if current_stream_id is not None:
        a314d.send_data(current_stream_id, struct.pack('>BBB', EJECT_NOTIFY, unit, 0))

def insert_adf(unit, filename, rw=False):
    eject_adf(unit)

    try:
        drives[unit] = open(filename, 'rb')
        drives_writable[unit] = rw
        logger.info("Opened ADF file '%s'", filename)
    except:
        logger.warning("Failed to open ADF file '%s'", filename)
        return

    if current_stream_id is not None:
        a314d.send_data(current_stream_id, struct.pack('>BBB', INSERT_NOTIFY, unit, 1 if rw else 0))

def process_stream_data(stream_id, data):
    kind, unit, length, offset, address = struct.unpack('>BBHII', data)
    f = drives[unit]
    if kind == READ_TRACK_REQ:
        if not f:
            a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return

        f.seek(offset, 0)
        adf_data = f.read(length)

        mem_write_queue.append((stream_id, unit))
        a314d.send_write_mem_req(address, adf_data)
    elif kind == WRITE_TRACK_REQ:
        if not f:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return
        if not drives_writable[unit]:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_WRITE_PROTECTED))
            return
        mem_read_queue.append((stream_id, unit, offset))
        a314d.send_read_mem_req(address, length)

def process_write_mem_res():
    stream_id, unit = mem_write_queue.pop(0)
    if stream_id == current_stream_id:
        a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_OK))

def process_read_mem_res(adf_data):
    stream_id, unit, offset = mem_read_queue.pop(0)
    f = drives[unit]
    if stream_id == current_stream_id:
        if not f:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return

        f.seek(offset, 0)
        f.write(adf_data)
        a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_OK))

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id

    if ptype == a314d.MSG_CONNECT:
        if payload == SERVICE_NAME and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            a314d.send_connect_response(stream_id, 0)

            for unit, (filename, rw) in auto_insert.items():
                insert_adf(unit, filename, rw)
        else:
            a314d.send_connect_response(stream_id, 3)
    elif ptype == a314d.MSG_READ_MEM_RES:
        process_read_mem_res(payload)
    elif ptype == a314d.MSG_WRITE_MEM_RES:
        process_write_mem_res()
    elif current_stream_id == stream_id:
        if ptype == a314d.MSG_DATA:
            process_stream_data(stream_id, payload)
        elif ptype == a314d.MSG_EOS:
            raise AssertionError('EOS is not supposed to be in use')
        elif ptype == a314d.MSG_RESET:
            current_stream_id = None
            for i in range(4):
                eject_adf(i)
            logger.info('Amiga disconnected')

if '-ondemand' in sys.argv:
    index = sys.argv.index('-ondemand')
    fd = int(sys.argv[index + 1])
    a314d.set_on_demand_fd(fd)
else:
    success = a314d.connect_a314d(SERVICE_NAME)
    if not success:
        logger.error("Unable to register service 'disk' with a314d, shutting down")
        exit(-1)

control_server_socket = socket.socket()
control_server_socket.bind(('localhost', 23890))
control_server_socket.listen(10)

logger.info('Disk service is running')

while not done:
    try:
        rl = [a314d.drv, control_server_socket] + control_sockets
        rl, _, _ = select.select(rl, [], [], 10.0)
    except KeyboardInterrupt:
        rl = []
        if current_stream_id is not None:
            a314d.send_reset(current_stream_id)
        done = True

    for s in rl:
        if s == a314d.drv:
            buf = a314d.drv.recv(8192)
            if not buf:
                if current_stream_id is not None:
                    a314d.send_reset(current_stream_id)
                done = True
            else:
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
        elif s == control_server_socket:
            cs, _ = control_server_socket.accept()
            control_sockets.append(cs)
        else:
            buf = s.recv(512)
            if not buf:
                s.close()
                control_sockets.remove(s)
            else:
                try:
                    buf = buf.decode('utf-8')
                    arr = buf.split(' ')
                    cmd = arr[0]
                    if cmd == 'eject':
                        unit = int(arr[1])
                        eject_adf(unit)
                    elif cmd == 'insert':
                        unit = int(arr[1])
                        rw = arr[2] == '-rw'
                        idx = 3 if rw else 2
                        fn = ' '.join(arr[idx:])
                        insert_adf(unit, fn, rw)
                    else:
                        logger.warning(f"Received unknown command '{cmd}' on control connection")
                except:
                    pass
                s.shutdown(socket.SHUT_WR)
                s.close()
                control_sockets.remove(s)

for s in control_sockets:
    s.close()
control_server_socket.close()
a314d.drv.close()

logger.info('Disk service stopped')
