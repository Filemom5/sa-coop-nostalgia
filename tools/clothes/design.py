"""The group's jackets, painted in the texture's space from the 3D place of each texel (model space: x front, y the
wearer's left, z up). A design is RGBA: A = 0 keeps the game's pixel (hands, a shirt under it), else the colour shaded
by the game's own folds (its brightness, smoothed) with that strength (A / 255).

  * SubUrban's "coach" jacket: Grn Windbreaker -> lilac "Matunas", Blk Windbreaker -> black and purple "Nostalgia";
  * Victim's "hoodjack" (the Beige Hooded Jacket) -> "Sunset": indigo to red, a synthwave sunset with palms on the back
    (after a "Los Angeles" jacket the group liked).

Drawn at 1024 x 1024: the mod puts that on the dressed player's texture (the game's 256 x 256 one scaled up under it),
and every 4th texel of it on the game's own texture as it loads (export.py).

python3 design.py -> out/<texture>.png (designs), out/*_final.png (as the game would show them), out/*_print.png;
export.py writes them for the mod. orig/: the models and textures from the game's models/player.img (README.md)"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, "../sign")
sys.path.insert(0, ".")
import rw
from uvmap import uv_map

LOGO = "logo.png"          # the group's logo (Diet Crystal Nostalgia Tears), 1024 x 1024
LETTERING = "matunas.png"  # "Matunas" as on the back of the real jacket (from its photo), as alpha
FONT = "fonts/Anton-Regular.ttf"  # NOSTALGIA's letters (condensed and heavy, as on the logo; SIL Open Font License)
SIZE = 1024  # the design; the game's texture is 256 x 256


class Model:
    """a jacket's texels: their 3D place and normal, the parts by place"""

    def __init__(self, dff, sleeve_rows):
        P, N = uv_map("orig/%s.dff" % dff, SIZE)
        self.ok = ~np.isnan(P[:, :, 0])
        self.x, self.y, self.z = (np.nan_to_num(P[:, :, k]) for k in range(3))
        self.nx = np.nan_to_num(N[:, :, 0])
        rows = np.arange(SIZE)[:, None] * np.ones((1, SIZE))
        self.sleeve = self.ok & (rows < sleeve_rows * SIZE / 256)  # the sleeves' strips at the texture's top
        self.body = self.ok & ~self.sleeve
        self.front = self.body & (self.nx > 0.25) & (self.x > 0.02)
        self.back = self.body & (self.nx < -0.25) & (self.x < 0.0)


COACH = Model("coach", 100)
HOOD = Model("hoodjack", 76)


def logo_patch(shape):
    """the group's logo as a patch: round or rounded square, RGBA"""
    im = Image.open(LOGO).convert("RGB").resize((1024, 1024), Image.LANCZOS)
    mask = Image.new("L", (1024, 1024), 0)
    d = ImageDraw.Draw(mask)
    if shape == "round":
        d.ellipse((12, 12, 1011, 1011), fill=255)
    else:
        d.rounded_rectangle((12, 12, 1011, 1011), radius=180, fill=255)
    out = im.copy()
    out.putalpha(mask)
    # a thin light edge, as a sewn patch
    edge = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    de = ImageDraw.Draw(edge)
    if shape == "round":
        de.ellipse((12, 12, 1011, 1011), outline=(235, 225, 245, 255), width=28)
    else:
        de.rounded_rectangle((12, 12, 1011, 1011), radius=180, outline=(235, 225, 245, 255), width=28)
    out.alpha_composite(edge)
    return np.array(out).astype(float) / 255


def matunas_text():
    """the back's lettering: its strokes as alpha"""
    return np.array(Image.open(LETTERING).convert("L")).astype(float) / 255


def text_mask(text, height=400, stroke=0):
    """the text in the logo's letters as alpha (0..1), tight around it; stroke: an outline that wide (px) instead"""
    font = ImageFont.truetype(FONT, height)
    box = font.getbbox(text, stroke_width=stroke)
    pad = 8 + stroke
    im = Image.new("L", (box[2] - box[0] + 2 * pad, box[3] - box[1] + 2 * pad), 0)
    d = ImageDraw.Draw(im)
    if stroke:
        d.text((pad - box[0], pad - box[1]), text, font=font, fill=255, stroke_width=stroke, stroke_fill=255)
        inner = Image.new("L", im.size, 0)
        ImageDraw.Draw(inner).text((pad - box[0], pad - box[1]), text, font=font, fill=255)
        im = Image.fromarray(np.clip(np.array(im).astype(int) - np.array(inner), 0, 255).astype(np.uint8))
    else:
        d.text((pad - box[0], pad - box[1]), text, font=font, fill=255)
    return np.array(im).astype(float) / 255


