"""NOSTALGIA in place of VINEWOOD: builds the sign's three models (letters' fronts, their backs and frames, the LOD) and
its collision from the game's own files, as a recipe the mod applies to them in the game (no Rockstar data in it:
copies are "these triangles of the original, moved"; new letters are ours, coloured from the original's vertices).

python3 build.py -> out/*.dff and out/vinesign.col (to look at: preview.py); recipe.py writes what the mod uses"""
import struct
import sys
import zlib

import numpy as np
from scipy.spatial import Delaunay, cKDTree

sys.path.insert(0, ".")
import font
import rw
from seg import components, load

YF, YB = -4.03, -3.53          # the letters' front and back (model space: x along, y back, z up)
H = font.H
WORD = "NOSTALGIA"

# ---------------------------------------------------------------- the originals
B = load("vinesign1_cunte01")   # the letters' fronts and edges
A = load("vinesign1_cunte")     # their backs and frames
L = load("lodvinesign1_cunte")  # far away: a plane with a texture of the word
ORIG = "VINEWOOD"
B_LAB = components(B)
ORIG_BOX = []                   # per original letter: left, bottom, width (from its front)
for k in range(8):
    v = B.verts[np.unique(B.tris[B_LAB == k][:, :3])]
    ORIG_BOX.append((v[:, 0].min(), v[:, 2].min(), v[:, 0].max() - v[:, 0].min()))


def tri_normals(g):
    a, b, c = g.verts[g.tris[:, 0]], g.verts[g.tris[:, 1]], g.verts[g.tris[:, 2]]
    n = np.cross(b - a, c - a)
    return n / np.linalg.norm(n, axis=1, keepdims=True)


B_N, A_N = tri_normals(B), tri_normals(A)
A_LAB = components(A)
A_BACK = (A.tris[:, 3] == 1) & (A_N[:, 1] > 0.9) & (np.abs(A.verts[A.tris[:, 0], 1] - YB) < 0.01)


def a_letter_of_component(k):
    """the original letter an A part belongs to (by the x it overlaps most)"""
    v = A.verts[np.unique(A.tris[A_LAB == k][:, :3])]
    x0, x1 = v[:, 0].min(), v[:, 0].max()
    best, letter = -1e9, 0
    for i, (l, b, w) in enumerate(ORIG_BOX):
        ov = min(x1, l + w) - max(x0, l)
        if ov > best:
            best, letter = ov, i
    return letter


A_LETTER = np.array([a_letter_of_component(k) for k in A_LAB])


def b_tris(letter):
    return np.nonzero(B_LAB == letter)[0]


def a_back_tris(letter):
    return np.nonzero((A_LETTER == letter) & A_BACK)[0]


def a_frame_tris(letter):
    return np.nonzero((A_LETTER == letter) & ~A_BACK)[0]


# ---------------------------------------------------------------- the ground (the hill's collision)
def _ground():
    import os
    if not os.path.exists("orig/ground.npy"):
        from terrain import ground_tris
        np.save("orig/ground.npy", ground_tris()[0])
    return np.load("orig/ground.npy") - np.array([1413.4140625, -804.7421875, 83.4375])  # to the sign's space


GROUND = _ground()


def ground_at(x, y):
    T = GROUND
    a, b, c = T[:, 0], T[:, 1], T[:, 2]
    v0, v1 = b - a, c - a
    d = v0[:, 0] * v1[:, 1] - v0[:, 1] * v1[:, 0]
    ok = np.abs(d) > 1e-9
    px, py = x - a[:, 0], y - a[:, 1]
    u = (px * v1[:, 1] - py * v1[:, 0]) / np.where(ok, d, 1)
    v = (v0[:, 0] * py - v0[:, 1] * px) / np.where(ok, d, 1)
    inside = ok & (u >= -1e-6) & (v >= -1e-6) & (u + v <= 1 + 1e-6)
    z = (a[:, 2] + u * v0[:, 2] + v * v1[:, 2])[inside]
    z = z[z < 20]
    return z.max() if len(z) else None


def ground_under(x0, w, y=-4.0):
    return float(np.mean([ground_at(x, y) for x in np.linspace(x0, x0 + w, 5)]))


