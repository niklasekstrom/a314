set -e
set -x
python gen_proto.py libdecl-$1.json
vc +aos68k -I${NDK32}/Include_H example_client.c -o ExampleClient
