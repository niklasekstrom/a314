import json
import sys

in_file = f'libdecl-{sys.argv[1]}.json'
out_file = f'proto_{sys.argv[1]}.h'

with open(in_file, 'rt') as f:
    libdecl = json.load(f)

funcs = libdecl['funcs']

library_name = libdecl['library-name'].replace('.library', '').lower()
guard = f'__PROTO_{library_name.upper()}_H'

contents = f'#ifndef {guard}\n'
contents += f'#define {guard}\n\n'
contents += '#include <exec/types.h>\n\n'

contents += f'#define {library_name.upper()}_LIB_NAME "{libdecl["library-name"]}"\n\n'

base = libdecl['library-base']

contents += f'extern struct Library *{base};\n\n'

def gen_reg_anon(reg):
    if reg[0] == 'd':
        return f'__reg("{reg}") ULONG'
    else:
        return f'__reg("{reg}") void *'

for i, func in enumerate(funcs):
    name = func['name']
    rtype = func['return-type']
    args = func['args']
    offset = -(i + 5) * 6

    if len(args):
        reglist = ', ' + ', '.join(gen_reg_anon(reg) for reg, _, _ in args)
    else:
        reglist = ''

    paramlist1 = ', '.join(name for _, name, _ in args)
    paramlist2 = f'{base}, {paramlist1}' if paramlist1 else base

    contents += f'{rtype} __{name}(__reg("a6") void *{reglist})="\\tjsr\\t{offset}(a6)";\n'
    contents += f'#define {name}({paramlist1}) __{name}({paramlist2})\n\n'

contents += f'#endif /* {guard} */\n'

with open(out_file, 'wt') as f:
    f.write(contents)