# ---------------------------------------------------------------- the layout
WIDTH = dict(font.WIDTH, N=9.0, O=9.0, I=3.38)
REUSE = {"N": 2, "O": 5, "I": 1}                    # the original letter each one is
FRAME = {"S": 6, "T": 1, "A": 4, "L": 3, "G": 7}    # the original whose frame the new one copies (second A: W too)
GAPS = [1.78, 1.78, 1.78, -1.14, 0.55, 1.78, 1.78, 0.55]   # between letters (T's bar over A's top)
LIFT = [0.44, 1.05, 0.30, -0.20, -0.80, 0.40, 0.95, 1.42, -0.10]  # bottom above the ground (VINEWOOD: -0.93 to +1.42)

total = sum(WIDTH[c] for c in WORD) + sum(GAPS)
SLOTS = []      # letter, left, bottom
x = -total / 2
for i, ch in enumerate(WORD):
    bottom = ground_under(x, WIDTH[ch]) + LIFT[i]
    SLOTS.append((ch, x, bottom))
    x += WIDTH[ch] + (GAPS[i] if i < len(GAPS) else 0)


# ---------------------------------------------------------------- output geometry + its recipe
class Out:
    def __init__(self, src):
        self.src = src
        self.pos, self.uv, self.pre, self.night, self.tris = [], [], [], [], []
        self.ops = []   # ("copy", tri_indices, (dx,dy,dz), {src_vertex: z}) / ("new", verts, tris)

    def copy(self, tri_idx, move, zfix=None):
        """the source's triangles, moved (and their feet set down to given heights)"""
        zfix = zfix or {}
        tri_idx = np.asarray(tri_idx)
        used = np.unique(self.src.tris[tri_idx][:, :3])
        base = len(self.pos)
        remap = {int(v): base + i for i, v in enumerate(used)}
        move = np.array(move, np.float32)
        for v in used:
            p = self.src.verts[v] + move  # in float32, as the mod does it
            if int(v) in zfix:
                p[2] = np.float32(zfix[int(v)])
            self.pos.append(p)
            self.uv.append(self.src.uvs[0][v])
            self.pre.append(self.src.prelit[v].astype(float))
            self.night.append(self.src.night[v].astype(float))
        for t in self.src.tris[tri_idx]:
            self.tris.append((remap[int(t[0])], remap[int(t[1])], remap[int(t[2])], int(t[3])))
        self.ops.append(("copy", tri_idx, tuple(float(m) for m in move), zfix))

    def new(self, verts, tris):
        """verts: (pos, uv, [(src_vertex, weight)]); tris: (a, b, c, material) in verts"""
        base = len(self.pos)
        for p, uv, w in verts:
            self.pos.append(np.array(p, float))
            self.uv.append(np.array(uv, float))
            self.pre.append(sum(wt * self.src.prelit[s].astype(float) for s, wt in w))
            self.night.append(sum(wt * self.src.night[s].astype(float) for s, wt in w))
        for a, b, c, m in tris:
            self.tris.append((base + a, base + b, base + c, m))
        self.ops.append(("new", verts, tris))


# ---------------------------------------------------------------- colour sources (IDW over the original's vertices)
def vertex_normals(g, normals, tri_mask):
    acc = np.zeros_like(g.verts)
    for t, n in zip(g.tris[tri_mask], normals[tri_mask]):
        acc[t[:3]] += n
    return acc / np.maximum(np.linalg.norm(acc, axis=1, keepdims=True), 1e-9)


class Source:
    def __init__(self, g, idx, coords):
        self.idx = np.asarray(idx)
        self.tree = cKDTree(coords)

    def weights(self, q, k=4):
        d, i = self.tree.query(q, k=k)
        w = 1 / np.maximum(d, 0.05) ** 2
        w /= w.sum()
        return [(int(self.idx[j]), float(x)) for j, x in zip(i, w)]


front_v = np.unique(B.tris[B.tris[:, 3] == 0][:, :3])
FRONT = Source(B, front_v, B.verts[front_v][:, [0, 2]])
back_v = np.unique(A.tris[A_BACK][:, :3])
BACK = Source(A, back_v, A.verts[back_v][:, [0, 2]])
rim_mask = B.tris[:, 3] == 1
rim_v = np.unique(B.tris[rim_mask][:, :3])
RIM_N = vertex_normals(B, B_N, rim_mask)


def rim_weights(p, n):
    cand = rim_v[(RIM_N[rim_v] @ n > 0.7) & (np.abs(B.verts[rim_v, 1] - p[1]) < 0.05)]
    if len(cand) < 3:
        cand = rim_v[np.abs(B.verts[rim_v, 1] - p[1]) < 0.05]
    d = np.linalg.norm(B.verts[cand][:, [0, 2]] - np.array([p[0], p[2]]), axis=1)
    o = np.argsort(d)[:3]
    w = 1 / np.maximum(d[o], 0.05) ** 2
    w /= w.sum()
    return [(int(cand[j]), float(x)) for j, x in zip(o, w)]


