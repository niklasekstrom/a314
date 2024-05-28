set -e
set -x
python gen_stubs.py libdecl-$1.json
vc +aos68k -I${NDK32}/Include_H romtag.asm library.c -nostdlib -O2 -o $1.library
