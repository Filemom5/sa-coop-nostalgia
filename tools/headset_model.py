#!/usr/bin/env python3
"""
Builds the headphones' model (a RenderWare 3.6 DFF, as GTA SA reads them) and writes it as a C++ array:
src/client/features/radio/HeadsetModel.h (python tools/headset_model.py). Headset.cpp reads it from memory
(RwStreamOpen of rwSTREAMMEMORY, RpClumpStreamRead) and draws it at the head bone of whoever has the headphones on.

Over-ear headphones, as the ones at a gym: oval cups with a soft cushion against the head and a ring on the outside, a
padded band over the head and a slider down to each cup. Made in the head bone's own space: X up the head, Y to the
front, Z across, from ear to ear. Three materials, coloured by Headset.cpp per player: 0 the body, 1 the cushions,
2 the ring. Smooth normals; every triangle goes both ways round, so no culling setting can hide it.
"""
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "client" / "features" / "radio" / "HeadsetModel.h"
RW_VERSION = 0x1803FFFF  # RenderWare 3.6.0.3, as the game's own files

BODY, CUSHION, RING = 0, 1, 2
COLOURS = [(28, 28, 32, 255), (18, 18, 20, 255), (255, 95, 176, 255)]  # as made; Headset.cpp sets its own

# sizes in metres (fitted in the game on CJ: the cups over the ears, the band 3 cm above where it first was)
CUP_X = 0.055          # the cups' middle, up the head bone
CUP_Z = 0.089          # either side (in 6 mm from the first fit: snug on the ears)
CUP_A = 0.050          # the oval's half height
CUP_B = 0.041          # its half width, front to back
CUSHION_IN, CUSHION_OUT = 0.071, 0.085  # the cushion, out from the head's middle
SHELL_IN, SHELL_OUT = 0.084, 0.111      # the shell
BAND_X = 0.085         # the band's arc: its middle up the head bone, its radius
BAND_R = 0.098
BAND_W = 0.032         # front to back
BAND_T = 0.011         # thickness
BAND_END = math.radians(73)  # the arc ends above the cups; the sliders go down from there
SEG = 44

verts, normals, tris = [], [], []


def norm(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / l for c in v)


def strip(rows, mat, close=False):
    """A surface from rows of (point, normal): quads between neighbouring rows and columns."""
    base = len(verts)
    cols = len(rows[0])
    for row in rows:
        for p, n in row:
            verts.append(p)
            normals.append(norm(n))
    for r in range(len(rows) - 1):
        for c in range(cols - (0 if close else 1)):
            c2 = (c + 1) % cols
            a, b = base + r * cols + c, base + r * cols + c2
            d, e = base + (r + 1) * cols + c, base + (r + 1) * cols + c2
            for t in ((a, b, e), (a, e, d)):
                tris.append((*t, mat))
                tris.append((t[0], t[2], t[1], mat))


def oval(t, s):
    return CUP_X + CUP_A * s * math.cos(t), CUP_B * s * math.sin(t)


def oval_normal(t):
    return math.cos(t) / CUP_A, math.sin(t) / CUP_B


def cup(side):
    ts = [2 * math.pi * k / SEG for k in range(SEG)]
    # the cushion: a soft ring against the head, rounded at its edge
    prof = []
    for i in range(9):  # half a round, from the head's side to the shell's
        a = math.pi * i / 8
        prof.append((0.62 + 0.38 * (0.5 + 0.5 * math.cos(math.pi - a)),
                     CUSHION_IN + (CUSHION_OUT - CUSHION_IN) * (0.5 - 0.5 * math.cos(a)),
                     math.sin(a), -math.cos(a)))
    rows = []
    for s, z, out, across in prof:
        row = []
        for t in ts:
            x, y = oval(t, s)
            nx, ny = oval_normal(t)
            row.append(((x, y, side * z), (nx * out * 0.02, ny * out * 0.02, side * across)))
        rows.append(row)
    strip(rows, CUSHION, close=True)
    # the cushion's hole, a little way in
    rows = []
    for z, s in ((CUSHION_IN, 0.62), (CUSHION_IN + 0.004, 0.62)):
        rows.append([((oval(t, s)[0], oval(t, s)[1], side * z), (0.0, 0.0, -side)) for t in ts])
    strip(rows, CUSHION, close=True)
    # the shell: from the cushion out, narrowing a little, then a gently rounded face
    rows = []
    for z, s in ((SHELL_IN, 1.0), (SHELL_IN + 0.012, 1.0), (SHELL_OUT - 0.006, 0.96), (SHELL_OUT, 0.90)):
        row = []
        for t in ts:
            x, y = oval(t, s)
            nx, ny = oval_normal(t)
            row.append(((x, y, side * z), (nx * 0.02, ny * 0.02, side * (0.6 if z == SHELL_OUT else 0.0))))
        rows.append(row)
    strip(rows, BODY, close=True)
    face = []
    for s, bulge in ((0.90, 0.0), (0.84, 0.0015), (0.78, 0.0025), (0.55, 0.0045), (0.25, 0.0055), (0.0, 0.006)):
        row = []
        for t in ts:
            x, y = oval(t, s)
            row.append(((x, y, side * (SHELL_OUT + bulge)), (0.0, 0.0, side)))
        face.append(row)
    strip(face[:2], BODY, close=True)
    strip(face[1:3], RING, close=True)  # the ring on the outside
    strip(face[2:], BODY, close=True)


