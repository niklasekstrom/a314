.PHONY: bin_dir all clean

ifndef VERBOSE
.SILENT:
endif

CPP=g++
CC=gcc
BIN=bin_pi

# Check that MODEL is set, unless target is help, clean or install_depends
ifeq (, $(filter $(MAKECMDGOALS), help clean install_depends))
    ifneq (, $(filter $(MODEL), TD CP))
        A314_MODEL=MODEL_${MODEL}
    else
        $(error Requires MODEL=TD or MODEL=CP, see "make help")
    endif
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

A314_PI_BINARIES     = ${BIN}/a314d

# Model specific additional files
ifeq ($(A314_MODEL), MODEL_CP)
A314D_SERVICE        = a314d/a314d-cp.service
A314_PI_BINARIES    += ${BIN}/start_gpclk
endif

ifeq ($(A314_MODEL), MODEL_TD)
A314_SERVICES       += remotewb/remotewb.py \
                       videoplayer/videoplayer.py
A314D_SERVICE        = a314d/a314d-td.service
A314_PI_OVERLAYS     = ${BIN}/spi-a314.dtbo
endif

# Generate a name for the installer
GIT_TAG=${shell git describe --tags}
SYS_BITS=${shell getconf LONG_BIT}
INSTALLER_NAME=a314install_${MODEL}_${SYS_BITS}_${GIT_TAG}.run

# Figure out the primary group and home directory of the user who used sudo
SUDO_GROUP=$(shell getent group ${SUDO_GID} | cut -d: -f1)
SUDO_HOME=$(shell awk -F':' -v U=${SUDO_USER} '$$1==U {print $$6}' /etc/passwd)

# Replace some magic strings in files while installing to work with any username
define modinstall
	sed -e s%##USER##%${SUDO_USER}%g -e s%##GROUP##%${SUDO_GROUP}%g -e s%##HOME##%${SUDO_HOME}%g $(1) >$(2)/$(shell basename $(1))
endef


default: all


help:
	echo " Building and installation of the A314 Pi co-processor project"
	echo " Use MODEL=TD for A500 and A600 trapdoor models, or MODEL=CP for clockport model"
	echo " Example: > make all MODEL=CP"
	echo
	echo " Usage: make [target] MODEL=<TD|CP>"
	echo
	echo " Targets:"
	echo " --------"
	echo " all (default)      - Build Pi executables"
	echo " install            - Install A314"
	echo " clean              - Remove temporary and output files"
	echo " help               - This text"
	echo
	echo " Extra targets:"
	echo " --------------"
	echo " install_depends    - Install build dependencies (this will take some time and should only be needed once)"
	echo " dist               - Build a self-extracting installer for distribution"
	echo


all: bin_dir ${A314_PI_BINARIES} ${A314_PI_OVERLAYS}


bin_dir:
	mkdir -p ${BIN}


${BIN}/a314d: a314d/a314d.cc
	echo "Building $@"
	${CPP} -D${A314_MODEL}=1 a314d/a314d.cc -O3 -o ${BIN}/a314d


${BIN}/start_gpclk: a314d/start_gpclk.c
	echo "Building $@"
	${CC} a314d/start_gpclk.c -I/opt/vc/include -L/opt/vc/lib -lbcm_host -o ${BIN}/start_gpclk


${BIN}/spi-a314.dtbo: a314d/spi-a314-overlay.dts
	dtc -I dts -O dtb -o ${BIN}/spi-a314.dtbo a314d/spi-a314-overlay.dts


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
	$(call modinstall, ${A314D_SERVICE}, /lib/systemd/system)
	mv /lib/systemd/system/`basename ${A314D_SERVICE}` /lib/systemd/system/a314d.service
	install -m644 ethernet/pi-config/a314net.service /lib/systemd/system

ifeq ($(A314_MODEL), MODEL_TD)
	install -d /boot/overlays
	install -m644 ${BIN}/spi-a314.dtbo /boot/overlays
	cd bpls2gif ; python3 setup.py install
endif
	echo "Installation completed!"


install_depends:
ifneq ($(shell id -u), 0)
	echo "Please run $@ using sudo"
	exit 1