# ---------------------------------------------------------------- texture mapping (as the original's)
def uv_front(x, z):
    return (0.0123 * x + 0.5, -0.0556 * z + 0.5249)


def uv_back(x, z):
    return (-0.25 * x + 0.5, -0.25 * z + 0.6122)


def uv_rim(x, z, front, n):
    t = 0.999 if front else 0.937
    if abs(n[0]) >= abs(n[2]):  # a side: along z
        return ((t if n[0] < 0 else 1.0 - t), -0.0556 * z + 0.5249)
    return ((0.0122 * x if n[2] > 0 else -0.0122 * x) + 0.5, t)


# ---------------------------------------------------------------- a letter's face, triangulated
def inside(loops, p):
    c = False
    for l in loops:
        for i in range(len(l)):
            (x1, y1), (x2, y2) = l[i - 1], l[i]
            if (y1 > p[1]) != (y2 > p[1]) and p[0] < x1 + (p[1] - y1) * (x2 - x1) / (y2 - y1):
                c = not c
    return c


def seg_dist(p, a, b):
    a, b, p = map(np.asarray, (a, b, p))
    t = np.clip(np.dot(p - a, b - a) / max(np.dot(b - a, b - a), 1e-12), 0, 1)
    return np.linalg.norm(p - (a + t * (b - a)))


def triangulate(loops, edge=1.7, grid=2.3):
    pts = []
    for l in loops:
        for i in range(len(l)):
            a, b = np.array(l[i]), np.array(l[(i + 1) % len(l)])
            n = max(1, int(np.ceil(np.linalg.norm(b - a) / edge)))
            pts += [tuple(a + (b - a) * k / n) for k in range(n)]
    xs = [p[0] for l in loops for p in l]
    zs = [p[1] for l in loops for p in l]
    for gx in np.arange(min(xs) + grid / 2, max(xs), grid):
        for gz in np.arange(min(zs) + grid / 2, max(zs), grid):
            if inside(loops, (gx, gz)) and min(seg_dist((gx, gz), l[i - 1], l[i]) for l in loops
                                              for i in range(len(l))) > 0.75:
                pts.append((gx, gz))
    pts = np.array(pts)
    tri = Delaunay(pts)
    keep = [s for s in tri.simplices if inside(loops, pts[s].mean(0))]
    # every outline segment must be an edge
    edges = {tuple(sorted((a, b))) for s in keep for a, b in ((s[0], s[1]), (s[1], s[2]), (s[2], s[0]))}
    key = {tuple(np.round(p, 6)): i for i, p in enumerate(pts)}
    bad = 0
    for l in loops:
        for i in range(len(l)):
            a, b = np.array(l[i]), np.array(l[(i + 1) % len(l)])
            n = max(1, int(np.ceil(np.linalg.norm(b - a) / edge)))
            seq = [key[tuple(np.round(a + (b - a) * k / n, 6))] for k in range(n + 1)]
            for u, v in zip(seq, seq[1:]):
                if tuple(sorted((u, v))) not in edges:
                    bad += 1
    assert bad == 0, "outline not kept by the triangulation"
    return pts, keep


