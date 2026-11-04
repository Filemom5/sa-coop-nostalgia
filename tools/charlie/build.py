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

import details
import furniture
import lot
import roofs
import textures

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}
ORIGIN = np.array([265.969, -1757.578, 8.906])  # santahouse04_law2 (LAw2 stream; heading 0), from coop\lote.txt
EAST = np.array([307.266, -1760.547, 7.625])    # santahouse02_law2, the block east of it
OLD_BOX = (-22.27, -12.35, 22.27, 12.36)        # their collisions' boxes, x and y
EAST_BOX = (-19.22, -10.71, 19.22, 10.72)
SCALE = 1.6
WEST, NORTH = -21.9, 8.5                        # the house's west side and front, in the block's axes (its front
                                                # 3.8 m from the street's wall: the walk along it)
DY = NORTH - 11.1                               # where the house went from the old block's front (the plan's numbers below)
FLOOR = 5.3                                     # its floor: 1.7 m over the street, 2.1 over the sand (the deck's stairs reach it)
GROUND = 3.45                                   # the plate of sand under the blocks
LOT = (-22.2, -27.5, 42.0, 12.3)                # the lot: the old block, and the east block's west part (x, y)
# up from the street (y 12.3): the drive (x from, to, its top's y), on to the garage's doors; the steps to the front
# door's court (9 steps of 30 cm), the gate at their top
RAMPS = [(-21.4, -14.0, 0.3, "drive"), (17.9, 20.7, 12.3 - 9 * 0.30, "steps")]


def beach_y(x):
    """south of it the lot is beach (the house over it on pilings): the old block's south side, the east block's; the
    drive goes on to the garage's doors"""
    return -18.7 if x < -7.5 else (-13.8 if x > 22.2 else -12.4)


# the flat roofs (heights over the floor) that get a hipped roof, the upper floor's two terraces (not roofed)
ROOF_LEVELS = [(4.3, 4.7), (6.9, 7.5), (8.5, 9.1)]
TERRACES = [(17.0, -0.2 + DY, 30.5, 9.0 + DY), (30.4, -10.6 + DY, 35.2, -3.6 + DY)]
# the court before the front door: a pergola over it (x, y from, to)
COURT = (16.45, 3.6 + DY, 22.05, 9.4 + DY)
# for the group's homes (/casa marcar): before the front door (in the court, facing it), the save disk inside by it
DOOR = (16.9, 4.45 + DY, 90.0)
DISK = (15.0, 4.45 + DY)
# iron lanterns on the walls by the doors: (x, y, height over the floor, the wall's outward x, y)
LANTERNS = [(16.62, 5.9 + DY, 3.0, 1, 0), (21.9, 4.6 + DY, 3.0, -1, 0), (-9.12, -3.3 + DY, 3.0, -1, 0),
            (-9.12, -1.1 + DY, 3.0, -1, 0), (-21.0, -15.55 + DY, 3.7, 0, 1), (-15.9, -15.55 + DY, 3.7, 0, 1),
            (-10.6, -15.55 + DY, 3.7, 0, 1), (27.92, 5.9 + DY, 3.0, 1, 0), (30.92, -6.2 + DY, 3.0, 1, 0)]
EAST_CUT = 308.0                                # the east block: what is west of this x goes (the safe house stays)
# the glass sliding doors (their leaves the door finder takes for doors): the second leaf of each pair over the first
SLIDING = {"SketchUp/group_69": (0, 0), "SketchUp/group_70": (-0.90, 0.06),
           "SketchUp/group_71": (0, 0), "SketchUp/group_72": (0.06, -0.89)}
