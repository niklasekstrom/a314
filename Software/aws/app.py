#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2021 Niklas Ekström

import awslib
import random
import select
import socket
import sys

window_sizes = {}

class ConnectionCallbacks(object):
    def open_window_fail(self, conn, wid):
        print('Window {} failed to open'.format(wid))

    def open_window_success(self, conn, wid, width, height, depth):
        print('Window {} opened successfully with size {}x{}x{}'.format(wid, width, height, depth))
        window_sizes[wid] = (width, height, depth)

    def event_close_window(self, conn, wid):
        print('Close button clicked for window {}'.format(wid))

conn = awslib.connect(ConnectionCallbacks())
if conn is None:
    print("Failed to connect to AWS")
    exit(-1)

def print_help():
    print("Possible commands:")
    print("  open <x> <y> <w> <h> <title>")
    print("  close <wid>")
    print("  flip <wid>")
    print("  help")
    print("  quit")

print("Connected to AWS")
print("Enter command (open/close/flip/help/quit):")

while True:
    rl = [sys.stdin, conn]
    try:
        rl, _, _ = select.select(rl, [], [])
    except KeyboardInterrupt:
        conn.close()
        break

    if conn in rl:
        conn.handle_readable()

    if sys.stdin in rl:
        arr = sys.stdin.readline().strip().split()
        try:
            if arr[0] == 'quit':
                conn.close()
                break
            elif arr[0] == 'help':
                print_help()
            elif arr[0] == 'open':
                x, y, w, h = int(arr[1]), int(arr[2]), int(arr[3]), int(arr[4])
                title = ' '.join(arr[5:])
                wid = conn.open_window(x, y, w, h, title)
                print('open_window returned wid {}'.format(wid))
            elif arr[0] == 'close':
                wid = int(arr[1])
                if wid not in window_sizes:
                    print('Unknown wid {}'.format(wid))
                else:
                    conn.close_window(wid)
                    del window_sizes[wid]
            elif arr[0] == 'flip':
                wid = int(arr[1])
                if wid not in window_sizes:
                    print('Unknown wid {}'.format(wid))
                else:
                    w, h, d = window_sizes[wid]
                    w = (w + 15) & ~15
                    pixels = w * h * d
                    buffer = bytes([random.randint(0, 255) for _ in range(pixels // 8)])
                    conn.copy_flip_window(wid, buffer)
            else:
                print('Unknown command: {}'.format(arr[0]))
        except Exception as e:
            print('Caught exception:', e)