def sample(img, u, v):
    """img (h, w, c) at u, v in 0..1 (nearest-bilinear), outside -> 0"""
    h, w = img.shape[:2]
    inside = (u >= 0) & (u < 1) & (v >= 0) & (v < 1)
    xu = np.clip(u * (w - 1), 0, w - 1)
    yv = np.clip(v * (h - 1), 0, h - 1)
    x0, y0 = np.floor(xu).astype(int), np.floor(yv).astype(int)
    x1, y1 = np.minimum(x0 + 1, w - 1), np.minimum(y0 + 1, h - 1)
    fx, fy = (xu - x0), (yv - y0)
    if img.ndim == 2:
        img = img[:, :, None]
    fx, fy = fx[..., None], fy[..., None]
    out = (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x1] * fx * (1 - fy) + img[y1, x0] * (1 - fx) * fy +
           img[y1, x1] * fx * fy)
    return out * inside[..., None]


def put(design, mask, rgb, strength=255):
    design[mask, :3] = rgb
    design[mask, 3] = strength


def paste(design, region, u, v, img, strength):
    """an RGBA picture onto the region at its u, v"""
    s = sample(img, u, v)
    a = s[..., 3] * region
    for c in range(3):
        design[..., c] = design[..., c] * (1 - a) + s[..., c] * 255 * a
    design[..., 3] = np.where(a > 0.5, strength, design[..., 3])


def gradient(t, stops):
    """colours at t (0..1) between stops [(t, (r, g, b)), ...]"""
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,))
    for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
        k = np.clip((t - t0) / (t1 - t0), 0, 1)[..., None]
        seg = (t >= t0) & (t <= t1)
        out[seg] = (np.array(c0) * (1 - k) + np.array(c1) * k)[seg]
    return out


def lilac():
    m = COACH
    d = np.zeros((SIZE, SIZE, 4))
    put(d, m.ok, (184, 154, 200))
    put(d, coach_hem(m) | coach_cuff(m), (34, 33, 38))
    put(d, m.front & (np.abs(m.y) < 0.006) & (m.z < 0.52), (30, 30, 34))  # the zip
    # the patch on the wearer's left chest (a viewer's right: +y), round
    cy, cz, r = 0.105, 0.395, 0.058
    paste(d, m.front, (m.y - cy) / (2 * r) + 0.5, (cz - m.z) / (2 * r) + 0.5, logo_patch("round"), 200)
    # Matunas across the upper back (seen from behind, the viewer's right is -y)
    t = matunas_text()
    w = 0.30
    h = w * t.shape[0] / t.shape[1]
    rgba = np.zeros(t.shape + (4,))
    rgba[..., :3] = 0.08
    rgba[..., 3] = t
    paste(d, m.back, (0.0 - m.y) / w + 0.5, (0.43 - m.z) / h + 0.5, rgba, 230)
    return d


def coach_hem(m):
    return m.body & (m.z < -0.155)


def coach_cuff(m):
    return m.sleeve & (np.abs(m.y) > 0.455)


