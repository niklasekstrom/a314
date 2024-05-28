import json
import sys

with open(sys.argv[1], 'rt') as f:
    libdecl = json.load(f)

funcs = libdecl['funcs']

contents = '#include "messages.h"\n\n'

contents += f'#define LIBRARY_NAME "{libdecl["library-name"]}"\n'
contents += f'#define SERVICE_NAME "{libdecl["service-name"]}"\n\n'

contents += '''struct LibRemote;
static ULONG null_func();
static BPTR expunge();
static BPTR close(__reg("a6") struct LibRemote *lib);
static ULONG send_request(__reg("a6") struct LibRemote *lib, UBYTE *msg, ULONG length);

'''

contents += f'#define LVO_COUNT {len(funcs) + 4}\n\n'

def gen_param(reg):
    if reg[0] == 'd':
        return f'__reg("{reg}") ULONG {reg}'
    else:
        return f'__reg("{reg}") void *{reg}'

for i, func in enumerate(funcs):
    args = func.get('args', [])
    contents += f'static ULONG func_{i}(__reg("a6") struct LibRemote *lib'
    if len(args):
        contents += ', ' + ', '.join(gen_param(reg) for reg, _, _ in args)
    contents += ')\n{\n'
    contents += f'    UBYTE msg[{2 + len(args)*4}];\n'
    for j, (reg, _, _) in enumerate(args):
        contents += f'    *(ULONG *)&msg[{2 + j*4}] = (ULONG){reg};\n'
    contents += '    msg[0] = MSG_OP_REQ;\n'
    contents += f'    msg[1] = {i};\n'
    contents += '    return send_request(lib, msg, sizeof(msg));\n'
    contents += '}\n\n'

contents += '''static ULONG funcs_vector[] =
{
    (ULONG)null_func,
    (ULONG)close,
    (ULONG)expunge,
    (ULONG)null_func,
'''

for i, _ in enumerate(funcs):
    contents += f'    (ULONG)func_{i},\n'

contents += '''};

'''

contents += '''static void fill_lvos(__reg("a6") struct LibRemote *lib)
{
    for (int i = 0; i < LVO_COUNT; i++)
    {
        UBYTE *lvo = (UBYTE *)lib - ((i + 1) * 6);
        *(UWORD *)lvo = 0x4ef9;
        *(ULONG *)&lvo[2] = funcs_vector[i];
    }
}
'''

with open('stubs.h', 'wt') as f:
    f.write(contents)
