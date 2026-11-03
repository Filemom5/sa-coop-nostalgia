"""Charlie Harper's beach house (docs/casa-charlie.md), from the SketchUp model of it (model.dae, in metres) to the
game, at 1.6 times its size (the game's people and doors are bigger than life: its doors are 3.2 m, as the game's big
ones): the model and the collision of santahouse04_law2, the block of Santa Maria Beach whose place it takes, and of
santahouse02_law2, the block east of it, which becomes sand (the house goes over part of it). The mod loads ours when
the game loads those blocks (features/charlie). Writes the data the mod uses.

  * what goes: the sample person of SketchUp, the backs of the faces (SketchUp's own material_0, a copy of each face),
    and the doors' leaves (the doorways stay open: one walks in);
  * where: turned round (the deck and the beach doors toward the sea, south; the front door to the road), its west
    side by the west neighbour, its front where the old block's was; the floor at 3.85 m, over the sand and the
    sidewalk, its base going down into the sand;
  * the look: our textures (textures.py: stucco, clay roof tiles, terracotta floor tiles, plank ceiling, dark wood,
    Spanish tiles on the stairs' risers...) by what a face is (its material, which way it looks, how much sky it sees);
    the light in the vertex colours, day and night, as the game's own buildings have it (the game doubles them: 128 is
    the texture as it is): from how much sky each corner sees, warm lamps inside at night. Every face both ways (the
    model has only one side of each);
  * the stairs: a ramp over each flight in the collision (the steps are 28 cm at this size), the steps seen;
  * what of the map stood there (palms, bushes...): listed for the mod to hide (from coop\\lote.txt).

python3 build.py model.dae lote.txt [CharlieData.h]  (out/: the .dff and .col, preview.npz)"""
import collections
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np

import textures

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}
ORIGIN = np.array([265.969, -1757.578, 8.906])  # santahouse04_law2 (LAw2 stream; heading 0), from coop\lote.txt
EAST = np.array([307.266, -1760.547, 7.625])    # santahouse02_law2, the block east of it
OLD_BOX = (-22.27, -12.35, 22.27, 12.36)        # their collisions' boxes, x and y
EAST_BOX = (-19.22, -10.71, 19.22, 10.72)
SCALE = 1.6
WEST, NORTH = -21.9, 11.1                       # the house's west side and front, in the block's axes
FLOOR = 3.85                                    # its floor (the sand: 3.2 to 3.5; the sidewalk up to 3.7)
GROUND = 3.45                                   # the plate of sand under the blocks
BASE = 2.9                                      # the house's base goes down to here
CELL = 0.5                                      # the roof's map (no rain under it)
VER = 0x1803FFFF                                # RenderWare 3.6.0.3, as the game's files
DROP = ("material_0", "Laura", "edge_color")
SUN = np.array([0.35, -0.55, 0.76])
SUN /= np.linalg.norm(SUN)
GLASS = (176, 206, 226, 110)
# the stairs, in the model's plan (turned round, unscaled; z over the floor): the house's flight and the deck's
STAIRS = [((3.55, 0.25, -0.05), (6.3, 4.0, 2.85)), ((-6.35, -7.6, -1.5), (-4.95, -5.35, 0.05))]
# texture: metres of the house (game size) one texture covers
TEX_SIZE = {"nch_stucco": 3.0, "nch_plaster": 3.0, "nch_tile": 1.0, "nch_roof": 2.4, "nch_ceiling": 2.0,
            "nch_wood": 1.5, "nch_floorwood": 2.4, "nch_sand": 4.0, "nch_siding": 2.6}
MAT_TEX = {"Color_C08": "nch_wood", "Color_M00": "nch_wood", "Wood_Veneer_02": "nch_floorwood",
           "Wallpaper_Stripe": "nch_plaster", "Cladding_Siding_Tan": "nch_siding", "sand": "nch_sand"}
# what of the map is the blocks and the road: not hidden
KEEP_MODELS = {6285, 6433, 6284, 6434, 6294, 6432, 6428, 6438}


