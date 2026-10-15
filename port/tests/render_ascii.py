#!/usr/bin/env python3
"""Render a triangle dump (from render_object_ah.sh) as ASCII, so the geometry
can be inspected in a text-only terminal.

Prints two panels:
  SOLID  - filled silhouette, depth-shaded (nearer = denser glyph)
  WIRE   - triangle edges only (best for spotting stretched 'skin' triangles)
"""
import sys, math

W, H = 640, 448
CW, CH = 192, 72
RAMP = " .:-=+*#%@"

def load(path):
    tris = []
    for ln in open(path):
        f = ln.split()
        if not f or f[0] not in ('F', 'T'):
            continue
        pts = [(int(f[1]) / 16, int(f[2]) / 16, int(f[3])),
               (int(f[4]) / 16, int(f[5]) / 16, int(f[6])),
               (int(f[7]) / 16, int(f[8]) / 16, int(f[9]))]
        tris.append((f[0], pts))
    return tris

def cell(x, y):
    return int(x / W * CW), int(y / H * CH)

def main():
    tris = load(sys.argv[1])
    solid = [[' '] * CW for _ in range(CH)]
    zdb = [[-1] * CW for _ in range(CH)]
    wire = [[' '] * CW for _ in range(CH)]
    zmin = min(t[1][i][2] for t in tris for i in range(3))
    zmax = max(t[1][i][2] for t in tris for i in range(3))
    for _kind, (p1, p2, p3) in tris:
        (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) = p1, p2, p3
        den = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
        if abs(den) < 1e-9:
            continue
        minx = max(0, int(min(x1, x2, x3))); maxx = min(W - 1, int(max(x1, x2, x3)))
        miny = max(0, int(min(y1, y2, y3))); maxy = min(H - 1, int(max(y1, y2, y3)))
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                px, py = x + 0.5, y + 0.5
                w1 = ((y2 - y3) * (px - x3) + (x3 - x2) * (py - y3)) / den
                w2 = ((y3 - y1) * (px - x3) + (x1 - x3) * (py - y3)) / den
                w3 = 1 - w1 - w2
                if w1 < -1e-4 or w2 < -1e-4 or w3 < -1e-4:
                    continue
                z = w1 * z1 + w2 * z2 + w3 * z3
                cx, cy = cell(px, py)
                if cy < CH and cx < CW and (zdb[cy][cx] < 0 or z < zdb[cy][cx]):
                    zdb[cy][cx] = z
        for (ax, ay, _), (bx, by, _) in ((p1, p2), (p2, p3), (p3, p1)):
            steps = int(max(abs(bx - ax), abs(by - ay))) + 1
            for s in range(steps + 1):
                t = s / steps
                cx, cy = cell(ax + (bx - ax) * t, ay + (by - ay) * t)
                if 0 <= cy < CH and 0 <= cx < CW:
                    wire[cy][cx] = '#'
    for cy in range(CH):
        for cx in range(CW):
            if zdb[cy][cx] < 0:
                continue
            n = (zdb[cy][cx] - zmin) / (zmax - zmin + 1e-9)
            solid[cy][cx] = RAMP[min(len(RAMP) - 1, int((1 - n) * (len(RAMP) - 1)))]
    print("=== SOLID (near=@ far=. ) ===")
    for row in solid:
        print(''.join(row))
    print("=== WIRE ===")
    for row in wire:
        print(''.join(row))

main()
