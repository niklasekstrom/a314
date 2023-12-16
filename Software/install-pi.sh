#!/usr/bin/env bash

if [ `id -u` != 0 ] ; then
	echo "Please run install-pi.sh using sudo (sudo ./install-pi.sh <model>)"
	exit 1
fi

BIN=bin_pi

A314_USER=$SUDO_USER
A314_GROUP=`sudo -u $A314_USER id -gn`
A314_HOME=`sudo -u $A314_USER printenv HOME`

modinstall() {
	sed -e s%##USER##%${A314_USER}%g -e s%##GROUP##%${A314_GROUP}%g -e s%##HOME##%${A314_HOME}%g $1 > $2/`basename $1`
}

install_common() {
	install a314d/a314d.py /opt/a314
	install picmd/picmd.py /opt/a314
	install a314fs/a314fs.py /opt/a314
	install piaudio/piaudio.py /opt/a314
	install remotewb/remotewb.py /opt/a314
	install disk/disk.py /opt/a314
	install ethernet/ethernet.py /opt/a314
	install hid/hid.py /opt/a314
	install remote-mouse/remote-mouse.py /opt/a314
	install videoplayer/videoplayer.py /opt/a314

	# Write configuration files, but don't overwrite
	[ -f /etc/opt/a314/a314d.conf ] || modinstall a314d/a314d.conf /etc/opt/a314
	[ -f /etc/opt/a314/picmd.conf ] || modinstall picmd/picmd.conf /etc/opt/a314
	[ -f /etc/opt/a314/a314fs.conf ] || modinstall a314fs/a314fs.conf /etc/opt/a314
	[ -f /etc/opt/a314/disk.conf ] || modinstall disk/disk.conf /etc/opt/a314

	# Add shared directory for a314fs
	sudo -u $A314_USER mkdir -p ${A314_HOME}/a314shared

	cd bpls2gif ; python3 setup.py install ; cd ..

	PIP_BREAK_SYSTEM_PACKAGES=1 pip3 install python-pytun

	# Add tap0 interface
	modinstall ethernet/pi-config/tap0 /etc/network/interfaces.d

	# Enable IP forwarding
	echo net.ipv4.ip_forward=1 > /etc/sysctl.d/a314eth.conf

	# Add service that sets iptable rules
	[ -f /lib/systemd/system/a314net.service ] || install -m644 ethernet/pi-config/a314net.service /lib/systemd/system

	systemctl daemon-reload
	systemctl enable a314d
	systemctl enable a314net

	echo
	echo "Installation complete"
	echo "Restart the Raspberry Pi (sudo reboot now) to automatically start a314 software"
}

install_trapdoor() {
	sudo -u $A314_USER mkdir -p ${BIN}
	sudo -u $A314_USER make ${BIN}/a314d-td
	sudo -u $A314_USER make ${BIN}/spi-a314.dtbo

	install -d /opt/a314
	install -d /etc/opt/a314
	install ${BIN}/a314d-td /opt/a314/a314d
	install ${BIN}/spi-a314.dtbo /boot/overlays

	modinstall a314d/a314d-td.service /lib/systemd/system
	mv /lib/systemd/system/a314d-td.service /lib/systemd/system/a314d.service

	# Set dtparam=spi=on
	CONFIG_FILE=/boot/config.txt

	PREV_DTPARAM_SPI=`grep ^dtparam=spi= $CONFIG_FILE`

	case "$PREV_DTPARAM_SPI" in
		"dtparam=spi=on"*)
			;;
		"dtparam=spi="*)
			sed -i "s%${PREV_DTPARAM_SPI}%#${PREV_DTPARAM_SPI}%g" $CONFIG_FILE
			printf "\ndtparam=spi=on\n" >> $CONFIG_FILE
			;;
		*)
			printf "\ndtparam=spi=on\n" >> $CONFIG_FILE
			;;
	esac

	# Set dtoverlay=spi-a314
	if ! grep -q ^dtoverlay=spi-a314 $CONFIG_FILE
	then
		printf "\ndtoverlay=spi-a314\n" >> $CONFIG_FILE
	fi

	# Set force_turbo=1
	PREV_FORCE_TURBO=`grep ^force_turbo= $CONFIG_FILE`

	case "$PREV_FORCE_TURBO" in
		"force_turbo=1"*)
			;;
		"force_turbo="*)
			sed -i "s%${PREV_FORCE_TURBO}%#${PREV_FORCE_TURBO}%g" $CONFIG_FILE
			printf "\nforce_turbo=1\n" >> $CONFIG_FILE
			;;
		*)
			printf "\nforce_turbo=1\n" >> $CONFIG_FILE
			;;
	esac

	# Add spidev.bufsiz=65536
	CMDLINE_FILE=/boot/cmdline.txt

	if ! grep -q "spidev.bufsiz=65536" $CMDLINE_FILE
	then
		echo `cat $CMDLINE_FILE` spidev.bufsiz=65536 > $CMDLINE_FILE
	fi

	install_common
}

install_clockport() {
	sudo -u $A314_USER mkdir -p ${BIN}
	sudo -u $A314_USER make ${BIN}/a314d-cp
	sudo -u $A314_USER make ${BIN}/start_gpclk

	install -d /opt/a314
	install -d /etc/opt/a314
	install ${BIN}/a314d-cp /opt/a314/a314d
	install ${BIN}/start_gpclk /opt/a314

	modinstall a314d/a314d-cp.service /lib/systemd/system
	mv /lib/systemd/system/a314d-cp.service /lib/systemd/system/a314d.service

	install_common
}

install_frontexpansion() {
	sudo -u $A314_USER mkdir -p ${BIN}
	sudo -u $A314_USER make ${BIN}/a314d-fe

	install -d /opt/a314
	install -d /etc/opt/a314
	install ${BIN}/a314d-fe /opt/a314/a314d

	modinstall a314d/a314d-td.service /lib/systemd/system
	mv /lib/systemd/system/a314d-td.service /lib/systemd/system/a314d.service

	install_common
}

case "$1" in
	td | TD) install_trapdoor
		;;
	cp | CP) install_clockport
		;;
	fe | FE) install_frontexpansion
		;;
	*)	echo "Usage: sudo ./install-pi.sh <model>"
		echo "       <model> is one of:"
		echo "         td   (trapdoor)          A314-500, A314-600"
		echo "         cp   (clockport)         A314-cp"
		echo "         fe   (front expansion)   A314-1000"
		;;
esac