# ---------------------------------------------------------------- reading the model
def load(path):
    """[(group path, material name, triangles n x 3 x 3)] in metres, the model's own axes"""
    root = ET.parse(path).getroot()
    unit = float(root.find("c:asset/c:unit", NS).get("meter"))
    names = {m.get("id"): m.get("name") for m in root.findall(".//c:library_materials/c:material", NS)}
    geoms = {}
    for g in root.findall(".//c:library_geometries/c:geometry", NS):
        m = g.find("c:mesh", NS)
        src = {s.get("id"): np.array(s.find("c:float_array", NS).text.split(), float)
               for s in m.findall("c:source", NS) if s.find("c:float_array", NS) is not None and s.find("c:float_array", NS).text}
        pos = src[m.find("c:vertices/c:input[@semantic='POSITION']", NS).get("source")[1:]].reshape(-1, 3)
        parts = []
        for tr in m.findall("c:triangles", NS):
            inputs = tr.findall("c:input", NS)
            stride = max(int(i.get("offset")) for i in inputs) + 1
            voff = [int(i.get("offset")) for i in inputs if i.get("semantic") == "VERTEX"][0]
            p = tr.find("c:p", NS)
            if p is not None and p.text:
                parts.append((tr.get("material"), np.array(p.text.split(), int).reshape(-1, stride)[:, voff].reshape(-1, 3)))
        geoms[g.get("id")] = (pos, parts)
    out = []

    def walk(node, M, path):
        mm = node.find("c:matrix", NS)
        if mm is not None:
            M = M @ np.array(mm.text.split(), float).reshape(4, 4)
        for ig in node.findall("c:instance_geometry", NS):
            P, parts = geoms[ig.get("url")[1:]]
            bind = {im.get("symbol"): im.get("target")[1:] for im in ig.findall(".//c:instance_material", NS)}
            W = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3] * unit
            for sym, idx in parts:
                out.append(("/".join(path), names.get(bind.get(sym), "?"), W[idx]))
        for ch in node.findall("c:node", NS):
            walk(ch, M, path + [ch.get("name") or ch.get("id") or ""])
        for inode in node.findall("c:instance_node", NS):
            tgt = root.find(".//c:node[@id='%s']" % inode.get("url")[1:], NS)
            if tgt is not None:
                walk(tgt, M, path + ["<" + (tgt.get("name") or "") + ">"])

    for n in root.find(".//c:library_visual_scenes/c:visual_scene", NS).findall("c:node", NS):
        walk(n, np.eye(4), [n.get("name") or ""])
    return out


def door_groups(parts):
    """the groups that are a door's leaf: about 0.6-1.3 m wide, 1.8-2.5 m tall, thin, on a floor"""
    boxes = {}
    for path, mat, T in parts:
        p = T.reshape(-1, 3)
        lo, hi = p.min(0), p.max(0)
        if path in boxes:
            lo, hi = np.minimum(lo, boxes[path][0]), np.maximum(hi, boxes[path][1])
        boxes[path] = (lo, hi)
    doors = set()
    for path, (lo, hi) in boxes.items():
        s = hi - lo
        if path.count("/") == 1 and 1.8 < s[2] < 2.5 and 0.5 < max(s[0], s[1]) < 1.3 and min(s[0], s[1]) < 0.25 and lo[2] < 0.5:
            doors.add(path)
    return doors


# ---------------------------------------------------------------- the faces we keep, in the block's place
def faces(parts):
    """(tris in the block's axes, materials, plan: the same faces in the model's plan: turned round, unscaled, z over
    the floor)"""
    doors = door_groups(parts)
    keep = [(mat, T) for path, mat, T in parts if path not in doors and not any(mat.startswith(d) for d in DROP)]
    tris = np.concatenate([T for _, T in keep])
    mats = np.concatenate([[mat] * len(T) for mat, T in keep])
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    mid = (lo + hi) / 2
    plan = np.empty_like(tris)  # turned round about the middle (x and y negated: the faces' winding stays)
    plan[..., 0] = -(tris[..., 0] - mid[0])
    plan[..., 1] = -(tris[..., 1] - mid[1])
    plan[..., 2] = tris[..., 2]
    seen, keep_i = set(), []
    for i, t in enumerate(plan):  # the same face twice (in two groups): once
        k = tuple(sorted(tuple(np.round(v * 1000).astype(int)) for v in t))
        if k in seen or np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])) < 1e-6:
            continue
        seen.add(k)
        keep_i.append(i)
    plan, mats = plan[keep_i], mats[keep_i]
    half = (hi - lo) / 2
    local = plan * SCALE
    local[..., 0] += WEST + half[0] * SCALE
    local[..., 1] += NORTH - half[1] * SCALE
    local[..., 2] += FLOOR - ORIGIN[2]
    # the base: the slab's underside (20-30 cm under the floor) down into the sand, but not the stairs'
    low = (plan[..., 2] > -0.35) & (plan[..., 2] < -0.05)
    for (a, b) in STAIRS:
        inside = np.all((plan >= np.array(a)) & (plan <= np.array(b)), axis=-1)
        low &= ~inside
    local[..., 2][low] = BASE - ORIGIN[2]
    print("model: %d faces kept of %d (%d door groups out), %.1f x %.1f x %.1f m in the game" % (
        len(plan), sum(len(T) for _, _, T in parts), len(doors), *((hi - lo) * SCALE)))
    return local, mats, plan


