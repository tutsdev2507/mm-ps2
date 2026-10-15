#!/usr/bin/env python3
"""Rasterise a triangle dump from render_object_ah.sh into a PNG.

Dump format (one per line, coords in 1/16 px):
  F x1 y1 z1 x2 y2 z2 x3 y3 z3 rrggbb
  T x1 y1 z1 x2 y2 z2 x3 y3 z3 rrggbb tex u1 v1 u2 v2 u3 v3

Textured triangles are painted with a per-texture colour (hashed) so limbs /
texture groups are visually distinct; every triangle is outlined so stretched
"skin" triangles bridging a wrong joint are easy to spot.
"""
import sys, zlib, struct, math

W, H = 640, 448

def load(path):
    tris = []
    for ln in open(path):
        f = ln.split()
        if not f or f[0] not in ('F', 'T'):
            continue
        x1, y1 = int(f[1]) / 16, int(f[2]) / 16
        x2, y2 = int(f[4]) / 16, int(f[5]) / 16
        x3, y3 = int(f[7]) / 16, int(f[8]) / 16
        z1, z2, z3 = int(f[3]), int(f[6]), int(f[9])
        rgb = int(f[10], 16)
        if f[0] == 'T':
            tex = int(f[11])
            hue = (tex * 2654435761) & 0xFFFFFF
            r = (hue >> 16) & 0xFF; g = (hue >> 8) & 0xFF; b = hue & 0xFF
            # keep it bright/pastel so black edges read
            rgb = ((r | 0x40) << 16) | ((g | 0x40) << 8) | (b | 0x40)
        tris.append(((x1, y1, z1), (x2, y2, z2), (x3, y3, z3), rgb))
    return tris

def main():
    tris = load(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else 'dump.png'
    buf = bytearray(b'\x00' * (W * H * 3))
    zbuf = [0] * (W * H)
    for (p1, p2, p3, rgb) in tris:
        (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) = p1, p2, p3
        minx = max(0, int(math.floor(min(x1, x2, x3))))
        maxx = min(W - 1, int(math.ceil(max(x1, x2, x3))))
        miny = max(0, int(math.floor(min(y1, y2, y3))))
        maxy = min(H - 1, int(math.ceil(max(y1, y2, y3))))
        den = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
        if abs(den) < 1e-9:
            continue
        r = (rgb >> 16) & 0xFF; g = (rgb >> 8) & 0xFF; b = rgb & 0xFF
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                px, py = x + 0.5, y + 0.5
                w1 = ((y2 - y3) * (px - x3) + (x3 - x2) * (py - y3)) / den
                w2 = ((y3 - y1) * (px - x3) + (x1 - x3) * (py - y3)) / den
                w3 = 1 - w1 - w2
                if w1 < -1e-4 or w2 < -1e-4 or w3 < -1e-4:
                    continue
                z = w1 * z1 + w2 * z2 + w3 * z3
                i = y * W + x
                if z > zbuf[i]:
                    zbuf[i] = z
                    o = i * 3
                    buf[o] = r; buf[o + 1] = g; buf[o + 2] = b
        # outline
        for (ax, ay, _), (bx, by, _) in ((p1, p2), (p2, p3), (p3, p1)):
            steps = int(max(abs(bx - ax), abs(by - ay))) + 1
            for s in range(steps + 1):
                t = s / steps
                x = int(ax + (bx - ax) * t)
                y = int(ay + (by - ay) * t)
                if 0 <= x < W and 0 <= y < H:
                    o = (y * W + x) * 3
                    buf[o] = 20; buf[o + 1] = 20; buf[o + 2] = 20
    # encode PNG
    raw = b''.join(b'\x00' + bytes(buf[y * W * 3:(y + 1) * W * 3]) for y in range(H))
    def chunk(typ, data):
        c = struct.pack('>I', len(data)) + typ + data
        return c + struct.pack('>I', zlib.crc32(typ + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6))
    png += chunk(b'IEND', b'')
    open(out, 'wb').write(png)
    print("wrote", out, len(tris), "triangles")

main()
