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


def plaster_storey(seed):
    """the inside walls: cream plaster a storey high, a dark wood skirting at its foot (the bottom of the texture)"""
    img = stucco((238, 226, 196), seed)
    wood = planks((86, 54, 32), 1, seed + 1, gap=(60, 38, 24), grain=0.18, vertical=False)
    k = 24  # 0.42 m of the storey's 4.48
    img[-k:] = wood[-k:]
    img[-k - 2:-k] = (120, 84, 54)
    return img


def door(seed):
    """the doors' leaves: dark stained planks, two battens, iron studs"""
    img = planks((92, 56, 32), 5, seed, gap=(48, 30, 20), grain=0.22)
    yy, xx = np.mgrid[0:256, 0:256]
    for y in (40, 216):
        band = np.abs(yy - y) < 9
        img[band] = img[band] * 0.78
        for x in range(26, 256, 51):
            img[(np.hypot(xx - x, yy - y) < 4.5)] = (30, 28, 26)
    return img


def glow():
    """a lantern's glass: warm, its panes' iron bars"""
    img = tint((250, 214, 150), noise(64, 6, 7), 0.06)
    img[:, 30:34] = (30, 30, 30)
    img[30:34, :] = (30, 30, 30)
    img[:3], img[-3:], img[:, :3], img[:, -3:] = 30, 30, 30, 30
    return np.asarray(Image.fromarray(img.astype(np.uint8)).resize((64, 64)), float)


# ---------------------------------------------------------------- the decoration's (decor.py, exterior.py)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVu%s.ttf"


def _font(px, style="Sans-Bold"):
    from PIL import ImageFont
    try:
        return ImageFont.truetype(FONT % style, px)
    except OSError:
        return ImageFont.load_default(size=px)


def stone(seed):
    """cut limestone: courses of blocks of different lengths, cream and grey, mortar between"""
    size = 256
    rng = np.random.default_rng(seed)
    img = np.zeros((size, size, 3))
    n = fbm(size, seed=seed) * 0.7 + noise(size, 90, seed + 1) * 0.5
    rows = 5
    h = size // rows
    for r in range(rows):
        x = int(rng.integers(0, 40))
        while x < size + 60:
            w = int(rng.integers(50, 110))
            shade = rng.uniform(0.86, 1.06)
            base = np.array([214, 202, 178]) * shade * np.array([1, 1 + rng.uniform(-0.02, 0.02), 1 + rng.uniform(-0.05, 0.03)])
            xs = np.arange(x, x + w) % size
            img[r * h:(r + 1) * h][:, xs] = base
            img[r * h:(r + 1) * h][:, xs[:3]] = (150, 140, 124)
            x += w
        img[r * h:r * h + 3] = (150, 140, 124)
    return np.clip(img * (1 + 0.10 * n)[..., None], 0, 255)


def fabric(base, seed, weave=0.05):
    size = 128
    yy, xx = np.mgrid[0:size, 0:size]
    w = ((xx % 4 < 2) ^ (yy % 4 < 2)) * 2 - 1
    return tint(base, fbm(size, (2, 6, 18), (0.5, 0.3, 0.2), seed) * 0.8 + w * weave * 4, 0.06)


def leather(seed):
    size = 128
    n = fbm(size, (2, 5, 14, 40), (0.5, 0.3, 0.2, 0.15), seed)
    return tint((122, 70, 38), n, 0.22)


