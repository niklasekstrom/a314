# a314eth.device - SANA-II driver for A314

This SANA-II driver works by copying Ethernet frames back and forth between the Amiga and a virtual ethernet interface (tap0) on the Raspberry Pi. The Pi will, when configured properly, do network address translation (NAT) and route packets from the Amiga to the Internet.

## Configuring the Raspberry Pi

- Install pytun: `sudo pip3 install python-pytun`.
- Copy `ethernet.py` to `/opt/a314/ethernet.py`.
- Update `/etc/opt/a314/a314d.conf` with a line that starts `ethernet.py` on demand.
  - In order for `a314d` to pick up the changes in `a314d.conf` you'll have to restart `a314d`, either by `sudo systemctl restart a314d` or by rebooting the Pi.
- Copy `pi-config/tap0` to `/etc/network/interfaces.d/tap0`. This file creates a tap device with ip address 192.168.2.1 when the Raspberry Pi is booted up.
- Enable ip forwarding by uncommenting the line in `/etc/sysctl.conf` that says `net.ipv4.ip_forward=1`.
- Add the lines in `pi-config/rc.local` to the bottom of `/etc/rc.local` just before `exit 0`. This create iptables rules that forwards packets from the `tap0` interface to the `wlan0` interface.
  - Please note that if the Pi is connected using wired ethernet then `wlan0` should be changed to `eth0`.

The first four steps are performed by `sudo make install`. The last steps you have to do manually.

## Configuring the Amiga

This has been tested with the Roadshow and the MiamiDX TCP/IP stacks. These instructions show how to configure Roadshow and Miami for a314ethernet, they do not describe how to install either.

### Common
- Build the `a314eth.device` binary, for example using the `rpi_docker_build.sh` script.
- Copy `bin/a314eth.device` to `DEVS:`.

### Roadshow
- Copy `amiga-config/A314Eth` to `DEVS:NetInterfaces/A314Eth`.
- Copy `amiga-config/routes` to `DEVS:Internet/routes`.
- Copy `amiga-config/name_resolution` to `DEVS:Internet/name_resolution`.
  - You should change the nameserver to a DNS server that works on your network.
  - Note that there may be settings in the above two files (`routes` and `name_resolution`) that you wish to keep, so look through the changes you are about to make first.

Reboot the Amiga and with some luck you should be able to access the Internet from your Amiga.

### Miami
- Create new device entry under **Hardware**
  - Click **New** and select **Ethernet**
  - Name it accordingly, for example `a314eth`
  - **Type** should be **SANA-II driver**
  - For **Driver**, type in, or browse to and select, `DEVS:a314eth.device` and click **OK**
- Create a new network interface in **Interfaces**
  - Click **New** and select **Ethernet** and **Internet**, click **OK**
  - Select the previously created **a314eth** and click **OK**
  - Set **IP** to **static** and type in `192.168.2.2`
  - Set **Netmask** to **static** and type in `255.255.255.0`
  - Set Gateway to **static** and type in `192.168.2.1`
  - Click **OK**
- Add a nameserver in **Database**
  - Select **DNS servers** from the spinner
  - Click **Add** and type in the address of your DNS server and press enter (you can use for example Google's DNS server at `8.8.8.8` if unsure)
- Remember to save your settings!

Click **Online** and hopefully your Amiga now has internet access.

Autostarting Miami and further configuration is left to the user.

## Important note:

After `ethernet.py` starts it waits for 15 seconds before it starts forwarding Ethernet frames. Without this waiting there is something that doesn't work properly (I don't know why this is). So when the Amiga boots for the first time after power off you'll have to wait up to 15 seconds before the Amiga can reach the Internet.
