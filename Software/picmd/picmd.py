import select
import sys
import socket
import threading
import time
import os
import struct
import pty
import signal
import termios
import fcntl
import logging
import json

logging.basicConfig(format = '%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

FS_CFG_FILE = '/etc/opt/a314/a314fs.conf'
PICMD_CFG_FILE = '/etc/opt/a314/picmd.conf'

volume_paths = {}
search_path = ""
env_vars = {}

def load_cfg():
    with open(FS_CFG_FILE, 'rt') as f:
        cfg = json.load(f)
        devs = cfg['devices']
        for _, dev in devs.items():
            volume_paths[dev['volume']] = dev['path']

    global search_path
    search_path = os.getenv('PATH')

    with open(PICMD_CFG_FILE, 'rt') as f:
        cfg = json.load(f)

        if 'paths' in cfg:
            search_path = ':'.join(cfg['paths']) + ':' + search_path
            os.environ['PATH'] = search_path

        if 'env_vars' in cfg:
            for key, val in cfg['env_vars'].items():
                env_vars[key] = val

load_cfg()

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
        if not data:
            logger.error('Connection to a314d was closed, terminating.')
            exit(-1)
        header += data
    (plen, stream_id, ptype) = struct.unpack('=IIB', header)
    payload = ''
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

CONSOLE_WIDTH = 77
CONSOLE_HEIGHT = 30

#CMAP = {'30':'32', '31':'33', '32':'33', '33':'33', '34':'32', '35':'32', '36':'32', '37':'37'}
CMAP = {'30':'30', '31':'31', '32':'32', '33':'33', '34':'34', '35':'35', '36':'36', '37':'37'}

sessions = {}

class PiCmdSession(object):
    def __init__(self, stream_id):
        self.stream_id = stream_id
        self.pid = 0

        self.first_packet = True
        self.reset_after = None

        self.was_esc = False
        self.in_cs = False
        self.holding = ''

        self.recv_in_cs = False
        self.recv_holding = ''

    def massage_ansi(self, text):
        out = ''
        for c in text:
            if not self.in_cs:
                if not self.was_esc:
                    if c == '\x1b':
                        self.was_esc = True
                    else:
                        out += c
                else: # self.was_esc
                    if c == '[':
                        self.was_esc = False
                        self.in_cs = True
                        self.holding = '\x1b['
                    elif c == '\x1b':
                        out += '\x1b'
                    else:
                        out += '\x1b'
                        out += c
                        self.was_esc = False
            else: # self.in_cs
                self.holding += c
                if c >= chr(0x40) and c <= chr(0x7e):
                    if c == 'm':
                        arr = self.holding[2:-1].split(';')
                        repl = '\x1b[' + (';'.join([CMAP[v] if v in CMAP else v for v in arr])) + 'm'
                        #print "Before:", self.holding[1:], " After:", repl[1:]
                        out += repl
                        #self.cmap['32'] = str((int(CMAP['32']) - 30 + 1) % 8 + 30)
                    else:
                        out += self.holding
                    self.holding = ''
                    self.in_cs = False
        return out

    def process_con_bytes(self, data):
        out = ''
        for c in data:
            if not self.recv_in_cs:
                if c == '\x9b':
                    self.recv_in_cs = True
                    self.recv_holding = '\x1b['
                else:
                    out += c
            else: # self.recv_in_cs
                self.recv_holding += c
                if c >= chr(0x40) and c <= chr(0x7e):
                    if c == 'r':
                        # Window Bounds Report
                        # ESC[1;1;rows;cols r
                        rows, cols = map(int, self.recv_holding[6:-2].split(';'))
                        winsize = struct.pack('HHHH', rows, cols, 0, 0)
                        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, winsize)
                    elif c == '|':
                        # Input Event Report
                        # ESC[12;0;0;x;x;x;x;x|
                        # Window resized
                        send_data(self.stream_id, '\x9b' + '0 q')
                    else:
                        out += self.recv_holding
                    self.recv_holding = ''
                    self.recv_in_cs = False
        if len(out) != 0:
            os.write(self.fd, out)

    def process_msg_data(self, data):
        if self.first_packet:
            if len(data) != 8:
                send_reset(self.stream_id)
                del sessions[self.stream_id]
            else:
                address, length = struct.unpack('>II', data)
                buf = read_mem(address, length)

                ind = 0
                component_count = ord(buf[ind])
                ind += 1

                components = []
                for _ in range(component_count):
                    n = ord(buf[ind])
                    ind += 1
                    components.append(buf[ind:ind+n])
                    ind += n

                arg_count = ord(buf[ind])
                ind += 1

                args = []
                for _ in range(arg_count):
                    n = ord(buf[ind])
                    ind += 1
                    args.append(buf[ind:ind+n])
                    ind += n

                if arg_count == 0:
                    args.append('bash')

                self.pid, self.fd = pty.fork()
                if self.pid == 0:
                    for key, val in env_vars.items():
                        os.putenv(key, val)
                    os.putenv('PATH', search_path)
                    os.putenv('TERM', 'ansi')
                    winsize = struct.pack('HHHH', CONSOLE_HEIGHT, CONSOLE_WIDTH, 0, 0)
                    fcntl.ioctl(sys.stdin, termios.TIOCSWINSZ, winsize)
                    if component_count != 0 and components[0] in volume_paths:
                        path = volume_paths[components[0]]
                        os.chdir(os.path.join(path, *components[1:]))
                    os.execvp(args[0], args)

                self.first_packet = False

                send_data(self.stream_id, '\x9b' + '12{')
                send_data(self.stream_id, '\x9b' + '0 q')

        elif self.pid:
            self.process_con_bytes(data)

    def close(self):
        if self.pid:
            os.kill(self.pid, signal.SIGTERM)
            self.pid = 0
            os.close(self.fd)
        del sessions[self.stream_id]

    def handle_text(self):
        try:
            text = os.read(self.fd, 1024)
            #text = self.massage_ansi(text)
            while len(text) > 0:
                take = min(len(text), 252)
                send_data(self.stream_id, text[:take])
                text = text[take:]
        except:
            #os.close(self.fd)
            os.kill(self.pid, signal.SIGTERM)
            self.pid = 0
            send_eos(self.stream_id)
            self.reset_after = time.time() + 10

    def handle_timeout(self):
        if self.reset_after and self.reset_after < time.time():
            send_reset(self.stream_id)
            del sessions[self.stream_id]

    def fileno(self):
        return self.fd

