set -x

python gen_stubs.py
python gen_proto.py
vc +aos68k -I${NDK32}/Include_H romtag.asm library.c -nostdlib -o example.library
vc +aos68k -I${NDK32}/Include_H example_client.c -o ExampleClient
