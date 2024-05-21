# A314 Software

The A314 software supports multiple variants of A314 hardware.
The variants of the software use the following acronyms:

- a314-td (trapdoor), works with A314-500 and A314-600
- a314-cp (clockport), works with A314-cp
- a314-fe (front expansion), works with A314-1000

## How to install

Software needs to be installed on both the Raspberry Pi side and on the Amiga side.

### Raspberry Pi

To build and install the software on the Raspberry Pi side, do the following:

- Install Raspberry Pi OS (I tend to use Raspberry Pi OS Lite 64-bit,
  but any recent version should work):
  <https://www.raspberrypi.com/software/operating-systems/>
- Update apt packages: `sudo apt update`, `sudo apt upgrade`
- Install Dependencies: `sudo apt install python3-dev python3-distutils python3-pip python3-virtualenv build-essential git iptables libraspberrypi-dev`
- Obtain a copy of the repository, either:
  - Clone the a314 repository: `git clone https://github.com/niklasekstrom/a314.git`, or
  - Download the sources from a release. Pick a release from
    <https://github.com/niklasekstrom/a314/releases>, download the file linked to as
    Source code (tar.gz), and extract with `tar xvf <filename>.tar.gz`.
- Change into the Software directory: `cd a314/Software`
- Run the installer script: `sudo ./install-pi.sh <model>`, where `<model>` is
  `td`, `cp` or `fe` depending on which variant of A314 is used.
- Reboot the Raspberry Pi: `sudo reboot now`

### Amiga

The binaries for the Amiga side are already built and are available as part of a release on the GitHub page: <https://github.com/niklasekstrom/a314/releases>

There are multiple sub directories in the release archive, and the files in those sub directories should be copied to the corresponding sub directories in the AmigaOS system volume.

Note that in the Devs directory there are multiple different versions of a314.device:
`a314-td.device`, `a314-cp.device`, `a314-fe.device`.
Copy only the version that matches your A314 hardware and rename it to `a314.device`.

After these files are copied to the Amiga it should be possible to run the pi command.
The pi command can be invoked by `pi` without any arguments to get an interactive bash terminal,
or with arguments to run a particular Linux command.

#### Configuration file

For the a314-cp driver it is possible to write a configuration file to `DEVS:a314.config`.
The configuration file is a text file, where each line has the format `<key> = <value>`.
The keys are:

- `ClockportAddress`: the address (in hexadecimal) of the clock port.
- `Interrupt`: the interrupt number used, either: 2 (INT2), 3 (Vertical blank), or 6 (INT6).

If the configuration file is not present then the default values are used, which for
a314-cp are:

```text
ClockportAddress = D80001
Interrupt = 6
```

#### Building

It is also possible to build the Amiga binaries from source, either on the Raspberry Pi or on a Linux PC.
To do so, follow these steps:

- Download and install VBCC:
  - On the bottom of this page, <http://www.compilers.de/vbcc.html>, there are archives with VBCC
    pre-compiled for different plattforms such as Linux (x64) and Raspberry Pi
  - Download the appropriate archive, e.g.: `wget http://www.ibaug.de/vbcc/vbcc_linux_x64.tar.gz`
  - Unpack to some suitable directory: `tar xvf vbcc_linux_x64.tar.gz -C ~/amiga/`
  - Set the VBCC environment variable: `export VBCC=~/amiga/vbcc`
  - Add VBCC to PATH: `export PATH=$VBCC/bin:$PATH`
- Download and add NDK3.2:
  - Download archive from Aminet: `wget http://aminet.net/dev/misc/NDK3.2.lha`
  - Install lha extraction software: `sudo apt install lhasa`
  - Extract archive: `lha xw=~/amiga/NDK3.2 NDK3.2.lha`
  - Add environment variable: `export NDK32=~/amiga/NDK3.2`
- Clone the a314 repo: `git clone https://github.com/niklasekstrom/a314.git ~/amiga/a314`
- Change into the Software directory: `cd ~/amiga/a314/Software`
- Build binaries by running: `make -f Makefile-amiga`

The binaries are now available in the sub directories of the `bin_amiga` directory.
