# A314 Software

## How to install

**Note** that there are additional instructions in the [wiki](https://github.com/niklasekstrom/a314/wiki/Installation-instructions) that you should also follow. These instructions are for building and installing the software in this repo, but there are additional steps that should be taken.

On the Raspberry Pi do the following:

- Install Raspbian: https://www.raspberrypi.org/downloads/raspbian/
- Enable SPI using sudo `raspi-config > Interfacing Options > SPI > Enable`.
- In the file `/boot/config.txt` you should add a line that says `force_turbo=1`. The reason you want this is because the active SPI frequency is a fraction of the core frequency, which is 400 MHz when running at full speed, but that frequency is reduced to 250 MHz when the CPUs are not very busy. The Raspberry does SPI transfers using DMA, so the CPU is typically idle during a SPI transfer, and unfortunately that reduces the core frequency and therefore the SPI frequency, which we don't want. Therefore we force the core frequency to 400 MHz using that option.
- In the file `/boot/cmdline.txt` you should add `spidev.bufsiz=65536` The options in the file form are a single line of text. Just add it on the end of the line. The `spidev.bufsize` is the maximum size of a single SPI transfer. The a314d daemon will use multiple SPI transfers to transfer more data than 64 kB.
- Manually add the line `dtoverlay=spi-a314` to the end of `/boot/config.txt`, and reboot
- To prepare to build the software you have to install Docker: `sudo apt install docker.io`
- Allow user pi to run Docker: `sudo usermod -aG docker pi`
- Install Dependencies: `sudo apt install python3-dev python3-distutils python3-pip build-essential git`
- Clone the a314 repo: `git clone https://github.com/niklasekstrom/a314.git`
- `cd a314/Software`
- Build software for both the Amiga and Pi: `./rpi_docker_build.sh`
- Install software for Pi: `sudo make install`

* Enable and start a314d:
  - `sudo systemctl daemon-reload`
  - `sudo systemctl enable a314d`
  - `sudo systemctl start a314d`
  - `sudo systemctl status a314` to check daemon is running and alive :-)

In the a314/Software/bin directory there are a number of binaries that should be copied to the Amiga boot disk:

- Create a boot disk with your typical files (for the first try a simple workbench floppy-disk should be enough).
  - `a314/Software/bin/a314.device` to DEVS:
  - `a314/Software/bin/a314fs` to L:
  - `a314/Software/a314fs/a314fs-mountlist` should be appended to the `DEVS:Mountlist` file. Note that if you clone the Git repo on Windows then Git may change the line endings of that file to CRLF instead of just LF (depending on your Git settings), and the Amiga mount command cannot handle that, so you may have to open the file in some editors that allows you to change the line endings back to just LF.
  - `a314/Software/bin/pi` to C:
  - `a314/Software/bin/piaudio` to C:
  - `a314/Software/bin/remotewb` to C:
  - `a314/Software/bin/videoplayer` to C:
- At this point you should be able to boot from the disk and in a CLI run `pi uname -a` to execute the uname command on the Raspberry and print out that information in the Amiga CLI. Running `pi` without any arguments drops you into an interactive bash shell on the Raspberry. Note that the bash terminal runs in ANSI mode and assumes the terminal has 8 colors. By default Amiga only uses 4 colors, so some colors will not be seen. There's a small program, `AddWBplane`, that is available on Fish disk 0543 that adds a bitplane so that you get 8 colors.
- To mount the PiDisk do mount `PI0:` from `DEVS:a314fs-mountlist`. You may also want to add the contents of `a314fs-mountlist` to your `DEVS:mountlist` file, so that you can simply write `mount PI0:` instead.
  After these files are copied it should be possible to mount the PiDisk: by `Mount PI0:`.
  The pi command can be invoked by pi without any arguments to run bash, or with arguments to run a particular Linux command.

You can download [this zip file](https://www.dropbox.com/s/g5f5c4zf1x55vx3/her_dither3.zip?dl=0) and unzip
to /home/pi/player/her_dither3/\*.ami in order to play those files back using VideoPlayer.
