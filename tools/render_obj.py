r"""Tiny software renderer: OBJ -> PNG, no third-party deps.

Why it exists: after `tools/gmdl_mesh.py` extracts geometry (positions verified against the
file's own bbox), we want a picture to confirm it is a coherent surface and not noise - a
cheap, honest check that needs neither the engine nor an external viewer. It is a TOOL, not
the renderer; the engine renders real assets on its own D3D9 path.

Usage: python tools/render_obj.py analysis/meshes/gmdl_pkg52.obj out.png [size]
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
import zlib
from pathlib import Path


def load_obj(path: Path):
    verts = []
    faces = []
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        if line.startswith("v "):
            _, x, y, z = line.split()[:4]
            verts.append((float(x), float(y), float(z)))
        elif line.startswith("f "):
            parts = line.split()[1:]
            idx = [int(p.split("/")[0]) - 1 for p in parts]
            for i in range(1, len(idx) - 1):
                faces.append((idx[0], idx[i], idx[i + 1]))
    return verts, faces


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(a):
    l = math.sqrt(dot(a, a)) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def render(verts, faces, size):
    # centre + fit
    cx = sum(v[0] for v in verts) / len(verts)
    cy = sum(v[1] for v in verts) / len(verts)
    cz = sum(v[2] for v in verts) / len(verts)
    r = max(math.dist(v, (cx, cy, cz)) for v in verts) or 1.0

    # camera: rotate a little so it is clearly 3D, then look at the centre
    ax, ay = math.radians(25), math.radians(35)
    ca, sa, cb, sb = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay)
    dist = r * 3.0

    def project(p):
        x, y, z = p[0] - cx, p[1] - cy, p[2] - cz
        # rotate around Y then X
        x, z = x * cb + z * sb, -x * sb + z * cb
        y, z = y * ca - z * sa, y * sa + z * ca
        zc = z + dist
        if zc < 1e-4:
            return None
        f = size * 0.9 / zc
        return (size * 0.5 + x * f, size * 0.5 - y * f, zc)

    zbuf = [1e30] * (size * size)
    img = bytearray([20] * (size * size))
    light = norm((0.4, 0.5, 1.0))

    for a, b, c in faces:
        pa, pb, pc = project(verts[a]), project(verts[b]), project(verts[c])
        if not (pa and pb and pc):
            continue
        n = norm(cross(sub(verts[b], verts[a]), sub(verts[c], verts[a])))
        shade = 40 + int(200 * abs(dot(n, light)))
        minx, maxx = int(min(pa[0], pb[0], pc[0])), int(max(pa[0], pb[0], pc[0]))
        miny, maxy = int(min(pa[1], pb[1], pc[1])), int(max(pa[1], pb[1], pc[1]))
        minx, maxx = max(0, minx), min(size - 1, maxx)
        miny, maxy = max(0, miny), min(size - 1, maxy)
        d = (pb[1] - pc[1]) * (pa[0] - pc[0]) + (pc[0] - pb[0]) * (pa[1] - pc[1])
        if abs(d) < 1e-9:
            continue
        for py in range(miny, maxy + 1):
            for px in range(minx, maxx + 1):
                w0 = ((pb[1] - pc[1]) * (px - pc[0]) + (pc[0] - pb[0]) * (py - pc[1])) / d
                w1 = ((pc[1] - pa[1]) * (px - pc[0]) + (pa[0] - pc[0]) * (py - pc[1])) / d
                w2 = 1 - w0 - w1
                if w0 < 0 or w1 < 0 or w2 < 0:
                    continue
                z = w0 * pa[2] + w1 * pb[2] + w2 * pc[2]
                o = py * size + px
                if z < zbuf[o]:
                    zbuf[o] = z
                    img[o] = shade
    return img


def write_png(path: Path, size: int, gray: bytearray):
    raw = bytearray()
    for y in range(size):
        raw.append(0)
        raw.extend(gray[y * size : (y + 1) * size])

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("obj")
    ap.add_argument("out")
    ap.add_argument("size", nargs="?", type=int, default=640)
    a = ap.parse_args()
    verts, faces = load_obj(Path(a.obj))
    print(f"{len(verts)} vertices, {len(faces)} triangles")
    img = render(verts, faces, a.size)
    write_png(Path(a.out), a.size, img)
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