def process_drv_msg(stream_id, ptype, payload):
    if ptype == MSG_CONNECT:
        if payload == 'picmd':
            s = PiCmdSession(stream_id)
            sessions[stream_id] = s
            send_connect_response(stream_id, 0)
        else:
            send_connect_response(stream_id, 3)
    elif stream_id in sessions:
        s = sessions[stream_id]

        if ptype == MSG_DATA:
            s.process_msg_data(payload)
        elif ptype == MSG_EOS:
            if s.pid:
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

    send_register_req('picmd')
    _, _, payload = wait_for_msg()
    if payload[0] != '\x01':
        logger.error('Unable to register picmd with driver, shutting down')
        drv.close()
        done = True

rbuf = ''

if not done:
    logger.info('picmd server is running')

while not done:
    sel_fds = [drv] + [s for s in sessions.values() if s.pid]
    if idx == -1:
        sel_fds.append(sys.stdin)
    rfd, wfd, xfd = select.select(sel_fds, [], [], 5.0)

    for fd in rfd:
        if fd == sys.stdin:
            line = sys.stdin.readline()
            if not line or line.startswith('quit'):
                for s in sessions.values():
                    s.close()
                drv.close()
                done = True
        elif fd == drv:
            buf = drv.recv(1024)
            if not buf:
                for s in sessions.values():
                    s.close()
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
        else:
            fd.handle_text()

    for s in sessions.values():
        s.handle_timeout()
