import sys; sys.path.insert(0, '../sign')
import rw, numpy as np


def uv_map(dff='orig/coach.dff', size=256):
    """per texel: the 3D point of the jacket there (nan where none), and its normal (the texture repeats: a
    triangle over its edge paints the other side)"""
    g = rw.read_dff(open(dff, 'rb').read()).geoms[0]
    P = np.full((size, size, 3), np.nan); N = np.full((size, size, 3), np.nan)
    uv = g.uvs[0]
    for a, b, c, m in g.tris:
        t0 = np.array([uv[a], uv[b], uv[c]]) * size
        n = np.cross(g.verts[b] - g.verts[a], g.verts[c] - g.verts[a]); n /= np.linalg.norm(n) + 1e-12
        for su in (-1, 0, 1):
            for sv in (-1, 0, 1):
                t = t0 + np.array([su, sv]) * size
                if t[:, 0].max() < 0 or t[:, 0].min() > size or t[:, 1].max() < 0 or t[:, 1].min() > size: continue
                x0, y0 = np.floor(t.min(0)).astype(int); x1, y1 = np.ceil(t.max(0)).astype(int)
                area = (t[1, 0] - t[0, 0]) * (t[2, 1] - t[0, 1]) - (t[2, 0] - t[0, 0]) * (t[1, 1] - t[0, 1])
                if abs(area) < 1e-9: continue
                ys = np.arange(max(y0, 0), min(y1, size)); xs = np.arange(max(x0 - 1, 0), min(x1 + 1, size))
                if not len(ys) or not len(xs): continue
                py, px = np.meshgrid(ys + 0.5, xs + 0.5, indexing='ij')
                w0 = ((t[1, 0] - px) * (t[2, 1] - py) - (t[2, 0] - px) * (t[1, 1] - py)) / area
                w1 = ((t[2, 0] - px) * (t[0, 1] - py) - (t[0, 0] - px) * (t[2, 1] - py)) / area
                w2 = 1 - w0 - w1
                inside = np.minimum(np.minimum(w0, w1), w2) >= -0.02
                if not inside.any(): continue
                pts = w0[..., None] * g.verts[a] + w1[..., None] * g.verts[b] + w2[..., None] * g.verts[c]
                yy, xx = py[inside].astype(int), px[inside].astype(int)
                P[yy, xx] = pts[inside]; N[yy, xx] = n
    return P, N
