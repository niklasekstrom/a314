.PHONY: bin_dir all

CPP=g++
CC=gcc
VC=vc

all: bin_dir bin/a314d bin/start_gpclk bin/a314.device bin/a314fs bin/pi bin/remote-mouse bin/a314eth.device bin/a314disk.device

bin_dir:
	mkdir -p bin

bin/a314d: a314d/a314d.cc
	${CPP} a314d/a314d.cc -O3 -o bin/a314d

bin/start_gpclk: a314d/start_gpclk.c
	${CC} a314d/start_gpclk.c -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o bin/start_gpclk

bin/a314.device: a314device/a314.h a314device/cp_pi_if.c a314device/cp_pi_if.h a314device/debug.c a314device/debug.h a314device/device.c a314device/device.h a314device/driver_task.c a314device/int_server.asm a314device/memory_allocator.c a314device/memory_allocator.h a314device/proto_a314.h a314device/protocol.h a314device/romtag.asm a314device/sockets.c a314device/sockets.h a314device/startup.c a314device/startup.h
	${VC} a314device/romtag.asm a314device/driver_task.c a314device/debug.c a314device/device.c a314device/startup.c a314device/cp_pi_if.c a314device/memory_allocator.c a314device/sockets.c a314device/int_server.asm -O3 -nostdlib -o bin/a314.device

bin/a314fs: a314device/a314.h a314device/proto_a314.h a314fs/messages.h a314fs/a314fs.c a314fs/bcpl_start.asm a314fs/bcpl_end.asm
	${VC} a314fs/a314fs.c -S -o bin/a314fs.asm
	sed -i 's/section/;section/g' bin/a314fs.asm
	${VC} a314fs/bcpl_start.asm bin/a314fs.asm a314fs/bcpl_end.asm -nostdlib -o bin/a314fs
	dd if=bin/a314fs bs=1 skip=28 count=4 of=bin/a314fs seek=32 conv=notrunc
	rm bin/a314fs.asm

bin/pi: a314device/a314.h a314device/proto_a314.h picmd/pi.c
	${VC} picmd/pi.c -lamiga -o bin/pi

bin/remote-mouse: a314device/a314.h remote-mouse/remote-mouse.c
	${VC} remote-mouse/remote-mouse.c -lamiga -o bin/remote-mouse

bin/a314eth.device: a314device/a314.h a314device/proto_a314.h ethernet/romtag.asm ethernet/device.c ethernet/sana2.h ethernet/tagitem.h
	${VC} ethernet/romtag.asm ethernet/device.c -O3 -nostdlib -o bin/a314eth.device -lamiga

bin/a314disk.device: a314device/a314.h a314device/proto_a314.h disk/romtag.asm disk/device.c disk/debug.h disk/debug.c
	${VC} disk/romtag.asm disk/device.c disk/debug.c -O3 -nostdlib -o bin/a314disk.device -lamiga

install: all
	mkdir -p /opt/a314
	cp bin/a314d /opt/a314
	cp bin/start_gpclk /opt/a314
	cp a314d/a314d.py /opt/a314
	cp a314fs/a314fs.py /opt/a314
	cp picmd/picmd.py /opt/a314
	cp remote-mouse/remote-mouse.py /opt/a314
	cp ethernet/ethernet.py /opt/a314
	cp disk/disk.py /opt/a314
	mkdir -p /etc/opt/a314
	cp a314d/a314d.conf /etc/opt/a314
	cp a314fs/a314fs.conf /etc/opt/a314
	cp picmd/picmd.conf /etc/opt/a314
	cp disk/disk.conf /etc/opt/a314
	pip3 install python-pytun
	cp ethernet/pi-config/tap0 /etc/network/interfaces.d
	cp a314d/a314d.service /lib/systemd/system
