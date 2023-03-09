#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2022 Niklas Ekström

import logging
logging.basicConfig(format='%(levelname)s, %(asctime)s, %(name)s, line %(lineno)d: %(message)s')

import errno
import fcntl
import os
import platform
import pyudev
import select
import struct
import time
from typing import Optional, Tuple, Dict

from a314d import A314d

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

SERVICE_NAME = 'hid'
MIN_SEND_INTERVAL = 0.02 # 20 ms.

EVIOCGRAB = 0x40044590

# Input event codes take from:
# https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

EV_SYN = 0x00
EV_KEY = 0x01
EV_REL = 0x02
EV_ABS = 0x03
EV_MSC = 0x04

REL_X = 0x00
REL_Y = 0x01
REL_WHEEL_HI_RES = 0x0b

MSC_SERIAL = 0x00
MSC_PULSELED = 0x01
MSC_GESTURE = 0x02
MSC_RAW = 0x03
MSC_SCAN = 0x04

BTN_MOUSE = 0x110
BTN_LEFT = 0x110
BTN_RIGHT = 0x111
BTN_MIDDLE = 0x112
BTN_SIDE = 0x113
BTN_EXTRA = 0x114
BTN_FORWARD = 0x115
BTN_BACK = 0x116
BTN_TASK = 0x117

KEY_RESERVED = 0
KEY_ESC = 1
KEY_1 = 2
KEY_2 = 3
KEY_3 = 4
KEY_4 = 5
KEY_5 = 6
KEY_6 = 7
KEY_7 = 8
KEY_8 = 9
KEY_9 = 10
KEY_0 = 11
KEY_MINUS = 12
KEY_EQUAL = 13
KEY_BACKSPACE = 14
KEY_TAB = 15
KEY_Q = 16
KEY_W = 17
KEY_E = 18
KEY_R = 19
KEY_T = 20
KEY_Y = 21
KEY_U = 22
KEY_I = 23
KEY_O = 24
KEY_P = 25
KEY_LEFTBRACE = 26
KEY_RIGHTBRACE = 27
KEY_ENTER = 28
KEY_LEFTCTRL = 29
KEY_A = 30
KEY_S = 31
KEY_D = 32
KEY_F = 33
KEY_G = 34
KEY_H = 35
KEY_J = 36
KEY_K = 37
KEY_L = 38
KEY_SEMICOLON = 39
KEY_APOSTROPHE = 40
KEY_GRAVE = 41
KEY_LEFTSHIFT = 42
KEY_BACKSLASH = 43
KEY_Z = 44
KEY_X = 45
KEY_C = 46
KEY_V = 47
KEY_B = 48
KEY_N = 49
KEY_M = 50
KEY_COMMA = 51
KEY_DOT = 52
KEY_SLASH = 53
KEY_RIGHTSHIFT = 54
KEY_KPASTERISK = 55
KEY_LEFTALT = 56
KEY_SPACE = 57
KEY_CAPSLOCK = 58
KEY_F1 = 59
KEY_F2 = 60
KEY_F3 = 61
KEY_F4 = 62
KEY_F5 = 63
KEY_F6 = 64
KEY_F7 = 65
KEY_F8 = 66
KEY_F9 = 67
KEY_F10 = 68
KEY_NUMLOCK = 69
KEY_SCROLLLOCK = 70
KEY_KP7 = 71
KEY_KP8 = 72
KEY_KP9 = 73
KEY_KPMINUS = 74
KEY_KP4 = 75
KEY_KP5 = 76
KEY_KP6 = 77
KEY_KPPLUS = 78
KEY_KP1 = 79
KEY_KP2 = 80
KEY_KP3 = 81
KEY_KP0 = 82
KEY_KPDOT = 83
KEY_102ND = 86
KEY_F11 = 87
KEY_F12 = 88
KEY_RO = 89
KEY_KATAKANA = 90
KEY_HIRAGANA = 91
KEY_HENKAN = 92
KEY_MUHENKAN = 94
KEY_KPJPCOMMA = 95
KEY_KPENTER = 96
KEY_RIGHTCTRL = 97
KEY_KPSLASH = 98
KEY_SYSRQ = 99
KEY_RIGHTALT = 100
KEY_LINEFEED = 101
KEY_HOME = 102
KEY_UP = 103
KEY_PAGEUP = 104
KEY_LEFT = 105
KEY_RIGHT = 106
KEY_END = 107
KEY_DOWN = 108
KEY_PAGEDOWN = 109
KEY_INSERT = 110
KEY_DELETE = 111
KEY_MACRO = 112
KEY_MUTE = 113
KEY_VOLUMEDOWN = 114
KEY_VOLUMEUP = 115
KEY_POWER = 116
KEY_KPEQUAL = 117
KEY_KPPLUSMINUS = 118
KEY_PAUSE = 119
KEY_SCALE = 120
KEY_KPCOMMA = 121
KEY_HANGEUL = 122
KEY_HANGUEL = KEY_HANGEUL
KEY_HANJA = 123
KEY_YEN = 124
KEY_LEFTMETA = 125
KEY_RIGHTMETA = 126
KEY_COMPOSE = 127