def rug(kind, seed):
    """the rugs, flat on the floors (their whole face one texture): drawn at 2x, shrunk"""
    S = 512
    im = Image.new("RGB", (S, S))
    d = ImageDraw.Draw(im)
    rng = np.random.default_rng(seed)
    if kind == "kilim":       # the living room's: rust red, navy diamond medallions, a patterned border
        d.rectangle([0, 0, S, S], fill=(176, 66, 40))
        d.rectangle([18, 18, S - 18, S - 18], outline=(42, 54, 92), width=22)
        d.rectangle([52, 52, S - 52, S - 52], outline=(214, 182, 120), width=8)
        for i in range(4):
            cy, cx = S / 2, 110 + i * 97
            for r, col in ((88, (40, 52, 90)), (64, (210, 178, 118)), (44, (40, 52, 90)), (22, (176, 66, 40))):
                d.polygon([(cx, cy - r * 1.8), (cx + r * 0.55, cy), (cx, cy + r * 1.8), (cx - r * 0.55, cy)], fill=col)
        for x in range(30, S, 24):
            d.polygon([(x, 22), (x + 8, 30), (x, 38), (x - 8, 30)], fill=(210, 178, 118))
            d.polygon([(x, S - 22), (x + 8, S - 30), (x, S - 38), (x - 8, S - 30)], fill=(210, 178, 118))
    elif kind == "red":       # the entry's: red, purple border, a stepped diamond
        d.rectangle([0, 0, S, S], fill=(150, 36, 40))
        d.rectangle([0, 0, S, S], outline=(88, 44, 82), width=56)
        d.rectangle([60, 60, S - 60, S - 60], outline=(210, 120, 70), width=6)
        c = S / 2
        for r, col in ((150, (88, 44, 82)), (110, (200, 72, 52)), (70, (230, 190, 150)), (30, (88, 44, 82))):
            d.polygon([(c, c - r), (c + r, c), (c, c + r), (c - r, c)], fill=col)
    elif kind == "stripe":    # the family room's: wide stripes, many colours
        cols = [(196, 92, 52), (226, 168, 70), (60, 64, 70), (156, 150, 120), (176, 60, 44), (214, 196, 150),
                (92, 70, 52), (200, 120, 60)]
        x = 0
        while x < S:
            w = int(rng.integers(30, 70))
            d.rectangle([x, 0, x + w, S], fill=cols[int(rng.integers(0, len(cols)))])
            x += w
        d.rectangle([0, 0, S, 16], fill=(220, 200, 160))
        d.rectangle([0, S - 16, S, S], fill=(220, 200, 160))
    elif kind == "navy":      # the kitchen's: navy, a floral border, a cream field of rosettes
        d.rectangle([0, 0, S, S], fill=(34, 42, 72))
        d.rectangle([70, 70, S - 70, S - 70], fill=(196, 176, 140))
        d.rectangle([100, 100, S - 100, S - 100], fill=(48, 58, 92))
        for i in range(40):
            x, y = rng.uniform(10, S - 10), rng.choice([rng.uniform(10, 60), rng.uniform(S - 60, S - 10)])
            if rng.random() < 0.5:
                x, y = y, x
            r = rng.uniform(6, 12)
            d.ellipse([x - r, y - r, x + r, y + r], fill=[(170, 60, 50), (210, 180, 120), (80, 110, 70)][i % 3])
        for x in range(130, S - 100, 60):
            for y in range(130, S - 100, 60):
                d.ellipse([x - 12, y - 12, x + 12, y + 12], fill=(200, 160, 110))
                d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=(160, 50, 44))
    else:                     # "cream": the piano's, beige with a soft red pattern
        d.rectangle([0, 0, S, S], fill=(214, 196, 164))
        d.rectangle([24, 24, S - 24, S - 24], outline=(170, 84, 64), width=14)
        for x in range(80, S - 60, 70):
            for y in range(80, S - 60, 70):
                d.polygon([(x, y - 22), (x + 16, y), (x, y + 22), (x - 16, y)], outline=(170, 84, 64), width=4)
    a = np.asarray(im.resize((256, 256), Image.LANCZOS), float)
    return np.clip(a * (1 + 0.07 * fbm(256, seed=seed)[..., None]), 0, 255)


def books(seed):
    """a shelf's row of books (u along the shelf, v up a book): spines of many colours and heights"""
    S = 256
    rng = np.random.default_rng(seed)
    img = np.full((S, S, 3), 40.0)
    x = 0
    cols = [(150, 40, 36), (40, 60, 110), (220, 210, 190), (30, 30, 34), (190, 150, 60), (70, 110, 80), (200, 90, 60),
            (240, 236, 226), (120, 80, 120)]
    while x < S:
        w = int(rng.integers(8, 22))
        h = int(rng.integers(150, 250))
        c = np.array(cols[int(rng.integers(0, len(cols)))]) * rng.uniform(0.85, 1.1)
        img[S - h:, x:x + w] = c
        img[S - h:, x:x + 1] *= 0.6
        for b in (S - h + 14, S - 30):  # the bands on a spine
            img[b:b + 4, x + 2:x + w - 2] = np.clip(c * 1.5 + 40, 0, 255)
        x += w
    return np.clip(img, 0, 255)


