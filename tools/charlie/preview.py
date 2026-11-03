"""A view of the house as the game draws it (texture x vertex colour x 2), from out/preview.npz (build.py).

python3 preview.py view.png "(eye x, y, z)" "(target x, y, z)" [faces under this height] [field of view] [night]
(the block's axes: x east, y north, z up from the block's origin, 8.9 m: the floor is at -5.06)"""
import sys

import numpy as np
from PIL import Image

import textures

GLASS = np.array([176, 206, 226]) / 255.0


def main(out, eye, target, zcut="99", fov="55", which="day"):
    d = np.load("out/preview.npz")
    T, C, UV, TEX = d["tris"], d[which].astype(float), d["uv"], d["tex"]
    tex = {k: textures.undxt1(textures.dxt1(v), v.shape[1], v.shape[0]) / 255.0 for k, v in textures.make().items()}
    eye, target = np.array(eval(eye), float), np.array(eval(target), float)
    sel = T[:, :, 2].max(1) < float(zcut)
    T, C, UV, TEX = T[sel], C[sel], UV[sel], TEX[sel]
    W, H = 1280, 800
    f = 1.0 / np.tan(np.radians(float(fov) / 2))
    fwd = target - eye
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    img = np.zeros((H, W, 3))
    img[:] = (0.55, 0.75, 0.95) if which == "day" else (0.05, 0.07, 0.12)
    zb = np.full((H, W), 1e9)
    for glass_pass in (False, True):
        for i in np.where((TEX == "glass") == glass_pass)[0]:
            t = T[i]
            if np.cross(t[1] - t[0], t[2] - t[0]) @ (eye - t.mean(0)) <= 0:
                continue  # its back (the game does not draw it)
            rel = t - eye
            z = rel @ fwd
            if (z < 0.1).any():
                continue
            x = (rel @ right) / z * f * H / 2 + W / 2
            y = H / 2 - (rel @ up) / z * f * H / 2
            x0, x1 = int(max(0, np.floor(x.min()))), int(min(W - 1, np.ceil(x.max())))
            y0, y1 = int(max(0, np.floor(y.min()))), int(min(H - 1, np.ceil(y.max())))
            if x0 > x1 or y0 > y1:
                continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            d0 = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            if abs(d0) < 1e-9:
                continue
            w1 = ((xs - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (ys - y[0])) / d0
            w2 = ((x[1] - x[0]) * (ys - y[0]) - (xs - x[0]) * (y[1] - y[0])) / d0
            w0 = 1 - w1 - w2
            m = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            # perspective-correct
            iz = w0 / z[0] + w1 / z[1] + w2 / z[2]
            zz = 1 / np.maximum(iz, 1e-9)
            sub = zb[y0:y1 + 1, x0:x1 + 1]
            m &= zz < sub
            if not m.any():
                continue
            b0, b1, b2 = (w0 / z[0]) * zz, (w1 / z[1]) * zz, (w2 / z[2]) * zz
            col = (b0[..., None] * C[i][0] + b1[..., None] * C[i][1] + b2[..., None] * C[i][2]) * 2 / 255.0
            region = img[y0:y1 + 1, x0:x1 + 1]
            if glass_pass:
                region[m] = region[m] * 0.57 + (GLASS * col[m]) * 0.43
                continue
            u = b0 * UV[i][0, 0] + b1 * UV[i][1, 0] + b2 * UV[i][2, 0]
            v = b0 * UV[i][0, 1] + b1 * UV[i][1, 1] + b2 * UV[i][2, 1]
            tx = tex[TEX[i]]
            th, tw = tx.shape[:2]
            px = tx[(np.floor(v * th).astype(int)) % th, (np.floor(u * tw).astype(int)) % tw]
            sub[m] = zz[m]
            region[m] = np.clip(px[m] * col[m], 0, 1)
    Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).save(out)


if __name__ == "__main__":
    main(*sys.argv[1:])