LIVING = ((-3.4, 2.7), (3.5, 9.5))              # the sunken living room (plan): a dark wood floor
CELL = 0.5                                      # the roof's map (no rain under it)
VER = 0x1803FFFF                                # RenderWare 3.6.0.3, as the game's files
DROP = ("material_0", "Laura", "edge_color")
SUN = np.array([0.35, -0.55, 0.76])
SUN /= np.linalg.norm(SUN)
GLASS = (58, 74, 86, 120)  # dark, as windows look from outside; the room seen through
# the stairs, in the model's plan (turned round, unscaled; z over the floor): the house's flight and the deck's
STAIRS = [((3.55, 0.25, -0.05), (6.3, 4.0, 2.85)), ((-6.35, -7.6, -1.5), (-4.95, -5.35, 0.05))]
# texture: metres of the house (game size) one texture covers
TEX_SIZE = {"nch_stucco": 3.0, "nch_plaster": 3.0, "nch_tile": 2.0, "nch_roof": 2.4, "nch_ceiling": 2.0,
            "nch_wood": 1.5, "nch_floorwood": 2.4, "nch_darkwood": 2.4, "nch_sand": 4.0, "nch_siding": 2.6,
            "nch_paving": 1.6, "nch_timber": 2.4, "nch_under": 3.0, "nch_white": 1.0, "nch_green": 1.6, "nch_cap": 0.8,
            "nch_door": 1.6, "nch_glow": 0.4, "nch_joist": 2.4}