def vaporwave():
    m = COACH
    X, Y, Z = m.x, m.y, m.z
    d = np.zeros((SIZE, SIZE, 4))
    put(d, m.ok, (36, 35, 41))
    # the yoke: a V on the front and the back, the sleeves' tops
    yoke_front = m.front & (Z > 0.20 + 0.55 * np.abs(Y))
    yoke_back = m.back & (Z > 0.27 + 0.40 * np.abs(Y))
    yoke_side = m.body & ~m.front & ~m.back & (Z > 0.46)
    yoke_sleeve = m.sleeve & (np.abs(Y) < 0.31) & (Z > 0.25)
    yoke = yoke_front | yoke_back | yoke_side | yoke_sleeve
    t = np.clip((Z - 0.18) / (0.58 - 0.18), 0, 1)[..., None]  # bottom 0 .. top 1
    magenta, purple = np.array([196, 26, 168]), np.array([104, 62, 168])
    grad = purple * (1 - t) + magenta * t
    d[yoke, :3] = grad[yoke]
    d[yoke, 3] = 255
    # the grid: a floor in perspective below a horizon, on the front and the back
    for region, zh in ((yoke_front, 0.37), (yoke_back, 0.42)):
        below = region & (Z < zh)
        depth = 0.05 / np.maximum(zh - Z, 1e-3)               # farther towards the horizon
        across = Y * depth
        lines = (np.abs(((depth * 3.0) % 1) - 0.5) > 0.44) | (np.abs(((across * 7.0) % 1) - 0.5) > 0.45)
        lines &= below
        d[lines, :3] = d[lines, :3] * 0.35 + np.array([236, 160, 255]) * 0.65
        horizon = region & (np.abs(Z - zh) < 0.004)
        d[horizon, :3] = (250, 190, 255)
    # cuffs and hem: black ribbing with a pink and a purple stripe
    hem, cuff = coach_hem(m), coach_cuff(m)
    put(d, hem | cuff | (m.ok & (Z > 0.525)), (28, 27, 31))
    put(d, hem & (np.abs(Z + 0.172) < 0.006), (226, 46, 182))
    put(d, hem & (np.abs(Z + 0.193) < 0.006), (142, 72, 226))
    ay = np.abs(Y)
    put(d, cuff & (np.abs(ay - 0.468) < 0.004), (226, 46, 182))
    put(d, cuff & (np.abs(ay - 0.480) < 0.004), (142, 72, 226))
    put(d, m.front & (np.abs(Y) < 0.005) & (Z < 0.52), (22, 22, 26))  # the zip
    # the patch on the yoke, a rounded square
    cy, cz, r = 0.105, 0.41, 0.054
    paste(d, m.front, (Y - cy) / (2 * r) + 0.5, (cz - Z) / (2 * r) + 0.5, logo_patch("square"), 200)
    return d


# ---------------------------------------------------------------- Sunset (the hooded jacket)
INDIGO, PLUM, RED, DARK_RED = (42, 38, 98), (118, 38, 96), (196, 28, 44), (118, 16, 30)
PRINT_W, PRINT_H, PRINT_Z0 = 0.40, 0.50, -0.02  # the back's print: metres across, high, its bottom's z
PX = 2000  # its pixels a metre


