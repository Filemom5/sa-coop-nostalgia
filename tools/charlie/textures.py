"""The house's textures (tools/charlie/build.py puts them in CharlieData.h), made here and not taken from anywhere:
stucco and plaster, terracotta tiles, the roof's clay tiles, the ceiling's planks and the beams' dark wood, the stairs'
Spanish tiles, sand, the garage's siding. Seamless (they repeat), 256 x 256 (the tiles' risers 128 x 128).

In the game the mod makes them as the house's texture dictionaries load (features/charlie), from DXT1 (dxt1()).

python3 textures.py: out/textures.png, all of them side by side, to look at."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

RNG = np.random.default_rng(1975)


def noise(size, scale, seed=None):
    """seamless noise of features ~size/scale px (white noise filtered in frequency: it wraps), -1..1"""
    rng = np.random.default_rng(seed) if seed is not None else RNG
    w = rng.standard_normal((size, size))
    f = np.fft.fftfreq(size)
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx ** 2 + fy ** 2) * size / scale
    spec = np.fft.fft2(w) * np.exp(-r ** 2)
    n = np.real(np.fft.ifft2(spec))
    return n / (np.abs(n).max() + 1e-9)


def fbm(size, scales=(2, 6, 18, 60), weights=(0.5, 0.3, 0.15, 0.08), seed=None):
    rng = np.random.default_rng(seed)
    return sum(w * noise(size, s, int(rng.integers(1 << 30))) for s, w in zip(scales, weights))


def tint(base, n, amount):
    """a colour (rgb) varied by noise n (-1..1) by amount (0..1)"""
    return np.clip(np.array(base, float)[None, None, :] * (1 + amount * n[..., None]), 0, 255)


def stucco(base, seed):
    n = fbm(256, seed=seed)
    grain = noise(256, 120, seed + 1) * 0.5 + noise(256, 60, seed + 2) * 0.5
    return tint(base, n * 0.9 + grain * 0.5, 0.09)


def tiles(base, n_tiles, grout, seed, jitter=0.12, bevel=True):
    """square tiles n_tiles x n_tiles, each its own shade, grout lines between"""
    size = 256
    rng = np.random.default_rng(seed)
    t = size // n_tiles
    img = np.zeros((size, size, 3))
    n = fbm(size, seed=seed) * 0.6 + noise(size, 80, seed + 3) * 0.4
    yy, xx = np.mgrid[0:size, 0:size]
    for i in range(n_tiles):
        for j in range(n_tiles):
            shade = 1 + rng.uniform(-jitter, jitter)
            hue = np.array([1 + rng.uniform(-0.04, 0.04), 1 + rng.uniform(-0.06, 0.06), 1 + rng.uniform(-0.08, 0.08)])
            img[i * t:(i + 1) * t, j * t:(j + 1) * t] = np.array(base) * shade * hue
    img *= (1 + 0.08 * n)[..., None]
    lx, ly = xx % t, yy % t
    g = 3
    edge = np.minimum(np.minimum(lx, t - 1 - lx), np.minimum(ly, t - 1 - ly))
    if bevel:
        img *= np.clip(0.85 + 0.15 * edge / 6, 0.85, 1.0)[..., None]
    img[edge < g] = np.array(grout) * (1 + 0.05 * n[edge < g])[:, None]
    return np.clip(img, 0, 255)


def roof_tiles(seed):
    """clay barrel tiles: rows down the slope, each tile a half-pipe (light in the middle), rows overlapping"""
    size = 256
    rng = np.random.default_rng(seed)
    cols, rows = 8, 6
    w, h = size / cols, size / rows
    yy, xx = np.mgrid[0:size, 0:size].astype(float)
    img = np.zeros((size, size, 3))
    base = np.array([176, 92, 58])
    row = np.floor(yy / h)
    off = (row % 2) * w / 2
    u = ((xx + off) % w) / w           # across a tile
    v = (yy % h) / h                   # down it
    col = np.floor((xx + off) / w) % cols
    shade = np.cos((u - 0.5) * np.pi) ** 0.6
    lip = np.clip((v - 0.82) / 0.18, 0, 1)           # the next row's edge over it: shadow
    var = np.zeros((size, size))
    for r in range(rows):
        for c in range(cols):
            var[(row == r) & (col == c)] = rng.uniform(-0.12, 0.12)
    n = fbm(size, seed=seed) * 0.5
    light = (0.55 + 0.5 * shade) * (1 - 0.45 * lip) * (1 + var + 0.08 * n)
    img = base[None, None, :] * light[..., None]
    img[..., 1] *= 1 + 0.08 * var
    return np.clip(img, 0, 255)


def planks(base, n_planks, seed, gap=(60, 40, 25), grain=0.16, vertical=True):
    size = 256
    rng = np.random.default_rng(seed)
    p = size // n_planks
    yy, xx = np.mgrid[0:size, 0:size]
    across, along = (xx, yy) if vertical else (yy, xx)
    idx = across // p
    # grain: noise stretched along the plank
    g = np.zeros((size, size))
    for k in range(n_planks):
        n = noise(size, 4, int(rng.integers(1 << 30))) * 0.6 + noise(size, 30, int(rng.integers(1 << 30))) * 0.4
        lines = np.sin((across + 14 * noise(size, 3, int(rng.integers(1 << 30))) * p) * (2 * np.pi / max(p / 3, 1)))
        m = idx == k
        g[m] = (0.6 * n + 0.25 * lines)[m] + rng.uniform(-0.6, 0.6)
    img = tint(base, g, grain)
    seam = (across % p) < 2
    img[seam] = gap
    # plank ends, staggered
    for k in range(n_planks):
        end = int(rng.integers(0, size))
        m = (idx == k) & (np.abs(along - end) < 1.5)
        img[m] = gap
    return np.clip(img, 0, 255)


def talavera(seed):
    """the stairs' risers: cream tiles with a black eight-petal flower, red in its heart and between its petals, and
    corner motifs that make rings where four tiles meet (as in the show). 4 x 4 tiles; drawn at 4x and shrunk"""
    k, n = 4, 4
    size = 128 * k
    t = size // n
    cream, dark, red, blue = (228, 216, 188), (30, 32, 36), (170, 52, 42), (48, 74, 122)
    im = Image.new("RGB", (size, size), cream)
    d = ImageDraw.Draw(im)
    for i in range(n):
        for j in range(n):
            x0, y0 = j * t, i * t
            cx, cy = x0 + t / 2, y0 + t / 2
            for qx, qy in ((x0, y0), (x0 + t, y0), (x0, y0 + t), (x0 + t, y0 + t)):  # the corners: quarter rings
                r = t * 0.22
                d.ellipse([qx - r, qy - r, qx + r, qy + r], fill=dark)
                r = t * 0.15
                d.ellipse([qx - r, qy - r, qx + r, qy + r], fill=cream)
                r = t * 0.07
                d.ellipse([qx - r, qy - r, qx + r, qy + r], fill=red)
            for p_ in range(8):  # petals
                a = p_ * np.pi / 4
                pts = []
                for s_ in np.linspace(0, 1, 9):
                    rr = t * (0.08 + 0.26 * s_)
                    wdt = np.sin(np.pi * s_) * 0.32
                    pts.append((cx + np.cos(a + wdt) * rr, cy + np.sin(a + wdt) * rr))
                for s_ in np.linspace(1, 0, 9):
                    rr = t * (0.08 + 0.26 * s_)
                    wdt = np.sin(np.pi * s_) * 0.32
                    pts.append((cx + np.cos(a - wdt) * rr, cy + np.sin(a - wdt) * rr))
                d.polygon(pts, fill=dark)
                b_ = a + np.pi / 8  # red between the petals
                d.ellipse([cx + np.cos(b_) * t * 0.27 - t * 0.035, cy + np.sin(b_) * t * 0.27 - t * 0.035,
                           cx + np.cos(b_) * t * 0.27 + t * 0.035, cy + np.sin(b_) * t * 0.27 + t * 0.035], fill=red)
            r = t * 0.085
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=red)
            r = t * 0.035
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=cream)
            for (ax, ay) in ((0, -1), (0, 1), (-1, 0), (1, 0)):  # small blue marks at the edges' middles
                mx, my = cx + ax * t * 0.46, cy + ay * t * 0.46
                d.ellipse([mx - t * 0.03, my - t * 0.03, mx + t * 0.03, my + t * 0.03], fill=blue)
            d.rectangle([x0, y0, x0 + t - 1, y0 + t - 1], outline=(160, 150, 128), width=k)
    im = im.resize((128, 128), Image.LANCZOS)
    a = np.asarray(im, float)
    return np.clip(a * (1 + 0.04 * noise(128, 30, seed)[..., None]), 0, 255)


def siding(seed):
    size = 256
    yy = np.mgrid[0:size, 0:size][0]
    board = 32
    v = (yy % board) / board
    n = fbm(size, seed=seed)
    img = tint((172, 138, 104), n, 0.08)
    img *= (0.78 + 0.22 * v)[..., None]   # each board's lower edge over the next: shadow at its top
    return np.clip(img, 0, 255)


def lacquer(seed):
    """the piano's black: a little light across it, as if glossy"""
    size = 64
    yy, xx = np.mgrid[0:size, 0:size] / size
    v = 18 + 26 * np.exp(-((xx - 0.3) ** 2) / 0.02) + 6 * noise(size, 20, seed)
    return np.clip(np.repeat(v[..., None], 3, -1) * np.array([1.0, 1.0, 1.08]), 0, 255)


