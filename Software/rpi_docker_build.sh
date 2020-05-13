#!/bin/sh
docker run --rm --volume "$PWD":/mnt --workdir /mnt -it niekstrom/a314-rpi-build make
