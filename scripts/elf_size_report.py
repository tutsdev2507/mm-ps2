#!/usr/bin/env python3
"""
Report the size composition of an EE object set.

The PS2 has 32 MiB of main RAM, so the single LOAD segment of the ported
ELF must fit in it.  This walks every .o in a directory, sums the
allocatable sections (.text/.data/.rodata/.bss) and prints the biggest
contributors, grouped by source directory.

Usage: python3 scripts/elf_size_report.py <objdir> [--top N]
"""
import os
import struct
import sys
from collections import defaultdict

SEC = ('.text', '.data', '.rodata', '.bss', '.sdata', '.sbss', '.rodata.str1.4')


def sections(path):
    """Return {section_name: size} for a 32-bit little-endian ELF."""
    with open(path, 'rb') as f:
        data = f.read(0x40)
        if len(data) < 0x40 or data[:4] != b'\x7fELF':
            return None
        if data[4] != 1:          # ELFCLASS32
            return None
        e_shoff = struct.unpack_from('<I', data, 0x20)[0]
        e_shentsize = struct.unpack_from('<H', data, 0x2E)[0]
        e_shnum = struct.unpack_from('<H', data, 0x30)[0]
        e_shstrndx = struct.unpack_from('<H', data, 0x32)[0]
        f.seek(e_shoff)
        sh = f.read(e_shentsize * e_shnum)
        if len(sh) < e_shentsize * e_shnum:
            return None
        off, size = struct.unpack_from('<II', sh, e_shstrndx * e_shentsize + 0x10)
        f.seek(off)
        strtab = f.read(size)
        out = {}
        for i in range(e_shnum):
            base = i * e_shentsize
            nameoff = struct.unpack_from('<I', sh, base)[0]
            sh_size = struct.unpack_from('<I', sh, base + 0x14)[0]
            sh_flags = struct.unpack_from('<I', sh, base + 0x8)[0]
            if not (sh_flags & 2):        # SHF_ALLOC
                continue
            end = strtab.find(b'\0', nameoff)
            name = strtab[nameoff:end].decode('latin-1')
            out[name] = out.get(name, 0) + sh_size
        return out


def main():
    objdir = sys.argv[1]
    top = 50
    if '--top' in sys.argv:
        top = int(sys.argv[sys.argv.index('--top') + 1])

    bydir = defaultdict(lambda: defaultdict(int))
    perfile = []
    for root, _dirs, files in os.walk(objdir):
        for fn in files:
            if not fn.endswith('.o'):
                continue
            path = os.path.join(root, fn)
            secs = sections(path)
            if not secs:
                continue
            rel = os.path.relpath(path, objdir)
            d = os.path.dirname(rel)
            total = 0
            for s, sz in secs.items():
                bydir[d][s] += sz
                total += sz
            perfile.append((total, rel, secs))

    print('== allocatable bytes by directory ==')
    agg = sorted(bydir.items(),
                 key=lambda kv: -sum(kv[1].values()))
    print('%-55s %10s %10s %10s %10s' % ('dir', 'text', 'data', 'rodata', 'bss'))
    for d, s in agg[:25]:
        print('%-55s %10d %10d %10d %10d'
              % (d[:55], s.get('.text', 0), s.get('.data', 0),
                 s.get('.rodata', 0), s.get('.bss', 0)))
    tot = defaultdict(int)
    for d, s in bydir.items():
        for k, v in s.items():
            tot[k] += v
    print('\nTOTAL: text=%d data=%d rodata=%d bss=%d  grand=%d'
          % (tot.get('.text', 0), tot.get('.data', 0),
             tot.get('.rodata', 0), tot.get('.bss', 0), sum(tot.values())))

    print('\n== top %d objects by allocatable bytes ==' % top)
    perfile.sort(reverse=True)
    for total, rel, secs in perfile[:top]:
        print('%9d  t=%-8d d=%-9d r=%-8d b=%-8d  %s'
              % (total, secs.get('.text', 0), secs.get('.data', 0),
                 secs.get('.rodata', 0), secs.get('.bss', 0), rel))


if __name__ == '__main__':
    main()