PLASTER_STOREY = 2.8 * SCALE  # the inside walls' texture: a storey high, its skirting at the bottom (both floors)
MAT_TEX = {"piano_black": "nch_black", "piano_keys": "nch_keys", "site_paving": "nch_paving",
           "site_stucco": "nch_stucco", "site_timber": "nch_timber", "site_under": "nch_under", "site_wood": "nch_wood",
           "site_white": "nch_white", "site_cap": "nch_cap", "site_green": "nch_green", "site_lamp": "nch_black",
           "site_glow": "nch_glow", "site_tile": "nch_tile", "door_leaf": "nch_door", "garage_door": "nch_siding",
           "rail_iron": "nch_black", "roof_tile": "nch_roof", "Color_C08": "nch_wood", "Color_M00": "nch_wood", "Wood_Veneer_02": "nch_floorwood",
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
    the floor). The glass sliding doors stay, one leaf of each pair slid over the other (half the doorway open)"""
    doors = door_groups(parts)
    rest = np.concatenate([T for path, mat, T in parts if path not in doors and not path.endswith("/open")
                           and not any(mat.startswith(d) for d in DROP)])
    lo, hi = rest.reshape(-1, 3).min(0), rest.reshape(-1, 3).max(0)
    mid = (lo + hi) / 2
    keep = [(path, mat, T) for path, mat, T in parts
            if (path not in doors or path in SLIDING) and not any(mat.startswith(d) for d in DROP)]
    tris = np.concatenate([T for _, _, T in keep])
    mats = np.concatenate([[mat] * len(T) for _, mat, T in keep])
    paths = np.concatenate([[path] * len(T) for path, _, T in keep])
    plan = np.empty_like(tris)  # turned round about the middle (x and y negated: the faces' winding stays)
    plan[..., 0] = -(tris[..., 0] - mid[0])
    plan[..., 1] = -(tris[..., 1] - mid[1])
    plan[..., 2] = tris[..., 2]
    for path, (dx, dy) in SLIDING.items():
        sel = paths == path
        plan[sel, :, 0] += dx
        plan[sel, :, 1] += dy
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
    print("model: %d faces kept of %d (%d door leaves out, %d slid), %.1f x %.1f x %.1f m in the game" % (
        len(plan), sum(len(T) for _, _, T in parts), len(doors) - len(SLIDING), len(SLIDING), *((hi - lo) * SCALE)))
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
    inbox = lambda box: np.all((plan >= np.array(box[0]) - 1e-3) & (plan <= np.array(box[1]) + 1e-3), axis=(1, 2))
    on_stairs, deck_stairs = inbox(STAIRS[0]), inbox(STAIRS[1])  # the house's flight; the deck's, down to the sand
    out = []
    geometric = {"Color_C08", "Color_M00", "Wood_Veneer_02", "Wallpaper_Stripe", "Cladding_Siding_Tan", "sand"}
    for i, m in enumerate(mats):
        n, o = normals[i], open_[i]
        if "Glass" in m:
            out.append("glass")
        elif m == "roof_tile":
            out.append("nch_roof" if n[2] > 0 else "nch_wood")  # the eaves from below: wood
        elif m in MAT_TEX and m not in geometric:
            out.append(MAT_TEX[m])
        elif deck_stairs[i] and m == "material":
            out.append("nch_timber")             # wooden, as the deck
        elif m == "sand":
            out.append("nch_sand")
        elif n[2] < -0.2 and abs(zmid[i]) < 0.3 and o > 0.15:
            out.append("nch_joist")              # the floor from under the house, over the beach
        elif 0.2 < n[2] < 0.95 and o > 0.45 and zmid[i] > 2.0:
            out.append("nch_roof")
        elif n[2] >= 0.95 and o > 0.45 and zmid[i] > 2.0:
            out.append("nch_tile")               # the terraces
        elif abs(n[2]) < 0.05 and 0.09 < h[i] < 0.21 and (on_stairs[i] or (z[i].min() > -0.01 and z[i].max() < 0.16)):
            out.append("nch_riser")
        elif m == "Wood_Veneer_02" and n[2] < -0.2:
            out.append("nch_ceiling")            # the boards between the beams, from below: light
        elif m in MAT_TEX:
            out.append(MAT_TEX[m])
        elif n[2] > 0.2:
            (lx, ly), (hx, hy) = LIVING
            c = plan[i].mean(0)
            if abs(zmid[i]) < 0.05 and lx < c[0] < hx and ly < c[1] < hy:
                out.append("nch_darkwood")
            elif 0.05 < zmid[i] < 2.7 and area[i] < 1.5 or zmid[i] >= 2.7:
                out.append("nch_floorwood")
            else:
                out.append("nch_tile")
        elif n[2] < -0.2:
            out.append("nch_ceiling")
        else:
            out.append("nch_stucco" if o > 0.3 else "nch_plaster")
    return np.array(out)


def uvs(tris, normals, tex, given, floor=0.0):
    """per corner: planar, by the way the face looks; the risers: one row of tiles each; given ones (not NaN) as they
    are"""
    uv = np.zeros(tris.shape[:2] + (2,))
    for i, (t, n, k) in enumerate(zip(tris, normals, tex)):
        if not np.isnan(given[i, 0, 0]):
            uv[i] = given[i]
            continue
        a = np.abs(n)
        if k == "nch_riser":
            hz = (t[:, 2].max() - t[:, 2].min()) or 0.3
            along = t[:, 1] if a[0] > a[1] else t[:, 0]
            uv[i, :, 0] = along / (4 * hz)
            uv[i, :, 1] = (t[:, 2].max() - t[:, 2]) / (4 * hz)
            continue
        size = TEX_SIZE.get(k, 3.0)
        if k == "nch_plaster" and a[2] < 0.3:  # a storey a texture, its skirting on the floors
            along = t[:, 1] if a[0] >= a[1] else t[:, 0]
            uv[i, :, 0] = along / PLASTER_STOREY
            uv[i, :, 1] = (floor + PLASTER_STOREY - t[:, 2]) / PLASTER_STOREY
            continue
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


def light(normals, corner_open, glass, corners=None, lamps=(), glow=None):
    """the vertex colours, day and night (the game doubles them: 128 = as the texture is)"""
    sun = np.clip(normals @ SUN, 0, 1)[:, None]
    o = corner_open
    inside = np.clip(1.0 - o / 0.35, 0, 1)            # 1: deep inside
    day = 128 * (0.56 + 0.36 * o + 0.12 * sun * o + 0.20 * inside)
    lamp = np.array([1.0, 0.84, 0.62])
    night = 128 * ((0.30 + 0.08 * o)[..., None] * np.array([0.86, 0.9, 1.0]) + lamp * (0.52 * inside)[..., None])
    if corners is not None and len(lamps):  # the lanterns light what is near them
        L = np.array(lamps)
        d = np.linalg.norm(corners[:, :, None, :] - L[None, None], axis=3)
        k = (np.clip(1 - d / 5.0, 0, 1) ** 2).sum(2)
        night = night + np.array([1.0, 0.78, 0.5]) * (95 * k)[..., None]
    day = np.repeat(day[..., None], 3, -1)
    day[glass], night[glass] = 128, 128 * 0.5
    if glow is not None:
        night[glow] = 255
    clip = lambda c: np.clip(np.round(c), 0, 255).astype(np.uint8)
    return clip(day), clip(night)


# ---------------------------------------------------------------- the files
def chunk(kind, payload):
    return struct.pack("<III", kind, len(payload), VER) + payload


def string(s):
    b = s.encode() + b"\0"
    return chunk(0x02, b.ljust((len(b) + 3) // 4 * 4, b"\0"))


def write_dff(tris, uv, day, night, tex, frame_name, lights=()):
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
    if len(lights):  # 2dfx: the lanterns' lights (coronas and the light on the ground at night)
        fx = struct.pack("<I", len(lights))
        for x, y, z in lights:
            light_ = struct.pack("<4B4f5B", 255, 196, 128, 200, 120.0, 7.0, 0.55, 4.0, 0, 0, 0, 60, 1 | 64)
            light_ += b"coronastar".ljust(24, b"\0") + b"shad_exp".ljust(24, b"\0") + struct.pack("<BBxx", 10, 0)
            light_ = light_[:76]
            fx += struct.pack("<3fII", x, y, z, 0, len(light_)) + light_
        ext += chunk(0x253F2F8, fx)
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
        on_east = EAST_BOX[0] < ex < EAST_BOX[2] and EAST_BOX[1] < ey < EAST_BOX[3] and x < EAST_CUT
        if (on_house or on_east) and (model, round(x, 1), round(y, 1)) not in [(m, round(a, 1), round(b, 1)) for m, a, b in out]:
            out.append((model, x, y))
    print("hidden: %s" % ", ".join("%d at %.1f,%.1f" % o for o in out))
    return out


def clip_base(tris, mats, plan, which, z0=-0.3):
    """the faces of which cut at plan height z0: what is under it goes (the house's base, down into the sand)"""
    out_t, out_m, out_p = [], [], []
    scale_off = None
    cut = 0
    for i in range(len(tris)):
        zs = plan[i, :, 2]
        if not which[i] or zs.min() >= z0:
            out_t.append(tris[i]), out_m.append(mats[i]), out_p.append(plan[i])
            continue
        cut += 1
        if zs.max() <= z0:
            continue
        poly_t, poly_p = [], []
        for k in range(3):
            a, b = k, (k + 1) % 3
            za, zb = zs[a], zs[b]
            if za >= z0:
                poly_t.append(tris[i, a]), poly_p.append(plan[i, a])
            if (za >= z0) != (zb >= z0):
                f = (z0 - za) / (zb - za)
                poly_t.append(tris[i, a] + (tris[i, b] - tris[i, a]) * f)
                poly_p.append(plan[i, a] + (plan[i, b] - plan[i, a]) * f)
        for k in range(1, len(poly_t) - 1):
            t = np.array([poly_t[0], poly_t[k], poly_t[k + 1]])
            if np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])) < 1e-6:
                continue
            out_t.append(t), out_m.append(mats[i]), out_p.append(np.array([poly_p[0], poly_p[k], poly_p[k + 1]]))
    print("base: %d faces cut at the floor" % cut)
    return np.array(out_t), np.array(out_m), np.array(out_p)


