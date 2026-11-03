#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2026 Jeremy Bauer
#
# pistats is a service that reports Raspberry Pi system statistics to
# the Amiga. The Amiga-side application (PiStats) connects to the
# "pistats" service and polls for statistics by sending a single-byte
# request (REQ_STATS). Each reply is a length-prefixed binary frame:
#
#   u32  body_len          (big-endian)
#   u8   proto_ver = 1
#   u8   ncores            (1..8)
#   u8   cpu_pct[8]        busy % per core (entries >= ncores are 0)
#   u16  reserved[2]
#   u32  mem_total_kb      /proc/meminfo MemTotal
#   u32  mem_avail_kb      /proc/meminfo MemAvailable
#   u32  disk_total_mb     statvfs("/") f_blocks * f_frsize / 1MiB
#   u32  disk_free_mb      statvfs("/") f_bavail * f_frsize / 1MiB
#   char kernel[64]        os.uname().release, NUL-terminated/padded
#
# Per-core CPU percentages are computed on the Pi from /proc/stat
# deltas; the baseline is sampled when the Amiga connects.

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s', level=logging.INFO)

import os
import select
import struct
import sys

from a314d import A314d

logger = logging.getLogger(__name__)

SERVICE_NAME = 'pistats'  # Service name for a314d

REQ_STATS = 1            # Request type for statistics

CONNECT_OK = 0               # Connect response: access granted
CONNECT_UNKNOWN_SERVICE = 3  # Connect response: service name not recognized

PROTO_VER = 1
MAX_CORES = 8

DRV_HEADER_SIZE = struct.calcsize('=IIB')  # a314d message header (plen, stream_id, ptype)
DRV_RECV_SIZE = 8192     # Read chunk size for the a314d connection
SELECT_TIMEOUT = 10.0    # Timeout for select, in seconds

BODY_FORMAT = '>BB8BHHIIII64s'  # proto_ver, ncores, cpu_pct[8],
                               # reserved[2], mem_total_kb, mem_avail_kb,
                               # disk_total_mb, disk_free_mb, kernel

def read_cpu_counters():
    """Return a list of (busy, total) jiffy tuples, one per cpuN line."""
    counters = []
    with open('/proc/stat') as f:
        for line in f:
            parts = line.split()
            if not parts or not parts[0].startswith('cpu'):
                break
            if not parts[0][3:].isdigit():
                continue  # skip the aggregate 'cpu' line
            if len(counters) >= MAX_CORES:
                break
            v = [int(x) for x in parts[1:]]
            v += [0] * (10 - len(v))  # pad if fewer fields
            busy = v[0] + v[1] + v[2] + v[5] + v[6] + v[7]  # user+nice+system+irq+softirq+steal
            total = busy + v[3] + v[4]                    # + idle + iowait
            counters.append((busy, total))
    return counters


def read_meminfo():
    total = avail = 0
    with open('/proc/meminfo') as f:
        for line in f:
            if line.startswith('MemTotal:'):
                total = int(line.split()[1])
            elif line.startswith('MemAvailable:'):
                avail = int(line.split()[1])
            if total and avail:
                break
    return total, avail


def read_disk():
    st = os.statvfs('/')
    total = (st.f_blocks * st.f_frsize) >> 20
    free = (st.f_bavail * st.f_frsize) >> 20
    return total, free


class InfoService(object):
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)
        self.stream_id = None
        self.prev_cpu = None   # baseline (busy, total) per core
        self.rbuf = b''

    def gather_stats(self):
        now = read_cpu_counters()
        ncores = len(now)
        pcts = [0] * MAX_CORES

        for i, (busy, total) in enumerate(now):
            if self.prev_cpu and i < len(self.prev_cpu):
                pbusy, ptotal = self.prev_cpu[i]
            else:
                pbusy, ptotal = 0, 0  # since-boot average
            dt = total - ptotal
            if dt > 0:
                pcts[i] = min(100, 100 * (busy - pbusy) // dt)

        self.prev_cpu = now

        mem_total, mem_avail = read_meminfo()
        disk_total, disk_free = read_disk()
        kernel = os.uname().release.encode()[:63]

        body = struct.pack(BODY_FORMAT, PROTO_VER, ncores, *pcts,
                           0, 0, mem_total, mem_avail, disk_total, disk_free, kernel)
        return struct.pack('>I', len(body)) + body

    def process_drv_msg(self, stream_id, ptype, payload):
        if ptype == self.a314d.MSG_CONNECT:
            if payload == SERVICE_NAME.encode():
                if self.stream_id is not None:
                    self.a314d.send_reset(self.stream_id)
                logger.info('Amiga connected to pistats service')
                self.stream_id = stream_id
                self.prev_cpu = read_cpu_counters()  # prime baseline
                self.a314d.send_connect_response(stream_id, CONNECT_OK)
            else:
                self.a314d.send_connect_response(stream_id, CONNECT_UNKNOWN_SERVICE)
        elif self.stream_id == stream_id:
            if ptype == self.a314d.MSG_DATA:
                self.a314d.send_data(stream_id, self.gather_stats())
            elif ptype == self.a314d.MSG_EOS:
                logger.info('Amiga disconnected from pistats service')
                self.a314d.send_eos(stream_id)
                self.stream_id = None
            elif ptype == self.a314d.MSG_RESET:
                logger.info('Amiga connection was reset')
                self.stream_id = None

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
        logger.info('pistats service is running')

        while True:
            try:
                rl = [self.a314d]
                if '-ondemand' not in sys.argv:
                    rl.append(sys.stdin)
                rl, _, _ = select.select(rl, [], [], SELECT_TIMEOUT)
            except KeyboardInterrupt:
                break

            for s in rl:
                if s == self.a314d:
                    self.handle_a314d_readable()
                elif s == sys.stdin:
                    line = sys.stdin.readline()
                    if not line or line.startswith('quit'):
                        if self.stream_id is not None:
                            self.a314d.send_reset(self.stream_id)
                        self.a314d.close()
                        logger.info('pistats service stopped')
                        return

        self.a314d.close()
        logger.info('pistats service stopped')


if __name__ == '__main__':
    service = InfoService()
    service.run()
