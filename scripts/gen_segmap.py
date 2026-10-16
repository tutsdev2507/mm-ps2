#!/usr/bin/env python3
import sys
import os
import struct

def main():
    mm = '/storage/emulated/0/Download/mm-ps2/third_party/mm'
    spec = os.path.join(mm, 'spec', 'spec')
    dmadata = os.path.join(mm, 'extracted', 'n64-us', 'baserom', 'dmadata')
    out_bin = '/storage/emulated/0/Download/mm-ps2/platform/ps2/segmap.bin'
    out_h = '/storage/emulated/0/Download/mm-ps2/platform/ps2/segmap.h'
    if not os.path.exists(spec):
        print('missing spec')
        sys.exit(1)
    if not os.path.exists(dmadata):
        print('missing dmadata')
        sys.exit(1)
    names = []
    with open(spec, 'r', encoding='latin-1', errors='replace') as f:
        for line in f:
            line = line.strip()
            if line.startswith('beginseg'):
                parts = line.split()
                if len(parts) >= 2:
                    names.append(parts[1])
    ent = struct.Struct('<IIIIHH')
    entries = []
    with open(dmadata, 'rb') as f:
        data = f.read()
    for i in range(0, len(data), 0x10):
        chunk = data[i:i+0x10]
        if len(chunk) != 0x10:
            break
        vstart, vend, rstart, rend = struct.unpack_from('<IIII', chunk, 0)
        if i // 0x10 < len(names):
            nm = names[i // 0x10]
        else:
            nm = f'seg_{i//0x10:03d}'
        entries.append((vstart, vend, rstart, rend, nm))
    name_blob = bytearray()
    name_off = []
    for _, _, _, _, nm in entries:
        b = nm.encode('latin-1') + b'\0'
        name_off.append(len(name_blob))
        name_blob.extend(b)
    with open(out_bin, 'wb') as f:
        f.write(struct.pack('<I', len(entries)))
        for j, (vstart, vend, rstart, rend, nm) in enumerate(entries):
            f.write(ent.pack(vstart, vend, rstart, rend, name_off[j], 0))
        f.write(name_blob)
    with open(out_h, 'w') as f:
        f.write('#ifndef SEGMAP_H\n#define SEGMAP_H\n')
        f.write('#define SEGMAP_ENTRIES %d\n' % len(entries))
        f.write('typedef struct { u32 vromStart,vromEnd,romStart,romEnd; const char* name; } SegMapEnt;\n')
        f.write('#endif\n')
    print('ok', len(entries))

if __name__ == '__main__':
    main()
