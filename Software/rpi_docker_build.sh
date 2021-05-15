#!/bin/sh
docker run --rm --volume "$PWD":/mnt --workdir /mnt -it niekstrom/a314-rpi-build:v2 make
sudo dtc -I dts -O dtb -o bin/spi-a314.dtbo spi-a314-overlay.dts
