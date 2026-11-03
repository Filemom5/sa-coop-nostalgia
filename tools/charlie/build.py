"""Charlie Harper's beach house (docs/casa-charlie.md), from the SketchUp model of it (model.dae, already in metres) to
the game: the model and the collision of santahouse04_law2, the block of houses of Santa Maria Beach whose place it
takes (the mod loads ours when the game loads that block: features/charlie). Writes the data the mod uses.

  * what goes: the sample person of SketchUp, the backs of the faces (SketchUp's own material_0, a copy of each face),
    and the doors' leaves (the doorways stay open: one walks in);
  * where: its middle on the block's middle, turned round: the deck and the beach doors toward the sea (south), the
    front door to the road; its floor 0.25 m over the sand, on a ground plate as big as the old block;
  * colours: by what a face is (the material, which way it looks, how much sky it sees): salmon walls and a terracotta
    roof outside as in the show's shots of Malibu, cream walls, terracotta floor and wooden beams inside, glass. They
    are in the vertex colours (the game's day and night ones), the materials white; every face both ways (the model has
    only one side of each).

python3 build.py model.dae [CharlieData.h]  (out/: the .dff and the .col, to look at)"""
import collections
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}
ORIGIN = np.array([265.969, -1757.578, 8.906])  # santahouse04_law2 (LAw2 stream; heading 0), from coop\lote.txt
OLD_BOX = (-22.27, -12.35, 22.27, 12.36)        # its collision's box, x and y
FLOOR = 3.6                                     # the house's floor (the sand there: 3.2 to 3.5)
GROUND = 3.45                                   # the plate of sand under it all
CELL = 0.5                                      # the roof's map (no rain under it)
VER = 0x1803FFFF                                # RenderWare 3.6.0.3, as the game's files
DROP = ("material_0", "Laura", "edge_color")
SUN = np.array([0.35, -0.55, 0.76])
SUN /= np.linalg.norm(SUN)

SALMON, ROOF, CREAM, TILE, CEILING = (214, 146, 118), (190, 112, 78), (236, 218, 166), (182, 102, 64), (214, 186, 142)
WOOD, SIDING, GLASS, SAND = (112, 70, 40), (163, 127, 96), (176, 206, 226), (196, 172, 128)
MAT_COLOUR = {"Color_C08": WOOD, "Wood_Veneer_02": (191, 129, 92), "Wallpaper_Stripe": (223, 186, 133),
              "Cladding_Siding_Tan": SIDING}


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
    doors = door_groups(parts)
    keep = [(mat, T) for path, mat, T in parts if path not in doors and not any(mat.startswith(d) for d in DROP)]
    tris = np.concatenate([T for _, T in keep])
    mats = np.concatenate([[mat] * len(T) for mat, T in keep])
    lo, hi = tris.reshape(-1, 3).min(0), tris.reshape(-1, 3).max(0)
    mid = (lo + hi) / 2
    # turned round about the middle: x and y negated
    local = np.empty_like(tris)
    local[..., 0] = -(tris[..., 0] - mid[0])
    local[..., 1] = -(tris[..., 1] - mid[1]) + (OLD_BOX[1] + OLD_BOX[3]) / 2
    local[..., 2] = tris[..., 2] + FLOOR - ORIGIN[2]    # (negating two axes keeps the faces' winding)
    # the same face twice (in two groups): once
    seen, out_t, out_m = set(), [], []
    for t, m in zip(local, mats):
        k = tuple(sorted(tuple(np.round(v * 1000).astype(int)) for v in t))
        n = np.cross(t[1] - t[0], t[2] - t[0])
        if k in seen or np.linalg.norm(n) < 1e-6:
            continue
        seen.add(k)
        out_t.append(t)
        out_m.append(m)
    print("model: %d faces kept of %d (%d door groups out), %.1f x %.1f x %.1f m" % (
        len(out_t), sum(len(T) for _, _, T in parts), len(doors), *(hi - lo)))
    return np.array(out_t), np.array(out_m)


def ground_plate():
    """sand under the old block's whole box, under the house's floor"""
    x0, y0, x1, y1 = OLD_BOX
    z = GROUND - ORIGIN[2]
    a, b, c, d = (x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)
    return np.array([[a, b, c], [a, c, d]], float)


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