def build_letter(out_b, out_a, ch, left, bottom):
    loops = font.OUTLINES[ch]
    pts, tris = triangulate(loops)
    # the front (B, material 0: LAcreamwall1), facing -y
    verts, ftris = [], []
    for x, z in pts:
        X, Z = left + x, bottom + z
        verts.append(((X, YF, Z), uv_front(X, Z), FRONT.weights((X, Z))))
    for s in tris:
        a, b, c = (int(i) for i in s)
        n = np.cross(np.array(verts[b][0]) - verts[a][0], np.array(verts[c][0]) - verts[a][0])
        ftris.append((a, b, c, 0) if n[1] < 0 else (a, c, b, 0))
    # the edges (B, material 1: boardgate_law), facing out
    for l in loops:
        for i in range(len(l)):
            (x1, z1), (x2, z2) = l[i], l[(i + 1) % len(l)]
            n = np.array([z2 - z1, 0.0, -(x2 - x1)])
            n /= np.linalg.norm(n)
            base = len(verts)
            for (x, z) in ((x1, z1), (x2, z2)):
                X, Z = left + x, bottom + z
                for y, front in ((YF, True), (YB, False)):
                    p = (X, y, Z)
                    verts.append((p, uv_rim(X, Z, front, n), rim_weights(np.array(p), n)))
            q = [base, base + 1, base + 2, base + 3]  # p1 front, p1 back, p2 front, p2 back
            for a, b, c in ((q[0], q[2], q[1]), (q[1], q[2], q[3])):
                nn = np.cross(np.array(verts[b][0]) - verts[a][0], np.array(verts[c][0]) - verts[a][0])
                ftris.append((a, b, c, 1) if nn @ n > 0 else (a, c, b, 1))
    out_b.new(verts, ftris)
    # the back (A, material 1: boardgate_law), facing +y
    verts, btris = [], []
    for x, z in pts:
        X, Z = left + x, bottom + z
        verts.append(((X, YB, Z), uv_back(X, Z), BACK.weights((X, Z))))
    for s in tris:
        a, b, c = (int(i) for i in s)
        n = np.cross(np.array(verts[b][0]) - verts[a][0], np.array(verts[c][0]) - verts[a][0])
        btris.append((a, b, c, 1) if n[1] > 0 else (a, c, b, 1))
    out_a.new(verts, btris)


def feet(out_a, tri_idx, move):
    """the frame's lowest vertices, down to the ground where it is lower (so that no leg floats)"""
    used = np.unique(A.tris[tri_idx][:, :3])
    zmin = A.verts[used][:, 2].min()
    fix = {}
    for v in used:
        p = A.verts[v] + np.array(move)
        if A.verts[v][2] - zmin < 0.2:
            gnd = ground_at(p[0], p[1])
            if gnd is not None and gnd - 0.25 < p[2]:
                fix[int(v)] = gnd - 0.25
    return fix


def build_sign():
    out_b, out_a = Out(B), Out(A)
    for ch, left, bottom in SLOTS:
        if ch in REUSE:
            k = REUSE[ch]
            l0, b0, w0 = ORIG_BOX[k]
            move = (left - l0, 0.0, bottom - b0)
            out_b.copy(b_tris(k), move)
            out_a.copy(a_back_tris(k), move)
            fr = a_frame_tris(k)
            out_a.copy(fr, move, feet(out_a, fr, move))
        else:
            build_letter(out_b, out_a, ch, left, bottom)
            k = FRAME[ch]
            l0, b0, w0 = ORIG_BOX[k]
            fr = a_frame_tris(k)
            fv = A.verts[np.unique(A.tris[fr][:, :3])]
            frame_mid = (fv[:, 0].min() + fv[:, 0].max()) / 2
            move = (left + WIDTH[ch] / 2 - frame_mid, 0.0, bottom - b0)
            out_a.copy(fr, move, feet(out_a, fr, move))
    return out_b, out_a


# ---------------------------------------------------------------- DFF writing (the original's chunks, our geometry)
def chunk(kind, payload, ver):
    return struct.pack("<III", kind, len(payload), ver) + payload


def geometry_chunk(src, out, ver, extra_2dfx=None, matlist=None):
    pos = np.array(out.pos, np.float32)
    nv, nt = len(pos), len(out.tris)
    flags = src.flags & ~1  # triangle lists (the mesh below), not strips
    s = struct.pack("<Iiii", flags, nt, nv, 1)
    s += np.clip(np.round(np.array(out.pre)), 0, 255).astype(np.uint8).tobytes()
    s += np.array(out.uv, np.float32).tobytes()
    for a, b, c, m in out.tris:
        s += struct.pack("<HHHH", b, a, m, c)
    ctr = (pos.min(0) + pos.max(0)) / 2
    rad = float(np.linalg.norm(pos - ctr, axis=1).max())
    s += struct.pack("<4fii", *ctr, rad, 1, 0) + pos.tobytes()
    # the material list as it was
    mat = matlist
    for t, o, sz, v in rw.chunks(src.raw, 12, len(src.raw)):
        if t == rw.CHUNK_MATLIST and mat is None:
            mat = src.raw[o - 12:o + sz]
    # extensions: our mesh, the rest as it was, our night colours
    ext = b""
    tris = np.array(out.tris)
    meshes = b""
    nmeshes = 0
    total = 0
    for m in range(int(tris[:, 3].max()) + 1):
        idx = tris[tris[:, 3] == m][:, :3].ravel()
        if not len(idx):
            continue
        meshes += struct.pack("<II", len(idx), m) + idx.astype(np.uint32).tobytes()
        nmeshes += 1
        total += len(idx)
    ext += chunk(0x50E, struct.pack("<III", 0, nmeshes, total) + meshes, ver)
    for t, data in src.ext:
        if t == 0x50E:
            continue
        if t == rw.CHUNK_NIGHTCOLS:
            data = struct.pack("<I", 1) + np.clip(np.round(np.array(out.night)), 0, 255).astype(np.uint8).tobytes()
        if t == rw.CHUNK_2DFX and extra_2dfx is not None:
            data = extra_2dfx
        ext += chunk(t, data, ver)
    return chunk(rw.CHUNK_GEOMETRY, chunk(1, s, ver) + mat + chunk(3, ext, ver), ver)


