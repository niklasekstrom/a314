.PHONY: bin_dir all

CPP=g++
VC=vc

all: bin_dir bin/a314d bin/a314.device bin/a314fs bin/pi bin/piaudio bin/remotewb bin/remote-mouse bin/videoplayer bin/diag bin/a314eth.device

bin_dir:
	mkdir -p bin

bin/a314d: a314d/a314d.cc
	${CPP} a314d/a314d.cc -O3 -o bin/a314d

bin/a314.device: a314device/a314.h a314device/cmem.h a314device/debug.h a314device/device.h a314device/fix_mem_region.h a314device/protocol.h a314device/proto_a314.h a314device/sockets.h a314device/startup.h a314device/romtag.asm a314device/a314driver.c a314device/device.c a314device/startup.c a314device/fix_mem_region.c a314device/cmem.c a314device/sockets.c a314device/int_server.asm a314device/check_a314_mapping.asm
	${VC} a314device/romtag.asm a314device/a314driver.c a314device/device.c a314device/startup.c a314device/fix_mem_region.c a314device/cmem.c a314device/sockets.c a314device/int_server.asm a314device/check_a314_mapping.asm -O3 -nostdlib -o bin/a314.device

bin/a314fs: a314device/a314.h a314device/proto_a314.h a314fs/messages.h a314fs/a314fs.c a314fs/bcpl_start.asm a314fs/bcpl_end.asm
	${VC} a314fs/a314fs.c -S -o bin/a314fs.asm
	sed -i 's/section/;section/g' bin/a314fs.asm
	${VC} a314fs/bcpl_start.asm bin/a314fs.asm a314fs/bcpl_end.asm -nostdlib -o bin/a314fs
	dd if=bin/a314fs bs=1 skip=28 count=4 of=bin/a314fs seek=32 conv=notrunc
	rm bin/a314fs.asm

bin/pi: a314device/a314.h a314device/proto_a314.h picmd/pi.c
	${VC} picmd/pi.c -lamiga -o bin/pi

bin/piaudio: a314device/a314.h a314device/proto_a314.h piaudio/piaudio.c
	${VC} piaudio/piaudio.c -lamiga -o bin/piaudio

bin/remotewb: a314device/a314.h a314device/proto_a314.h remotewb/remotewb.c remotewb/vblank_server.asm
	${VC} remotewb/remotewb.c remotewb/vblank_server.asm -lamiga -o bin/remotewb

bin/remote-mouse: a314device/a314.h remote-mouse/remote-mouse.c
	${VC} remote-mouse/remote-mouse.c -lamiga -o bin/remote-mouse

bin/videoplayer: a314device/a314.h a314device/proto_a314.h videoplayer/videoplayer.c videoplayer/vblank_server.asm
	${VC} videoplayer/videoplayer.c videoplayer/vblank_server.asm -lamiga -o bin/videoplayer

bin/diag: diag/diag.c
	${VC} diag/diag.c -o bin/diag

bin/a314eth.device: a314device/a314.h a314device/proto_a314.h ethernet/romtag.asm ethernet/device.c ethernet/sana2.h ethernet/tagitem.h
	${VC} ethernet/romtag.asm ethernet/device.c -O3 -nostdlib -o bin/a314eth.device -lamiga

install: all
	mkdir -p /opt/a314
	cp bin/a314d /opt/a314
	cp a314fs/a314fs.py /opt/a314
	cp picmd/picmd.py /opt/a314
	cp piaudio/piaudio.py /opt/a314
	cp remotewb/remotewb.py /opt/a314
	cp remote-mouse/remote-mouse.py /opt/a314
	cp videoplayer/videoplayer.py /opt/a314
	cp ethernet/ethernet.py /opt/a314
	mkdir -p /etc/opt/a314
	cp a314d/a314d.conf /etc/opt/a314
	cp a314fs/a314fs.conf /etc/opt/a314
	cp picmd/picmd.conf /etc/opt/a314
	cd bpls2gif ; python3 setup.py install
	pip3 install websockets
	pip3 install python-pytun
	cp ethernet/pi-config/tap0 /etc/network/interfaces.d
	cp a314d/a314d.service /lib/systemd/system