# Amiga input event definitions taken from <devices/inputevent.h>
IEQUALIFIER_LSHIFT = 0x0001
IEQUALIFIER_RSHIFT = 0x0002
IEQUALIFIER_CAPSLOCK = 0x0004
IEQUALIFIER_CONTROL = 0x0008
IEQUALIFIER_LALT = 0x0010
IEQUALIFIER_RALT = 0x0020
IEQUALIFIER_LCOMMAND = 0x0040
IEQUALIFIER_RCOMMAND = 0x0080
IEQUALIFIER_NUMERICPAD = 0x0100
IEQUALIFIER_REPEAT = 0x0200
IEQUALIFIER_INTERRUPT = 0x0400
IEQUALIFIER_MULTIBROADCAST = 0x0800
IEQUALIFIER_MIDBUTTON = 0x1000
IEQUALIFIER_RBUTTON = 0x2000
IEQUALIFIER_LEFTBUTTON = 0x4000
IEQUALIFIER_RELATIVEMOUSE = 0x8000

IECODE_UP_PREFIX = 0x80
IECODE_LBUTTON = 0x68
IECODE_RBUTTON = 0x69
IECODE_MBUTTON = 0x6A
IECODE_NOBUTTON = 0xFF

NM_WHEEL_UP = 0x7a
NM_WHEEL_DOWN = 0x7b

MOUSE_CODE_QUAL = {
    BTN_LEFT: (IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON),
    BTN_RIGHT: (IECODE_RBUTTON, IEQUALIFIER_RBUTTON),
    BTN_MIDDLE: (IECODE_MBUTTON, IEQUALIFIER_MIDBUTTON)
}

RK_GRAVE = 0x0
RK_1 = 0x1
RK_2 = 0x2
RK_3 = 0x3
RK_4 = 0x4
RK_5 = 0x5
RK_6 = 0x6
RK_7 = 0x7
RK_8 = 0x8
RK_9 = 0x9
RK_0 = 0xa
RK_MINUS = 0xb
RK_EQUAL = 0xc
RK_BACKSLASH = 0xd
RK_BACKSPACE = 0x41
RK_TAB = 0x42
RK_Q = 0x10
RK_W = 0x11
RK_E = 0x12
RK_R = 0x13
RK_T = 0x14
RK_Y = 0x15
RK_U = 0x16
RK_I = 0x17
RK_O = 0x18
RK_P = 0x19
RK_LEFTBRACE = 0x1a
RK_RIGHTBRACE = 0x1b
RK_ENTER = 0x44
RK_CTRL = 0x63
RK_CAPSLOCK = 0x62
RK_A = 0x20
RK_S = 0x21
RK_D = 0x22
RK_F = 0x23
RK_G = 0x24
RK_H = 0x25
RK_J = 0x26
RK_K = 0x27
RK_L = 0x28
RK_SEMICOLON = 0x29
RK_APOSTROPHE = 0x2a
RK_INTERNATIONAL1 = 0x2b
RK_LEFTSHIFT = 0x60
RK_LESSTHAN = 0x30
RK_Z = 0x31
RK_X = 0x32
RK_C = 0x33
RK_V = 0x34
RK_B = 0x35
RK_N = 0x36
RK_M = 0x37
RK_COMMA = 0x38
RK_DOT = 0x39
RK_SLASH = 0x3a
RK_RIGHTSHIFT = 0x61
RK_LEFTALT = 0x64
RK_LEFTMETA = 0x66
RK_SPACE = 0x40
RK_RIGHTMETA = 0x67
RK_RIGHTALT = 0x64
RK_ESC = 0x45
RK_F1 = 0x50
RK_F2 = 0x51
RK_F3 = 0x52
RK_F4 = 0x53
RK_F5 = 0x54
RK_F6 = 0x55
RK_F7 = 0x56
RK_F8 = 0x57
RK_F9 = 0x58
RK_F10 = 0x59
RK_UP = 0x4c
RK_DOWN = 0x4d
RK_RIGHT = 0x4e
RK_LEFT = 0x4f
RK_DELETE = 0x46
RK_HELP = 0x5f
RK_KPLEFTPAREN = 0x5a
RK_KPRIGHTPAREN = 0x5b
RK_KPSLASH = 0x5c
RK_KPASTERISK = 0x5d
RK_KP7 = 0x3d
RK_KP8 = 0x3e
RK_KP9 = 0x3f
RK_KPMINUS = 0x4a
RK_KP4 = 0x2d
RK_KP5 = 0x2e
RK_KP6 = 0x2f
RK_KPPLUS = 0x5e
RK_KP1 = 0x1d
RK_KP2 = 0x1e
RK_KP3 = 0x1f
RK_KP0 = 0xf
RK_KPDOT = 0x3c
RK_KPENTER = 0x43