def band():
    w = BAND_W / 2
    phis = [-BAND_END + 2 * BAND_END * i / SEG for i in range(SEG + 1)]
    def at(phi, r, y):
        return (BAND_X + r * math.cos(phi), y, r * math.sin(phi))
    ro, ri = BAND_R + BAND_T / 2, BAND_R - BAND_T / 2
    out = [[(at(p, ro, y), (math.cos(p), 0.0, math.sin(p))) for p in phis] for y in (-w, w)]
    inn = [[(at(p, ri, y), (-math.cos(p), 0.0, -math.sin(p))) for p in phis] for y in (-w, w)]
    front = [[(at(p, r, w), (0.0, 1.0, 0.0)) for p in phis] for r in (ri, ro)]
    back = [[(at(p, r, -w), (0.0, -1.0, 0.0)) for p in phis] for r in (ri, ro)]
    for s in (out, inn, front, back):
        strip(s, BODY)
    # the pad under its middle
    pw = w * 0.75
    pads = [p for p in phis if abs(p) <= math.radians(48)]
    rp_out, rp_in = ri, ri - 0.009
    def pad_at(p, r, y):
        return at(p, r, y)
    rows = []
    for k in range(7):  # a rounded cross-section
        a = math.pi * k / 6
        y = -pw * math.cos(a)
        r = rp_out - (rp_out - rp_in) * math.sin(a)
        rows.append([(pad_at(p, r, y), (-math.cos(p) * math.sin(a), -math.cos(a), -math.sin(p) * math.sin(a))) for p in pads])
    strip(rows, CUSHION)
    # the sliders: from the band's ends down to the top of the cups
    for side in (-1, 1):
        phi = side * BAND_END
        top_x, top_z = BAND_X + BAND_R * math.cos(phi), BAND_R * math.sin(phi)
        bottom_x = CUP_X + CUP_A * 0.70
        sw, st = w * 0.62, 0.008
        z0, z1 = top_z - side * st / 2, top_z + side * st / 2
        corners = lambda x: [(x, -sw, z0), (x, sw, z0), (x, sw, z1), (x, -sw, z1)]
        lo, hi = corners(bottom_x), corners(top_x)
        faces = [((0, 1), (0.0, 0.0, -side)), ((1, 2), (0.0, 1.0, 0.0)), ((2, 3), (0.0, 0.0, side)), ((3, 0), (0.0, -1.0, 0.0))]
        for (i, j), n in faces:
            strip([[(lo[i], n), (lo[j], n)], [(hi[i], n), (hi[j], n)]], BODY)


def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), RW_VERSION) + payload


def dff():
    band()
    cup(1)
    cup(-1)
    nv, nt = len(verts), len(tris)
    assert nv < 65536, nv
    flags = 0x02 | 0x10 | 0x20 | 0x40  # positions, normals, light, modulate material colour
    geo = struct.pack("<Iiii", flags, nt, nv, 1)
    for a, b, c, m in tris:
        geo += struct.pack("<HHHH", b, a, m, c)  # vertex2, vertex1, material, vertex3
    geo += struct.pack("<ffff", CUP_X + 0.03, 0.0, 0.0, 0.2)  # bounding sphere
    geo += struct.pack("<II", 1, 1)
    for v in verts:
        geo += struct.pack("<fff", *v)
    for n in normals:
        geo += struct.pack("<fff", *n)

    def material(rgba):
        body = struct.pack("<I4BiI", 0, *rgba, 0, 0) + struct.pack("<fff", 1.0, 1.0, 1.0)
        return chunk(0x07, chunk(0x01, body) + chunk(0x03, b""))

    mats = chunk(0x08, chunk(0x01, struct.pack("<I", len(COLOURS)) + struct.pack(f"<{len(COLOURS)}i", *([-1] * len(COLOURS))))
                 + b"".join(material(c) for c in COLOURS))
    meshes = b""
    total = 0
    for m in range(len(COLOURS)):
        idx = [i for a, b, c, mm in tris if mm == m for i in (a, b, c)]
        meshes += struct.pack("<II", len(idx), m) + struct.pack(f"<{len(idx)}I", *idx)
        total += len(idx)
    binmesh = chunk(0x50E, struct.pack("<III", 0, len(COLOURS), total) + meshes)
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
        f"// {len(verts)} vertices, {len(tris)} triangles; materials: 0 the body, 1 the cushions, 2 the ring.\n"
        "#include <cstdint>\n\n"
        "namespace coop::headphones {\n\n"
        f"inline constexpr uint8_t kHeadsetDff[{len(data)}] = {{\n    " + ",\n    ".join(lines) + "\n};\n\n"
        "}  // namespace coop::headphones\n",
        encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(data)} bytes, {len(verts)} vertices, {len(tris)} triangles")


if __name__ == "__main__":
    main()