def keys():
    """one octave of a keyboard, across (u), the keys' length down (v): 7 white keys, 5 black (to 0.62 of it)"""
    w, h = 112, 64
    im = np.full((h, w, 3), 236.0)
    kw = w / 7
    for i in range(1, 7):
        im[:, int(i * kw) - 1:int(i * kw) + 1] = 120
    for c in (1, 2, 4, 5, 6):  # the black keys sit across the gaps before keys 1, 2, 4, 5, 6
        x = c * kw
        im[:int(h * 0.62), int(x - kw * 0.3):int(x + kw * 0.3)] = 22
    out = np.zeros((128, 128, 3))
    big = np.asarray(Image.fromarray(im.astype(np.uint8)).resize((128, 128), Image.LANCZOS), float)
    return big


def make():
    """{name: (rgb array, size)} in the order the header gets them"""
    return {
        "nch_stucco": stucco((222, 200, 160), 11),      # outside: beige stucco, as the reference pictures
        "nch_plaster": stucco((238, 226, 196), 21),     # inside: cream
        "nch_tile": tiles((132, 64, 44), 4, (78, 58, 48), 31, jitter=0.10),  # floors: dark terracotta tiles
        "nch_roof": roof_tiles(41),
        "nch_ceiling": planks((206, 170, 118), 8, 51, grain=0.10),
        "nch_wood": planks((104, 62, 34), 4, 61, gap=(70, 40, 22), grain=0.20),
        "nch_floorwood": planks((150, 96, 56), 6, 71, grain=0.16, vertical=False),
        "nch_darkwood": planks((74, 50, 38), 6, 72, gap=(40, 28, 22), grain=0.14, vertical=False),
        "nch_riser": talavera(81),
        "nch_sand": tint((206, 182, 136), fbm(256, seed=91) * 0.6 + noise(256, 100, 92) * 0.6, 0.08),
        "nch_siding": siding(101),
        "nch_black": np.asarray(Image.fromarray(lacquer(111).astype(np.uint8)).resize((128, 128)), float),
        "nch_keys": keys(),
        "nch_paving": tiles((200, 182, 156), 2, (150, 136, 118), 121, jitter=0.07),
        "nch_timber": planks((132, 114, 92), 6, 131, gap=(60, 52, 44), grain=0.18),
        "nch_under": tint((34, 30, 26), noise(256, 30, 141), 0.25),
        "nch_white": tint((236, 234, 226), fbm(256, seed=151), 0.04),
        "nch_green": planks((62, 84, 58), 6, 161, gap=(36, 48, 34), grain=0.12),
        "nch_cap": tiles((176, 86, 54), 2, (140, 72, 48), 171, jitter=0.08),
    }


