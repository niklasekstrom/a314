# A314 Software

## How to install

Software needs to be built and installed for both the Raspberry Pi side and the Amiga side.

### Raspberry Pi

To build and install on the Pi side, do the following:

- Install Raspberry Pi OS (I tend to use Raspberry Pi OS Lite 64-bit): https://www.raspberrypi.com/software/operating-systems/
- Install Dependencies: `sudo apt install python3-dev python3-distutils python3-pip build-essential git`
- Clone the a314 repo: `git clone -b clockport_if https://github.com/niklasekstrom/a314.git`
- `cd a314/Software`
- Build binaries: `make`
- Install software: `sudo make install`
- Enable and start a314d:
  - `sudo systemctl daemon-reload`
  - `sudo systemctl enable a314d`
  - `sudo systemctl start a314d`

### Amiga

The binaries for the Amiga side are already built and are available as part of a release on the GitHub page: https://github.com/niklasekstrom/clockport_pi_interface/releases

There are multiple sub directories in the release archive, and the files in those sub directories should be copied to the corresponding sub directories in the AmigaOS system volume.

It is also possible to build the Amiga binaries from source on the Raspberry Pi. To do so, follow these steps:

- Install Docker: `curl -sSL https://get.docker.com | sh`
- Allow user pi to run Docker: `sudo usermod -aG docker pi`
- Log out and then log back in again to make the previous command take effect
- Clone the a314 repo: `git clone -b clockport_if https://github.com/niklasekstrom/a314.git`
- `cd a314/Software`
- Build binaries by running: `./rpi_docker_build.sh`

The binaries are now available in the sub directories of the `bin_amiga` directory.

After these files are copied to the Amiga it should be possible to mount the PiDisk: by `Mount PI0:`.
The pi command can be invoked by `pi` without any arguments to run bash, or with arguments to run a particular Linux command.
