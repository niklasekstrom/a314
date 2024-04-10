# Disk Service

The disk service lets you mount disk images (ADF or HDF files) on the Raspberry
Pi as disk drives on the Amiga, PD0: through PD3:.

The a314disk.device is *romable* so it is possible to add it to a kickstart ROM
image and then it is possible to boot from a virtual drive. In the following
we focus on using a314disk.device as a normal device driver stored on disk.

## Disk geometry

The first time that a disk image (an ADF/HDF file) is inserted in a virtual
drive, the size of the ADF file is read, and from the size the disk geometry is
inferred. If the size of the image is 80\*2\*11\*512 = 901120 bytes then
the disk is assumed to be a standard floppy, with CHS geometry (80, 2, 11).
Otherwise the image is assumed to be an HDF file that has geometry (1, 32, x)
where x is the size of the image divided by 16384 (16 kB). This means that the
size of the disk image must be a multiple of 16 kB. The HDF image must also be
at least 1 MB.

The first time after each Amiga reboot that a disk image is inserted into a
virtual drive, the disk geometry for that drive becomes locked to the geometry
of the image. If the disk image is ejected from the drive and another image is
inserted then the newly inserted image must have the same geometry as the
previous one.

## Auto-insert

In the configuration file `/etc/opt/a314/disk.conf` on the Pi it is possible to
configure disk images that are automatically inserted when a virtual drive is
mounted on the Amiga. You specify the name of the disk image, which drive unit
the image should be inserted into, and if the image should be writable
(`"rw": true`) or only readable (`"rw": false`). A list of images can be
specified to be inserted into different drives.

## Manually insert and eject images

It is possible to manually insert and eject images. The commands for doing so
are a bit cryptic, and perhaps someone can write scripts that make this simpler.

The command to insert the disk image `/home/pi/disk.adf` into drive unit 0 and
make it readable and writable is:

`echo insert 0 -rw /home/pi/disk.adf | nc localhost 23890`

Remove the `-rw` to not make the image writable (i.e., only readable).

The above command should be executed on the Pi. By using the `Pi` command you
can run it from the Amiga as:

`pi bash -c "echo insert 0 -rw /home/pi/disk.adf | nc localhost 23890"`

Ejecting the image from drive unit 0 is done similarly using the following
command:

`echo eject 0 | nc localhost 23890`

## Mounting the drive

Before being able to use the virtual drive you have to mount it on the Amiga.
This is done using the `Mount` command. If you have an entry for drive PD0: in
the mount list file `DEVS:MountList` then you can mount PD0: with the command:

`mount PD0: from DEVS:MountList`

Note that you must first edit the mount list to set the geometry (Surfaces,
BlocksPerTrack, LowCyl and HighCyl) to match the geometry of the disk image.
I'm not sure if there is some way that the geometry can be taken from the values
reported by a314disk.device.
