"""A small software renderer (numpy) for previews: textured, vertex-coloured triangles, a z-buffer, alpha test and a
little sun so that the shapes read."""
import numpy as np
from PIL import Image


def look_at(eye, target, up=(0, 0, 1)):
    eye, target, up = map(lambda v: np.asarray(v, float), (eye, target, up))
    f = target - eye
    f /= np.linalg.norm(f)
    r = np.cross(f, up)
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    m = np.eye(4)
    m[0, :3], m[1, :3], m[2, :3] = r, u, -f
    m[:3, 3] = -m[:3, :3] @ eye
    return m


class Mesh:
    def __init__(self, verts, tris, uvs=None, colors=None, textures=None, tex_of_tri=None, normals=None):
        self.verts = np.asarray(verts, float)
        self.tris = np.asarray(tris, int)
        self.uvs = np.zeros((len(verts), 2)) if uvs is None else np.asarray(uvs, float)
        self.colors = np.ones((len(verts), 4)) if colors is None else np.asarray(colors, float)
        self.textures = textures or [None]
        self.tex_of_tri = np.zeros(len(tris), int) if tex_of_tri is None else np.asarray(tex_of_tri, int)


def render(meshes, eye, target, w=1280, h=720, fov=50, sun=(0.4, -0.5, 0.75), sky=True, up=(0, 0, 1), ambient=0.55,
           near=0.1):
    view = look_at(eye, target, up)
    f = 1 / np.tan(np.radians(fov) / 2)
    img = np.zeros((h, w, 3))
    if sky:
        t = np.linspace(0, 1, h)[:, None]
        img[:] = ((1 - t) * np.array([0.55, 0.72, 0.92]) + t * np.array([0.86, 0.88, 0.86]))[:, None, :]
    zbuf = np.full((h, w), np.inf)
    sun = np.asarray(sun, float)
    sun /= np.linalg.norm(sun)
    for m in meshes:
        v = np.c_[m.verts, np.ones(len(m.verts))] @ view.T
        z = -v[:, 2]
        sx = w / 2 + v[:, 0] / np.maximum(z, 1e-6) * f * h / 2
        sy = h / 2 - v[:, 1] / np.maximum(z, 1e-6) * f * h / 2
        for ti, (a, b, c) in enumerate(m.tris):
            if min(z[a], z[b], z[c]) < near:
                continue
            p = np.array([[sx[a], sy[a]], [sx[b], sy[b]], [sx[c], sy[c]]])
            x0, y0 = np.floor(p.min(0)).astype(int)
            x1, y1 = np.ceil(p.max(0)).astype(int)
            x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, w - 1), min(y1, h - 1)
            if x0 > x1 or y0 > y1:
                continue
            area = (p[1, 0] - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (p[1, 1] - p[0, 1])
            if abs(area) < 1e-9:
                continue
            ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1] + 0.5
            w0 = ((p[1, 0] - xs) * (p[2, 1] - ys) - (p[2, 0] - xs) * (p[1, 1] - ys)) / area
            w1 = ((p[2, 0] - xs) * (p[0, 1] - ys) - (p[0, 0] - xs) * (p[2, 1] - ys)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            if not inside.any():
                continue
            iz = w0 / z[a] + w1 / z[b] + w2 / z[c]
            depth = 1 / iz
            ys_i, xs_i = (ys - 0.5).astype(int), (xs - 0.5).astype(int)
            closer = inside & (depth < zbuf[ys_i, xs_i])
            if not closer.any():
                continue
            # perspective-correct attributes
            k0, k1, k2 = w0 / z[a] / iz, w1 / z[b] / iz, w2 / z[c] / iz
            uv = k0[..., None] * m.uvs[a] + k1[..., None] * m.uvs[b] + k2[..., None] * m.uvs[c]
            col = k0[..., None] * m.colors[a] + k1[..., None] * m.colors[b] + k2[..., None] * m.colors[c]
            tex = m.textures[m.tex_of_tri[ti]]
            if tex is not None:
                th, tw = tex.shape[:2]
                tu = (np.mod(uv[..., 0], 1) * tw).astype(int) % tw
                tv = (np.mod(uv[..., 1], 1) * th).astype(int) % th
                texel = tex[tv, tu] / 255.0
                col = col * texel
            closer &= col[..., 3] > 0.5
            n = np.cross(m.verts[b] - m.verts[a], m.verts[c] - m.verts[a])
            nl = np.linalg.norm(n)
            light = ambient + (1 - ambient) * abs(n @ sun) / nl if nl > 0 else 1
            rgb = np.clip(col[..., :3] * light, 0, 1)
            sel = closer
            zbuf[ys_i[sel], xs_i[sel]] = depth[sel]
            img[ys_i[sel], xs_i[sel]] = rgb[sel]
    return Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