def write_dff(name, src, out):
    data = open("orig/%s.dff" % name, "rb").read()
    co, cs, ver = rw.find(data, 0, len(data), rw.CHUNK_CLUMP)
    parts = b""
    for t, o, sz, v in rw.chunks(data, co, co + cs):
        if t == rw.CHUNK_GEOMLIST:
            parts += chunk(t, chunk(1, struct.pack("<i", 1), v) + geometry_chunk(src, out, v), v)
        else:
            parts += data[o - 12:o + sz]
    return chunk(rw.CHUNK_CLUMP, parts, ver)


if __name__ == "__main__":
    import os
    os.makedirs("out", exist_ok=True)
    for ch, left, bottom in SLOTS:
        print("%s left %7.2f bottom %6.2f" % (ch, left, bottom))
    ob, oa = build_sign()
    open("out/vinesign1_cunte01.dff", "wb").write(write_dff("vinesign1_cunte01", B, ob))
    open("out/vinesign1_cunte.dff", "wb").write(write_dff("vinesign1_cunte", A, oa))
    print("B", len(ob.pos), len(ob.tris), "A", len(oa.pos), len(oa.tris))


# ---------------------------------------------------------------- far away (the LOD): the letters' fronts, flat
from world import quat_matrix
from scipy.spatial import ConvexHull

LOD_ROT = quat_matrix((-0.7071067691, 0, 0, 0.7071067691))  # its placement (LAhills.ipl)
LOD_Y = -4.37                                              # in front of the letters, as the original plane


def coarse(loops):
    for edge in (4.0, 3.0, 2.0, 1.5, 1.0):
        try:
            return triangulate(loops, edge=edge, grid=1000)
        except AssertionError:
            pass
    raise AssertionError("no coarse triangulation")


def cream_of_lod():
    t = rw.read_txd(open("orig/lahillsa_lod.txd", "rb").read())["vinewood2lod"].rgba
    solid = t[t[:, :, 3] == 255][:, :3]
    return tuple(int(round(c)) for c in solid.mean(0))


LOD_W = L.verts @ LOD_ROT.T
LOD_SRC = Source(L, np.arange(len(L.verts)), LOD_W[:, [0, 2]])


def build_lod():
    out = Out(L)
    for ch, left, bottom in SLOTS:
        pts, tris = coarse(font.OUTLINES[ch])
        verts, ft = [], []
        for x, z in pts:
            w = np.array([left + x, LOD_Y, bottom + z])
            verts.append((LOD_ROT.T @ w, (0.0, 0.0), LOD_SRC.weights((w[0], w[2]))))
        for s in tris:
            a, b, c = (int(i) for i in s)
            n = np.cross(LOD_ROT @ (verts[b][0] - verts[a][0]), LOD_ROT @ (verts[c][0] - verts[a][0]))
            ft.append((a, b, c, 0) if n[1] < 0 else (a, c, b, 0))
        out.new(verts, ft)
    return out


def material_list(colour, ver):
    surf = struct.pack("<fff", 1.0, 0.0, 1.0)
    mat = chunk(rw.CHUNK_STRUCT, struct.pack("<i4Bii", 0, *colour, 0, 0) + surf, ver) + chunk(3, b"", ver)
    return chunk(rw.CHUNK_MATLIST, chunk(1, struct.pack("<ii", 1, -1), ver) + chunk(rw.CHUNK_MATERIAL, mat, ver), ver)