def wall_at(house, x, y, z, d):
    """the first wall from (x, y) at height z going d (unit, along x or y), facing back: its x or y, else None"""
    axis = 0 if d[0] else 1
    o = 1 - axis
    sign = d[axis]
    n = np.cross(house[:, 1] - house[:, 0], house[:, 2] - house[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1), 1e-9)[:, None]
    at, other = (x, y)[axis], (x, y)[o]
    sel = (n[:, axis] * -sign > 0.9) & (house[..., 2].min(1) < z) & (house[..., 2].max(1) > z) & \
          (house[..., o].min(1) < other) & (house[..., o].max(1) > other)
    c = house[sel][..., axis].mean(1)
    ahead = c[(c - at) * sign > 0]
    return ahead[np.argmin(np.abs(ahead - at))] if len(ahead) else None


def decorate(house, plan, pnorm, mats, off):
    """furniture.DECOR where it goes: (model, what, plan x, y, z, heading, height), as furniture.place gives"""
    out = []
    zf = FLOOR - ORIGIN[2]
    for model, what, x, y, where, floor, extra in furniture.DECOR:
        y = y + (DY if where != "lot" else 0.0)
        if where == "lot":           # on the lot's top
            z_plan = -0.03 / SCALE
            head = extra
            px, py = (x - off[0]) / SCALE, (y - off[1]) / SCALE
            out.append((model, what, px, py, z_plan, head, 0.0))
            continue
        if where == "wall":          # a picture: on the wall that way (extra: its direction, degrees)
            d = (round(np.cos(np.radians(extra))), round(np.sin(np.radians(extra))))
            zc = zf + (0.4 if floor == "ground" else 2.8 * SCALE + 0.2) + 1.7
            w = wall_at(house, x, y, zc, d)
            if w is None:
                print("  decor: %s (%d): no wall, left out" % (what, model))
                continue
            if d[0]:
                x = w - d[0] * 0.04
            else:
                y = w - d[1] * 0.04
            nx, ny = -d[0], -d[1]   # its front: away from the wall (the game's objects look to -y)
            head = np.degrees(np.arctan2(nx, -ny)) % 360
            px, py = (x - off[0]) / SCALE, (y - off[1]) / SCALE
            fz = furniture.floor_z(plan, pnorm, mats, np.array([px, py]), floor)
            if fz is None:
                continue
            out.append((model, what, px, py, fz, head, 1.6))
            continue
        px, py = (x - off[0]) / SCALE, (y - off[1]) / SCALE  # on the house's floor
        fz = furniture.floor_z(plan, pnorm, mats, np.array([px, py]), floor)
        if fz is None:
            print("  decor: %s (%d): no floor, left out" % (what, model))
            continue
        out.append((model, what, px, py, fz, extra, 0.0))
    print("decor: %d pieces" % len(out))
    return out


