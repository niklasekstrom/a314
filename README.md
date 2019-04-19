# A314

## What is it?

The *A314* is an expansion board for the Amiga 500 that goes in the trapdoor expansion slot. A Raspberry Pi (RPi) is attached to the A314, and the A500 and the RPi can communicate through a shared memory.

|         |            |
| ------------- |---------------|
| ![PCB](Documentation/Images/populated_pcb.jpg)      | ![A314 with RPi attached](Documentation/Images/a314_with_rpi.jpg) |

Drivers running on both sides ([*a314.device*](Software/a314device) on the Amiga and [*a314d*](Software/a314d) on the RPi) let processes on the RPi expose services to which processes on the Amiga can connect to, and establish logical communication channels between the processes. When a process on one side sends a packet to a process on the other side, an interrupt is generated that wakes up the receiving process.

## What can you do with it today?

We have implemented a few services that run on the RPi and on the A500:

*  [*a314fs*](Software/a314fs) is a file system that is mounted in AmigaDOS as a device, PI0:, and the volume in it, PiDisk:, is mapped to a directory on the RPi.

*  [*pi*](Software/picmd) is a command that lets you invoke commands from the AmigaDOS command line, that are then run on the RPi. For example, if you stand in a directory on PiDisk: and run "pi vc hello.c -o hello" then the vc program (the VBCC cross-compiler) is run on the RPi with the given arguments, which compiles hello.c to the executable hello. As the resulting binary is accessible through the a314fs, the resulting program can be run directly after. Interactive programs can also be executed using the pi command, such as "pi mc -a" which runs Midnight Commander. Running pi without any arguments is equivalent to "pi bash".

<img src="Documentation/Images/workbench.jpg" width="500px"/>

*  [*RemoteWB*](Software/remotewb) works by moving the Workbench bitplanes over to the chip memory on the A314 (this requires that the A500 has at least a 8372 Agnus) and then each frame the RPi reads those bitplanes, encodes those as a [GIF image](Software/bpls2gif), and then sends that image to a web browser through a web socket. The web browser sends key presses and mouse movements back to the Amiga through the web socket, and the effect is that the Workbench is remoted the a web browser.

*  [*VideoPlayer*](Software/videoplayer) is a simple program that displays a sequence of images on the A500 by letting the RPi write bitplanes directly to the shared memory (this again requires that the A314 memory is chip memory).

## What could it potentially be used for in the future?

Here are some services that we have considered but not gotten around to implement:

* Networking either through a *bsdsocket.library* implementation that forwards socket operations to the RPi and executes those operations there, or a *Sana II* driver used with a full network stack.

* An mp3 player that lets the RPi stream samples directly to the shared chip memory, from where Paula would play those samples.

## How can you get involved?

If this sounds interesting, you'll probably want an A314 of your own to play with. We have released all the information needed to make a board free in this GitHub repository.

In the [Hardware](Hardware) directory there is [schematics](Hardware/Beta-2/Schematics/A314B2.pdf) and [Gerber files](Hardware/Beta-2/Gerbers) that can be used to produce a PCB.

The Verilog source code used to generate a programming object file (.pof) for the Intel MAX 10 FPGA is available in the [HDL](HDL) directory. You'll need a USB-Blaster download cable (or a clone) to connect to the JTAG connector on the A314 board. You can compile the design using the [Quartus Prime Lite Edition](http://fpgasoftware.intel.com/?edition=lite).

The source code for the software that runs on the Amiga and on the RPi is available in the [Software](Software) directory.

If you have an idea about something cool to make using the A314, but you don't have the means to build a PCB on your own, then we have a small number of pre-built boards that we plan to hand out, if the idea sounds interesting enough. Send a message to Eriond on EAB and describe what you would like to make, and perhaps you can get one.
