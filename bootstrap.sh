#!/bin/bash

set -xe

cd /tmp
wget http://www.ibaug.de/vbcc/vbcc_linux_arm.tar.gz
tar xvzf vbcc_linux_arm.tar.gz

wget http://aminet.net/dev/misc/NDK3.2.lha
7z x NDK3.2.lha -ondk32
chmod -R 755 ndk32

sudo cp -r vbcc  /opt/vbcc
sudo cp -r ndk32 /opt/ndk32

if ! [ -z ${USER+x} ]; then
	sudo chown -R $USER /opt/vbcc
	sudo chown -R $USER /opt/ndk32
fi

cd ~
rm -rf /tmp/vbcc
rm -rf /tmp/ndk32
