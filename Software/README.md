# A314 Software

**Please note** that these are the instructions for installing the A500/A600 versions of A314.

If you have the clockport version of A314 then you should follow the instructions from the clockport branch: [https://github.com/niklasekstrom/a314/tree/clockport_if/Software](https://github.com/niklasekstrom/a314/tree/clockport_if/Software).

## How to install

**Note** that there are additional instructions in the [wiki](https://github.com/niklasekstrom/a314/wiki/Installation-instructions) that you should also follow. These instructions are for building and installing the software in this repo, but there are additional steps that should be taken.

On the Raspberry Pi do the following:
- Install Raspbian: https://www.raspberrypi.org/downloads/raspbian/
- Install Docker: ```curl -sSL https://get.docker.com | sh```
- Allow user pi to run Docker: ```sudo usermod -aG docker pi```
- Log out and then log back in again to make the previous command take effect
- Install Dependencies: ```sudo apt install python3-dev python3-distutils python3-pip build-essential git```
- Clone the a314 repo: ```git clone https://github.com/niklasekstrom/a314.git```
- ```cd a314/Software```
- Set the build script as executable: ```sudo chmod +x rpi_docker_build.sh```
- Build software for both the Amiga and Pi: ```./rpi_docker_build.sh```
- Install software for Pi: ```sudo make install```
- Manually add the line `dtoverlay=spi-a314` to the end of `/boot/config.txt` (unless it's already added), and reboot
- Enable and start a314d:
  - ```sudo systemctl daemon-reload```
  - ```sudo systemctl enable a314d```
  - ```sudo systemctl start a314d```

In the a314/Software/bin directory there are a number of binaries that should be copied to the Amiga boot disk:
- a314/Software/bin/a314.device to DEVS:
- a314/Software/bin/a314fs to L:
- a314/Software/a314fs/a314fs-mountlist should be appended to the DEVS:Mountlist file
- a314/Software/bin/pi to C:
- a314/Software/bin/piaudio to C:
- a314/Software/bin/remotewb to C:
- a314/Software/bin/videoplayer to C:

After these files are copied it should be possible to mount the PiDisk: by ```Mount PI0:```.
The pi command can be invoked by pi without any arguments to run bash, or with arguments to run a particular Linux command.

You can download [this zip file](https://www.dropbox.com/s/g5f5c4zf1x55vx3/her_dither3.zip?dl=0) and unzip
to /home/pi/player/her_dither3/*.ami in order to play those files back using VideoPlayer.
