#!/usr/bin/env python3
import os
import re
import subprocess
import sys

NM = 'mips64r5900el-ps2-elf-nm'
SKIP = re.compile(r'(__os|_gp|_fdata|_ftext|_end|_edata|_fbss|_stack)')

def symbols_of(obj):
    out = subprocess.run([NM, '-S', '--defined-only', obj],
                         capture_output=True, text=True)
    if out.returncode != 0:
        return []
    syms = []
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        addr = parts[0]
        size = parts[1]
        kind = parts[2]
        name = parts[3]
        if kind in 'tT' or SKIP.search(name):
            continue
        if name.startswith('.'):
            continue
        try:
            sz = int(size, 16)
        except:
            sz = 8
        if sz == 0:
            sz = 8
        syms.append((name, kind, sz))
    return syms

def main():
    if len(sys.argv) != 3:
        return
    objdir, out = sys.argv[1], sys.argv[2]
    seen = set()
    entries = []
    for root, _dirs, files in os.walk(objdir):
        for fn in sorted(files):
            if not fn.endswith('.o'):
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, objdir)
            if not (rel.startswith('extracted/') or rel.startswith('assets/')):
                continue
            for name, kind, sz in symbols_of(path):
                if name in seen:
                    continue
                seen.add(name)
                # use size, align to 16
                entries.append('u8 %s[%d] __attribute__((used, aligned(16)));\n' % (name, max(sz, 8)))
    with open(out, 'w') as f:
        f.write('/* Generated - do not edit */\n\n')
        f.write('\n'.join(entries))
        f.write('\n')
    print('wrote', out, len(entries))

if __name__ == '__main__':
    main()