CODE_MAP = {KEY_GRAVE: RK_GRAVE, KEY_1: RK_1, KEY_2: RK_2, KEY_3: RK_3, KEY_4: RK_4, KEY_5: RK_5, KEY_6: RK_6, KEY_7: RK_7, KEY_8: RK_8, KEY_9: RK_9, KEY_0: RK_0, KEY_MINUS: RK_MINUS, KEY_EQUAL: RK_EQUAL, KEY_INSERT: RK_BACKSLASH, KEY_BACKSPACE: RK_BACKSPACE, KEY_TAB: RK_TAB, KEY_Q: RK_Q, KEY_W: RK_W, KEY_E: RK_E, KEY_R: RK_R, KEY_T: RK_T, KEY_Y: RK_Y, KEY_U: RK_U, KEY_I: RK_I, KEY_O: RK_O, KEY_P: RK_P, KEY_LEFTBRACE: RK_LEFTBRACE, KEY_RIGHTBRACE: RK_RIGHTBRACE, KEY_ENTER: RK_ENTER, KEY_LEFTCTRL: RK_CTRL, KEY_RIGHTCTRL: RK_CTRL, KEY_CAPSLOCK: RK_CAPSLOCK, KEY_A: RK_A, KEY_S: RK_S, KEY_D: RK_D, KEY_F: RK_F, KEY_G: RK_G, KEY_H: RK_H, KEY_J: RK_J, KEY_K: RK_K, KEY_L: RK_L, KEY_SEMICOLON: RK_SEMICOLON, KEY_APOSTROPHE: RK_APOSTROPHE, KEY_BACKSLASH: RK_INTERNATIONAL1, KEY_LEFTSHIFT: RK_LEFTSHIFT, KEY_102ND: RK_LESSTHAN, KEY_Z: RK_Z, KEY_X: RK_X, KEY_C: RK_C, KEY_V: RK_V, KEY_B: RK_B, KEY_N: RK_N, KEY_M: RK_M, KEY_COMMA: RK_COMMA, KEY_DOT: RK_DOT, KEY_SLASH: RK_SLASH, KEY_RIGHTSHIFT: RK_RIGHTSHIFT, KEY_LEFTALT: RK_LEFTALT, KEY_LEFTMETA: RK_LEFTMETA, KEY_SPACE: RK_SPACE, KEY_RIGHTMETA: RK_RIGHTMETA, KEY_RIGHTALT: RK_RIGHTALT, KEY_ESC: RK_ESC, KEY_F1: RK_F1, KEY_F2: RK_F2, KEY_F3: RK_F3, KEY_F4: RK_F4, KEY_F5: RK_F5, KEY_F6: RK_F6, KEY_F7: RK_F7, KEY_F8: RK_F8, KEY_F9: RK_F9, KEY_F10: RK_F10, KEY_UP: RK_UP, KEY_DOWN: RK_DOWN, KEY_RIGHT: RK_RIGHT, KEY_LEFT: RK_LEFT, KEY_DELETE: RK_DELETE, KEY_HOME: RK_HELP, KEY_NUMLOCK: RK_KPLEFTPAREN, KEY_SCROLLLOCK: RK_KPRIGHTPAREN, KEY_KPSLASH: RK_KPSLASH, KEY_KPASTERISK: RK_KPASTERISK, KEY_KP7: RK_KP7, KEY_KP8: RK_KP8, KEY_KP9: RK_KP9, KEY_KPMINUS: RK_KPMINUS, KEY_KP4: RK_KP4, KEY_KP5: RK_KP5, KEY_KP6: RK_KP6, KEY_KPPLUS: RK_KPPLUS, KEY_KP1: RK_KP1, KEY_KP2: RK_KP2, KEY_KP3: RK_KP3, KEY_KP0: RK_KP0, KEY_KPDOT: RK_KPDOT, KEY_KPENTER: RK_KPENTER}