def plate(box, z, origin):
    """sand over a block's box, in that block's axes"""
    x0, y0, x1, y1 = box
    z -= origin[2]
    a, b, c, d = (x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)
    return np.array([[a, b, c], [a, c, d]], float)


# ---------------------------------------------------------------- the stairs' ramps (collision only)
def ramps(plan, tris, normals_plan):
    """per riser of the stairs (vertical, 9-21 cm): a slope from the step below's edge to its own, so one walks up a
    ramp through the steps' edges"""
    out = []
    up = normals_plan[:, 2] > 0.95
    zs = plan[..., 2]
    for (a, b) in STAIRS:
        a, b = np.array(a), np.array(b)
        inbox = np.all((plan >= a - 1e-3) & (plan <= b + 1e-3), axis=(1, 2))
        treads = np.where(inbox & up)[0]

        def height(x, y):
            """the treads' heights over (x, y)"""
            hs = []
            for i in treads:
                (ax, ay, az), (bx, by, _), (cx, cy, _) = plan[i]
                d = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
                if abs(d) < 1e-9:
                    continue
                u = ((x - ax) * (cy - ay) - (cx - ax) * (y - ay)) / d
                v = ((bx - ax) * (y - ay) - (x - ax) * (by - ay)) / d
                if u >= -1e-3 and v >= -1e-3 and u + v <= 1 + 1e-3:
                    hs.append(az)
            return hs

        risers = collections.defaultdict(list)
        for i in np.where(inbox & (np.abs(normals_plan[:, 2]) < 0.05))[0]:
            h = zs[i].max() - zs[i].min()
            if not 0.09 < h < 0.21:
                continue
            axis = 0 if abs(normals_plan[i, 0]) > abs(normals_plan[i, 1]) else 1
            risers[(round(zs[i].min(), 3), round(zs[i].max(), 3), axis, round(plan[i, 0, axis], 3))].append(i)
        n = 0
        for (zlo, zhi, axis, at), fs in risers.items():
            other = 1 - axis
            span = plan[fs][..., other]
            s0, s1 = span.min(), span.max()
            if s1 - s0 < 0.6:  # a step's side, not its riser
                continue
            mid = (s0 + s1) / 2
            for sign in (-1, 1):  # sign: toward the step below
                p = [0, 0]
                p[axis], p[other] = at + sign * 0.08, mid
                below = height(*p)
                p[axis] = at - sign * 0.08
                above = height(*p)
                if any(abs(h - zhi) < 0.02 for h in above) and (not below or min(below) <= zlo + 0.02) and \
                        not any(h > zlo + 0.02 for h in below):
                    break
            else:
                continue
            depth = 0.27  # the treads' depth
            q = []
            for s, z in ((s0, zlo), (s1, zlo), (s1, zhi), (s0, zhi)):
                p = [0, 0, z]
                p[other] = s
                p[axis] = at + sign * depth if z == zlo else at
                q.append(p)
            out += [[q[0], q[1], q[2]], [q[0], q[2], q[3]]]
            n += 1
        print("stairs: %d risers ramped" % n)
    return np.array(out, float).reshape(-1, 3, 3)


# ---------------------------------------------------------------- what a face is
def classify(mats, plan, normals, open_, piece_area):
    """each side's texture (or 'glass'): normals and open_ are that side's"""
    z = plan[..., 2]
    zmid, h = z.mean(1), z.max(1) - z.min(1)
    area = np.linalg.norm(np.cross(plan[:, 1] - plan[:, 0], plan[:, 2] - plan[:, 0]), axis=1) / 2
    on_stairs = np.zeros(len(plan), bool)
    for (lo, hi) in STAIRS:
        on_stairs |= np.all((plan >= np.array(lo) - 1e-3) & (plan <= np.array(hi) + 1e-3), axis=(1, 2))
    out = []
    for i, m in enumerate(mats):
        n, o = normals[i], open_[i]
        if "Glass" in m:
            out.append("glass")
        elif m == "sand":
            out.append("nch_sand")
        elif n[2] > 0.2 and o > 0.45 and zmid[i] > 2.0:
            out.append("nch_roof")
        elif abs(n[2]) < 0.05 and 0.09 < h[i] < 0.21 and (on_stairs[i] or (z[i].min() > -0.01 and z[i].max() < 0.16)):
            out.append("nch_riser")
        elif m == "Color_C08" and n[2] < -0.2 and piece_area[i] > 2.0:
            out.append("nch_ceiling")            # the boards between the beams
        elif m in MAT_TEX:
            out.append(MAT_TEX[m])
        elif n[2] > 0.2:
            if 0.05 < zmid[i] < 2.7 and area[i] < 1.5 or zmid[i] >= 2.7:
                out.append("nch_floorwood")
            else:
                out.append("nch_tile")
        elif n[2] < -0.2:
            out.append("nch_ceiling")
        else:
            out.append("nch_stucco" if o > 0.3 else "nch_plaster")
    return np.array(out)


