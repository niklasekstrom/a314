set -e
set -x
python gen_proto.py example
vc +aos68k -I${NDK32}/Include_H example_client.c -o ExampleClient
