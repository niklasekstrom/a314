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
SET_GEOMETRY = 7

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

class Drive(object):
    def __init__(self, unit):
        self.unit = unit
        self.f = None
        self.writable = False
        self.geometry = None
        self.geometry_sent = False

drives = [Drive(i) for i in range(NUM_DRIVES)]

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
    drive = drives[unit]
    if not drive.f:
        return f"No disk in unit {unit} to eject"

    drive.f.close()
    drive.f = None

    if current_stream_id is not None:
        a314d.send_data(current_stream_id, struct.pack('>BB', EJECT_NOTIFY, unit))

    return f"Ejected disk from unit {unit}"

def insert_adf(unit, filename, writable=False):
    eject_adf(unit)

    try:
        f = open(filename, 'r+b')
    except:
        return f"Failed to open disk file '{filename}'"

    f.seek(0, 2)
    size = f.tell()

    if size == 80*2*11*512:
        geometry = (2, 11, 80)
    elif size < 2**20:
        f.close()
        return "Failed: HDF must be at least 1 MB"
    elif (size & (2**14 - 1)) != 0:
        f.close()
        return "Failed: HDF size must be a multiple of 16 kB"
    else:
        geometry = (1, 32, size // (2**14))

    drive = drives[unit]

    if drive.geometry_sent and drive.geometry != geometry:
        f.close()
        return "Failed: A disk with a different geometry was previously inserted"

    drive.f = f
    drive.writable = writable
    drive.geometry = geometry

    if current_stream_id is not None:
        if not drive.geometry_sent:
            a314d.send_data(current_stream_id, struct.pack('>BBBBI', SET_GEOMETRY, unit, *geometry))
            drive.geometry_sent = True
        a314d.send_data(current_stream_id, struct.pack('>BBB', INSERT_NOTIFY, unit, 1 if writable else 0))

    return f"Inserted disk file '{filename}' in unit {unit}"

boot_code = bytes([
    0x43, 0xFA, 0x00, 0x18, 0x4E, 0xAE, 0xFF, 0xA0,
    0x4A, 0x80, 0x67, 0x0A, 0x20, 0x40, 0x20, 0x68,
    0x00, 0x16, 0x70, 0x00, 0x4E, 0x75, 0x70, 0xFF,
    0x60, 0xFA, 0x64, 0x6F, 0x73, 0x2E, 0x6C, 0x69,
    0x62, 0x72, 0x61, 0x72, 0x79])

boot_code_silent = bytes([
    0x43, 0xFA, 0x00, 0x3E, 0x70, 0x25, 0x4E, 0xAE,
    0xFD, 0xD8, 0x4A, 0x80, 0x67, 0x0C, 0x22, 0x40,
    0x08, 0xE9, 0x00, 0x06, 0x00, 0x22, 0x4E, 0xAE,
    0xFE, 0x62, 0x43, 0xFA, 0x00, 0x18, 0x4E, 0xAE,
    0xFF, 0xA0, 0x4A, 0x80, 0x67, 0x0A, 0x20, 0x40,
    0x20, 0x68, 0x00, 0x16, 0x70, 0x00, 0x4E, 0x75,
    0x70, 0xFF, 0x4E, 0x75, 0x64, 0x6F, 0x73, 0x2E,
    0x6C, 0x69, 0x62, 0x72, 0x61, 0x72, 0x79, 0x00,
    0x65, 0x78, 0x70, 0x61, 0x6E, 0x73, 0x69, 0x6F,
    0x6E, 0x2E, 0x6C, 0x69, 0x62, 0x72, 0x61, 0x72,
    0x79])

def fixup_boot_block(disk_data, drive):
    checksum = struct.unpack_from('>I', disk_data, 4)[0]

    if disk_data[:3] == b'DOS' and checksum == 0:
        size = len(disk_data)
        if size < 1024:
            drive.f.seek(0, 0)
            disk_data = drive.f.read(1024)

        dd = bytearray(disk_data)
        dd[12:12 + len(boot_code_silent)] = boot_code_silent

        geometry = drive.geometry
        sector_count = geometry[0] * geometry[1] * geometry[2]
        dd[8:12] = struct.pack('>I', sector_count // 2)

        arr = struct.unpack_from('>256I', dd)
        checksum = 0
        for x in arr:
            checksum += x
            if checksum >= 2**32:
                checksum = checksum - 2**32 + 1
        checksum = checksum ^ (2**32-1)
        dd[4:8] = struct.pack('>I', checksum)

        disk_data = bytes(dd[:size])

    return disk_data

def process_stream_data(stream_id, data):
    kind, unit, length, offset, address = struct.unpack('>BBHII', data)
    drive = drives[unit]
    if kind == READ_TRACK_REQ:
        if not drive.f:
            a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return

        drive.f.seek(offset, 0)
        disk_data = drive.f.read(length)
        if offset == 0:
            disk_data = fixup_boot_block(disk_data, drive)

        mem_write_queue.append((stream_id, unit))
        a314d.send_write_mem_req(address, disk_data)
    elif kind == WRITE_TRACK_REQ:
        if not drive.f:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return
        if not drive.writable:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_WRITE_PROTECTED))
            return
        mem_read_queue.append((stream_id, unit, offset))
        a314d.send_read_mem_req(address, length)

def process_write_mem_res():
    stream_id, unit = mem_write_queue.pop(0)
    if stream_id == current_stream_id:
        a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_OK))

def process_read_mem_res(disk_data):
    stream_id, unit, offset = mem_read_queue.pop(0)
    drive = drives[unit]
    if stream_id == current_stream_id:
        if not drive.f:
            a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
            return

        drive.f.seek(offset, 0)
        drive.f.write(disk_data)
        a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_OK))

def process_drv_msg(stream_id, ptype, payload):
    global current_stream_id

    if ptype == a314d.MSG_CONNECT:
        if payload == SERVICE_NAME and current_stream_id is None:
            logger.info('Amiga connected')
            current_stream_id = stream_id
            a314d.send_connect_response(stream_id, 0)

            for unit, (filename, writable) in auto_insert.items():
                insert_adf(unit, filename, writable)
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
                drives[i].geometry_sent = False
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
                    arr = buf.strip().split(' ')
                    cmd = arr[0]
                    if cmd == 'eject':
                        unit = int(arr[1])
                        res = eject_adf(unit)
                    elif cmd == 'insert':
                        unit = int(arr[1])
                        rw = arr[2] == '-rw'
                        idx = 3 if rw else 2
                        fn = ' '.join(arr[idx:])
                        res = insert_adf(unit, fn, rw)
                    else:
                        res = f"Unknown command '{cmd}'"
                except:
                    res = "Exception raised when processing command"
                s.sendall((res + '\n').encode('utf-8'))
                s.shutdown(socket.SHUT_WR)
                s.close()
                control_sockets.remove(s)

for s in control_sockets:
    s.close()
control_server_socket.close()
a314d.drv.close()

logger.info('Disk service stopped')
