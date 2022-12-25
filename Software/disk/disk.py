#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Niklas Ekström

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)

import io
import json
import logging
import select
import socket
import struct
from typing import Optional, Tuple, List, Dict

from a314d import A314d

logger = logging.getLogger(__name__)

SERVICE_NAME = 'disk'

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

class Drive(object):
    def __init__(self, unit: int):
        self.unit = unit
        self.f: Optional[io.BufferedRandom] = None
        self.writable = False
        self.geometry: Optional[Tuple[int, int, int]] = None
        self.geometry_sent = False

class DiskService(object):
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)

        self.control_server_socket = socket.socket()
        self.control_server_socket.bind(('localhost', 23890))
        self.control_server_socket.listen(10)

        self.control_sockets: List[socket.socket] = []
        self.drives = [Drive(i) for i in range(NUM_DRIVES)]
        self.auto_insert: Dict[int, Tuple[str, bool]] = {}
        self.current_stream_id: Optional[int] = None
        self.rbuf = b''
        self.mem_read_queue = []
        self.mem_write_queue = []

    def load_config(self):
        with open(CONF_FILE, 'rt') as f:
            j = json.load(f)
            for e in j.get('auto-insert', []):
                unit: int = e['unit']
                filename: str = e['filename']
                rw: bool = e.get('rw', False)
                self.auto_insert[unit] = (filename, rw)

    def eject_adf(self, unit: int):
        drive = self.drives[unit]
        if not drive.f:
            return f"No disk in unit {unit} to eject"

        drive.f.close()
        drive.f = None

        if self.current_stream_id is not None:
            self.a314d.send_data(self.current_stream_id, struct.pack('>BB', EJECT_NOTIFY, unit))

        return f"Ejected disk from unit {unit}"

    def insert_adf(self, unit: int, filename: str, writable=False):
        self.eject_adf(unit)

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

        drive = self.drives[unit]

        if drive.geometry_sent and drive.geometry != geometry:
            f.close()
            return "Failed: A disk with a different geometry was previously inserted"

        drive.f = f
        drive.writable = writable
        drive.geometry = geometry

        if self.current_stream_id is not None:
            if not drive.geometry_sent:
                self.a314d.send_data(self.current_stream_id, struct.pack('>BBBBI', SET_GEOMETRY, unit, *geometry))
                drive.geometry_sent = True
            self.a314d.send_data(self.current_stream_id, struct.pack('>BBB', INSERT_NOTIFY, unit, 1 if writable else 0))

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

    def fixup_boot_block(self, disk_data: bytes, drive: Drive):
        assert type(drive.f) is io.BufferedRandom
        assert type(drive.geometry) is tuple

        checksum = struct.unpack_from('>I', disk_data, 4)[0]

        if disk_data[:3] == b'DOS' and checksum == 0:
            size = len(disk_data)
            if size < 1024:
                drive.f.seek(0, 0)
                disk_data = drive.f.read(1024)

            dd = bytearray(disk_data)
            dd[12:12 + len(self.boot_code_silent)] = self.boot_code_silent

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

    def process_stream_data(self, stream_id: int, data: bytes):
        kind, unit, length, offset, address = struct.unpack('>BBHII', data)
        drive = self.drives[unit]
        if kind == READ_TRACK_REQ:
            if not drive.f:
                self.a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_NOT_PRESENT))
                return

            drive.f.seek(offset, 0)
            disk_data = drive.f.read(length)
            if offset == 0:
                disk_data = self.fixup_boot_block(disk_data, drive)

            self.mem_write_queue.append((stream_id, unit))
            self.a314d.send_write_mem_req(address, disk_data)
        elif kind == WRITE_TRACK_REQ:
            if not drive.f:
                self.a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
                return
            if not drive.writable:
                self.a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_WRITE_PROTECTED))
                return
            self.mem_read_queue.append((stream_id, unit, offset))
            self.a314d.send_read_mem_req(address, length)

    def process_write_mem_res(self):
        stream_id, unit = self.mem_write_queue.pop(0)
        if stream_id == self.current_stream_id:
            self.a314d.send_data(stream_id, struct.pack('>BBB', READ_TRACK_RES, unit, OP_RES_OK))

    def process_read_mem_res(self, disk_data: bytes):
        stream_id, unit, offset = self.mem_read_queue.pop(0)
        drive = self.drives[unit]
        if stream_id == self.current_stream_id:
            if not drive.f:
                self.a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_NOT_PRESENT))
                return

            drive.f.seek(offset, 0)
            drive.f.write(disk_data)
            self.a314d.send_data(stream_id, struct.pack('>BBB', WRITE_TRACK_RES, unit, OP_RES_OK))

    def process_drv_msg(self, stream_id: int, ptype: int, payload: bytes):
        if ptype == self.a314d.MSG_CONNECT:
            if payload == SERVICE_NAME.encode() and self.current_stream_id is None:
                logger.info('Amiga connected to disk service')
                self.current_stream_id = stream_id
                self.a314d.send_connect_response(stream_id, 0)

                for unit, (filename, writable) in self.auto_insert.items():
                    self.insert_adf(unit, filename, writable)
            else:
                self.a314d.send_connect_response(stream_id, 3)
        elif ptype == self.a314d.MSG_READ_MEM_RES:
            self.process_read_mem_res(payload)
        elif ptype == self.a314d.MSG_WRITE_MEM_RES:
            self.process_write_mem_res()
        elif self.current_stream_id == stream_id:
            if ptype == self.a314d.MSG_DATA:
                self.process_stream_data(stream_id, payload)
            elif ptype == self.a314d.MSG_EOS:
                raise AssertionError('EOS is not supposed to be in use')
            elif ptype == self.a314d.MSG_RESET:
                self.current_stream_id = None
                for i in range(4):
                    self.eject_adf(i)
                    self.drives[i].geometry_sent = False
                logger.info('Amiga disconnected from disk service')

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

    def handle_control_socket_readable(self, s: socket.socket):
        buf = s.recv(512)
        if not buf:
            s.close()
            self.control_sockets.remove(s)
            return

        try:
            buf = buf.decode('utf-8')
            arr = buf.strip().split(' ')
            cmd = arr[0]
            if cmd == 'eject':
                unit = int(arr[1])
                res = self.eject_adf(unit)
            elif cmd == 'insert':
                unit = int(arr[1])
                rw = arr[2] == '-rw'
                idx = 3 if rw else 2
                fn = ' '.join(arr[idx:])
                res = self.insert_adf(unit, fn, rw)
            else:
                res = f"Unknown command '{cmd}'"
        except:
            res = "Exception raised when processing command"
        s.sendall((res + '\n').encode('utf-8'))
        s.shutdown(socket.SHUT_WR)
        s.close()
        self.control_sockets.remove(s)

    def run(self):
        self.load_config()

        logger.info('Disk service is running')

        while True:
            try:
                rl = [self.a314d, self.control_server_socket] + self.control_sockets
                rl, _, _ = select.select(rl, [], [], 10.0)
            except KeyboardInterrupt:
                break

            for s in rl:
                if s == self.a314d:
                    self.handle_a314d_readable()
                elif s == self.control_server_socket:
                    cs, _ = self.control_server_socket.accept()
                    self.control_sockets.append(cs)
                else:
                    self.handle_control_socket_readable(s)

        for s in self.control_sockets:
            s.close()
        self.control_server_socket.close()
        self.a314d.close()

        logger.info('Disk service stopped')

if __name__ == '__main__':
    service = DiskService()
    service.run()