def extras(house):
    """the pergola over the front door's court, iron lanterns on the walls by the doors: (tris, mats, lamps)"""
    m = lot.Mesh()
    zf = FLOOR - ORIGIN[2]
    x0, y0, x1, y1 = COURT
    zb = zf + 3.4
    for y in np.arange(y0 + 0.4, y1 + 0.01, 0.7):  # the rafters across, from wall to wall
        m.box(x0 - 0.1, y - 0.07, zb, x1 + 0.1, y + 0.07, zb + 0.3, "site_wood", bottom=True)
    for x in (x0 + 0.6, x1 - 0.6):                 # two beams along, on posts at the open end
        m.box(x - 0.09, y0, zb + 0.3, x + 0.09, y1 + 0.5, zb + 0.5, "site_wood", bottom=True)
        m.box(x - 0.14, y1 + 0.22, zf - 0.03, x + 0.14, y1 + 0.5, zb + 0.3, "site_wood")
    n = np.cross(house[:, 1] - house[:, 0], house[:, 2] - house[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1), 1e-9)[:, None]
    lamps = []
    for x, y, hgt, nx, ny in LANTERNS:
        z = zf + hgt
        # the wall: the nearest face looking out (nx, ny) behind the given point
        axis = 0 if nx else 1
        o = 1 - axis
        sel = (n[:, axis] * (nx or ny) > 0.9) & (house[..., 2].min(1) < z) & (house[..., 2].max(1) > z + 0.5) & \
              (house[..., o].min(1) < (y, x)[axis] + 1e-3) & (house[..., o].max(1) > (y, x)[axis] - 1e-3)
        at = (x, y)[axis]
        faces_at = house[sel][..., axis].mean(1)
        behind = faces_at[(faces_at - at) * (nx or ny) < 0.6]
        wall = behind[np.argmax(behind * (nx or ny))] if len(behind) else at
        px, py = (wall + nx * 0.28, y) if nx else (x, wall + ny * 0.28)
        if nx:  # its bracket from the wall
            m.box(min(wall, px), py - 0.03, z + 0.48, max(wall, px), py + 0.03, z + 0.54, "site_lamp")
        else:
            m.box(px - 0.03, min(wall, py), z + 0.48, px + 0.03, max(wall, py), z + 0.54, "site_lamp")
        lot.lantern(m, px, py, z)
        lamps.append((px, py, z + 0.25))
    T, M = m.arrays()
    print("extras: the pergola, %d lanterns on the walls" % len(lamps))
    return T, M, lamps


