#!/usr/bin/python3
# -*- coding: utf-8 -*-

# Copyright (c) 2026 Jeremy Bauer

import socket
import struct
import sys

CONTROL_HOST = 'localhost'
CONTROL_PORT = 23891      # Port must match a314shd.py.

RESPONSE_HEADER_SIZE = 8  # Response header: rc (4 bytes) + output length (4 bytes)
FAILURE_RC = 255          # Return code used when the command could not be run


def recv_exact(s, n):
    data = b''
    while len(data) < n:
        chunk = s.recv(n - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def run_command(command):
    s = socket.socket()
    try:
        s.connect((CONTROL_HOST, CONTROL_PORT))
    except OSError:
        sys.stderr.write('a314sh: unable to reach the a314sh service\n'
                         '(is the a314shd daemon running on the Amiga?)\n')
        return FAILURE_RC

    data = command.encode('utf-8', 'replace')
    s.sendall(struct.pack('>I', len(data)) + data)

    hdr = recv_exact(s, RESPONSE_HEADER_SIZE)
    if hdr is None:
        s.close()
        sys.stderr.write('a314sh: connection to the service was lost\n')
        return FAILURE_RC

    rc, out_len = struct.unpack('>iI', hdr)

    out = recv_exact(s, out_len)
    s.close()

    if out is None:
        sys.stderr.write('a314sh: connection to the service was lost\n')
        return FAILURE_RC

    if out:
        sys.stdout.buffer.write(out)
        sys.stdout.buffer.flush()

    return rc


def main():
    if len(sys.argv) > 1:
        command = ' '.join(sys.argv[1:])
        sys.exit(run_command(command) & 0xff)

    interactive = sys.stdin.isatty()
    last_rc = 0

    while True:
        try:
            if interactive:
                command = input('a314sh> ')
            else:
                command = sys.stdin.readline()
                if command == '':
                    break
        except (EOFError, KeyboardInterrupt):
            if interactive:
                print()
            break

        command = command.strip()
        if not command:
            continue
        if command in ('exit', 'quit'):
            break

        last_rc = run_command(command)

    sys.exit(last_rc & 0xff)


if __name__ == '__main__':
    main()
