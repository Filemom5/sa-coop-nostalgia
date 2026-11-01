#!/usr/bin/env python3
"""
Builds the headphones' model (a RenderWare 3.6 DFF, as GTA SA reads them) and writes it as a C++ array:
src/client/features/radio/HeadsetModel.h (python tools/headset_model.py). Headset.cpp reads it from memory
(RwStreamOpen of rwSTREAMMEMORY, RpClumpStreamRead) and draws it at the head bone of whoever has the headphones on.

The model is made in the head bone's own space: X up the head, Y to the front, Z across, from ear to ear. A band over the
top of the head and a cup over each ear, dark grey with the cups' outer faces pink. Untextured: material colours, lit.
Every triangle goes both ways round, so no culling setting can hide it.
"""
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "client" / "features" / "radio" / "HeadsetModel.h"
RW_VERSION = 0x1803FFFF  # RenderWare 3.6.0.3, as the game's own files

# sizes in metres
CENTRE_X = 0.055       # the head's middle above the head bone
BAND_RADIUS = 0.100    # from that middle to the band
BAND_WIDTH = 0.028     # front to back
BAND_THICK = 0.010
CUP_Z = 0.095          # the cups' middle, either side
CUP_RADIUS = 0.042
CUP_DEPTH = 0.030
SEGMENTS = 24

DARK = (38, 38, 44, 255)
PINK = (255, 95, 176, 255)

verts, normals, tris = [], [], []  # tris: (a, b, c, material)


def quad(p0, p1, p2, p3, n, mat):
    base = len(verts)
    for p in (p0, p1, p2, p3):
        verts.append(p)
        normals.append(n)
    for a, b, c in ((0, 1, 2), (0, 2, 3)):
        tris.append((base + a, base + b, base + c, mat))
        tris.append((base + a, base + c, base + b, mat))  # the other way round


def tri(p0, p1, p2, n, mat):
    base = len(verts)
    for p in (p0, p1, p2):
        verts.append(p)
        normals.append(n)
    tris.append((base, base + 1, base + 2, mat))
    tris.append((base, base + 2, base + 1, mat))


def band():
    w = BAND_WIDTH / 2
    for i in range(SEGMENTS):
        t0 = -math.pi / 2 + math.pi * i / SEGMENTS
        t1 = -math.pi / 2 + math.pi * (i + 1) / SEGMENTS
        def at(t, r, y):
            return (CENTRE_X + r * math.cos(t), y, r * math.sin(t))
        ro, ri = BAND_RADIUS + BAND_THICK / 2, BAND_RADIUS - BAND_THICK / 2
        tm = (t0 + t1) / 2
        n_out = (math.cos(tm), 0.0, math.sin(tm))
        n_in = (-n_out[0], 0.0, -n_out[2])
        quad(at(t0, ro, -w), at(t1, ro, -w), at(t1, ro, w), at(t0, ro, w), n_out, 0)
        quad(at(t0, ri, -w), at(t0, ri, w), at(t1, ri, w), at(t1, ri, -w), n_in, 0)
        quad(at(t0, ri, w), at(t0, ro, w), at(t1, ro, w), at(t1, ri, w), (0.0, 1.0, 0.0), 0)
        quad(at(t0, ri, -w), at(t1, ri, -w), at(t1, ro, -w), at(t0, ro, -w), (0.0, -1.0, 0.0), 0)


def cup(side):
    z_in = side * (CUP_Z - CUP_DEPTH / 2)
    z_out = side * (CUP_Z + CUP_DEPTH / 2)
    def ring(k, z):
        a = 2 * math.pi * k / SEGMENTS
        return (CENTRE_X + CUP_RADIUS * math.cos(a), CUP_RADIUS * math.sin(a), z)
    for k in range(SEGMENTS):
        a = 2 * math.pi * (k + 0.5) / SEGMENTS
        n = (math.cos(a), math.sin(a), 0.0)
        quad(ring(k, z_in), ring(k + 1, z_in), ring(k + 1, z_out), ring(k, z_out), n, 0)
        tri((CENTRE_X, 0.0, z_in), ring(k, z_in), ring(k + 1, z_in), (0.0, 0.0, -side), 0)    # against the head
        tri((CENTRE_X, 0.0, z_out), ring(k + 1, z_out), ring(k, z_out), (0.0, 0.0, side), 1)  # the pink outside


def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), RW_VERSION) + payload


def dff():
    band()
    cup(1)
    cup(-1)
    nv, nt = len(verts), len(tris)
    assert nv < 65536
    flags = 0x02 | 0x10 | 0x20 | 0x40  # positions, normals, light, modulate material colour
    geo = struct.pack("<Iiii", flags, nt, nv, 1)
    for a, b, c, m in tris:
        geo += struct.pack("<HHHH", b, a, m, c)  # vertex2, vertex1, material, vertex3
    geo += struct.pack("<ffff", CENTRE_X, 0.0, 0.0, 0.17)  # bounding sphere
    geo += struct.pack("<II", 1, 1)
    for v in verts:
        geo += struct.pack("<fff", *v)
    for n in normals:
        geo += struct.pack("<fff", *n)

    def material(rgba):
        body = struct.pack("<I4BiI", 0, *rgba, 0, 0) + struct.pack("<fff", 1.0, 1.0, 1.0)
        return chunk(0x07, chunk(0x01, body) + chunk(0x03, b""))

    mats = chunk(0x08, chunk(0x01, struct.pack("<Iii", 2, -1, -1)) + material(DARK) + material(PINK))
    meshes = b""
    total = 0
    for m in (0, 1):
        idx = [i for a, b, c, mm in tris if mm == m for i in (a, b, c)]
        meshes += struct.pack("<II", len(idx), m) + struct.pack(f"<{len(idx)}I", *idx)
        total += len(idx)
    binmesh = chunk(0x50E, struct.pack("<III", 0, 2, total) + meshes)
    geometry = chunk(0x0F, chunk(0x01, geo) + mats + chunk(0x03, binmesh))
    geolist = chunk(0x1A, chunk(0x01, struct.pack("<I", 1)) + geometry)

    frame = struct.pack("<9f3fiI", 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, 0)
    framelist = chunk(0x0E, chunk(0x01, struct.pack("<I", 1) + frame) + chunk(0x03, b""))
    atomic = chunk(0x14, chunk(0x01, struct.pack("<IIII", 0, 0, 5, 0)) + chunk(0x03, b""))
    return chunk(0x10, chunk(0x01, struct.pack("<III", 1, 0, 0)) + framelist + geolist + atomic + chunk(0x03, b""))


def main():
    data = dff()
    lines = [", ".join(f"0x{b:02X}" for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    OUT.write_text(
        "#pragma once\n"
        "// The headphones' model: a RenderWare DFF made by tools/headset_model.py (do not edit, run that again).\n"
        f"// {len(verts)} vertices, {len(tris)} triangles.\n"
        "#include <cstdint>\n\n"
        "namespace coop::headphones {\n\n"
        f"inline constexpr uint8_t kHeadsetDff[{len(data)}] = {{\n    " + ",\n    ".join(lines) + "\n};\n\n"
        "}  // namespace coop::headphones\n",
        encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(data)} bytes, {len(verts)} vertices, {len(tris)} triangles")


if __name__ == "__main__":
    main()