def poster(kind):
    """the framed posters (our own, in the style of the show's old jazz and car posters): drawn 3:4, made square"""
    W, H = 384, 512
    im = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(im)
    def text(y, s, px, col, style="Sans-Bold"):
        f = _font(px, style)
        w = d.textlength(s, font=f)
        d.text(((W - w) / 2, y), s, font=f, fill=col)
    def figure(cx, top, col, horn=True, k=1.0):  # a musician in a suit, his horn up
        P = lambda x, y: (cx + x * k, top + y * k)
        d.ellipse([*P(-26, 0), *P(26, 56)], fill=col)
        d.polygon([P(-60, 70), P(60, 70), P(50, 250), P(-50, 250)], fill=col)
        d.rectangle([*P(-44, 250), *P(-8, 330)], fill=col)
        d.rectangle([*P(8, 250), *P(44, 330)], fill=col)
        if horn:
            d.polygon([P(20, 40), P(120, -10), P(130, 20), P(26, 56)], fill=(214, 170, 60))
            d.ellipse([*P(110, -30), *P(150, 30)], fill=(214, 170, 60))
    if kind == 1:      # green and yellow, a big band
        d.rectangle([0, 0, W, H], fill=(36, 82, 58))
        d.rectangle([22, 22, W - 22, H - 22], fill=(232, 210, 120))
        text(40, "AMERICA'S GREATEST", 22, (36, 82, 58))
        text(70, "SWING", 72, (30, 30, 30), "Serif-Bold")
        figure(W / 2, 160, (30, 30, 30), horn=False, k=0.68)
        text(400, "AND HIS GRAND", 26, (36, 82, 58))
        text(430, "ORCHESTRA", 48, (150, 40, 36), "Serif-Bold")
    elif kind == 2:    # blue, a trumpet player
        d.rectangle([0, 0, W, H], fill=(70, 110, 170))
        d.rectangle([18, 18, W - 18, H - 18], outline=(240, 230, 200), width=6)
        text(36, "THE ROYAL ROOST", 26, (240, 230, 200))
        figure(W / 2 - 40, 110, (30, 26, 36), k=0.85)
        text(400, "BE-BOP", 64, (240, 196, 80), "Serif-Bold")
        text(470, "ALL NIGHT", 24, (240, 230, 200))
    elif kind == 3:    # orange and black, over the fireplace
        d.rectangle([0, 0, W, H], fill=(226, 120, 40))
        d.rectangle([30, 30, W - 30, H - 30], fill=(36, 30, 30))
        text(52, "SAVOY BALLROOM", 26, (226, 120, 40))
        figure(W / 2, 110, (226, 120, 40), horn=False, k=0.62)
        d.ellipse([W / 2 - 110, 330, W / 2 + 110, 380], fill=(150, 40, 36))
        text(400, "WEBB", 80, (240, 220, 160), "Serif-Bold")
    elif kind == 4:    # red, an old racing car (the kitchen's)
        d.rectangle([0, 0, W, H], fill=(178, 40, 36))
        d.rectangle([14, 14, W - 14, H - 14], outline=(20, 20, 20), width=8)
        text(40, "C'E UNA", 34, (240, 220, 160), "Serif-Bold")
        text(84, "CORSA", 70, (240, 220, 160), "Serif-Bold")
        d.polygon([(60, 330), (110, 270), (250, 260), (320, 300), (330, 340), (60, 350)], fill=(40, 70, 140))
        for x in (110, 280):
            d.ellipse([x - 32, 320, x + 32, 384], fill=(20, 20, 20))
            d.ellipse([x - 12, 340, x + 12, 364], fill=(200, 200, 200))
        d.ellipse([170, 240, 210, 280], fill=(236, 210, 170))
        text(430, "NON SI PASSA", 30, (240, 220, 160))
    else:              # a print: a blue square on a white mat
        d.rectangle([0, 0, W, H], fill=(236, 234, 228))
        d.rectangle([70, 110, W - 70, H - 110], fill=(84, 96, 196) if kind == 5 else (60, 64, 120))
        d.rectangle([70, 110, W - 70, H - 110], outline=(30, 30, 40), width=3)
    a = np.asarray(im.resize((256, 256), Image.LANCZOS), float)
    return np.clip(a * (1 + 0.04 * noise(256, 40, kind)[..., None]), 0, 255)


def cabinet(seed):
    """a kitchen cabinet's door: white frame, a tan panel, a dark handle (one door a texture)"""
    S = 128
    img = tint((238, 236, 228), noise(S, 30, seed), 0.02)
    img[16:-16, 16:-16] = tint((196, 168, 118), noise(S, 20, seed + 1), 0.04)[16:-16, 16:-16]
    img[12:16, 12:-12] = img[-16:-12, 12:-12] = (210, 208, 200)
    img[12:-12, 12:16] = img[12:-12, -16:-12] = (210, 208, 200)
    img[50:78, 104:110] = (40, 40, 42)
    img[:2], img[-2:], img[:, :2], img[:, -2:] = 150, 150, 150, 150
    return img


