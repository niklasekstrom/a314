import glob
import os
import select
import struct
import socket
import sys
import time

MSG_REGISTER_REQ		= 1
MSG_REGISTER_RES		= 2
MSG_DEREGISTER_REQ		= 3
MSG_DEREGISTER_RES		= 4
MSG_READ_MEM_REQ		= 5
MSG_READ_MEM_RES		= 6
MSG_WRITE_MEM_REQ		= 7
MSG_WRITE_MEM_RES		= 8
MSG_CONNECT		    	= 9
MSG_CONNECT_RESPONSE	        = 10
MSG_DATA		    	= 11
MSG_EOS			    	= 12
MSG_RESET		    	= 13

def wait_for_msg():
    header = ''
    while len(header) < 9:
        data = drv.recv(9 - len(header))
        header += data
    (plen, pstream, ptype) = struct.unpack('=IIB', header)
    payload = ''
    while len(payload) < plen:
        data = drv.recv(plen - len(payload))
        payload += data
    return (header, payload)

def send_register(name):
    m = struct.pack('=IIB', len(name), 0, MSG_REGISTER_REQ) + name
    drv.sendall(m)

def send_write_mem_req(address, data):
    m = struct.pack('=IIBI', 4 + len(data), 0, MSG_WRITE_MEM_REQ, address) + data
    drv.sendall(m)

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

write_mem_q = []
current_write_mem_stream_id = 0

def enqueue_write_mem_req(stream_id, address, data):
    global current_write_mem_stream_id

    if current_write_mem_stream_id == 0:
        current_write_mem_stream_id = stream_id
        send_write_mem_req(address, data)
    else:
        write_mem_q.append((stream_id, address, data))
    
sessions = {}

class VideoPlayerSession(object):
    def __init__(self, stream_id):
        self.stream_id = stream_id

        self.reset_after = None
        self.received_bpl_ptrs = False

    def start(self):
        self.fns = sorted(glob.glob('/home/pi/player/her_dither3/*.ami'))
        self.fc = 0
        self.read_next_frame()

    def close(self):
        del sessions[self.stream_id]

    def massage_pal(self):
        new_pal = ''
        for i in range(16):
            creg, col = struct.unpack('>HH', self.pal[4*i:4*(i+1)])
            new_pal += struct.pack('>H', col)
        self.pal = new_pal

    def read_next_frame(self):
        if self.fc < len(self.fns):
            fn = self.fns[self.fc]
            with open(fn, 'rb') as f:
                self.pal = f.read(64)
                self.bpl_data = f.read(40960)
            self.massage_pal()
            self.fc += 1
        else:
            self.bpl_data = None
            self.pal = None

    def process_msg_data(self, data):
        if not self.received_bpl_ptrs:
            self.addresses = struct.unpack('>II', data)
            self.received_bpl_ptrs = True
        else:
            self.next_bpl = ord(data[0])
            if self.bpl_data is None:
                send_data(self.stream_id, struct.pack('>H', 0))
            else:
                address = self.addresses[self.next_bpl]
                enqueue_write_mem_req(self.stream_id, address, self.bpl_data)

    def process_write_mem_res(self, data):
        send_data(self.stream_id, struct.pack('>H', 1) + self.pal)
        self.read_next_frame()

    def handle_timeout(self):
        if self.reset_after and self.reset_after < time.time():
            send_reset(self.stream_id)
            self.close()

    def fileno(self):
        return self.fd

def process_drv_write_mem_res(payload):
    global current_write_mem_stream_id

    stream_id = current_write_mem_stream_id

    if len(write_mem_q) != 0:
        current_write_mem_stream_id, address, data = write_mem_q.pop(0)
        send_write_mem_req(address, data)
    else:
        current_write_mem_stream_id = 0

    if stream_id in sessions:
        s = sessions[stream_id]
        s.process_write_mem_res(payload)

def process_drv_msg(stream_id, ptype, payload):
    if ptype == MSG_CONNECT:
        if payload == 'videoplayer':
            s = VideoPlayerSession(stream_id)
            sessions[stream_id] = s
            s.start()
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif ptype == MSG_WRITE_MEM_RES:
        process_drv_write_mem_res(payload)
    elif stream_id in sessions:
        s = sessions[stream_id]

        if ptype == MSG_DATA:
            s.process_msg_data(payload)
        elif ptype == MSG_EOS:
            send_eos(s.stream_id)
            s.close()
        elif ptype == MSG_RESET:
            s.close()

done = False

try:
    idx = sys.argv.index('-ondemand')
except ValueError:
    idx = -1

if idx != -1:
    fd = int(sys.argv[idx + 1])
    sockobj = socket.fromfd(fd, socket.AF_UNIX, socket.SOCK_STREAM)
    drv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0, _sock = sockobj)
    os.close(fd)
else:
    drv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    drv.connect(('localhost', 7110))
    drv.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    send_register('videoplayer')
    header, payload = wait_for_msg()
    if payload[0] != '\x01':
        print 'Unable to register videoplayer with driver'
        drv.close()
        done = True

rbuf = ''

if not done:
    print 'Video player server is running'
    
while not done:
    sel_fds = [drv]
    if idx == -1:
        sel_fds.append(sys.stdin)
    rfd, wfd, xfd = select.select(sel_fds, [], [], 5.0)

    for fd in rfd:
        if fd == sys.stdin:
            line = sys.stdin.readline()
            if not line or line.startswith('quit'):
                drv.close()
                done = True
        elif fd == drv:
            buf = drv.recv(1024)
            if not buf:
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

    if not done:
        for s in sessions.values():
            s.handle_timeout()