def sunset_print():
    """the back's print as RGBA: a synthwave sunset over a grid, palms at its sides and NOSTALGIA in the sky, its
    edges fading into the jacket"""
    w, h = int(PRINT_W * PX), int(PRINT_H * PX)
    yy, xx = np.mgrid[0:h, 0:w]
    sx = xx / PX - PRINT_W / 2           # across, as seen from behind (left to right)
    sz = PRINT_Z0 + (h - yy) / PX        # height on the jacket
    horizon = 0.12
    img = np.zeros((h, w, 3))
    sky = sz >= horizon
    img[sky] = gradient((sz[sky] - horizon) / (PRINT_Z0 + PRINT_H - horizon),
                        [(0, (255, 150, 112)), (0.3, (242, 84, 162)), (0.62, (248, 156, 216)), (0.86, (246, 170, 222)),
                         (1, (150, 84, 196))])
    # the sun: yellow to pink, its lower half cut by bands that widen downwards
    cx, cz, r = 0.0, 0.215, 0.092
    sun = sky & ((sx - cx) ** 2 + (sz - cz) ** 2 < r * r)
    img[sun] = gradient((cz + r - sz[sun]) / (2 * r), [(0, (255, 226, 96)), (0.55, (255, 120, 120)), (1, (246, 64, 150))])
    below = sun & (sz < cz)
    k = (cz - sz) / r                    # 0 at its middle .. 1 at its bottom
    cut = below & (((cz - sz) / 0.0145) % 1 < 0.12 + 0.55 * k)
    img[cut] = gradient((sz[cut] - horizon) / (PRINT_Z0 + PRINT_H - horizon),
                        [(0, (255, 150, 112)), (0.35, (240, 78, 158)), (1, (126, 48, 160))])
    # the floor: dark purple with a magenta grid in perspective
    floor = ~sky
    img[floor] = (38, 18, 66)
    depth = 0.04 / np.maximum(horizon - sz, 1e-3)
    lines = floor & ((np.abs(((depth * 2.2) % 1) - 0.5) > 0.43) | (np.abs(((sx * depth * 9.0) % 1) - 0.5) > 0.44))
    img[lines] = img[lines] * 0.3 + np.array([255, 92, 210]) * 0.7
    img[np.abs(sz - horizon) < 0.0025] = (255, 200, 240)
    # palms: dark silhouettes on both sides
    pil = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(pil)

    def P(x, z):
        return ((x + PRINT_W / 2) * PX, h - (z - PRINT_Z0) * PX)

    def palm(x0, z0, lean, top, size):
        pts = [(x0 + lean * (s ** 1.6), z0 + (top - z0) * s) for s in np.linspace(0, 1, 24)]
        for i in range(len(pts) - 1):
            wdt = (0.012 - 0.005 * i / len(pts)) * PX * size / 0.09
            dr.line([P(*pts[i]), P(*pts[i + 1])], fill=255, width=max(2, int(wdt)))
        tx, tz = pts[-1]
        for ang, ln in ((15, 1.0), (40, 0.9), (75, 0.7), (110, 0.75), (140, 0.95), (165, 1.0), (195, 0.8), (-15, 0.8),
                        (95, 0.55)):
            a = np.radians(ang)
            poly = []
            for s in np.linspace(0, 1, 14):  # a frond: out and drooping, tapering
                dx = np.cos(a) * size * ln * s
                dz = np.sin(a) * size * ln * s - 0.9 * size * ln * s * s
                poly.append((tx + dx, tz + dz))
            upper = [P(px, pz + 0.010 * size / 0.09 * (1 - s)) for (px, pz), s in zip(poly, np.linspace(0, 1, 14))]
            lower = [P(px, pz - 0.006 * size / 0.09 * (1 - s)) for (px, pz), s in zip(poly, np.linspace(0, 1, 14))]
            dr.polygon(upper + lower[::-1], fill=255)

    palm(-0.165, horizon - 0.02, 0.035, 0.40, 0.085)
    palm(0.165, horizon - 0.02, -0.04, 0.37, 0.08)
    palm(-0.115, horizon - 0.01, -0.02, 0.27, 0.055)
    palm(0.12, horizon - 0.01, 0.02, 0.25, 0.05)
    trees = np.array(pil.filter(ImageFilter.GaussianBlur(1.2))).astype(float)[..., None] / 255
    img = img * (1 - trees) + np.array([28, 14, 52]) * trees
    # NOSTALGIA across the sky, blue with red and cyan beside it, as on the logo
    t = text_mask("NOSTALGIA", 420)
    tw = 0.32 * PX
    th = tw * t.shape[0] / t.shape[1]
    tim = np.array(Image.fromarray((t * 255).astype(np.uint8)).resize((int(tw), int(th)), Image.LANCZOS)).astype(float) / 255
    tz = 0.385  # its middle's height
    top = int(h - (tz - PRINT_Z0) * PX - th / 2)
    left = int((w - tw) / 2)
    off = int(0.0035 * PX)
    for (dx, dy), col in (((off, off), (236, 30, 60)), ((-off, -off), (40, 214, 236)), ((0, 0), (30, 46, 206))):
        a = np.zeros((h, w))
        a[top + dy:top + dy + tim.shape[0], left + dx:left + dx + tim.shape[1]] = tim
        img = img * (1 - a[..., None]) + np.array(col) * a[..., None]
    # its edges fade into the jacket: the sides and the top
    edge = np.minimum.reduce([np.clip((sx + PRINT_W / 2) / 0.035, 0, 1), np.clip((PRINT_W / 2 - sx) / 0.035, 0, 1),
                              np.clip((PRINT_Z0 + PRINT_H - sz) / 0.05, 0, 1), np.clip((sz - PRINT_Z0) / 0.02, 0, 1)])
    out = np.concatenate([img, edge[..., None] * 255], -1)
    return np.clip(out, 0, 255) / 255


