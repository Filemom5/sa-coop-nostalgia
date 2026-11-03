"""Cutting a part off one of the game's own models (a DFF of one geometry, as the map's buildings are) and off its
collision (COL3): what lies on one side of a vertical plane goes, the faces across it are cut along it (their new
corners get everything of the old ones in between: place, texture, colours of day and night, normal). The rest of
the file is kept as it was (its frames, materials, extensions); the 2d effects (lights...) on the side that goes go
too.

build.py uses it for santahouse02_law2: the two houses west of the beach safe house go, the safe house stays."""
import struct

import numpy as np


def chunks(b, p, end):
    while p + 12 <= end:
        t, s, v = struct.unpack_from("<III", b, p)
        yield t, p, s, v
        p += 12 + s


def chunk(kind, payload, ver):
    return struct.pack("<III", kind, len(payload), ver) + payload


class Geometry:
    """a geometry chunk's data, its arrays per vertex and per triangle"""

    def __init__(self, b, p, s, ver):
        self.ver = ver
        self.parts = []  # (kind, bytes) of the chunk's children other than the struct and the extension
        self.ext = []    # (kind, bytes, ver) of the extension's children
        for t, q, sz, v in chunks(b, p + 12, p + 12 + s):
            if t == 0x01:
                self.read_struct(b[q + 12:q + 12 + sz])
            elif t == 0x03:
                self.ext = [(t2, b[q2 + 12:q2 + 12 + sz2], v2) for t2, q2, sz2, v2 in chunks(b, q + 12, q + 12 + sz)]
            else:
                self.parts.append((t, b[q:q + 12 + sz]))

    def read_struct(self, d):
        self.flags, nt, nv, nm = struct.unpack_from("<Iiii", d, 0)
        assert nm == 1, "one morph target"
        q = 16
        ntex = (self.flags >> 16) & 0xFF or (2 if self.flags & 0x80 else (1 if self.flags & 0x04 else 0))
        self.ntex = ntex
        self.pre = None
        if self.flags & 0x08:
            self.pre = np.frombuffer(d, np.uint8, nv * 4, q).reshape(-1, 4).astype(float)
            q += nv * 4
        self.uv = []
        for _ in range(ntex):
            self.uv.append(np.frombuffer(d, np.float32, nv * 2, q).reshape(-1, 2).astype(float))
            q += nv * 8
        t = np.frombuffer(d, "<u2", nt * 4, q).reshape(-1, 4)
        q += nt * 8
        self.tri = t[:, [1, 0, 3]].astype(np.int64)  # v1, v2, v3 (the file: v2, v1, material, v3)
        self.mat = t[:, 2].astype(np.int64)
        sphere = struct.unpack_from("<4fii", d, q)
        q += 24
        self.pos = np.frombuffer(d, np.float32, nv * 3, q).reshape(-1, 3).astype(float)
        q += nv * 12
        self.nrm = None
        if sphere[5]:
            self.nrm = np.frombuffer(d, np.float32, nv * 3, q).reshape(-1, 3).astype(float)
            q += nv * 12

    def night(self):
        for k, (t, d, v) in enumerate(self.ext):
            if t == 0x253F2F9:
                return k
        return None

    def cut(self, keep):
        """keep(points) -> bool per point: the side that stays. Faces across: cut (keep(p) is assumed linear: a plane
        x >= c, given as (axis, c, sign))"""
        axis, c, sign = keep
        f = lambda p: (p[..., axis] - c) * sign  # >= 0: stays
        nk = self.night()
        night = None
        if nk is not None:
            night = np.frombuffer(self.ext[nk][1], np.uint8, len(self.pos) * 4, 4).reshape(-1, 4).astype(float)
        attrs = [self.pos] + ([self.pre] if self.pre is not None else []) + self.uv + \
                ([self.nrm] if self.nrm is not None else []) + ([night] if night is not None else [])
        new = [list(a) for a in attrs]
        tris, mats = [], []
        cache = {}

        def mid(i, j):
            key = (min(i, j), max(i, j))
            if key not in cache:
                di, dj = f(self.pos[i]), f(self.pos[j])
                t = di / (di - dj)
                for a, n in zip(attrs, new):
                    n.append(a[i] + (a[j] - a[i]) * t)
                cache[key] = len(new[0]) - 1
            return cache[key]

        for (a, b, cc), m in zip(self.tri, self.mat):
            d = [f(self.pos[a]), f(self.pos[b]), f(self.pos[cc])]
            if min(d) >= 0:
                tris.append((a, b, cc)), mats.append(m)
                continue
            if max(d) <= 0:
                continue
            poly = []
            vs = [a, b, cc]
            for k in range(3):
                i, j = vs[k], vs[(k + 1) % 3]
                di, dj = d[k], d[(k + 1) % 3]
                if di >= 0:
                    poly.append(i)
                if (di >= 0) != (dj >= 0) and di != 0 and dj != 0:
                    poly.append(mid(i, j))
            for k in range(1, len(poly) - 1):
                tris.append((poly[0], poly[k], poly[k + 1])), mats.append(m)
        arrs = [np.array(n, float) for n in new]
        k = 0
        self.pos = arrs[k]; k += 1
        if self.pre is not None:
            self.pre = arrs[k]; k += 1
        self.uv = arrs[k:k + self.ntex]; k += self.ntex
        if self.nrm is not None:
            n = arrs[k]; k += 1
            self.nrm = n / np.maximum(np.linalg.norm(n, axis=1), 1e-9)[:, None]
        if night is not None:
            night = arrs[k]
        self.tri, self.mat = np.array(tris, np.int64).reshape(-1, 3), np.array(mats, np.int64)
        # drop the vertices no face uses any more
        used = np.unique(self.tri)
        remap = -np.ones(len(self.pos), np.int64)
        remap[used] = np.arange(len(used))
        self.tri = remap[self.tri]
        self.pos = self.pos[used]
        if self.pre is not None:
            self.pre = self.pre[used]
        self.uv = [u[used] for u in self.uv]
        if self.nrm is not None:
            self.nrm = self.nrm[used]
        if night is not None:
            night = night[used]
            self.ext[nk] = (0x253F2F9, struct.pack("<I", 1) + np.clip(np.round(night), 0, 255).astype(np.uint8).tobytes(),
                            self.ext[nk][2])
        # the 2d effects on the side that goes
        for e, (t, d, v) in enumerate(self.ext):
            if t != 0x253F2F8:
                continue
            n = struct.unpack_from("<I", d, 0)[0]
            q, keepfx = 4, []
            for _ in range(n):
                px, py, pz, kind, size = struct.unpack_from("<3fII", d, q)
                entry = d[q:q + 20 + size]
                q += 20 + size
                if f(np.array([px, py, pz])) >= 0:
                    keepfx.append(entry)
            self.ext[e] = (t, struct.pack("<I", len(keepfx)) + b"".join(keepfx), v)

    def write(self):
        nv, nt = len(self.pos), len(self.tri)
        assert nv < 65536
        flags = self.flags & ~0x01  # triangle lists, not strips
        s = struct.pack("<Iiii", flags, nt, nv, 1)
        if self.pre is not None:
            s += np.clip(np.round(self.pre), 0, 255).astype(np.uint8).tobytes()
        for u in self.uv:
            s += u.astype(np.float32).tobytes()
        t = np.stack([self.tri[:, 1], self.tri[:, 0], self.mat, self.tri[:, 2]], 1).astype("<u2")
        s += t.tobytes()
        lo, hi = self.pos.min(0), self.pos.max(0)
        ctr = (lo + hi) / 2
        rad = float(np.linalg.norm(self.pos - ctr, axis=1).max())
        s += struct.pack("<4fii", *ctr, rad, 1, 1 if self.nrm is not None else 0) + self.pos.astype(np.float32).tobytes()
        if self.nrm is not None:
            s += self.nrm.astype(np.float32).tobytes()
        ext = b""
        for t2, d, v in self.ext:
            if t2 == 0x50E:  # the meshes: anew, as lists
                meshes, total, n = b"", 0, 0
                for m in np.unique(self.mat):
                    idx = self.tri[self.mat == m].ravel()
                    meshes += struct.pack("<II", len(idx), int(m)) + idx.astype("<u4").tobytes()
                    total += len(idx)
                    n += 1
                d = struct.pack("<III", 0, n, total) + meshes
            ext += chunk(t2, d, v)
        out = chunk(0x01, s, self.ver)
        for t2, raw in self.parts:
            out += raw
        return chunk(0x0F, out + chunk(0x03, ext, self.ver), self.ver)


