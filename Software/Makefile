.PHONY: bin_dir all installer clean

ifndef VERBOSE
.SILENT:
endif

A314_SERVICES        = a314d/a314d.py \
                       a314fs/a314fs.py \
                       picmd/picmd.py \
                       piaudio/piaudio.py \
                       remote-mouse/remote-mouse.py \
                       hid/hid.py \
                       ethernet/ethernet.py \
                       disk/disk.py

A314_SERVICE_CONFIGS = a314d/a314d.conf \
                       a314fs/a314fs.conf \
                       picmd/picmd.conf \
                       disk/disk.conf

A314_PI_BINARIES     = ${BIN}/a314d \
                       ${BIN}/start_gpclk


CPP=g++
CC=gcc
BIN=bin_pi

# Figure out the primary group and home directory of the user who used sudo
SUDO_GROUP=$(shell getent group ${SUDO_GID} | cut -d: -f1)
SUDO_HOME=$(shell awk -F':' -v U=${SUDO_USER} '$$1==U {print $$6}' /etc/passwd)

# Replace some magic strings in files while installing to work with any username
define modinstall
	sed -e s%##USER##%${SUDO_USER}%g -e s%##GROUP##%${SUDO_GROUP}%g -e s%##HOME##%${SUDO_HOME}%g $(1) >$(2)/$(shell basename $(1))
endef

help:
	echo "Building and installation of the A314 Pi co-processor project"
	echo
	echo "Targets:"
	echo "--------"
	echo "all                - Build Pi executables"
	echo "install            - Install A314"
	echo "clean              - Remove temporary and output files"
	echo
	echo "Extra targets:"
	echo "--------------"
	echo "install_depend     - Install dependencies (this will take some time and should only be needed once)"
	echo "installer          - Build a self-extracting installer for distribution"
	echo

all: bin_dir ${BIN}/a314d ${BIN}/start_gpclk

bin_dir:
	mkdir -p ${BIN}

${BIN}/a314d: a314d/a314d.cc
	echo "Building $@"
	${CPP} a314d/a314d.cc -O3 -o ${BIN}/a314d

${BIN}/start_gpclk: a314d/start_gpclk.c
	echo "Building $@"
	${CC} a314d/start_gpclk.c -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o ${BIN}/start_gpclk

install:
ifneq ($(shell id -u), 0)
	echo "Please run $@ using sudo"
	exit 1
endif
	echo "Installing A314 files..."
	install -d /${SUDO_HOME}/a314shared -o ${SUDO_USER} -g ${SUDO_GROUP}
	install -d /opt/a314
	install -d /etc/opt/a314
	install ${A314_SERVICES} /opt/a314
	install ${A314_PI_BINARIES} /opt/a314
	$(foreach filename, $(A314_SERVICE_CONFIGS), $(call modinstall, $(filename), /etc/opt/a314/);)
	install -d /etc/network/interfaces.d
	$(call modinstall, ethernet/pi-config/tap0, /etc/network/interfaces.d)
	install -d /lib/systemd/system
	$(call modinstall, a314d/a314d.service, /lib/systemd/system)
	install ethernet/pi-config/a314net.service /lib/systemd/system

	echo "Starting A314 daemons..."
	systemctl daemon-reload
	systemctl enable a314d a314net
	systemctl restart a314d a314net

	echo "Installation completed!"

install_depend:
ifneq ($(shell id -u), 0)
	echo "Please run $@ using sudo"
	exit 1
endif
	echo "Installing build dependencies"
	apt -y install python3-dev python3-distutils python3-pip build-essential git makeself
	echo "Installing a314-ethernet dependencies"
	pip3 install pyudev python-pytun
	echo "Installing docker for building Amiga binaries"
	curl -sSL https://get.docker.com | sh
	usermod -aG docker ${SUDO_USER}
	echo "Done!""

clean:
	rm -rf installer ${BIN} a314install.run


# Below targets are only for those wanting to make a self-extracting installer for A314
define INSTALL_SCRIPT
#!/bin/bash

SUDO_GROUP=$$(getent group $$SUDO_GID | cut -d: -f1)
SUDO_HOME=$$(awk -F':' -v U=$$SUDO_USER '$$1==U {print $$6}' /etc/passwd)

MSG="This will install all the services and configuration files for A314 for user \"$$SUDO_USER\". Any custom changes to these files will be overwritten!\n\nContinue?"
if ! whiptail --yesno "$$MSG" 15 40 ;then
	exit 1
fi

echo "Installing A314..."
find root -type f | xargs -n1 sed -i -e s%##USER##%$$SUDO_USER%g -e s%##GROUP##%$$SUDO_GROUP%g -e s%##HOME##%$$SUDO_HOME%g
rsync -aK root/ /

install -d $$SUDO_HOME/a314shared -o $$SUDO_USER -g $$SUDO_GROUP

echo "Starting A314 daemons..."
systemctl daemon-reload
systemctl enable a314d a314net
systemctl restart a314d a314net

echo "Installation completed!"
endef
export INSTALL_SCRIPT

_install_script:
	mkdir -p installer
	echo "$$INSTALL_SCRIPT" > installer/install.sh
	chmod +x installer/install.sh

_fakeroot_install:
	install -d installer/root/opt/a314
	install -d installer/root/etc/opt/a314
	install ${A314_SERVICES} installer/root/opt/a314
	install ${A314_PI_BINARIES} installer/root/opt/a314
	install $(A314_SERVICE_CONFIGS) installer/root/etc/opt/a314
	install -d installer/root/etc/network/interfaces.d
	install ethernet/pi-config/tap0 installer/root/etc/network/interfaces.d
	install -d installer/root/lib/systemd/system
	install a314d/a314d.service installer/root/lib/systemd/system
	install ethernet/pi-config/a314net.service installer/root/lib/systemd/system

	makeself --needroot installer a314install.run "A314 Software Installer" "./install.sh"

installer: all _install_script
	fakeroot -- make _fakeroot_install