def uvs(tris, normals, tex, plan):
    """per corner: planar, by the way the face looks; the risers: one row of tiles each"""
    uv = np.zeros(tris.shape[:2] + (2,))
    for i, (t, n, k) in enumerate(zip(tris, normals, tex)):
        a = np.abs(n)
        if k == "nch_riser":
            hz = (t[:, 2].max() - t[:, 2].min()) or 0.3
            along = t[:, 1] if a[0] > a[1] else t[:, 0]
            uv[i, :, 0] = along / (4 * hz)
            uv[i, :, 1] = (t[:, 2].max() - t[:, 2]) / (4 * hz)
            continue
        size = TEX_SIZE.get(k, 3.0)
        if a[2] >= a[0] and a[2] >= a[1]:
            if k == "nch_roof" and a[0] > a[1]:  # the tiles' rows across the slope
                uv[i] = t[:, [1, 0]] / size
            else:
                uv[i] = t[:, :2] / size
        elif a[0] >= a[1]:
            uv[i, :, 0], uv[i, :, 1] = t[:, 1] / size, -t[:, 2] / size
        else:
            uv[i, :, 0], uv[i, :, 1] = t[:, 0] / size, -t[:, 2] / size
    return uv


def light(normals, corner_open, glass):
    """the vertex colours, day and night (the game doubles them: 128 = as the texture is)"""
    sun = np.clip(normals @ SUN, 0, 1)[:, None]
    o = corner_open
    inside = np.clip(1.0 - o / 0.35, 0, 1)            # 1: deep inside
    day = 128 * (0.56 + 0.36 * o + 0.12 * sun * o + 0.20 * inside)
    lamp = np.array([1.0, 0.84, 0.62])
    night = 128 * ((0.30 + 0.08 * o)[..., None] * np.array([0.86, 0.9, 1.0]) + lamp * (0.52 * inside)[..., None])
    day = np.repeat(day[..., None], 3, -1)
    day[glass], night[glass] = 128, 128 * 0.5
    clip = lambda c: np.clip(np.round(c), 0, 255).astype(np.uint8)
    return clip(day), clip(night)


# ---------------------------------------------------------------- the files
def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), VER) + payload


