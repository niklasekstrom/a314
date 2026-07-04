#!/usr/bin/python3
# -*- coding: utf-8 -*-

import json
import logging
import select
import socket
import struct
from typing import Dict, List, Optional

from a314d import A314d

logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)
logger = logging.getLogger(__name__)

SERVICE_NAME = 'scsi'
CONF_FILE = '/etc/opt/a314/scsi.conf'

# Wire protocol (matches a314scsi_protocol.h).
REQ_FMT = '>BBBB16sBBHII'   # kind,unit,cdb_len,rsv,cdb,dir,pad,rsv,data_length,address
RES_FMT = '>BBBBBBHI'       # kind,status,sense_key,asc,ascq,pad,rsv,actual_length
CMD_RES = 2

DIR_NONE = 0
DIR_READ = 1
DIR_WRITE = 2

STATUS_GOOD = 0x00
STATUS_CHECK_CONDITION = 0x02

SENSE_NOT_READY = 0x02
SENSE_ILLEGAL_REQUEST = 0x05


class DiskImage(object):
    def __init__(self, filename: str, writable: bool, block_size: int = 512):
        self.filename = filename
        self.writable = writable
        self.block_size = block_size
        self.f = None
        self.blocks = 0
        # Physical geometry reported via MODE SENSE (HDToolBox reads this).
        self.cylinders = 0
        self.heads = 0
        self.sectors = 0  # sectors per track

    def open(self) -> bool:
        try:
            self.f = open(self.filename, 'r+b' if self.writable else 'rb')
            self.f.seek(0, 2)
            self.blocks = self.f.tell() // self.block_size
            self._compute_geometry()
            logger.info('Opened %s: %d blocks x %d bytes (%dc/%dh/%ds)%s',
                        self.filename, self.blocks, self.block_size,
                        self.cylinders, self.heads, self.sectors,
                        '' if self.writable else ' (read-only)')
            return True
        except Exception as e:
            logger.error('Failed to open %s: %s', self.filename, e)
            return False

    def _compute_geometry(self):
        # Prefer the RDB's own geometry so HDToolBox agrees with the on-disk
        # partitioning; otherwise synthesize a plausible geometry.
        self.f.seek(0)
        b0 = self.f.read(self.block_size)
        if b0[:4] == b'RDSK':
            cyls = int.from_bytes(b0[64:68], 'big')
            secs = int.from_bytes(b0[68:72], 'big')
            heads = int.from_bytes(b0[72:76], 'big')
            if cyls and secs and heads:
                self.cylinders, self.sectors, self.heads = cyls, secs, heads
                return
        self.heads = 16
        self.sectors = 63
        self.cylinders = max(1, self.blocks // (self.heads * self.sectors))

    def read(self, lba: int, blocks: int) -> Optional[bytes]:
        if not self.f or lba + blocks > self.blocks:
            return None
        self.f.seek(lba * self.block_size)
        return self.f.read(blocks * self.block_size)

    def write(self, lba: int, data: bytes) -> bool:
        if not self.f or not self.writable:
            return False
        if lba + (len(data) // self.block_size) > self.blocks:
            return False
        self.f.seek(lba * self.block_size)
        self.f.write(data)
        return True


class ScsiService(object):
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)
        self.current_stream_id: Optional[int] = None
        self.rbuf = b''
        self.images: Dict[int, DiskImage] = {}

        # Pending bulk transfers awaiting a314 memory completion.
        self.mem_write_queue = []   # (stream_id, status, sense_key, asc, ascq, actual)
        self.mem_read_queue = []    # (stream_id, unit, cdb)

    def load_config(self):
        try:
            with open(CONF_FILE, 'rt') as f:
                j = json.load(f)
        except Exception as e:
            logger.warning('No config (%s): %s', CONF_FILE, e)
            return
        for e in j.get('units', []):
            unit = e['unit']
            img = DiskImage(e['filename'], e.get('rw', False), e.get('block_size', 512))
            if img.open():
                self.images[unit] = img

    # --- SCSI CDB execution --------------------------------------------------

    def inquiry_data(self, img: DiskImage) -> bytes:
        d = bytearray(36)
        d[0] = 0x00              # direct-access block device
        d[1] = 0x00              # not removable
        d[2] = 0x02              # SCSI-2
        d[3] = 0x02              # response data format
        d[4] = 31                # additional length
        d[8:16] = b'A314    '
        d[16:32] = b'HDF             '
        d[32:36] = b'0001'
        return bytes(d)

    def read_capacity10(self, img: DiskImage) -> bytes:
        last = img.blocks - 1
        if last > 0xFFFFFFFF:
            last = 0xFFFFFFFF
        return struct.pack('>II', last, img.block_size)

    def mode_page_format(self, img: DiskImage) -> bytes:
        # Page 3: Format Device Parameters - carries the sectors-per-track that
        # HDToolBox reports as "track size".
        p = bytearray(24)
        p[0] = 0x03
        p[1] = 22
        struct.pack_into('>H', p, 10, img.sectors & 0xFFFF)     # sectors per track
        struct.pack_into('>H', p, 12, img.block_size & 0xFFFF)  # bytes per sector
        return bytes(p)

    def mode_page_geometry(self, img: DiskImage) -> bytes:
        # Page 4: Rigid Disk Drive Geometry - cylinders and heads.
        p = bytearray(24)
        p[0] = 0x04
        p[1] = 22
        cyl = img.cylinders
        p[2] = (cyl >> 16) & 0xFF
        p[3] = (cyl >> 8) & 0xFF
        p[4] = cyl & 0xFF
        p[5] = img.heads & 0xFF
        return bytes(p)

    def mode_sense6(self, img: DiskImage, cdb: bytes) -> bytes:
        page = cdb[2] & 0x3F
        wp = 0x80 if not img.writable else 0x00
        body = b''
        if page in (0x03, 0x3F):
            body += self.mode_page_format(img)
        if page in (0x04, 0x3F):
            body += self.mode_page_geometry(img)
        # Header: mode data length, medium type, device-specific (WP), block-desc length.
        header = bytes([3 + len(body), 0, wp, 0])
        return header + body

    # Execute a device->host / no-data command, returning (status, sk, asc, ascq, data).
    def exec_read(self, img: DiskImage, cdb: bytes, alloc: int):
        op = cdb[0]
        if op == 0x00:                                   # TEST UNIT READY
            return (STATUS_GOOD, 0, 0, 0, b'')
        if op == 0x12:                                   # INQUIRY
            return (STATUS_GOOD, 0, 0, 0, self.inquiry_data(img)[:alloc or 36])
        if op == 0x25:                                   # READ CAPACITY(10)
            return (STATUS_GOOD, 0, 0, 0, self.read_capacity10(img))
        if op == 0x1A:                                   # MODE SENSE(6)
            return (STATUS_GOOD, 0, 0, 0, self.mode_sense6(img, cdb)[:alloc or 255])
        if op in (0x08, 0x28):                           # READ(6)/READ(10)
            lba, blocks = self.rw_params(cdb)
            data = img.read(lba, blocks)
            if data is None:
                return (STATUS_CHECK_CONDITION, SENSE_ILLEGAL_REQUEST, 0x21, 0, b'')
            return (STATUS_GOOD, 0, 0, 0, data)
        # Unknown command.
        return (STATUS_CHECK_CONDITION, SENSE_ILLEGAL_REQUEST, 0x20, 0, b'')

    def rw_params(self, cdb: bytes):
        if cdb[0] in (0x08, 0x0A):                       # READ(6)/WRITE(6)
            lba = ((cdb[1] & 0x1F) << 16) | (cdb[2] << 8) | cdb[3]
            blocks = cdb[4] or 256                       # a length of 0 means 256
        else:                                            # READ(10)/WRITE(10)
            lba = int.from_bytes(cdb[2:6], 'big')
            blocks = int.from_bytes(cdb[7:9], 'big')
        return lba, blocks

    # --- Request handling ----------------------------------------------------

    def reply(self, stream_id, status, sk, asc, ascq, actual):
        self.a314d.send_data(stream_id, struct.pack(RES_FMT, CMD_RES, status, sk, asc, ascq, 0, 0, actual))

    def process_stream_data(self, stream_id: int, data: bytes):
        kind, unit, cdb_len, _, cdb, direction, _, _, data_length, address = struct.unpack(REQ_FMT, data)
        cdb = cdb[:cdb_len]

        img = self.images.get(unit)
        if img is None:
            self.reply(stream_id, STATUS_CHECK_CONDITION, SENSE_NOT_READY, 0x3A, 0, 0)
            return

        if direction == DIR_WRITE:
            self.mem_read_queue.append((stream_id, unit, bytes(cdb)))
            self.a314d.send_read_mem_req(address, data_length)
            return

        status, sk, asc, ascq, respdata = self.exec_read(img, cdb, data_length)

        if direction == DIR_READ and respdata and address:
            self.mem_write_queue.append((stream_id, status, sk, asc, ascq, len(respdata)))
            self.a314d.send_write_mem_req(address, respdata)
        else:
            self.reply(stream_id, status, sk, asc, ascq, len(respdata))

    def process_write_mem_res(self):
        stream_id, status, sk, asc, ascq, actual = self.mem_write_queue.pop(0)
        if stream_id == self.current_stream_id:
            self.reply(stream_id, status, sk, asc, ascq, actual)

    def process_read_mem_res(self, data: bytes):
        stream_id, unit, cdb = self.mem_read_queue.pop(0)
        if stream_id != self.current_stream_id:
            return
        img = self.images.get(unit)
        if img is None or cdb[0] not in (0x0A, 0x2A):  # WRITE(6)/WRITE(10)
            self.reply(stream_id, STATUS_CHECK_CONDITION, SENSE_ILLEGAL_REQUEST, 0x20, 0, 0)
            return
        lba, _ = self.rw_params(cdb)
        if img.write(lba, data):
            self.reply(stream_id, STATUS_GOOD, 0, 0, 0, len(data))
        else:
            self.reply(stream_id, STATUS_CHECK_CONDITION, SENSE_ILLEGAL_REQUEST, 0x21, 0, 0)

    def process_drv_msg(self, stream_id: int, ptype: int, payload: bytes):
        if ptype == self.a314d.MSG_CONNECT:
            if payload == SERVICE_NAME.encode() and self.current_stream_id is None:
                logger.info('Amiga connected to scsi service')
                self.current_stream_id = stream_id
                self.a314d.send_connect_response(stream_id, 0)
            else:
                self.a314d.send_connect_response(stream_id, 3)
        elif ptype == self.a314d.MSG_READ_MEM_RES:
            self.process_read_mem_res(payload)
        elif ptype == self.a314d.MSG_WRITE_MEM_RES:
            self.process_write_mem_res()
        elif self.current_stream_id == stream_id:
            if ptype == self.a314d.MSG_DATA:
                self.process_stream_data(stream_id, payload)
            elif ptype == self.a314d.MSG_RESET:
                self.current_stream_id = None
                logger.info('Amiga disconnected from scsi service')

    def handle_a314d_readable(self):
        buf = self.a314d.drv.recv(8192)
        if not buf:
            logger.error('Connection to a314d closed, shutting down')
            exit(-1)

        self.rbuf += buf
        while len(self.rbuf) >= 9:
            (plen, stream_id, ptype) = struct.unpack('=IIB', self.rbuf[:9])
            if len(self.rbuf) < 9 + plen:
                break
            payload = self.rbuf[9:9 + plen]
            self.rbuf = self.rbuf[9 + plen:]
            self.process_drv_msg(stream_id, ptype, payload)

    def run(self):
        self.load_config()
        logger.info('SCSI service is running')

        while True:
            try:
                rl, _, _ = select.select([self.a314d], [], [], 10.0)
            except KeyboardInterrupt:
                break
            for s in rl:
                if s == self.a314d:
                    self.handle_a314d_readable()

        self.a314d.close()
        logger.info('SCSI service stopped')


if __name__ == '__main__':
    ScsiService().run()