def shutter(seed):
    """a wooden shutter: dark green-brown louvres in a frame"""
    S = 128
    img = tint((92, 62, 40), noise(S, 30, seed), 0.08)
    yy = np.mgrid[0:S, 0:S][0]
    lou = (yy % 10) / 10
    img[12:-12, 12:-12] *= (0.65 + 0.4 * lou[12:-12, 12:-12])[..., None]
    img[60:68, 12:-12] *= 0.7
    return np.clip(img, 0, 255)


def leaves(seed, flowers=None):
    """a climber's or a bush's leaves, flowers among them (bougainvillea's pink)"""
    S = 128
    rng = np.random.default_rng(seed)
    n = fbm(S, (4, 10, 30), (0.5, 0.3, 0.3), seed)
    img = tint((62, 96, 44), n, 0.5)
    if flowers:
        for _ in range(60):
            x, y = rng.integers(0, S, 2)
            r = rng.integers(2, 5)
            yy, xx = np.ogrid[0:S, 0:S]
            m = ((xx - x) % S) ** 2 + ((yy - y) % S) ** 2 < r * r
            img[m] = np.array(flowers) * rng.uniform(0.8, 1.1)
    return np.clip(img, 0, 255)


def make():
    """{name: (rgb array, size)} in the order the header gets them"""
    return {
        "nch_stucco": stucco((230, 206, 162), 11),      # outside: warm cream stucco, as the reference pictures
        "nch_plaster": plaster_storey(21),              # inside: cream, a storey high, its skirting
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
        "nch_paving": tiles((168, 164, 156), 2, (118, 114, 108), 121, jitter=0.06),  # the lot: grey concrete slabs
        "nch_timber": planks((132, 114, 92), 6, 131, gap=(60, 52, 44), grain=0.18),
        "nch_green": planks((62, 84, 58), 6, 161, gap=(36, 48, 34), grain=0.12),
        "nch_cap": tiles((176, 86, 54), 2, (140, 72, 48), 171, jitter=0.08),
        "nch_door": door(181),
        "nch_glow": glow(),
        "nch_joist": planks((88, 72, 58), 5, 191, gap=(36, 30, 26), grain=0.18),
        # the decoration (decor.py, exterior.py)
        "nch_stone": stone(201),
        "nch_white": tint((236, 234, 226), noise(128, 20, 211), 0.02),
        "nch_cream": stucco((238, 228, 202), 212)[:128, :128],
        "nch_sofa": fabric((70, 96, 62), 221),             # the living room's green sofa
        "nch_leather": leather(222),
        "nch_beige": fabric((192, 178, 140), 223),
        "nch_grey": fabric((200, 200, 196), 224),          # the family room's sofa
        "nch_ottoman": fabric((176, 58, 42), 225, weave=0.12),
        "nch_cane": fabric((168, 136, 96), 226, weave=0.25),
        "nch_rug1": rug("kilim", 231), "nch_rug2": rug("red", 232), "nch_rug3": rug("stripe", 233),
        "nch_rug4": rug("navy", 234), "nch_rug5": rug("cream", 235),
        "nch_books": books(241),
        "nch_post1": poster(1), "nch_post2": poster(2), "nch_post3": poster(3), "nch_post4": poster(4),
        "nch_print1": poster(5), "nch_print2": poster(6),
        "nch_cabinet": cabinet(251),
        "nch_ytile": tiles((226, 180, 56), 8, (236, 226, 200), 252, jitter=0.05),
        "nch_steel": tint((186, 188, 190), noise(128, 60, 253) * 0.3 + noise(128, 2, 254), 0.05),
        "nch_copper": tint((186, 112, 80), fbm(128, (2, 8), (0.6, 0.4), 255), 0.12),
        "nch_bluegrey": tint((112, 128, 146), noise(128, 10, 256), 0.06),
        "nch_lightwood": planks((200, 166, 116), 4, 257, gap=(150, 120, 86), grain=0.12),
        "nch_shutter": shutter(261),
        "nch_leaves": leaves(271),
        "nch_bloom": leaves(272, flowers=(214, 50, 120)),
        "nch_walk": tiles((176, 172, 164), 2, (130, 126, 120), 291, jitter=0.04),
        "nch_soil": tint((84, 62, 44), fbm(128, (3, 12, 40), (0.5, 0.3, 0.3), 281), 0.25),
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
