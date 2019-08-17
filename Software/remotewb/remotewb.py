# Copyright (c) 2018 Niklas Ekström

import asyncio
import json
import struct
import time
import websockets
import socket
import os
import sys

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

protocol = None

class MyProtocol(asyncio.Protocol):
    def connection_made(self, transport):
        global protocol
        protocol = self

        print('Connection to driver made')

        self.transport = transport
        sock = transport.get_extra_info('socket')

        if sock.family == socket.AF_UNIX:
            asyncio.ensure_future(create_websockets_server())
        else:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            name = 'remotewb'
            msg = struct.pack('=IIB', len(name), 0, MSG_REGISTER_REQ) + name.encode()
            self.transport.write(msg)

        self.rbuf = b''
        self.active_stream_id = None
        self.ptr = None
        self.first_msg = True

    def handle_msg(self, stream_id, mtype, msg):
        if mtype == MSG_REGISTER_RES:
            if msg[0] == 1:
                print('Registered name successfully')
                asyncio.ensure_future(create_websockets_server())
            else:
                print('Unable to register name with driver, shutting down')
                self.transport.close()
        elif mtype == MSG_CONNECT:
            if self.active_stream_id is not None:
                msg = struct.pack('=IIB', 0, self.active_stream_id, MSG_RESET)
                self.transport.write(msg)
            self.active_stream_id = stream_id
            msg = struct.pack('=IIB', 1, self.active_stream_id, MSG_CONNECT_RESPONSE) + b'\x00'
            self.transport.write(msg)
            self.first_msg = True
            print('Amiga connected')
        elif mtype == MSG_DATA:
            if self.active_stream_id == stream_id:
                if self.first_msg:
                    w, h, d, bpr, ptr, count = struct.unpack('>HHHHIH', msg[:14])
                    cmap = struct.unpack('>' + ('H' * count), msg[14:14 + 2*count])
                    self.w = w
                    self.h = h
                    self.d = d
                    self.bpr = bpr
                    self.ptr = ptr
                    self.cmap = cmap
                    self.first_msg = False
                    print('Received data from app', w, h, d, ptr, cmap)
                else:
                    x, y = struct.unpack('>HH', msg)
                    start_read_screenshot()
        elif mtype == MSG_EOS:
            if self.active_stream_id == stream_id:
                msg = struct.pack('=IIB', 0, self.active_stream_id, MSG_EOS)
                self.transport.write(msg)
                self.active_stream_id = None
        elif mtype == MSG_RESET:
            if self.active_stream_id == stream_id:
                self.active_stream_id = None
        elif mtype == MSG_READ_MEM_RES:
            read_screenshot_completed(msg)

    def data_received(self, data):
        self.rbuf += data

        while True:
            if len(self.rbuf) < 9:
                break

            mlen, stream_id, mtype = struct.unpack('=IIB', self.rbuf[:9])

            if len(self.rbuf) < 9 + mlen:
                break

            payload = self.rbuf[9:9+mlen]
            self.rbuf = self.rbuf[9+mlen:]
            self.handle_msg(stream_id, mtype, payload)

    def connection_lost(self, exc):
        print('Connection to driver lost', exc)
        if ws_server:
            ws_server.close()
            ws_server.wait_closed()
        loop.stop()

loop = asyncio.get_event_loop()
ws_server = None
active_browser = None

if os.name == 'nt':
    from PIL import Image
    import io

    def bpls2gif(bpls):
        pixels = bytearray(640*256)
        for i in range(3):
            bit = 1 << i
            for y in range(256):
                for b in range(80):
                    by = bpls[(i * 256 + y) * 80 + b]
                    for j in range(8):
                        if by & (1 << (7 - j)):
                            pixels[y*640 + b * 8 + j] |= bit

        pal = []
        for i in range(8):
            c = protocol.cmap[i]
            r = (c & 0x0f00) >> 8
            g = (c & 0x00f0) >> 4
            b = (c & 0x000f)
            pal.append((r << 4) | r)
            pal.append((g << 4) | g)
            pal.append((b << 4) | b)

        im = Image.frombytes('P', (640, 256), bytes(pixels))
        im.putpalette(pal)

        with io.BytesIO() as output:
            im.save(output, format = 'GIF')
            contents = output.getvalue()
            return contents
else:
    import bpls2gif as b2g

    pal_set = False

    def bpls2gif(bpls):
        global pal_set
        if not pal_set:
            pal = []
            for i in range(8):
                c = protocol.cmap[i]
                r = (c & 0x0f00) >> 8
                g = (c & 0x00f0) >> 4
                b = (c & 0x000f)
                pal.append((r << 4) | r)
                pal.append((g << 4) | g)
                pal.append((b << 4) | b)
            b2g.set_palette(bytes(pal))
            pal_set = True
        return b2g.encode(bpls)

async def send_img_to_active_browser(gif):
    jpart = json.dumps({'image':1, 'amiga_present':1}).encode()
    msg = len(jpart).to_bytes(4, 'big') + jpart + gif
    await active_browser.send(msg)

is_reading_screen = False

current_mouse = (0, 0, 0)
last_sent = (0, 0, 0)
event_q = []