def main(dae, lote, header=None):
    """dae: the house's model; lote: coop\\lote.txt; header: CharlieData.h"""
    parts = load(dae)
    parts = details.open_doors(parts, door_groups(parts), SLIDING)  # the doors open, the garage's rolled up
    tris, mats, plan = faces(parts)
    # the house's base under its floor (down into the sand) goes: the house stands on its pilings (the deck's stairs stay)
    deck = np.all((plan >= np.array(STAIRS[1][0]) - 1e-3) & (plan <= np.array(STAIRS[1][1]) + 1e-3), axis=(1, 2))
    tris, mats, plan = clip_base(tris, mats, plan, ~deck)
    off = np.array([tris[0, 0, 0] - plan[0, 0, 0] * SCALE, tris[0, 0, 1] - plan[0, 0, 1] * SCALE, FLOOR - ORIGIN[2]])
    # the furniture: where it goes (the game's objects: the mod puts them); the piano: ours, in the model
    pieces = furniture.place(plan, mats, SCALE)
    pnorm = np.cross(plan[:, 1] - plan[:, 0], plan[:, 2] - plan[:, 0])
    pnorm /= np.maximum(np.linalg.norm(pnorm, axis=1), 1e-9)[:, None]
    # the living room's stairs: their railing
    rt, rm = details.railing(plan, pnorm, STAIRS[0], 3.65, 0.63, 3.88, SCALE)
    tris = np.concatenate([tris, rt * SCALE + off])
    mats = np.concatenate([mats, rm])
    plan = np.concatenate([plan, rt])
    n_house = len(tris)
    pu, pv, phead = furniture.PIANO
    pxy = furniture.to_plan(pu, pv, "ground")
    pz = furniture.floor_z(plan[:len(pnorm)], pnorm, mats[:len(pnorm)], pxy, "ground")
    ptris, pmats, puv = furniture.piano(pxy * SCALE + off[:2], pz * SCALE + off[2], phead)
    given = np.full(tris.shape[:2] + (2,), np.nan)
    tris = np.concatenate([tris, ptris])
    mats = np.concatenate([mats, pmats])
    plan = np.concatenate([plan, (ptris - off) / SCALE])
    given = np.concatenate([given, puv])
    # the lot: raised, its walls, the pilings, the drive and the steps up from the street, the gates (lot.py)
    zl = lambda z: z - ORIGIN[2]
    stris, smats, scol, kind, lamps = lot.build(tris[:n_house], zl(FLOOR), LOT, RAMPS, beach_y,
                                                street_z=zl(3.62), sand_z=zl(3.15))
    print("lot: %d faces" % len(stris))
    pieces += decorate(tris[:n_house], plan[:len(pnorm)], pnorm, mats[:len(pnorm)], off)
    # the hipped roofs, the terraces' walls (roofs.py); the pergola, the lanterns on the walls
    rtris, rmats = roofs.build(tris[:n_house], zl(FLOOR), ROOF_LEVELS, TERRACES)
    etris, emats, elamps = extras(tris[:n_house])
    lamps = list(lamps) + elamps
    add = [(stris, smats), (rtris, rmats), (etris, emats)]
    for T_, M_ in add:
        tris = np.concatenate([tris, T_])
        mats = np.concatenate([mats, M_])
        plan = np.concatenate([plan, (T_ - off) / SCALE])
        given = np.concatenate([given, np.full((len(T_), 3, 2), np.nan)])
    normals = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    sand = plate((-45, -50, 65, 25), 3.0, ORIGIN)  # the beach under it all (for the light only)
    sky = Sky(np.concatenate([tris, sand]))
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
        uv = uvs(tris, n, tex, given, FLOOR - ORIGIN[2])
        day, night = light(n, corner, tex == "glass", tris, lamps, tex == "nch_glow")
        t = tris if sign > 0 else tris[:, [0, 2, 1]]
        if sign < 0:
            uv, day, night = uv[:, [0, 2, 1]], day[:, [0, 2, 1]], night[:, [0, 2, 1]]
        sides.append((t, uv, day, night, tex, open_))
    T, UV, DAY, NIGHT, TEX, OPEN = (np.concatenate(x) for x in zip(*sides))
    print("textures: %s" % dict(collections.Counter(TEX)))
    dff = write_dff(T, UV, DAY, NIGHT, TEX, "santahouse04_law2", [(x, y, z) for x, y, z in lamps])
    far = OPEN > 0.2                                               # far away: what is seen from outside
    lod = write_dff(T[far], UV[far], DAY[far], NIGHT[far], TEX[far], "LODtahouse04_law2")
    tex = sides[0][4]
    # the collision: the faces, and the stairs' ramps
    nplan = np.cross(plan[:, 1] - plan[:, 0], plan[:, 2] - plan[:, 0])
    nplan /= np.maximum(np.linalg.norm(nplan, axis=1), 1e-9)[:, None]
    ramp = ramps(plan[:n_house], tris[:n_house], nplan[:n_house])
    ramp_l = ramp * SCALE + off                    # (as faces() places the plan)
    col = write_col("santahouse04_law2", 6285, np.concatenate([tris, ramp_l, scol]))
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
            "the lot round it), the textures, the roof's map and what of the map to hide.",
            "#pragma once", "", "#include <cstdint>", "", "namespace coop::charlie::data {", "",
            "// santahouse04_law2 (near) and LODtahouse04_law2 (far: the outside only)",
            c_array("kHouseDff", dff), "", c_array("kLodDff", lod), "",
            "// the collisions, from the file's 32-byte header on (what CFileLoader::LoadCollisionModelVer3 reads)",
            c_array("kHouseCol", col[32:]), "",
            "// santahouse02_law2 (and its LOD): the game's own, west of this x (world) cut off as it loads",
            "inline constexpr float kEastCutX = %.2ff;" % EAST_CUT, "",
            "// where a roof is over (no rain there): cells of %.1f m from the block's x, y; a bit each, row by row" % CELL,
            "inline constexpr float kRoofX = %.2ff, kRoofY = %.2ff, kRoofCell = %.2ff, kRoofTop = %.2ff;  // the block's axes"
            % (rx, ry, CELL, rtop),
            "inline constexpr float kFloorZ = %.2ff;  // the house's floor (world)" % FLOOR,
            "// the lot (the block's axes); the front door and the save disk of a house of the group's homes here (world)",
            "inline constexpr float kLotX0 = %.2ff, kLotY0 = %.2ff, kLotX1 = %.2ff, kLotY1 = %.2ff;" % LOT,
            "inline constexpr float kDoorX = %.3ff, kDoorY = %.3ff, kDoorZ = %.2ff, kDoorHeading = %.1ff;" % (
                DOOR[0] + ORIGIN[0], DOOR[1] + ORIGIN[1], FLOOR + 1.0, DOOR[2]),
            "inline constexpr float kDiskX = %.3ff, kDiskY = %.3ff, kDiskZ = %.2ff;" % (
                DISK[0] + ORIGIN[0], DISK[1] + ORIGIN[1], FLOOR + 1.0),
            "inline constexpr int kRoofW = %d, kRoofH = %d;" % (rw, rh), c_array("kRoofBits", rbits), "",
            "// what of the map stood there: hidden (model, world x, y)",
            "struct Hidden {", "    int model;", "    float x, y;", "};",
            "inline constexpr Hidden kHidden[] = {"] + \
           ["    {%d, %.3ff, %.3ff}," % h for h in hide] + ["};", "",
            "// the furniture: the game's objects (model, world x, y, z of the floor under it + how high on what is"
            " under it, heading in degrees); the mod puts each with its collision's bottom there",
            "struct Piece {", "    int model;", "    float x, y, z, heading;", "};",
            "inline constexpr Piece kFurniture[] = {"] + \
           ["    {%d, %.3ff, %.3ff, %.3ff, %.1ff},  // %s" % (m, x * SCALE + off[0] + ORIGIN[0], y * SCALE + off[1] + ORIGIN[1],
                                                      z * SCALE + off[2] + ORIGIN[2] + h, hd, what)
            for m, what, x, y, z, hd, h in pieces] + ["};", "",
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