endif
	echo "Installing dependencies"
	apt -y install python3-dev python3-distutils python3-pip build-essential git makeself
	su -l ${SUDO_USER} -c "pip3 install python-pytun"

	echo "Installing docker for building Amiga binaries"
	curl -sSL https://get.docker.com | sh
	usermod -aG docker ${SUDO_USER}
	echo "Done!"


clean:
	rm -rf installer ${BIN}


# Below targets are only for those wanting to make a self-extracting installer for A314
define INSTALL_SCRIPT
#!/bin/bash

SUDO_GROUP=$$(getent group $$SUDO_GID | cut -d: -f1)
SUDO_HOME=$$(awk -F':' -v U=$$SUDO_USER '$$1==U {print $$6}' /etc/passwd)

MSG="This will install ${SYS_BITS} bit versions of all the services and configuration files for A314-${MODEL} for user \"$$SUDO_USER\".
Any custom changes to these files will be overwritten!

Continue?"
if ! whiptail --yesno "$$MSG" 20 50 ;then
	exit 1
fi

echo "Installing A314..."
find root -type f | xargs -n1 sed -i -e s%##USER##%$$SUDO_USER%g -e s%##GROUP##%$$SUDO_GROUP%g -e s%##HOME##%$$SUDO_HOME%g
rsync -aK root/ /

install -d $$SUDO_HOME/a314shared -o $$SUDO_USER -g $$SUDO_GROUP

MSG="Do you want to install the following dependencies and enable IP forwarding? These are needed for Amiga networking.
(If this is the first time you install A314 you probably want to answer Yes)

* python3-pip
* websockets
* pyudev
* python-pytun"

if whiptail --yesno "$$MSG" 20 50 ;then
	echo "Installing dependencies..."
	apt -y install python3-pip python3-websockets python3-pyudev
	su -l $$SUDO_USER -c "pip3 install python-pytun"
	echo net.ipv4.ip_forward=1 > /etc/sysctl.d/a314eth.conf
fi

MSG="Do you want to update files in /boot?
This should only be needed once and this script will attempt to detect if expected changes have already been made."

if [ ${MODEL} == TD ]; then
	if whiptail --yesno "$$MSG" 20 50 ;then
		if ! grep -q ^dtparam=spi=on /boot/config.txt; then
			sed -i -e s/^#dtparam=spi=on$$/dtparam=spi=on/ /boot/config.txt
		fi
		if ! grep -q ^force_turbo=1$$ /boot/config.txt; then
			echo force_turbo=1 >> /boot/config.txt
		fi
		if ! grep -q ^dtoverlay=spi-a314$$ /boot/config.txt; then
			echo dtoverlay=spi-a314 >> /boot/config.txt
		fi
		if ! grep -q spidev.bufsiz=65536 /boot/cmdline.txt; then
			echo -n " spidev.bufsiz=65536" >> /boot/cmdline.txt
		fi
	fi
fi

echo "Starting A314 daemons..."
systemctl daemon-reload
systemctl enable a314net a314d
systemctl restart a314d
systemctl restart a314net
systemctl restart networking systemd-sysctl

echo "Installation completed!"

if [ ${MODEL} == TD ]; then
	echo
	echo "Note: If this is the initial A314 installation you will need to reboot your Pi"
fi
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
	install -m644 $(A314_SERVICE_CONFIGS) installer/root/etc/opt/a314
	install -d installer/root/etc/network/interfaces.d
	install -m644 ethernet/pi-config/tap0 installer/root/etc/network/interfaces.d
	install -d installer/root/lib/systemd/system
	install -m644 ${A314D_SERVICE} installer/root/lib/systemd/system/a314d.service
	install -m644 ethernet/pi-config/a314net.service installer/root/lib/systemd/system

ifeq ($(A314_MODEL), MODEL_TD)
	install -d installer/root/boot/overlays
	install -m644 ${BIN}/spi-a314.dtbo installer/root/boot/overlays
	cd bpls2gif ; python3 setup.py install --prefix=../installer/root/
endif

	makeself --needroot installer ${INSTALLER_NAME} "A314 Software Installer" "./install.sh"
	rm -rf installer

dist: clean all _install_script
	fakeroot -- ${MAKE} _fakeroot_install