def sunset_shirt():
    """the white T-shirt under the open jacket (the game's: kept), from the game's texture"""
    o = rw.read_txd(open("orig/hoodjackbeige.txd", "rb").read())["hoodjackbeige"].rgba.astype(float)
    lum = o[..., :3] @ np.array([0.299, 0.587, 0.114])
    sat = o[..., :3].max(-1) - o[..., :3].min(-1)
    m = ((lum > 150) & (sat < 40)).astype(np.uint8) * 255
    m = np.array(Image.fromarray(m).filter(ImageFilter.MedianFilter(3)).resize((SIZE, SIZE), Image.BILINEAR))
    return (m > 127) & HOOD.body


def sunset():
    m = HOOD
    X, Y, Z = m.x, m.y, m.z
    d = np.zeros((SIZE, SIZE, 4))
    stops = [(0, RED), (0.45, PLUM), (1, INDIGO)]
    body = m.body
    d[body, :3] = gradient((Z[body] + 0.17) / (0.60 + 0.17), stops)
    sl = m.sleeve
    d[sl, :3] = gradient(1 - (np.abs(Y[sl]) - 0.20) / (0.47 - 0.20), stops)  # down the arm: indigo to red
    d[m.ok, 3] = 255
    # hem and cuffs: dark red ribbing
    put(d, body & (Z < -0.165), DARK_RED)
    put(d, sl & (np.abs(Y) > 0.455), DARK_RED)
    # the back's print
    paste(d, m.back, (-Y + PRINT_W / 2) / PRINT_W, (PRINT_Z0 + PRINT_H - Z) / PRINT_H, sunset_print(), 235)
    # NOSTALGIA in outline down the right sleeve's front (the arm goes out and down: along it)
    t = text_mask("NOSTALGIA", 300, stroke=14)
    a0, a1 = np.array([0.215, 0.315]), np.array([0.445, 0.075])  # the arm's line (|y|, z), shoulder to cuff
    along = (a1 - a0) / np.linalg.norm(a1 - a0)
    norm = np.array([-along[1], along[0]])
    rel_y, rel_z = -Y - a0[0], Z - a0[1]
    s = rel_y * along[0] + rel_z * along[1]
    q = rel_y * norm[0] + rel_z * norm[1]
    length = 0.20
    tall = length * t.shape[0] / t.shape[1]
    rgba = np.zeros(t.shape + (4,))
    rgba[..., :3] = (250, 214, 236)
    rgba[..., 3] = t
    # read from the cuff up to the shoulder: from the front, left to right
    paste(d, sl & (Y < 0) & (X > 0.01), 1 - (s - 0.015) / length, 0.5 - (q - 0.012) / tall, rgba, 230)
    # the zip's tapes along the open front, by the shirt
    shirt = sunset_shirt()
    near = np.array(Image.fromarray(shirt.astype(np.uint8) * 255).filter(ImageFilter.MaxFilter(9))) > 0
    put(d, near & ~shirt & m.front & (Z < 0.50), (24, 22, 30))
    d[shirt] = 0  # the shirt stays the game's
    # the patch on the wearer's left chest, round
    cy, cz, r = 0.135, 0.37, 0.052
    paste(d, m.front & ~shirt, (Y - cy) / (2 * r) + 0.5, (cz - Z) / (2 * r) + 0.5, logo_patch("round"), 200)
    return d


JACKETS = (("coach", lilac), ("coachsemi", vaporwave), ("hoodjackbeige", sunset))
MODEL_OF = {"coach": "coach", "coachsemi": "coach", "hoodjackbeige": "hoodjack"}

if __name__ == "__main__":
    import export
    os.makedirs("out", exist_ok=True)
    Image.fromarray((sunset_print() * 255).astype(np.uint8), "RGBA").save("out/hoodjackbeige_print.png")
    for name, fn in JACKETS:
        d = np.clip(fn(), 0, 255).astype(np.uint8)
        Image.fromarray(d, "RGBA").save("out/%s.png" % name)
        level = export.original_level(name)
        hd = export.compose_hd(d, export.compose(export.small(d), level), export.ratio(export.small(d), level))
        Image.fromarray(np.frombuffer(hd, np.uint8).reshape(SIZE, SIZE, 4)[..., [2, 1, 0]]).save("out/%s_final.png" % name)
        print(name, "painted", int((d[..., 3] > 0).sum()), "texels")