# ---------------------------------------------------------------- DXT1
def _565(c):
    c = np.clip(np.round(c), 0, 255).astype(np.int64)
    return ((c[..., 0] >> 3) << 11) | ((c[..., 1] >> 2) << 5) | (c[..., 2] >> 3)


def _from565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(float)


def dxt1(rgb):
    """rgb (h, w, 3) -> DXT1 bytes (opaque blocks: color0 > color1), blocks row by row"""
    h, w, _ = rgb.shape
    blocks = rgb.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(float)
    mean = blocks.mean(1, keepdims=True)
    d = blocks - mean
    cov = np.einsum("bij,bik->bjk", d, d)
    axis = np.ones((len(blocks), 3)) / np.sqrt(3)
    for _ in range(8):  # the main axis (power iteration)
        axis = np.einsum("bjk,bk->bj", cov, axis)
        axis /= np.linalg.norm(axis, axis=1, keepdims=True) + 1e-9
    t = np.einsum("bij,bj->bi", d, axis)
    lo = mean[:, 0] + axis * t.min(1, keepdims=True)
    hi = mean[:, 0] + axis * t.max(1, keepdims=True)
    c0, c1 = np.maximum(_565(hi), _565(lo)), np.minimum(_565(hi), _565(lo))  # color0 > color1: 4 colours, no alpha
    same = c0 == c1                                                            # a flat block: index 0 everywhere
    p0, p1 = _from565(c0), _from565(c1)
    pal = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1)          # blocks x 4 x 3
    dist = ((blocks[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)       # blocks x 16 x 4
    idx = dist.argmin(-1)
    idx[same] = 0
    bits = (idx.astype(np.uint32) << (2 * np.arange(16, dtype=np.uint32))).sum(1).astype(np.uint32)
    out = np.zeros((len(blocks), 8), np.uint8)
    out[:, 0:2] = c0.astype("<u2").view(np.uint8).reshape(-1, 2)
    out[:, 2:4] = c1.astype("<u2").view(np.uint8).reshape(-1, 2)
    out[:, 4:8] = bits.astype("<u4").view(np.uint8).reshape(-1, 4)
    return out.tobytes()


def undxt1(data, w, h):
    """the other way (to look at what the game will get)"""
    b = np.frombuffer(data, np.uint8).reshape(-1, 8)
    c0 = b[:, 0].astype(np.int64) | (b[:, 1].astype(np.int64) << 8)
    c1 = b[:, 2].astype(np.int64) | (b[:, 3].astype(np.int64) << 8)
    bits = b[:, 4:8].copy().view("<u4")[:, 0].astype(np.int64)
    p0, p1 = _from565(c0), _from565(c1)
    pal = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1)
    idx = (bits[:, None] >> (2 * np.arange(16))) & 3
    px = np.take_along_axis(pal, idx[..., None].repeat(3, -1), 1)
    return px.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)


if __name__ == "__main__":
    tex = make()
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    tiles_ = []
    for name, rgb in tex.items():
        back = undxt1(dxt1(rgb), rgb.shape[1], rgb.shape[0])
        im = np.zeros((256, 256, 3))
        im[:] = np.tile(back, (256 // back.shape[0], 256 // back.shape[1], 1))
        tiles_.append(im)
    while len(tiles_) % 6:
        tiles_.append(np.zeros((256, 256, 3)))
    sheet = np.concatenate([np.concatenate(tiles_[i:i + 6], 1) for i in range(0, len(tiles_), 6)], 0)
    Image.fromarray(sheet.astype(np.uint8)).save(out / "textures.png")
    print("out/textures.png:", ", ".join(tex))
