# PiStats

`PiStats` is an Amiga command that displays live system statistics from the
Raspberry Pi: per-core CPU usage, memory usage, kernel version, and disk usage
of the root filesystem. If no options are specified, it prints information to the
console. If MUI is specified, it uses a MUI 3.8 user interface when
`muimaster.library` is available and otherwise falls back to a simpler GadTools/
Intuition interface. The GadTools interface can be invoked directly with the `GT`
option. PiStats requires AmigaOS 2.04 or newer.

If using a graphical interface, the statistics refresh interval can be set in the window
using the Refresh slider, with the `REFRESH` argument when starting from a shell
(`PiStats MUI REFRESH=2000`), or with the `REFRESH` tooltype when starting from Workbench.
The interval is given in milliseconds, from 500 to 30000, with a default of 2000.

On the Pi side, the `pistats` service is installed by `install-pi.sh` and started
on demand by `a314d`. If `pistats` was added to an existing `/etc/opt/a314/a314d.conf`,
restart the daemon with `sudo systemctl restart a314d`.

## Building

To build PiStats in the Amiga software build, you will need the MUI developer archive
(MUI SDK, e.g. `dev/mui/mui38dev.lha` from Aminet). Extract it and set the `MUISDK` 
environment variable to the directory that contains `libraries/mui.h`, for example:
`export MUISDK=~/amiga/MUI/Developer/C/Include`