def string(s):
    b = s.encode() + b"\0"
    return chunk(0x02, b.ljust((len(b) + 3) // 4 * 4, b"\0"))


def write_dff(tris, uv, day, night, tex, frame_name):
    """the triangles as they are (one side each); vertices shared where everything of them is the same"""
    names = sorted(set(tex) - {"glass"}) + (["glass"] if "glass" in tex else [])
    mat_of = np.array([names.index(k) for k in tex])
    nt = len(tris)
    pos = tris.reshape(-1, 3).astype(np.float32)
    uvf = uv.reshape(-1, 2).astype(np.float32)
    pre = np.concatenate([day.reshape(-1, 3), np.full((nt * 3, 1), 255, np.uint8)], 1)
    nig = np.concatenate([night.reshape(-1, 3), np.full((nt * 3, 1), 255, np.uint8)], 1)
    rec = np.concatenate([pos.view(np.uint8).reshape(-1, 12), uvf.view(np.uint8).reshape(-1, 8), pre, nig], 1)
    uniq, first, inv = np.unique(rec, axis=0, return_index=True, return_inverse=True)
    order = np.argsort(first)                      # in the faces' order
    rank = np.empty_like(order)
    rank[order] = np.arange(len(order))
    idx = rank[inv.ravel()].reshape(nt, 3)
    pos, uvf, pre, nig = pos[first[order]], uvf[first[order]], pre[first[order]], nig[first[order]]
    nv = len(pos)
    assert nv < 65536, nv
    faces_, fmat = idx, mat_of
    flags = 0x02 | 0x04 | 0x08 | 0x40 | (1 << 16)  # positions, textured, prelit, modulate material colour; 1 UV set
    s = struct.pack("<Iiii", flags, len(faces_), nv, 1) + pre.tobytes() + uvf.tobytes()
    s += np.stack([faces_[:, 1], faces_[:, 0], fmat, faces_[:, 2]], 1).astype("<u2").tobytes()
    lo, hi = pos.min(0), pos.max(0)
    ctr = (lo + hi) / 2
    rad = float(np.linalg.norm(pos - ctr, axis=1).max())
    s += struct.pack("<4fii", *ctr, rad, 1, 0) + pos.tobytes()
    surf = struct.pack("<fff", 1.0, 0.0, 1.0)
    mats = b""
    for k in names:
        if k == "glass":
            mats += chunk(0x07, chunk(0x01, struct.pack("<i4Bii", 0, *GLASS, 0, 0) + surf) + chunk(0x03, b""))
        else:
            texture = chunk(0x06, chunk(0x01, struct.pack("<I", 0x1106)) + string(k) + string("") + chunk(0x03, b""))
            mats += chunk(0x07, chunk(0x01, struct.pack("<i4Bii", 0, 255, 255, 255, 255, 0, 1) + surf) + texture +
                          chunk(0x03, b""))
    matlist = chunk(0x08, chunk(0x01, struct.pack("<i", len(names)) + struct.pack("<i", -1) * len(names)) + mats)
    meshes, total = b"", 0
    for m in range(len(names)):
        f = faces_[fmat == m].ravel()
        meshes += struct.pack("<II", len(f), m) + f.astype("<u4").tobytes()
        total += len(f)
    ext = chunk(0x50E, struct.pack("<III", 0, len(names), total) + meshes)
    ext += chunk(0x253F2F9, struct.pack("<I", 1) + nig.tobytes())
    geometry = chunk(0x0F, chunk(0x01, s) + matlist + chunk(0x03, ext))
    frame = struct.pack("<9f3fii", 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, 0)
    frames = chunk(0x0E, chunk(0x01, struct.pack("<i", 1) + frame) + chunk(0x03, chunk(0x253F2FE, frame_name.encode())))
    geoms = chunk(0x1A, chunk(0x01, struct.pack("<i", 1)) + geometry)
    atomic = chunk(0x14, chunk(0x01, struct.pack("<iiii", 0, 0, 5, 0)) + chunk(0x03, b""))
    print("  %s: %d faces, %d vertices, materials %s" % (frame_name, nt, nv, ", ".join(names)))
    return chunk(0x10, chunk(0x01, struct.pack("<iii", 1, 0, 0)) + frames + geoms + atomic + chunk(0x03, b""))


# ---------------------------------------------------------------- how much sky a point sees
class Sky:
    """the model in 20 cm cells; a point's share of its half of 32 directions that leave it without a hit (16 m)"""

    def __init__(self, tris, voxel=0.2):
        pts = []
        for a, b, c in tris:  # the faces' surface, sampled every ~8 cm
            n = max(2, int(max(np.linalg.norm(b - a), np.linalg.norm(c - a), np.linalg.norm(c - b)) / 0.08) + 1)
            u, v = np.meshgrid(np.linspace(0, 1, n), np.linspace(0, 1, n))
            m = u + v <= 1
            pts.append(a + np.outer(u[m], b - a) + np.outer(v[m], c - a))
        pts = np.concatenate(pts)
        self.voxel, self.min = voxel, pts.min(0) - 1.0
        self.dims = np.ceil((pts.max(0) + 1.0 - self.min) / voxel).astype(int) + 1
        self.occ = np.zeros(self.dims, bool)
        i = np.floor((pts - self.min) / voxel).astype(int)
        self.occ[i[:, 0], i[:, 1], i[:, 2]] = True
        k = np.arange(32) + 0.5
        phi, th = np.arccos(1 - 2 * k / 32), np.pi * (1 + 5 ** 0.5) * k
        self.dirs = np.c_[np.cos(th) * np.sin(phi), np.sin(th) * np.sin(phi), np.cos(phi)]

    def __call__(self, points, normals):
        out = np.zeros(len(points))
        for s in range(0, len(points), 2000):
            o, n = points[s:s + 2000], normals[s:s + 2000]
            use = (n @ self.dirs.T) > 0.15
            hit = np.zeros(use.shape, bool)
            for step in range(1, 80):
                i = np.floor((o[:, None, :] + self.dirs[None] * (step * self.voxel) - self.min) / self.voxel).astype(int)
                inside = np.all((i >= 0) & (i < self.dims), axis=2)
                i = np.clip(i, 0, self.dims - 1)
                hit |= inside & self.occ[i[..., 0], i[..., 1], i[..., 2]]
            out[s:s + 2000] = ((~hit) & use).sum(1) / np.maximum(use.sum(1), 1)
        return out


def surfaces(tris, normals):
    """the faces in flat pieces (a wall round its windows: one piece), so a piece is coloured as a whole"""
    key = lambda v: tuple(np.round(v * 1000).astype(int))
    parent = list(range(len(tris)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    edges = collections.defaultdict(list)
    for f, t in enumerate(tris):
        k = [key(v) for v in t]
        for e in ((k[0], k[1]), (k[1], k[2]), (k[2], k[0])):
            edges[tuple(sorted(e))].append(f)
    plane = np.einsum("ij,ij->i", normals, tris[:, 0])
    for fs in edges.values():
        for a in fs:
            for b in fs:
                if a < b:
                    d = normals[a] @ normals[b]
                    if abs(d) > 0.999 and abs(plane[a] - np.sign(d) * plane[b]) < 0.01:
                        parent[root(a)] = root(b)
    return np.array([root(i) for i in range(len(tris))])


def write_col(name, model_id, tris):
    """COL3 (as tools/sign: vertices in 1/128 m), the faces as one solid each side they show"""
    q = np.round(tris.reshape(-1, 3) * 128).astype(np.int16)
    uniq, inv = np.unique(q, axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    faces_ = [tuple(f) for f in inv if len(set(f)) == 3]
    verts = uniq.astype(float) / 128
    vmin, vmax = verts.min(0), verts.max(0)
    ctr = (vmin + vmax) / 2
    rad = float(np.linalg.norm(verts - ctr, axis=1).max())
    assert len(faces_) < 65536 and len(uniq) < 65536
    body = struct.pack("<10f", *vmin, *vmax, *ctr, rad)
    head = 28 + 40 + 12 + 24 + 12
    vsize = len(uniq) * 6
    vsize += (-vsize) % 4
    body += struct.pack("<HHHBBI", 0, 0, len(faces_), 0, 0, 2)
    body += struct.pack("<6I", 0, 0, 0, head, head + vsize, 0) + struct.pack("<3I", 0, 0, 0)
    data = uniq.tobytes().ljust(vsize, b"\0")
    for a, b, c in faces_:
        data += struct.pack("<3HBB", a, b, c, 0, 111)
    payload = name.encode().ljust(22, b"\0") + struct.pack("<H", model_id) + body + data
    print("collision: %d faces, %d vertices" % (len(faces_), len(uniq)))
    return b"COL3" + struct.pack("<I", len(payload)) + payload


def c_array(name, data):
    lines = ["alignas(4) inline constexpr unsigned char %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join("0x%02x" % x for x in data[i:i + 24]) + ",")
    lines.append("};")
    return "\n".join(lines)


def roof_map(tris, tex):
    """the cells (CELL m) a roof covers: something over 3 m above the floor (2 m of the model), over the cell's middle"""
    P = tris.reshape(-1, 3)
    x0, y0 = np.floor(P[:, 0].min() / CELL) * CELL, np.floor(P[:, 1].min() / CELL) * CELL
    w = int(np.ceil((P[:, 0].max() - x0) / CELL)) + 1
    h = int(np.ceil((P[:, 1].max() - y0) / CELL)) + 1
    grid = np.zeros((h, w), bool)
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    floor = FLOOR - ORIGIN[2]
    for t, k in zip(tris, tex):
        if k == "nch_sand" or t[:, 2].min() < floor + 2.0 * SCALE:
            continue
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        if abs(d) < 1e-6:
            continue
        u = ((cx - ax) * (qy - ay) - (qx - ax) * (cy - ay)) / d
        v = ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / d
        grid |= (u >= 0) & (v >= 0) & (u + v <= 1)
    roof = max(t[:, 2].max() for t, k in zip(tris, tex) if k != "nch_sand")
    print("roof map: %d x %d cells, %d covered" % (w, h, grid.sum()))
    return x0, y0, w, h, np.packbits(grid.ravel(), bitorder="little").tobytes(), roof, grid


def hidden(lote, tris, tex):
    """what of the map stands where the house is (its floor, 1.5 m round) or on the east block: (model, x, y)"""
    floor = FLOOR - ORIGIN[2]
    keep = [t for t, k in zip(tris, tex) if k != "nch_sand" and t[:, 2].min() < floor + 0.5]
    P = np.concatenate(keep).reshape(-1, 3)
    x0, y0 = P[:, 0].min() - 2, P[:, 1].min() - 2
    w, h = int((P[:, 0].max() + 2 - x0) / 0.5) + 1, int((P[:, 1].max() + 2 - y0) / 0.5) + 1
    grid = np.zeros((h, w), bool)
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * 0.5, y0 + (np.arange(h) + 0.5) * 0.5)
    for t in keep:
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        if abs(d) < 1e-6:
            continue
        u = ((cx - ax) * (qy - ay) - (qx - ax) * (cy - ay)) / d
        v = ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / d
        grid |= (u >= -0.05) & (v >= -0.05) & (u + v <= 1.05)
    for _ in range(3):  # 1.5 m round
        g = grid.copy()
        g[1:] |= grid[:-1]
        g[:-1] |= grid[1:]
        g[:, 1:] |= grid[:, :-1]
        g[:, :-1] |= grid[:, 1:]
        grid = g
    out = []
    for line in Path(lote).read_text().splitlines():
        p = line.split()
        if not p or p[0] not in ("B", "O") or int(p[1]) in KEEP_MODELS:
            continue
        model, x, y = int(p[1]), float(p[2]), float(p[3])
        lx, ly = x - ORIGIN[0], y - ORIGIN[1]
        ex, ey = x - EAST[0], y - EAST[1]
        i, j = int((ly - y0) / 0.5), int((lx - x0) / 0.5)
        on_house = 0 <= i < h and 0 <= j < w and grid[i, j]
        on_east = EAST_BOX[0] < ex < EAST_BOX[2] and EAST_BOX[1] < ey < EAST_BOX[3]
        if (on_house or on_east) and (model, round(x, 1), round(y, 1)) not in [(m, round(a, 1), round(b, 1)) for m, a, b in out]:
            out.append((model, x, y))
    print("hidden: %s" % ", ".join("%d at %.1f,%.1f" % o for o in out))
    return out


def main(dae, lote, header=None):
    tris, mats, plan = faces(load(dae))
    # the plate of sand: the old block's box, under the house
    tris = np.concatenate([tris, plate(OLD_BOX, GROUND, ORIGIN)])
    plan = np.concatenate([plan, np.zeros((2, 3, 3)) - 9])
    mats = np.concatenate([mats, ["sand", "sand"]])
    normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    sky = Sky(tris)
    mid = tris.mean(1)
    front, back = sky(mid + normals * 0.4, normals), sky(mid - normals * 0.4, -normals)
    piece = surfaces(tris, normals)
    area = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1) / 2
    sums = lambda v: np.bincount(piece, v * area, len(tris)) / np.maximum(np.bincount(piece, area, len(tris)), 1e-9)
    f_open, b_open = sums(front)[piece], sums(back)[piece]
    piece_area = np.bincount(piece, area, len(tris))[piece] / SCALE ** 2
    corners = tris.reshape(-1, 3)
    n3 = np.repeat(normals, 3, axis=0)
    f_corner = sky(corners + n3 * 0.4, n3).reshape(-1, 3)
    b_corner = sky(corners - n3 * 0.4, -n3).reshape(-1, 3)
    print("light: %d flat pieces" % len(np.unique(piece)))
    # each face both ways, each side by what it looks at: the back of a roof is a ceiling, of an outside wall a room's
    sides = []
    for sign, open_, corner in ((1, f_open, f_corner), (-1, b_open, b_corner)):
        n = normals * sign
        tex = classify(mats, plan, n, open_, piece_area)
        uv = uvs(tris, n, tex, plan)
        day, night = light(n, corner, tex == "glass")
        t = tris if sign > 0 else tris[:, [0, 2, 1]]
        if sign < 0:
            uv, day, night = uv[:, [0, 2, 1]], day[:, [0, 2, 1]], night[:, [0, 2, 1]]
        sides.append((t, uv, day, night, tex, open_))
    T, UV, DAY, NIGHT, TEX, OPEN = (np.concatenate(x) for x in zip(*sides))
    print("textures: %s" % dict(collections.Counter(TEX)))
    dff = write_dff(T, UV, DAY, NIGHT, TEX, "santahouse04_law2")
    far = (OPEN > 0.2) | (TEX == "nch_sand")                       # far away: what is seen from outside
    lod = write_dff(T[far], UV[far], DAY[far], NIGHT[far], TEX[far], "LODtahouse04_law2")
    tex = sides[0][4]
    # the collision: the faces, and the stairs' ramps
    nplan = np.cross(plan[:, 1] - plan[:, 0], plan[:, 2] - plan[:, 0])
    nplan /= np.maximum(np.linalg.norm(nplan, axis=1), 1e-9)[:, None]
    ramp = ramps(plan[:-2], tris[:-2], nplan[:-2])
    off = np.array([tris[0, 0, 0] - plan[0, 0, 0] * SCALE, tris[0, 0, 1] - plan[0, 0, 1] * SCALE, FLOOR - ORIGIN[2]])
    ramp_l = ramp * SCALE + off                    # (as faces() places the plan)
    col = write_col("santahouse04_law2", 6285, np.concatenate([tris, ramp_l]) if len(ramp_l) else tris)
    # the east block: sand
    east = plate(EAST_BOX, GROUND, EAST)
    en = np.tile([0, 0, 1.0], (2, 1))
    euv = east[..., :2] / TEX_SIZE["nch_sand"]
    eday, enight = light(en, np.ones((2, 3)), np.zeros(2, bool))
    etex = np.array(["nch_sand", "nch_sand"])
    east_dff = write_dff(east, euv, eday, enight, etex, "santahouse02_law2")
    east_lod = write_dff(east, euv, eday, enight, etex, "LODtahouse02_law2")
    east_col = write_col("santahouse02_law2", 6284, east)
    rx, ry, rw, rh, rbits, rtop, _ = roof_map(tris, tex)
    hide = hidden(lote, tris, tex)
    tx = textures.make()
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    (out / "santahouse04_law2.dff").write_bytes(dff)
    (out / "santahouse04_law2.col").write_bytes(col)
    np.savez(out / "preview.npz", tris=T, day=DAY, night=NIGHT, uv=UV, tex=TEX)
    print("dff %d bytes (far: %d), col %d bytes" % (len(dff), len(lod), len(col)))
    if not header:
        return
    text = ["// Generated by tools/charlie/build.py from the SketchUp model of Charlie Harper's house: do not edit.",
            "// The models and collisions the mod loads in place of the game's (santahouse04_law2: the house; "
            "santahouse02_law2: sand), the textures, the roof's map and what of the map to hide.",
            "#pragma once", "", "#include <cstdint>", "", "namespace coop::charlie::data {", "",
            "// santahouse04_law2 (near) and LODtahouse04_law2 (far: the outside only)",
            c_array("kHouseDff", dff), "", c_array("kLodDff", lod), "",
            "// santahouse02_law2 and LODtahouse02_law2: sand", c_array("kEastDff", east_dff), "",
            c_array("kEastLodDff", east_lod), "",
            "// the collisions, from the file's 32-byte header on (what CFileLoader::LoadCollisionModelVer3 reads)",
            c_array("kHouseCol", col[32:]), "", c_array("kEastCol", east_col[32:]), "",
            "// where a roof is over (no rain there): cells of %.1f m from the block's x, y; a bit each, row by row" % CELL,
            "inline constexpr float kRoofX = %.2ff, kRoofY = %.2ff, kRoofCell = %.2ff, kRoofTop = %.2ff;  // the block's axes"
            % (rx, ry, CELL, rtop),
            "inline constexpr float kFloorZ = %.2ff;  // the house's floor (world)" % FLOOR,
            "inline constexpr int kRoofW = %d, kRoofH = %d;" % (rw, rh), c_array("kRoofBits", rbits), "",
            "// what of the map stood there: hidden (model, world x, y)",
            "struct Hidden {", "    int model;", "    float x, y;", "};",
            "inline constexpr Hidden kHidden[] = {"] + \
           ["    {%d, %.3ff, %.3ff}," % h for h in hide] + ["};", "",
            "// the textures (textures.py), DXT1, square", "struct Texture {", "    const char* name;", "    int size;",
            "    const unsigned char* dxt1;", "};"]
    names = []
    for name, rgb in tx.items():
        text += [c_array("kTex_" + name, textures.dxt1(rgb)), ""]
        names.append('    {"%s", %d, kTex_%s},' % (name, rgb.shape[0], name))
    text += ["inline constexpr Texture kTextures[] = {"] + names + ["};", "", "}  // namespace coop::charlie::data", ""]
    Path(header).write_text("\n".join(text))


if __name__ == "__main__":
    main(*sys.argv[1:])
