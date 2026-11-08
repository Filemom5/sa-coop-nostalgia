"""The house for a map editor (Ariane, docs/casa-charlie.md): a modloader folder with the house's models and textures in
the game's files' place, so the editor (and the game) shows the house as the mod does; the furniture's placements come
from the game (/editorcasa writes casa_charlie.ipl there).

    python editor.py CharlieData.h extrair_dir out_dir

extrair_dir: the game's files /extrair wrote (bev_law2.txd, welod_law2.txd, santahouse02_law2.dff)."""
import re
import struct
import sys
from pathlib import Path

import numpy as np

import rwedit
import textures

VER = 0x1803FFFF
EAST_CUT = 308.0 - 307.265625  # the east block: what is west of this x (its own axes) goes, as the mod does


def arrays(header):
    text = Path(header).read_text()
    out = {}
    for m in re.finditer(r"(\w+)\[\d+\] = \{([^}]*)\}", text):
        if m.group(1).startswith("k") and "0x" in m.group(2)[:20]:
            out[m.group(1)] = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", m.group(2)))
    sizes = {n: int(s) for n, s in re.findall(r'\{"(\w+)", (\d+), kTex_', text)}
    return out, sizes


def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), VER) + payload


def native(name, rgb):
    """a texture native (D3D9, DXT1, its mipmaps down to 4x4), as the game's own"""
    levels, w = [], rgb.shape[0]
    while True:
        levels.append(textures.dxt1(np.clip(np.round(rgb), 0, 255).astype(np.uint8)))
        if rgb.shape[0] <= 4:
            break
        rgb = rgb.reshape(rgb.shape[0] // 2, 2, rgb.shape[1] // 2, 2, 3).mean((1, 3))
    head = struct.pack("<II", 9, 0x1106) + name.encode().ljust(32, b"\0") + b"".ljust(32, b"\0")
    head += struct.pack("<I4sHHBBBB", 0x0200 | 0x8000, b"DXT1", w, w, 16, len(levels), 4, 8)
    for lv in levels:
        head += struct.pack("<I", len(lv)) + lv
    return chunk(0x15, chunk(0x01, head) + chunk(0x03, b""))


def add_textures(txd, ours):
    """the game's dictionary txd with ours added ({name: rgb})"""
    count, device = struct.unpack_from("<HH", txd, 24)
    _, size, _ = struct.unpack_from("<III", txd, 0)
    body = txd[24 + 4:12 + size]
    p, natives = 0, b""
    while p + 12 <= len(body):
        t, s, _ = struct.unpack_from("<III", body, p)
        if t != 0x15:
            break
        natives += body[p:p + 12 + s]
        p += 12 + s
    have = {m.lower() for m in re.findall(rb"\x09\x00\x00\x00.{4}([\w]+)", natives, re.S)}
    added = [native(n, rgb) for n, rgb in ours.items() if n.encode().lower() not in have]
    head = chunk(0x01, struct.pack("<HH", count + len(added), device))
    return chunk(0x16, head + natives + b"".join(added) + chunk(0x03, b""))


def undxt(data, size):
    return textures.undxt1(data, size, size).astype(np.float64)


def main(header, extrair, out):
    data, sizes = arrays(header)
    src, out = Path(extrair), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "santahouse04_law2.dff").write_bytes(data["kHouseDff"])
    (out / "LODtahouse04_law2.dff").write_bytes(data["kLodDff"])
    near = {n: undxt(data["kTex_" + n], s) for n, s in sizes.items()}
    far = {}
    for n, rgb in near.items():
        while rgb.shape[0] > 64:
            rgb = rgb.reshape(rgb.shape[0] // 2, 2, rgb.shape[1] // 2, 2, 3).mean((1, 3))
        far[n] = rgb
    (out / "bev_law2.txd").write_bytes(add_textures((src / "bev_law2.txd").read_bytes(), near))
    (out / "welod_law2.txd").write_bytes(add_textures((src / "welod_law2.txd").read_bytes(), far))
    east = rwedit.cut_dff((src / "santahouse02_law2.dff").read_bytes(), (0, EAST_CUT, 1))
    (out / "santahouse02_law2.dff").write_bytes(east)
    for f in sorted(out.iterdir()):
        print("%-24s %8d" % (f.name, f.stat().st_size))


if __name__ == "__main__":
    main(*sys.argv[1:])
