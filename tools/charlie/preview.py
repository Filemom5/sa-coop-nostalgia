import numpy as np, sys
from PIL import Image
d = np.load(sys.argv[1]); T = d["tris"]; C = d[sys.argv[7] if len(sys.argv) > 7 else "day"].astype(float); A = d["alpha"]
out = sys.argv[2]
eye = np.array(eval(sys.argv[3]), float); target = np.array(eval(sys.argv[4]), float)
zcut = float(sys.argv[5]); fov = float(sys.argv[6])
W, H = 1200, 750; f = 1.0 / np.tan(np.radians(fov / 2))
sel = T[:, :, 2].max(1) < zcut; T = T[sel]; C = C[sel]; A = A[sel]
fwd = target - eye; fwd /= np.linalg.norm(fwd); right = np.cross(fwd, [0, 0, 1]); right /= np.linalg.norm(right); up = np.cross(right, fwd)
img = np.zeros((H, W, 3)); img[:] = (150, 190, 230); zb = np.full((H, W), 1e9)
order = np.argsort(A < 255)  # opaque first
for i in order:
    t = T[i]; rel = t - eye; z = rel @ fwd
    if (z < 0.1).any(): continue
    x = (rel @ right) / z * f * H / 2 + W / 2; y = H / 2 - (rel @ up) / z * f * H / 2
    x0, x1 = int(max(0, np.floor(x.min()))), int(min(W - 1, np.ceil(x.max()))); y0, y1 = int(max(0, np.floor(y.min()))), int(min(H - 1, np.ceil(y.max())))
    if x0 > x1 or y0 > y1: continue
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    d0 = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
    if abs(d0) < 1e-9: continue
    w1 = ((xs - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (ys - y[0])) / d0
    w2 = ((x[1] - x[0]) * (ys - y[0]) - (xs - x[0]) * (y[1] - y[0])) / d0
    w0 = 1 - w1 - w2; m = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    zz = 1 / (w0 / z[0] + w1 / z[1] + w2 / z[2])
    sub = zb[y0:y1 + 1, x0:x1 + 1]; m &= zz < sub
    if A[i] < 255:
        a = A[i] / 255; reg = img[y0:y1 + 1, x0:x1 + 1]; reg[m] = reg[m] * (1 - a) + (C[i] if C.ndim == 2 else C[i].mean(0)) * a
    else:
        sub[m] = zz[m]
        col = C[i] if C.ndim == 2 else (w0[..., None] * C[i][0] + w1[..., None] * C[i][1] + w2[..., None] * C[i][2])
        img[y0:y1 + 1, x0:x1 + 1][m] = col if C.ndim == 2 else col[m]
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out)