# ---------------------------------------------------------------- colours
def colour(mats, tris, normals, piece_open, corner_open):
    """per corner, day and night, and the faces' alpha"""
    z = tris[..., 2].mean(1) + ORIGIN[2] - FLOOR  # over the floor
    base = np.zeros((len(tris), 3))
    alpha = np.full(len(tris), 255)
    for i, (m, n, o) in enumerate(zip(mats, normals, piece_open)):
        if m == "sand":
            base[i] = SAND
        elif "Glass" in m:
            base[i], alpha[i] = GLASS, 110
        elif m in MAT_COLOUR:
            base[i] = MAT_COLOUR[m]
        elif n[2] > 0.7:
            base[i] = ROOF if (o > 0.45 and z[i] > 2.0) else TILE
        elif n[2] < -0.7:
            base[i] = CEILING
        else:
            base[i] = SALMON if o > 0.3 else CREAM
    sun = np.clip(normals @ SUN, 0, 1)[:, None]
    o = corner_open
    inside = np.clip(1.0 - o / 0.35, 0, 1)            # 1: deep inside
    day = base[:, None, :] * (0.45 + 0.45 * o + 0.18 * sun * o + 0.27 * inside)[..., None]
    lamp = np.array([1.0, 0.86, 0.66])
    night = base[:, None, :] * ((0.16 + 0.10 * o)[..., None] + lamp * (0.50 * inside)[..., None])
    clip = lambda c: np.clip(np.round(c), 0, 255).astype(np.uint8)
    return clip(day), clip(night), alpha


# ---------------------------------------------------------------- the files
def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), VER) + payload


def write_dff(tris, day, night, alpha, frame_name):
    nt = len(tris)
    pos = tris.reshape(-1, 3).astype(np.float32)          # every face its own three corners (flat colours)
    nv = len(pos)
    assert nv < 65536, nv
    a = np.repeat(alpha, 3)[:, None]
    pre = np.concatenate([day.reshape(-1, 3), a], 1).astype(np.uint8)   # per corner
    nig = np.concatenate([night.reshape(-1, 3), a], 1).astype(np.uint8)
    mat_of = (alpha < 255).astype(int)                    # 0 opaque, 1 glass
    # each face both ways
    idx = np.arange(nv).reshape(nt, 3)
    faces_ = np.concatenate([idx, idx[:, [0, 2, 1]]])
    fmat = np.concatenate([mat_of, mat_of])
    flags = 0x02 | 0x08 | 0x40                            # positions, prelit, modulate material colour
    s = struct.pack("<Iiii", flags, len(faces_), nv, 1) + pre.tobytes()
    for (a, b, c), m in zip(faces_, fmat):
        s += struct.pack("<HHHH", b, a, m, c)
    lo, hi = pos.min(0), pos.max(0)
    ctr = (lo + hi) / 2
    rad = float(np.linalg.norm(pos - ctr, axis=1).max())
    s += struct.pack("<4fii", *ctr, rad, 1, 0) + pos.tobytes()
    surf = struct.pack("<fff", 1.0, 0.0, 1.0)
    mats = chunk(0x07, chunk(0x01, struct.pack("<i4Bii", 0, 255, 255, 255, 255, 0, 0) + surf) + chunk(0x03, b"")) + \
        chunk(0x07, chunk(0x01, struct.pack("<i4Bii", 0, 255, 255, 255, 255, 0, 0) + surf) + chunk(0x03, b""))
    matlist = chunk(0x08, chunk(0x01, struct.pack("<iii", 2, -1, -1)) + mats)
    meshes, nmesh, total = b"", 0, 0
    for m in (0, 1):
        f = faces_[fmat == m].ravel()
        if len(f):
            meshes += struct.pack("<II", len(f), m) + f.astype(np.uint32).tobytes()
            nmesh += 1
            total += len(f)
    ext = chunk(0x50E, struct.pack("<III", 0, nmesh, total) + meshes)
    ext += chunk(0x253F2F9, struct.pack("<I", 1) + nig.tobytes())
    geometry = chunk(0x0F, chunk(0x01, s) + matlist + chunk(0x03, ext))
    frame = struct.pack("<9f3fii", 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1, 0)
    frames = chunk(0x0E, chunk(0x01, struct.pack("<i", 1) + frame) + chunk(0x03, chunk(0x253F2FE, frame_name.encode())))
    geoms = chunk(0x1A, chunk(0x01, struct.pack("<i", 1)) + geometry)
    atomic = chunk(0x14, chunk(0x01, struct.pack("<iiii", 0, 0, 5, 0)) + chunk(0x03, b""))
    return chunk(0x10, chunk(0x01, struct.pack("<iii", 1, 0, 0)) + frames + geoms + atomic + chunk(0x03, b""))


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