def cut_dff(b, keep):
    """the DFF b with its one geometry cut (Geometry.cut)"""
    def rebuild(p, end):
        out = b""
        for t, q, s, v in chunks(b, p, end):
            if t == 0x0F:
                g = Geometry(b, q, s, v)
                g.cut(keep)
                out += g.write()
            elif t in (0x10, 0x1A):
                out += chunk(t, rebuild(q + 12, q + 12 + s), v)
            else:
                out += b[q:q + 12 + s]
        return out
    t, q, s, v = next(chunks(b, 0, len(b)))
    return rebuild(0, 12 + s)


def cut_col(col, keep):
    """a parsed COL3 (col.parse) cut the same way: faces cut, boxes and spheres by their middle"""
    axis, c, sign = keep
    f = lambda p: (p[..., axis] - c) * sign
    V = list(col["verts"])
    faces = []
    cache = {}

    def mid(i, j):
        key = (min(i, j), max(i, j))
        if key not in cache:
            di, dj = f(V[i]), f(V[j])
            t = di / (di - dj)
            V.append(V[i] + (V[j] - V[i]) * t)
            cache[key] = len(V) - 1
        return cache[key]

    for a, b, cc, m, l in col["faces"]:
        d = [f(V[a]), f(V[b]), f(V[cc])]
        if min(d) >= 0:
            faces.append((a, b, cc, m, l))
            continue
        if max(d) <= 0:
            continue
        poly, vs = [], [a, b, cc]
        for k in range(3):
            i, j = vs[k], vs[(k + 1) % 3]
            if d[k] >= 0:
                poly.append(i)
            if (d[k] >= 0) != (d[(k + 1) % 3] >= 0) and d[k] != 0 and d[(k + 1) % 3] != 0:
                poly.append(mid(i, j))
        for k in range(1, len(poly) - 1):
            faces.append((poly[0], poly[k], poly[k + 1], m, l))
    boxes = [bx for bx in col["boxes"] if f((np.array(bx[:3]) + np.array(bx[3:6])) / 2) >= 0]
    spheres = [sp for sp in col["spheres"] if f(np.array(sp[:3])) >= 0]
    V = np.array(V)
    used = sorted({i for fc in faces for i in fc[:3]})
    remap = {o: n for n, o in enumerate(used)}
    faces = [(remap[a], remap[b], remap[cc], m, l) for a, b, cc, m, l in faces]
    return spheres, boxes, V[used], faces
