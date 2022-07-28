#!/bin/sh
docker run --rm --volume "$PWD":/mnt --workdir /mnt -it niekstrom/a314-rpi-build:v2 make
sudo chown -R pi:pi bin