def got_browser_event(e):
    global current_mouse

    if 'kc' in e:
        d = e['d']
        kc = e['kc']
        event_q.append(('k', d, kc))
    else:
        x, y, b = e['x'], e['y'], e['b']
        cx, cy, cb = current_mouse

        if (b ^ cb) & 1:
            cb ^= 1
            event_q.append(('m', x, y, cb))
        if (b ^ cb) & 2:
            cb ^= 2
            event_q.append(('m', x, y, cb))

        current_mouse = (x, y, b)

pc_to_ami = {8: 65, 9: 66, 13: 68, 20: 98, 27: 69, 32: 64, 37: 79, 38: 76, 39: 78, 40: 77, 46: 70, 47: 95, 48: 10, 49: 1, 50: 2, 51: 3, 52: 4, 53: 5, 54: 6, 55: 7, 56: 8, 57: 9, 65: 32, 66: 53, 67: 51, 68: 34, 69: 18, 70: 35, 71: 36, 72: 37, 73: 23, 74: 38, 75: 39, 76: 40, 77: 55, 78: 54, 79: 24, 80: 25, 81: 16, 82: 19, 83: 33, 84: 20, 85: 22, 86: 52, 87: 17, 88: 50, 89: 21, 90: 49, 91: 102, 92: 103, 96: 15, 97: 29, 98: 30, 99: 31, 100: 45, 101: 46, 102: 47, 103: 61, 104: 62, 105: 63, 106: 93, 107: 94, 109: 74, 110: 60, 111: 92, 112: 80, 113: 81, 114: 82, 115: 83, 116: 84, 117: 85, 118: 86, 119: 87, 120: 88, 121: 89, 160: 96, 161: 97, 162: 99, 164: 100, 165: 101, 186: 27, 187: 11, 188: 56, 189: 58, 190: 57, 191: 43, 192: 41, 219: 12, 221: 26, 222: 42, 226: 48}

def send_buffered_events():
    global last_sent
    if len(event_q) == 0 and last_sent == current_mouse:
        return

    lst = b''

    while len(event_q) != 0 and len(lst) < 240:
        e = event_q.pop(0)
        if e[0] == 'k':
            _, d, kc = e
            n = 0x4000 | pc_to_ami.get(kc, 0x40)
            if d == 'u':
                n |= 0x2000
            lst += struct.pack('>H', n)
        elif e[0] == 'm':
            _, x, y, b = e
            lst += struct.pack('>HHH', x, y, b)
            last_sent = (x, y, b)

    if len(event_q) == 0 and last_sent != current_mouse:
        x, y, b = current_mouse
        lst += struct.pack('>HHH', x, y, b)
        last_sent = current_mouse

    msg = struct.pack('=IIB', len(lst), protocol.active_stream_id, MSG_DATA) + lst
    protocol.transport.write(msg)

def read_screenshot_completed(bpls):
    global is_reading_screen
    is_reading_screen = False
    if active_browser is not None and protocol is not None and protocol.active_stream_id is not None:
        if protocol.bpr == 240:
            src = bpls
            dst = bytearray(80*256*3)
            for i in range(256):
                for j in range(3):
                    sr = 3*i + j
                    dr = j*256 + i
                    dst[dr*80:(dr+1)*80] = src[sr*80:(sr+1)*80]
            bpls = bytes(dst)
        gif = bpls2gif(bpls)
        asyncio.ensure_future(send_img_to_active_browser(gif))
        send_buffered_events()

def start_read_screenshot():
    global last_read_started, is_reading_screen
    if active_browser is not None and protocol is not None and protocol.active_stream_id is not None and not is_reading_screen:
        is_reading_screen = True
        msg = struct.pack('=IIBII', 8, 0, MSG_READ_MEM_REQ, protocol.ptr, 3*256*80)
        protocol.transport.write(msg)

async def browser_handler(websocket, path):
    global active_browser

    print('Connection to ', path, ' was made')

    is_active = active_browser is None
    if is_active:
        active_browser = websocket

    ami_present = False
    if protocol is not None and protocol.active_stream_id is not None:
        ami_present = True

    jpart = json.dumps({'is_active':(1 if is_active else 0), 'amiga_present':(1 if ami_present else 0)}).encode()
    msg = len(jpart).to_bytes(4, 'big') + jpart
    await websocket.send(msg)

    try:
        while True:
            msg = await websocket.recv()
            if is_active and ami_present:
                e = json.loads(msg)
                got_browser_event(e)
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print('Connection to ', path, ' was closed')

        if is_active:
            active_browser = None

async def create_websockets_server():
    global ws_server
    ws_server = await websockets.serve(browser_handler, '0.0.0.0', 6789, loop = loop)
    print('Websocket server created')

try:
    idx = sys.argv.index('-ondemand')
except ValueError:
    idx = -1

if idx != -1:
    fd = int(sys.argv[idx + 1])
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM, 0, fd)
    asyncio.ensure_future(loop.create_unix_connection(lambda: MyProtocol(), path = None, sock = sock))
else:
    asyncio.ensure_future(loop.create_connection(lambda: MyProtocol(), 'localhost', 7110))

loop.run_forever()