SHIFT_KEY_QUALS = {
    RK_LEFTSHIFT: IEQUALIFIER_LSHIFT,
    RK_RIGHTSHIFT: IEQUALIFIER_RSHIFT,
    RK_CAPSLOCK: IEQUALIFIER_CAPSLOCK,
    RK_CTRL: IEQUALIFIER_CONTROL,
    RK_LEFTALT: IEQUALIFIER_LALT,
    RK_RIGHTALT: IEQUALIFIER_RALT,
    RK_LEFTMETA: IEQUALIFIER_LCOMMAND,
    RK_RIGHTMETA: IEQUALIFIER_RCOMMAND
}

NUMPAD_KEYS = set([
    RK_KPLEFTPAREN, RK_KPRIGHTPAREN, RK_KPSLASH, RK_KPASTERISK,
    RK_KP7, RK_KP8, RK_KP9, RK_KPMINUS,
    RK_KP4, RK_KP5, RK_KP6, RK_KPPLUS,
    RK_KP1, RK_KP2, RK_KP3,
    RK_KP0, RK_KPDOT, RK_KPENTER
])

class Device(object):
    def __init__(self, name: str, type: str, fd: int):
        self.name = name
        self.type = type
        self.fd = fd

        self.qualifier: int = IEQUALIFIER_RELATIVEMOUSE if type == 'mouse' else 0
        self.buffered_movement: Optional[Tuple[int, int]] = None

    def fileno(self):
        return self.fd

