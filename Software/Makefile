.PHONY: bin_dir all

CPP=g++
CC=gcc
BIN=bin_pi

all: bin_dir ${BIN}/a314d ${BIN}/start_gpclk

bin_dir:
	mkdir -p ${BIN}

${BIN}/a314d: a314d/a314d.cc
	${CPP} a314d/a314d.cc -O3 -o ${BIN}/a314d

${BIN}/start_gpclk: a314d/start_gpclk.c
	${CC} a314d/start_gpclk.c -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o ${BIN}/start_gpclk

install:
	su -c "mkdir -p /home/pi/a314shared" pi
	mkdir -p /opt/a314
	cp ${BIN}/a314d /opt/a314
	cp ${BIN}/start_gpclk /opt/a314
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
