"""Writes the jackets' data for the mod (src/client/features/clothes/ClothesData.h): for each texture of the game's
(player.img: coach.txd, coachsemi.txd, hoodjackbeige.txd) the CRC-32 of its original pixels and our design (RGBA
1024 x 1024, run-length coded: count, R, G, B, A). No pixel of the game's is in the data. The mod (JacketPaint.cpp,
the same sums as here):

  * as the texture loads (256 x 256): every 4th texel of the design (small()), shaded by the original's folds
    (compose());
  * on the dressed player's texture (the game's builds it from that one, 256 x 256): that scaled up 4 times
    (bilinear), the design over it shaded by the same folds scaled up (compose_hd()), and its mipmaps (mips()).

python3 export.py <ClothesData.h>"""
import struct
import sys
import zlib

import numpy as np
from PIL import Image

sys.path.insert(0, "../sign")
sys.path.insert(0, ".")
import rw

TEXTURES = (("coach", "COACHA", "Corta-vento Matunas"), ("coachsemi", "COACHB", "Corta-vento Nostalgia"),
            ("hoodjackbeige", "HODJCK1", "Jaqueta Sunset Nostalgia"))
SMALL, BIG = 256, 1024


def original_level(name):
    raw = open("orig/%s.txd" % name, "rb").read()
    to, ts, ver = rw.find(raw, 0, len(raw), rw.CHUNK_TEXDICT)
    for t, o, s, v in rw.chunks(raw, to, to + ts):
        if t == rw.CHUNK_TEXNATIVE:
            so, ss, _ = rw.find(raw, o, o + s, rw.CHUNK_STRUCT)
            size = struct.unpack_from("<I", raw, so + 88)[0]
            return raw[so + 92:so + 92 + size]  # 256 x 256, B G R X


def small(design):
    """the design at the game's 256 x 256: every 4th texel (the 3rd of each 4)"""
    return np.ascontiguousarray(design[2::4, 2::4])


def rle(px):
    out = bytearray()
    i, n = 0, len(px)
    while i < n:
        j = i
        while j + 1 < n and j - i < 254 and (px[j + 1] == px[i]).all():
            j += 1
        out.append(j - i + 1)
        out += px[i].astype(np.uint8).tobytes()
        i = j + 1
    return bytes(out)


def median5(lum):
    """5 x 5 median, the edges clamped (as JacketPaint.cpp)"""
    h, w = lum.shape
    pad = np.pad(lum, 2, mode="edge")
    stack = np.stack([pad[dy:dy + h, dx:dx + w] for dy in range(5) for dx in range(5)], -1)
    return np.sort(stack, -1)[..., 12]


def shade(design, level):
    """the original's folds: each texel's shade (0.55 .. 1.2, float32) against the mean under the design"""
    o = np.frombuffer(level, np.uint8).reshape(SMALL, SMALL, 4)
    r, g, b = o[..., 2].astype(np.float32), o[..., 1].astype(np.float32), o[..., 0].astype(np.float32)
    lum = (r * np.float32(0.299) + g * np.float32(0.587) + b * np.float32(0.114)).astype(np.uint8)
    med = median5(lum).astype(np.float32)
    mask = design[..., 3] > 0
    mean = np.float32(med[mask].astype(np.float64).sum() / max(mask.sum(), 1))
    ratio = med / max(mean, np.float32(1))
    return np.where(ratio > 1.6, np.float32(1), np.clip(ratio, np.float32(0.55), np.float32(1.2))).astype(np.float32)


def compose(design, level):
    """design RGBA (256, 256, 4); level: the original's B G R X bytes -> the new B G R X bytes"""
    o = np.frombuffer(level, np.uint8).reshape(SMALL, SMALL, 4)
    ratio = shade(design, level)
    mask = design[..., 3] > 0
    k = design[..., 3].astype(np.float32) / 255
    out = o.copy()
    for c, idx in ((0, 2), (1, 1), (2, 0)):  # design R G B -> bytes 2 1 0
        v = design[..., c].astype(np.float32) * (1 + (ratio - 1) * k)
        v = np.clip(v, 0, 255).astype(np.uint8)
        out[..., idx] = np.where(mask, v, o[..., idx])
    out[..., 3] = np.where(mask, 255, o[..., 3])
    return out.tobytes()