# ---------------------------------------------------------------- collision (COL3), ours
def col3(name, model_id, verts, faces, light=111):
    verts = np.asarray(verts, float)
    vmin, vmax = verts.min(0), verts.max(0)
    ctr = (vmin + vmax) / 2
    rad = float(np.linalg.norm(verts - ctr, axis=1).max())
    q = np.round(verts * 128).astype(np.int16)
    body = struct.pack("<10f", *vmin, *vmax, *ctr, rad)
    head = 28 + 40 + 12 + 24 + 12              # where the data starts, counted from after the fourcc
    vsize = len(q) * 6
    vsize += (-vsize) % 4
    off_v, off_f = head, head + vsize
    body += struct.pack("<HHHBBI", 0, 0, len(faces), 0, 0, 2)
    body += struct.pack("<6I", 0, 0, 0, off_v, off_f, 0) + struct.pack("<3I", 0, 0, 0)
    data = q.tobytes().ljust(vsize, b"\0")
    for a, b, c in faces:
        data += struct.pack("<3HBB", a, b, c, 0, light)
    payload = name.encode().ljust(22, b"\0") + struct.pack("<H", model_id) + body + data
    return b"COL3" + struct.pack("<I", len(payload)) + payload


def solid_letter(loops, left, bottom):
    """a letter as a closed solid: front, back and edges"""
    pts, tris = coarse(loops)
    n = len(pts)
    verts = [(left + x, YF, bottom + z) for x, z in pts] + [(left + x, YB, bottom + z) for x, z in pts]
    faces = []
    for s in tris:
        a, b, c = (int(i) for i in s)
        faces += [(a, b, c), (a + n, c + n, b + n)]
    key = {tuple(np.round(p, 6)): i for i, p in enumerate(pts)}
    for l in loops:
        for i in range(len(l)):
            p, q = np.array(l[i]), np.array(l[(i + 1) % len(l)])
            for edge in (4.0, 3.0, 2.0, 1.5, 1.0):  # the points the coarse triangulation put on this side
                k = max(1, int(np.ceil(np.linalg.norm(q - p) / edge)))
                seq = [key.get(tuple(np.round(p + (q - p) * j / k, 6))) for j in range(k + 1)]
                if None not in seq:
                    break
            for u, v in zip(seq, seq[1:]):
                faces += [(u, v, v + n), (u, v + n, u + n)]
    return verts, faces


def build_col(out_a):
    letters_v, letters_f = [], []
    for ch, left, bottom in SLOTS:
        v, f = solid_letter(font.OUTLINES[ch], left, bottom)
        base = len(letters_v)
        letters_v += v
        letters_f += [(a + base, b + base, c + base) for a, b, c in f]
    frames_v, frames_f = [], []
    for op in out_a.ops:     # every frame copied: its convex hull
        if op[0] != "copy" or np.all(A_BACK[op[1]]):
            continue
        used = np.unique(A.tris[op[1]][:, :3])
        p = A.verts[used] + np.array(op[2])
        for v, z in op[3].items():
            p[np.searchsorted(used, v), 2] = z
        p = p[p[:, 1] > YB - 0.01]
        hull = ConvexHull(p)
        base = len(frames_v)
        frames_v += [tuple(x) for x in p[hull.vertices]]
        remap = {int(v): base + i for i, v in enumerate(hull.vertices)}
        for s, eq in zip(hull.simplices, hull.equations):
            a, b, c = (remap[int(i)] for i in s)
            nn = np.cross(np.array(frames_v[b]) - frames_v[a], np.array(frames_v[c]) - frames_v[a])
            frames_f.append((a, b, c) if nn @ eq[:3] > 0 else (a, c, b))
    return (col3("VineSign1_cunte01", 13831, letters_v, letters_f),
            col3("VineSign1_cunte", 13722, frames_v, frames_f))


def write_lod(out):
    data = open("orig/lodvinesign1_cunte.dff", "rb").read()
    co, cs, ver = rw.find(data, 0, len(data), rw.CHUNK_CLUMP)
    parts = b""
    for t, o, sz, v in rw.chunks(data, co, co + cs):
        if t == rw.CHUNK_GEOMLIST:
            parts += chunk(t, chunk(1, struct.pack("<i", 1), v) + geometry_chunk(L, out, v, matlist=material_list(
                cream_of_lod() + (255,), v)), v)
        else:
            parts += data[o - 12:o + sz]
    return chunk(rw.CHUNK_CLUMP, parts, ver)


if __name__ == "__main__":
    ol = build_lod()
    open("out/lodvinesign1_cunte.dff", "wb").write(write_lod(ol))
    c01, c = build_col(build_sign()[1])
    open("out/vinesign.col", "wb").write(c01 + c)
    print("LOD", len(ol.pos), len(ol.tris), "cream", cream_of_lod(), "col", len(c01), len(c))