class HidService(object):
    def __init__(self):
        self.a314d = A314d(SERVICE_NAME)

        is_64bit = '64' in platform.architecture()[0]
        self.event_length = 24 if is_64bit else 16

        self.devices: Dict[str, Device] = {}

        self.context = pyudev.Context()

        for device in self.context.list_devices(subsystem='input'):
            device_type = None

            for dl in device.device_links:
                if dl.endswith('event-mouse'):
                    device_type = 'mouse'
                    break
                elif dl.endswith('event-kbd'):
                    device_type = 'keyboard'
                    break

            if device_type:
                self.open_device(device, device_type)

        self.monitor = pyudev.Monitor.from_netlink(self.context)
        self.monitor.filter_by('input')
        self.monitor.start()

        self.rbuf = b''
        self.current_stream_id = None
        self.next_send = time.time()

    def open_device(self, device, device_type: str):
        device_name = device.get('DEVNAME')
        fd = os.open(device_name, os.O_RDONLY | os.O_NONBLOCK)
        if fd < 0:
            logger.error('Unable to open %s device %s', device_type, device_name)
        else:
            try:
                fcntl.ioctl(fd, EVIOCGRAB, 1)
                logger.info('Opened %s device %s', device_type, device.get('ID_SERIAL'))
                self.devices[device_name] = Device(device_name, device_type, fd)
            except:
                logger.warning('Unable to grab %s device %s', device_type, device_name)
                os.close(fd)

    def process_a314d_msg(self, stream_id, ptype, payload):
        if ptype == self.a314d.MSG_CONNECT:
            if payload == SERVICE_NAME.encode() and self.current_stream_id is None:
                logger.info('Amiga connected to HID service')
                self.current_stream_id = stream_id
                self.a314d.send_connect_response(stream_id, 0)
            else:
                self.a314d.send_connect_response(stream_id, 3)
        elif self.current_stream_id == stream_id:
            if ptype == self.a314d.MSG_DATA:
                # Shouldn't receive messages from Amiga.
                pass
            elif ptype == self.a314d.MSG_EOS:
                # Shouldn't receive messages from Amiga.
                pass
            elif ptype == self.a314d.MSG_RESET:
                self.current_stream_id = None
                for dev in self.devices.values():
                    dev.qualifier &= IEQUALIFIER_RELATIVEMOUSE
                    dev.buffered_movement = None
                logger.info('Amiga disconnected from HID service')

    def handle_a314d_readable(self):
        buf = self.a314d.drv.recv(1024)
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

            self.rbuf = self.rbuf[9:]
            payload = self.rbuf[:plen]
            self.rbuf = self.rbuf[plen:]

            self.process_a314d_msg(stream_id, ptype, payload)

    def handle_udev_event(self):
        device = self.monitor.poll()
        if device is None:
            return

        device_type = None

        for dl in device.device_links:
            if dl.endswith('event-mouse'):
                device_type = 'mouse'
                break
            elif dl.endswith('event-kbd'):
                device_type = 'keyboard'
                break

        action = device.action
        if not device_type or action not in ('add', 'remove'):
            return

        device_name: str = device.get('DEVNAME')

        if action == 'add':
            if device_name not in self.devices:
                self.open_device(device, device_type)
        elif action == 'remove':
            if device_name in self.devices:
                dev = self.devices[device_name]
                os.close(dev.fd)
                del self.devices[device_name]
                logger.info('Removed %s device %s', dev.type, dev.name)

    def handle_dev_readable(self, dev: Device):
        try:
            data = os.read(dev.fd, self.event_length)
        except OSError as e:
            if e.errno != errno.ENODEV:
                raise

            os.close(dev.fd)
            logger.info('Removed %s device %s', dev.type, dev.name)
            if dev.name in self.devices:
                del self.devices[dev.name]
            return

        if not self.current_stream_id or len(data) < self.event_length:
            return

        typ, code, value = struct.unpack_from('<HHi', data, self.event_length - 8)
        logger.debug(f'Event: type={typ}, code={code:2}, value={value}')

        do_send = False
        iecode = IECODE_NOBUTTON
        extra_qualifier = 0

        if typ == EV_KEY:
            if dev.type == 'mouse':
                if code in MOUSE_CODE_QUAL:
                    iec, qual_mask = MOUSE_CODE_QUAL[code]
                    if value == 0:
                        dev.qualifier &= ~qual_mask
                        iecode = iec | IECODE_UP_PREFIX
                    elif value == 1:
                        dev.qualifier |= qual_mask
                        iecode = iec
                    do_send = True
            elif dev.type == 'keyboard':
                if code in CODE_MAP and value >= 0 and value <= 2:
                    iecode = CODE_MAP[code]

                    if iecode in SHIFT_KEY_QUALS:
                        qual_mask = SHIFT_KEY_QUALS[iecode]
                        if iecode == RK_CAPSLOCK:
                            if value == 1:
                                dev.qualifier ^= qual_mask
                        else:
                            if value == 0:
                                dev.qualifier &= ~qual_mask
                            elif value == 1:
                                dev.qualifier |= qual_mask
                        if value != 2:
                            do_send = True
                    else:
                        do_send = True

                    if iecode in NUMPAD_KEYS:
                        extra_qualifier |= IEQUALIFIER_NUMERICPAD

                    if value == 0:
                        iecode |= IECODE_UP_PREFIX
                    elif value == 2:
                        extra_qualifier |= IEQUALIFIER_REPEAT
        elif typ == EV_REL:
            if code == REL_WHEEL_HI_RES:
                # Bewarned: The IEQUALIFIER_INTERRUPT qualifier bit
                # is repurposed to signal a scroll wheel event.
                # The Amiga side will fix up the qualifier bits.
                iecode = NM_WHEEL_UP if value > 0 else NM_WHEEL_DOWN
                self.a314d.send_data(self.current_stream_id, struct.pack('>hhHB', 0, 0, dev.qualifier | IEQUALIFIER_INTERRUPT, iecode))
                return

            dx: int = 0
            dy: int = 0
            if code == REL_X:
                dx = value
            elif code == REL_Y:
                dy = value

            if dev.buffered_movement is None:
                dev.buffered_movement = (dx, dy)
            else:
                bdx, bdy = dev.buffered_movement
                dev.buffered_movement = (bdx + dx, bdy + dy)

        now = time.time()

        if do_send or dev.buffered_movement and self.next_send <= now:
            if dev.buffered_movement:
                dx, dy = dev.buffered_movement
            else:
                dx, dy = 0, 0
            self.a314d.send_data(self.current_stream_id, struct.pack('>hhHB', dx, dy, dev.qualifier | extra_qualifier, iecode))
            dev.buffered_movement = None
            self.next_send = now + MIN_SEND_INTERVAL

    def run(self):
        logger.info('HID service is running')

        while True:
            sel_fds = [self.a314d, self.monitor] + list(self.devices.values())

            timeout = 5.0

            any_buffered_movement = any(dev.buffered_movement for dev in self.devices.values())
            if any_buffered_movement and self.current_stream_id:
                now = time.time()
                if now < self.next_send:
                    timeout = self.next_send - now
                else:
                    for dev in self.devices.values():
                        if dev.buffered_movement:
                            self.a314d.send_data(self.current_stream_id, struct.pack('>hhHB', *dev.buffered_movement, dev.qualifier, IECODE_NOBUTTON))
                            dev.buffered_movement = None
                    self.next_send = now + MIN_SEND_INTERVAL

            rfd, _, _ = select.select(sel_fds, [], [], timeout)

            for fd in rfd:
                if fd == self.a314d:
                    self.handle_a314d_readable()
                elif fd == self.monitor:
                    self.handle_udev_event()
                else:
                    self.handle_dev_readable(fd)

if __name__ == '__main__':
    service = HidService()
    service.run()