def ratio(design, level):
    """the shades as the HD texture takes them: 1024 for 1.0 (uint16)"""
    return np.floor(shade(design, level) * np.float32(1024) + np.float32(0.5)).astype(np.int64)


def up4(a):
    """4 times as big, bilinear (texel centres, the edges clamped), in whole numbers: the weights are eighths"""
    n = a.shape[0]
    j = np.arange(4 * n)
    i = j >> 2
    lo = np.where((j & 3) < 2, i - 1, i)
    wlo = np.array([3, 1, 7, 5])[j & 3]
    lo_c, hi_c = np.clip(lo, 0, n - 1), np.clip(lo + 1, 0, n - 1)
    rows = a[lo_c] * wlo[:, None] + a[hi_c] * (8 - wlo)[:, None] if a.ndim == 2 else \
        a[lo_c] * wlo[:, None, None] + a[hi_c] * (8 - wlo)[:, None, None]
    out = rows[:, lo_c] * (wlo[None, :, None] if a.ndim == 3 else wlo[None, :]) + \
        rows[:, hi_c] * ((8 - wlo)[None, :, None] if a.ndim == 3 else (8 - wlo)[None, :])
    return (out + 32) >> 6


def compose_hd(design, base, r16):
    """design RGBA (1024, 1024, 4); base: the dressed player's texture (256 x 256 B G R X bytes); r16: ratio() ->
    the HD texture's B G R X bytes"""
    b = np.frombuffer(base, np.uint8).reshape(SMALL, SMALL, 4).astype(np.int64)
    out = up4(b[..., :3])
    rup = up4(r16)
    a = design[..., 3].astype(np.int64)
    f = 1024 * 255 + (rup - 1024) * a
    mask = a > 0
    for c, idx in ((0, 2), (1, 1), (2, 0)):
        v = np.clip((design[..., c].astype(np.int64) * f + 130560) // 261120, 0, 255)
        out[..., idx] = np.where(mask, v, out[..., idx])
    res = np.empty((BIG, BIG, 4), np.uint8)
    res[..., :3] = out
    res[..., 3] = 255
    return res.tobytes()


def mips(level0, size=BIG):
    """the smaller levels: each texel the mean of 4 (rounded)"""
    out = []
    a = np.frombuffer(level0, np.uint8).reshape(size, size, 4).astype(np.int64)
    while a.shape[0] > 1:
        a = (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2] + 2) >> 2
        out.append(a.astype(np.uint8).tobytes())
    return out


def c_array(name, data):
    lines = ["inline constexpr unsigned char %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join("0x%02x" % x for x in data[i:i + 24]) + ",")
    lines.append("};")
    return "\n".join(lines)


def ident(name):
    return "".join(p.capitalize() for p in name.split("_")).capitalize()


def main(path):
    text = ["// Generated by tools/clothes/export.py: do not edit. Our designs of the group's jackets (1024 x 1024,",
            "// run-length coded RGBA) and the CRC-32 of the game's pixels they go over.",
            "#pragma once", "#include <cstdint>", "", "namespace coop::clothes::data {", ""]
    for name, key, title in TEXTURES:
        d = np.array(Image.open("out/%s.png" % name).convert("RGBA"))
        level = original_level(name)
        text.append("inline constexpr uint32_t k%sCrc = 0x%08x;" % (ident(name), zlib.crc32(level)))
        text.append(c_array("k%sDesign" % ident(name), rle(d.reshape(-1, 4))))
        text.append("")
        ours = compose(small(d), level)
        open("out/%s_ours.bgra" % name, "wb").write(ours)
        hd = compose_hd(d, ours, ratio(small(d), level))
        open("out/%s_hd.bgra" % name, "wb").write(hd + b"".join(mips(hd)))
    text += ["}  // namespace coop::clothes::data", ""]
    open(path, "w").write("\n".join(text))


if __name__ == "__main__":
    main(sys.argv[1])