def roof_map(tris, mats):
    """the cells (CELL m) a roof covers: something over 2 m above the floor, over the cell's middle"""
    floor = FLOOR - ORIGIN[2]
    P = tris.reshape(-1, 3)
    x0, y0 = np.floor(P[:, 0].min() / CELL) * CELL, np.floor(P[:, 1].min() / CELL) * CELL
    w = int(np.ceil((P[:, 0].max() - x0) / CELL)) + 1
    h = int(np.ceil((P[:, 1].max() - y0) / CELL)) + 1
    grid = np.zeros((h, w), bool)
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    for t, m in zip(tris, mats):
        if m == "sand" or t[:, 2].min() < floor + 2.0:
            continue
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        if abs(d) < 1e-6:
            continue
        u = ((cx - ax) * (qy - ay) - (qx - ax) * (cy - ay)) / d
        v = ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / d
        grid |= (u >= 0) & (v >= 0) & (u + v <= 1)
    bits = np.packbits(grid.ravel(), bitorder="little")
    roof = max(t[:, 2].max() for t, m in zip(tris, mats) if m != "sand")
    print("roof map: %d x %d cells, %d covered" % (w, h, grid.sum()))
    return x0, y0, w, h, bits.tobytes(), roof


def main(dae, header=None):
    tris, mats = faces(load(dae))
    plate = ground_plate()
    tris = np.concatenate([tris, plate])
    mats = np.concatenate([mats, ["sand", "sand"]])
    normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    sky = Sky(tris)
    mid = tris.mean(1)
    front, back = sky(mid + normals * 0.25, normals), sky(mid - normals * 0.25, -normals)
    piece = surfaces(tris, normals)
    area = np.linalg.norm(np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]), axis=1)
    sums = lambda v: np.bincount(piece, v * area, len(tris)) / np.maximum(np.bincount(piece, area, len(tris)), 1e-9)
    f_mean, b_mean = sums(front)[piece], sums(back)[piece]
    # a piece with sky on its back but not in front (a one-sided SketchUp face turned inward): its back is what counts
    normals[b_mean > f_mean + 0.25] *= -1
    piece_open = np.maximum(f_mean, b_mean)
    corners = tris + normals[:, None, :] * 0.25
    corner_open = sky(corners.reshape(-1, 3), np.repeat(normals, 3, axis=0)).reshape(-1, 3)
    print("light: %d flat pieces" % len(np.unique(piece)))
    day, night, alpha = colour(mats, tris, normals, piece_open, corner_open)
    dff = write_dff(tris, day, night, alpha, "santahouse04_law2")
    out_ = (piece_open > 0.2) | (mats == "sand")          # far away (LODtahouse04_law2): what is seen from outside
    lod = write_dff(tris[out_], day[out_], night[out_], alpha[out_], "LODtahouse04_law2")
    rx, ry, rw, rh, rbits, rtop = roof_map(tris, mats)
    col = write_col("santahouse04_law2", 6285, tris)
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    (out / "santahouse04_law2.dff").write_bytes(dff)
    (out / "santahouse04_law2.col").write_bytes(col)
    np.savez(out / "preview.npz", tris=tris, day=day, night=night, alpha=alpha)
    print("dff %d bytes (far: %d), col %d bytes" % (len(dff), len(lod), len(col)))
    if header:
        text = ["// Generated by tools/charlie/build.py from the SketchUp model of Charlie Harper's house: do not edit.",
                "// The model and the collision of santahouse04_law2 (Santa Maria Beach) the mod loads in place of the game's.",
                "#pragma once", "", "namespace coop::charlie::data {", "",
                "// santahouse04_law2 (near) and LODtahouse04_law2 (far: the outside only)",
                c_array("kHouseDff", dff), "", c_array("kLodDff", lod), "",
                "// from the collision file's 32-byte header on (what CFileLoader::LoadCollisionModelVer3 reads)",
                c_array("kHouseCol", col[32:]), "",
                "// where a roof is over (no rain there): cells of %.1f m from the block's x, y; a bit each, row by row" % CELL,
                "inline constexpr float kRoofX = %.2ff, kRoofY = %.2ff, kRoofCell = %.2ff, kRoofTop = %.2ff;  // the block's axes"
                % (rx, ry, CELL, rtop),
                "inline constexpr int kRoofW = %d, kRoofH = %d;" % (rw, rh),
                c_array("kRoofBits", rbits), "", "}  // namespace coop::charlie::data", ""]
        Path(header).write_text("\n".join(text))


if __name__ == "__main__":
    main(*sys.argv[1:])
