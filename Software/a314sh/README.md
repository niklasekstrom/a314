# a314sh - Amiga Shell over A314

The a314sh command on the Pi provides the ability to execute commands on the Amiga from the
Raspberry Pi. On the Amiga, start the daemon `A314shd` (e.g. `run >NIL: A314shd`, or add it
to `S:user-startup`), and then invoke `a314sh` on the Pi:

Direct commands:
```text
a314sh info
a314sh status
a314sh break 1
```

Interactive shell:
```text
a314sh> rx MyScript.rexx
Running test script
a314sh> copy work:file.txt RAM:
a314sh> exit
```

The command runs in the daemon's shell context, so its output and return code are sent back
to the Pi. Running `a314sh` without arguments reads commands from standard input, one per line,
which also makes it usable as a simple interactive Amiga shell from the Pi.