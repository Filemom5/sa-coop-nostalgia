"""The two windbreakers (SubUrban's "coach" jacket: Grn Windbreaker -> lilac "Matunas", Blk Windbreaker -> black and
purple "Nostalgia"), painted in the texture's space from the 3D place of each texel (model space: x front, y the
wearer's left, z up). A design is RGBA: A = 0 keeps the game's pixel (hands, skin), else the colour shaded by the game's
own folds (its brightness, smoothed) with that strength (A / 255).

python3 design.py -> out/coach.png, out/coachsemi.png (designs), out/*_final.png (as the game would show them);
export.py writes them for the mod. orig/: coach.dff, coach.txd, coachsemi.txd from the game's models/player.img"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, "../sign")
sys.path.insert(0, ".")
import rw
from uvmap import uv_map

LOGO = "logo.png"          # the group's logo (Diet Crystal Nostalgia Tears)
LETTERING = "matunas.png"  # "Matunas" as on the back of the real jacket (from its photo), as alpha
SIZE = 256

P, N = uv_map()
OK = ~np.isnan(P[:, :, 0])
X, Y, Z = (np.nan_to_num(P[:, :, k]) for k in range(3))
NX = np.nan_to_num(N[:, :, 0])
ROWS = np.arange(SIZE)[:, None] * np.ones((1, SIZE))
SLEEVE = OK & (ROWS < 100)           # the sleeves' strips at the texture's top
BODY = OK & ~SLEEVE
CUFF = SLEEVE & (np.abs(Y) > 0.455)
HEM = BODY & (Z < -0.155)
COLLAR = OK & (Z > 0.525)
FRONT = BODY & (NX > 0.25) & (X > 0.02)
BACK = BODY & (NX < -0.25) & (X < 0.0)


def logo_patch(shape):
    """the group's logo as a patch: round or rounded square, RGBA"""
    im = Image.open(LOGO).convert("RGB").resize((512, 512), Image.LANCZOS)
    mask = Image.new("L", (512, 512), 0)
    d = ImageDraw.Draw(mask)
    if shape == "round":
        d.ellipse((6, 6, 505, 505), fill=255)
    else:
        d.rounded_rectangle((6, 6, 505, 505), radius=90, fill=255)
    out = im.copy()
    out.putalpha(mask)
    # a thin light edge, as a sewn patch
    edge = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    de = ImageDraw.Draw(edge)
    if shape == "round":
        de.ellipse((6, 6, 505, 505), outline=(235, 225, 245, 255), width=14)
    else:
        de.rounded_rectangle((6, 6, 505, 505), radius=90, outline=(235, 225, 245, 255), width=14)
    out.alpha_composite(edge)
    return np.array(out).astype(float) / 255


def matunas_text():
    """the back's lettering: its strokes as alpha"""
    return np.array(Image.open(LETTERING).convert("L")).astype(float) / 255


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


def lilac():
    d = np.zeros((SIZE, SIZE, 4))
    put(d, OK, (184, 154, 200))
    put(d, HEM | CUFF, (34, 33, 38))
    put(d, FRONT & (np.abs(Y) < 0.006) & (Z < 0.52), (30, 30, 34))  # the zip
    # the patch on the wearer's left chest (a viewer's right: +y), round
    cy, cz, r = 0.105, 0.40, 0.046
    paste(d, FRONT, (Y - cy) / (2 * r) + 0.5, (cz - Z) / (2 * r) + 0.5, logo_patch("round"), 200)
    # Matunas across the upper back (seen from behind, the viewer's right is -y)
    t = matunas_text()
    w = 0.30
    h = w * t.shape[0] / t.shape[1]
    rgba = np.zeros(t.shape + (4,))
    rgba[..., :3] = 0.08
    rgba[..., 3] = t
    paste(d, BACK, (0.0 - Y) / w + 0.5, (0.43 - Z) / h + 0.5, rgba, 230)
    return d


def vaporwave():
    d = np.zeros((SIZE, SIZE, 4))
    put(d, OK, (36, 35, 41))
    # the yoke: a V on the front and the back, the sleeves' tops
    yoke_front = FRONT & (Z > 0.20 + 0.55 * np.abs(Y))
    yoke_back = BACK & (Z > 0.27 + 0.40 * np.abs(Y))
    yoke_side = BODY & ~FRONT & ~BACK & (Z > 0.46)
    yoke_sleeve = SLEEVE & (np.abs(Y) < 0.31) & (Z > 0.25)
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
    put(d, HEM | CUFF | COLLAR, (28, 27, 31))
    put(d, HEM & (np.abs(Z + 0.172) < 0.006), (226, 46, 182))
    put(d, HEM & (np.abs(Z + 0.193) < 0.006), (142, 72, 226))
    ay = np.abs(Y)
    put(d, CUFF & (np.abs(ay - 0.468) < 0.004), (226, 46, 182))
    put(d, CUFF & (np.abs(ay - 0.480) < 0.004), (142, 72, 226))
    put(d, FRONT & (np.abs(Y) < 0.005) & (Z < 0.52), (22, 22, 26))  # the zip
    # the patch on the yoke, a rounded square
    cy, cz, r = 0.105, 0.42, 0.042
    paste(d, FRONT, (Y - cy) / (2 * r) + 0.5, (cz - Z) / (2 * r) + 0.5, logo_patch("square"), 200)
    return d


def compose(design, original_rgba):
    """as the game would show it: the design shaded by the original's folds"""
    o = original_rgba[..., :3].astype(float)
    lum = o @ np.array([0.299, 0.587, 0.114])
    med = np.array(Image.fromarray(lum.astype(np.uint8)).filter(ImageFilter.MedianFilter(5))).astype(float)
    mean = med[OK].mean()
    ratio = med / max(mean, 1)
    ratio = np.where(ratio > 1.6, 1.0, np.clip(ratio, 0.55, 1.2))  # the original's white bits (shirt, hem): flat
    k = design[..., 3:4] / 255
    shaded = design[..., :3] * (1 + (ratio[..., None] - 1) * k)
    out = np.where(design[..., 3:4] > 0, shaded, o)
    return np.clip(out, 0, 255).astype(np.uint8)


if __name__ == "__main__":
    os.makedirs("out", exist_ok=True)
    for name, fn in (("coach", lilac), ("coachsemi", vaporwave)):
        d = fn()
        Image.fromarray(np.clip(d, 0, 255).astype(np.uint8), "RGBA").save("out/%s.png" % name)
        orig = rw.read_txd(open("orig/%s.txd" % name, "rb").read())[name].rgba
        Image.fromarray(compose(d, orig)).save("out/%s_final.png" % name)
        print(name, "painted", int((d[..., 3] > 0).sum()), "texels")
