"""The house's furniture: the game's own objects (their ids, from the game's IDE files), where the floor plan of the
show's house has them (the drawing by Inaki Aliste Lizarralde the model follows), and the grand piano, which the game
has none of: made here (piano()) and put in the house's model.

The drawing's pixels go to the model's plan (turned round, unscaled; metres) by where the drawing's walls lie on the
model's (found by matching the two: 43.5 px a metre, turned round). The upper floor's drawing is its own: matched on
its own. Then each piece is checked against the model's walls at its floor and moved off them if it stood in one.

A piece: (model, what, drawing x, drawing y, facing, floor, height over the floor). Facing: where its front looks in
the drawing, in degrees (0 up, 90 right, 180 down, 270 left); the game's furniture has its front to -y (a sofa's seat,
a bed's foot, a TV's screen, a cabinet's doors). Floor: "ground" or "upper". The objects lie on the floor (the mod puts
each so its collision's bottom is on it) or on another piece (height)."""
import numpy as np

PX_PER_M = 43.5
RASTER = dict(x0=-19.35, y0=-12.075, res=0.05, rows=482)   # the model's walls, as matched (north up)
DRAWING = (1466, 2000)                                        # the drawing's size (rows, columns)
REG = {"ground": (1, 3, 56, 147), "upper": (89, 429, 974, 1100)}  # raster offset (rows, columns), crop (rows, columns)

# the game's objects (model id: size x, y, z, from their files): for the checks
SIZE = {1766: (2.75, 0.97, 1.11), 1761: (2.85, 0.98, 1.04), 1754: (0.97, 0.97, 0.93), 1762: (1.44, 0.98, 1.04),
        1735: (0.87, 0.93, 1.28), 1822: (1.11, 1.09, 0.5), 1814: (1.71, 0.93, 0.5), 2119: (1.98, 0.99, 0.79),
        2069: (0.5, 0.5, 2.13), 2023: (0.75, 0.75, 2.09), 2833: (1.98, 1.10, 0.01), 2834: (1.98, 1.10, 0.01),
        2835: (2.0, 1.35, 0.01), 2836: (1.98, 1.10, 0.01), 2086: (1.71, 1.68, 0.8), 2030: (1.71, 1.68, 0.8),
        1811: (0.76, 0.68, 1.27), 2310: (0.61, 0.55, 1.01), 2147: (0.97, 0.84, 1.8), 2134: (1.0, 0.81, 1.05),
        2133: (1.0, 0.81, 1.05), 2339: (1.0, 0.81, 1.05), 2341: (1.0, 1.0, 1.05), 2132: (2.0, 0.81, 1.22),
        2404: (0.54, 0.18, 2.5), 1746: (0.97, 0.97, 0.5), 2311: (2.43, 0.95, 0.5), 1786: (1.17, 0.56, 0.71),
        2094: (1.98, 0.64, 1.0), 2306: (0.98, 0.64, 1.0), 2328: (0.77, 0.78, 0.75), 1745: (1.99, 2.67, 1.19),
        1802: (1.88, 2.51, 1.19), 1798: (1.99, 2.76, 0.76), 2299: (1.93, 2.52, 1.19), 2165: (1.95, 0.9, 1.32),
        1806: (0.56, 0.55, 1.04), 2514: (0.42, 0.59, 0.94), 2516: (2.23, 0.97, 0.59), 2518: (0.63, 0.47, 0.85),
        2011: (0.98, 0.98, 2.0), 2001: (0.94, 0.95, 1.78), 2241: (1.1, 1.06, 1.02), 2096: (0.5, 0.72, 0.96),
        1819: (1.17, 1.17, 0.52), 1742: (1.44, 0.37, 2.03), 1645: (0.9, 2.1, 0.8)}

PIECES = [
    # ---- the living room (sunken), the piano's raised floor by the deck's doors
    (1766, "the green sofa", 960, 1125, 90, "ground", 0),
    (2119, "the table behind the sofa", 918, 1125, 90, "ground", 0),
    (1822, "the round coffee table", 1040, 1112, 0, "ground", 0),
    (2833, "the rug", 1035, 1140, 90, "ground", 0),
    (1754, "the brown leather armchair", 1090, 1058, 224, "ground", 0),
    (1762, "the green armchair", 1062, 1255, 0, "ground", 0),
    (2069, "the floor lamp", 1140, 1082, 270, "ground", 0),
    (2023, "the lamp by the sofa", 962, 1045, 90, "ground", 0),
    (2241, "a plant", 910, 1012, 90, "ground", 0),
    (2011, "a palm", 918, 1262, 90, "ground", 0),
    (2165, "the desk by the deck's doors", 1108, 922, 180, "ground", 0),
    (1806, "its chair", 1108, 952, 0, "ground", 0),
    # ---- the deck
    (2086, "the deck's round table", 900, 822, 0, "ground", 0),
    (1811, "a chair", 868, 790, 135, "ground", 0),
    (1811, "a chair", 932, 790, 225, "ground", 0),
    (1811, "a chair", 868, 854, 45, "ground", 0),
    (1811, "a chair", 932, 854, 315, "ground", 0),
    (1645, "a lounge chair", 1050, 842, 225, "ground", 0),
    (1645, "a lounge chair", 1112, 792, 225, "ground", 0),
    (2096, "a rocking chair", 780, 800, 180, "ground", 0),
    # ---- the kitchen: the counter along the wall, turning at the corner; the round table
    (2147, "the steel fridge", 1268, 815, 180, "ground", 0),
    (2134, "a cabinet", 1295, 815, 180, "ground", 0),
    (2339, "the stove", 1322, 815, 180, "ground", 0),
    (2133, "a cabinet", 1349, 815, 180, "ground", 0),
    (2134, "a cabinet", 1376, 815, 180, "ground", 0),
    (2341, "the corner", 1403, 815, 180, "ground", 0),
    (2132, "the sink", 1405, 855, 270, "ground", 0),
    (2134, "a cabinet", 1405, 895, 270, "ground", 0),
    (2404, "Charlie's surfboard", 1238, 815, 180, "ground", 0),
    (2030, "the round table", 1295, 930, 0, "ground", 0),
    (1811, "a chair", 1263, 898, 135, "ground", 0),
    (1811, "a chair", 1327, 898, 225, "ground", 0),
    (1811, "a chair", 1263, 962, 45, "ground", 0),
    (1811, "a chair", 1327, 962, 315, "ground", 0),
    (2836, "the kitchen's rug", 1295, 930, 0, "ground", 0),
    # ---- the family room
    (1761, "the sofa", 1300, 1095, 180, "ground", 0),
    (2094, "the cabinet behind it", 1300, 1045, 180, "ground", 0),
    (1814, "the coffee table", 1300, 1160, 0, "ground", 0),
    (2834, "the rug", 1300, 1160, 0, "ground", 0),
    (1746, "the footstool", 1300, 1215, 0, "ground", 0),
    (2311, "the TV's table", 1300, 1278, 0, "ground", 0),
    (1786, "the TV", 1300, 1278, 0, "ground", 0.5),
    (1735, "the armchair", 1440, 1150, 270, "ground", 0),
    (1762, "an armchair", 1420, 1232, 315, "ground", 0),
    (1742, "the bookshelf", 1215, 1240, 90, "ground", 0),
    # ---- the west wing: three bedrooms, two bathrooms, the hall
    (1745, "a bed", 505, 750, 270, "ground", 0),
    (2328, "a nightstand", 548, 700, 270, "ground", 0),
    (2328, "a nightstand", 548, 805, 270, "ground", 0),
    (1754, "an armchair", 412, 848, 45, "ground", 0),
    (2094, "a dresser", 396, 745, 90, "ground", 0),
    (2516, "a bath", 430, 550, 180, "ground", 0),
    (2514, "a toilet", 398, 640, 90, "ground", 0),
    (2518, "a basin", 470, 645, 0, "ground", 0),
    (1802, "a bed", 490, 1048, 180, "ground", 0),
    (2328, "a nightstand", 440, 985, 180, "ground", 0),
    (2328, "a nightstand", 545, 985, 180, "ground", 0),
    (2306, "a chest", 588, 1095, 270, "ground", 0),
    (1798, "Jake's bed", 505, 1188, 180, "ground", 0),
    (2165, "Jake's desk", 580, 1242, 270, "ground", 0),
    (1806, "its chair", 552, 1242, 90, "ground", 0),
    (2516, "a bath", 285, 1080, 180, "ground", 0),
    (2514, "a toilet", 338, 1120, 270, "ground", 0),
    (2518, "a basin", 222, 1172, 90, "ground", 0),
    (2514, "the toilet by the stairs", 768, 1025, 270, "ground", 0),
    (2518, "its basin", 722, 988, 180, "ground", 0),
    (2836, "the hall's rug", 500, 905, 0, "ground", 0),
    # ---- the upper floor: Charlie's room, his bathroom, the balcony
    (2299, "Charlie's bed", 425, 160, 180, "upper", 0),
    (2328, "a nightstand", 368, 112, 180, "upper", 0),
    (2328, "a nightstand", 484, 112, 180, "upper", 0),
    (1746, "the bench at its foot", 425, 222, 0, "upper", 0),
    (1735, "an armchair", 305, 142, 135, "upper", 0),
    (1754, "an armchair", 300, 255, 45, "upper", 0),
    (1819, "a small table", 362, 272, 0, "upper", 0),
    (2516, "the bath", 178, 438, 0, "upper", 0),
    (2514, "the toilet", 268, 395, 270, "upper", 0),
    (2518, "the basin", 150, 290, 90, "upper", 0),
    (1645, "a lounge chair on the balcony", 530, 356, 270, "upper", 0),
    (2241, "a plant", 330, 418, 0, "upper", 0),
]

PIANO = (945, 960, 56.5)  # the drawing's x, y; its heading in the game (its keyboard's side to -y)


def to_plan(u, v, floor):
    """a pixel of the drawing -> the model's plan (x, y), metres"""
    oy, ox, ys0, xs0 = REG[floor]
    f = 20 / PX_PER_M
    rr, rc = DRAWING[0] - 1 - v, DRAWING[1] - 1 - u
    row, col = oy + (rr - ys0) * f, ox + (rc - xs0) * f
    i = RASTER["rows"] - 1 - row
    return np.array([RASTER["x0"] + col * RASTER["res"], RASTER["y0"] + i * RASTER["res"]])


def heading(facing):
    """the drawing's facing -> the game's heading (degrees): turned round, the front is -y"""
    return (-facing) % 360


def walls(plan, normals, mats, z):
    """the model's walls at height z (plan), 5 cm cells, as a set of cells"""
    res = RASTER["res"]
    zz = plan[..., 2]
    sel = (np.abs(normals[:, 2]) < 0.1) & (zz.min(1) < z - 0.1) & (zz.max(1) > z + 0.1) & \
          np.array(["Glass" not in m for m in mats])
    cells = set()
    for t in plan[sel]:
        a = t[:, :2]
        i = np.argmax(np.linalg.norm(a[:, None] - a[None], axis=2).ravel())
        p, q = a[i // 3], a[i % 3]
        for s in np.linspace(0, 1, int(np.linalg.norm(q - p) / res) + 2):
            x, y = p + (q - p) * s
            cells.add((int(np.floor(x / res)), int(np.floor(y / res))))
    return cells


def footprint(center, size, head, scale):
    """the cells of a piece's footprint (plan metres; the piece's size is the game's, so / scale), 8 cm in"""
    res = RASTER["res"]
    hx, hy = max(size[0] / 2 / scale - 0.08, 0.02), max(size[1] / 2 / scale - 0.08, 0.02)
    c, s = np.cos(np.radians(head)), np.sin(np.radians(head))
    out = set()
    for x in np.arange(-hx, hx + 1e-6, res / 2):
        for y in np.arange(-hy, hy + 1e-6, res / 2):
            px, py = center[0] + c * x - s * y, center[1] + s * x + c * y
            out.add((int(np.floor(px / res)), int(np.floor(py / res))))
    return out


def floor_z(plan, normals, mats, xy, floor):
    """the floor's height (plan z) under xy: the highest floor face there, of the ground floor or the upper"""
    lo, hi = (-0.1, 0.6) if floor == "ground" else (2.5, 3.3)
    zz = plan[..., 2].mean(1)
    sel = np.where((normals[:, 2] > 0.95) & (zz > lo) & (zz < hi))[0]
    best = None
    x, y = xy
    for i in sel:
        (ax, ay, az), (bx, by, _), (cx, cy, _) = plan[i]
        d = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
        if abs(d) < 1e-9:
            continue
        u = ((x - ax) * (cy - ay) - (cx - ax) * (y - ay)) / d
        w = ((bx - ax) * (y - ay) - (x - ax) * (by - ay)) / d
        if u >= -1e-3 and w >= -1e-3 and u + w <= 1 + 1e-3 and (best is None or az > best):
            best = az
    return best


def place(plan, mats, scale):
    """[(model, what, plan x, y, plan z of the floor, heading, height)], each moved off the walls if it was on one"""
    normals = np.cross(plan[:, 1] - plan[:, 0], plan[:, 2] - plan[:, 0])
    normals /= np.maximum(np.linalg.norm(normals, axis=1), 1e-9)[:, None]
    wall = {"ground": walls(plan, normals, mats, 1.0), "upper": walls(plan, normals, mats, 3.8)}
    out, moved = [], []
    for model, what, u, v, facing, floor, height in PIECES:
        xy, head = to_plan(u, v, floor), heading(facing)
        size = SIZE.get(model, (1.0, 1.0, 1.0))
        if height == 0 and size[2] > 0.05:  # rugs and things on others: no check
            best = None
            for r in np.arange(0, 1.01, 0.05):
                for a in np.arange(0, 360, 30 if r else 360):
                    p = xy + r * np.array([np.cos(np.radians(a)), np.sin(np.radians(a))])
                    if not footprint(p, size, head, scale) & wall[floor]:
                        best = p
                        break
                if best is not None:
                    break
            if best is None:
                print("  furniture: %s (%d) stands in a wall, left as it is" % (what, model))
            elif np.linalg.norm(best - xy) > 1e-6:
                moved.append("%s %.2f m" % (what, np.linalg.norm(best - xy) * scale))
                xy = best
        z = floor_z(plan, normals, mats, xy, floor)
        if z is None:
            print("  furniture: %s (%d): no floor under it, left out" % (what, model))
            continue
        out.append((model, what, xy[0], xy[1], z, head, height))
    print("furniture: %d pieces%s" % (len(out), (", moved off the walls: " + ", ".join(moved)) if moved else ""))
    return out


# ---------------------------------------------------------------- the grand piano (in the house's model)
def piano(center, z, head):
    """a baby grand at the game's furniture size (1.3 x life), as faces in the block's axes: (tris, mats, uv);
    its keyboard to the local -y, turned by head (degrees)"""
    k = 1.3
    outline = np.array([(0, 0), (1.48, 0), (1.48, 0.42), (1.40, 0.70), (1.12, 0.95), (0.98, 1.18), (0.94, 1.42),
                        (0.84, 1.62), (0.62, 1.76), (0.32, 1.80), (0.10, 1.74), (0.0, 1.62)]) * k
    outline -= np.array([0.74 * k, 0.9 * k])  # its middle at 0
    tris, mats, uvs = [], [], []

    def quad(a, b, c, d, mat, uv=None):
        tris.extend([[a, b, c], [a, c, d]])
        mats.extend([mat, mat])
        if uv is None:
            uv = [(0, 0), (1, 0), (1, 1), (0, 1)]
        uvs.extend([[uv[0], uv[1], uv[2]], [uv[0], uv[2], uv[3]]])

    def poly(pts, zz, mat, up=True):
        c = pts.mean(0)
        for i in range(len(pts)):
            p, q = pts[i], pts[(i + 1) % len(pts)]
            t = [(c[0], c[1], zz), (p[0], p[1], zz), (q[0], q[1], zz)]
            if not up:
                t = [t[0], t[2], t[1]]
            tris.append(t)
            mats.append(mat)
            uvs.append([(0.5, 0.5), (0, 0), (1, 0)])

    def box(x0, y0, z0, x1, y1, z1, mat):
        p = lambda x, y, zz: (x, y, zz)
        quad(p(x0, y0, z1), p(x1, y0, z1), p(x1, y1, z1), p(x0, y1, z1), mat)
        quad(p(x0, y0, z0), p(x0, y1, z0), p(x1, y1, z0), p(x1, y0, z0), mat)
        quad(p(x0, y0, z0), p(x1, y0, z0), p(x1, y0, z1), p(x0, y0, z1), mat)
        quad(p(x1, y1, z0), p(x0, y1, z0), p(x0, y1, z1), p(x1, y1, z1), mat)
        quad(p(x0, y1, z0), p(x0, y0, z0), p(x0, y0, z1), p(x0, y1, z1), mat)
        quad(p(x1, y0, z0), p(x1, y1, z0), p(x1, y1, z1), p(x1, y0, z1), mat)

    zb, zt = 0.66 * k, 0.98 * k   # the case: its bottom and rim
    for i in range(len(outline)):  # its sides
        p, q = outline[i], outline[(i + 1) % len(outline)]
        quad((p[0], p[1], zb), (q[0], q[1], zb), (q[0], q[1], zt), (p[0], p[1], zt), "piano_black")
    poly(outline, zb, "piano_black", up=False)
    poly(outline * 0.97, zt - 0.02, "piano_black")  # inside: the soundboard, black too
    # the lid, open on the bass side's hinge (x the case's left) by 38 degrees
    left = outline[:, 0].min()
    lid = []
    a = np.radians(38)
    for x, y in outline:
        dx = x - left
        lid.append((left + dx * np.cos(a), y, zt + dx * np.sin(a)))
    lid = np.array(lid)
    c = lid.mean(0)
    for i in range(len(lid)):
        tris.append([tuple(c), tuple(lid[i]), tuple(lid[(i + 1) % len(lid)])])
        mats.append("piano_black")
        uvs.append([(0.5, 0.5), (0, 0), (1, 0)])
    # the lid's stick
    box(0.55 * k, 0.15 * k, zt, 0.58 * k, 0.18 * k, zt + 0.62 * k, "piano_black")
    # the keyboard, in front
    x0, x1 = outline[:, 0].min() + 0.08 * k, outline[:, 0].max() - 0.08 * k
    y0 = outline[:, 1].min()
    box(x0 - 0.06 * k, y0 - 0.20 * k, zb - 0.06 * k, x1 + 0.06 * k, y0, zb + 0.06 * k, "piano_black")
    octave = 0.164 * k
    quad((x0, y0 - 0.19 * k, zb + 0.065 * k), (x1, y0 - 0.19 * k, zb + 0.065 * k), (x1, y0, zb + 0.065 * k),
         (x0, y0, zb + 0.065 * k), "piano_keys", [(0, 1), ((x1 - x0) / octave, 1), ((x1 - x0) / octave, 0), (0, 0)])
    box(x0 - 0.06 * k, y0 - 0.20 * k, zb + 0.06 * k, x0, y0, zb + 0.14 * k, "piano_black")   # its cheeks
    box(x1, y0 - 0.20 * k, zb + 0.06 * k, x1 + 0.06 * k, y0, zb + 0.14 * k, "piano_black")
    # the music stand
    box(x0 + 0.25 * k, y0 + 0.05 * k, zt, x1 - 0.25 * k, y0 + 0.08 * k, zt + 0.28 * k, "piano_black")
    # three legs and the pedals' lyre
    for lx, ly in ((x0 + 0.02 * k, y0 + 0.08 * k), (x1 - 0.06 * k, y0 + 0.08 * k), (-0.36 * k, 0.68 * k)):
        box(lx, ly, 0, lx + 0.08 * k, ly + 0.08 * k, zb, "piano_black")
    box(-0.08 * k, y0 + 0.1 * k, 0, 0.08 * k, y0 + 0.16 * k, zb - 0.1 * k, "piano_black")
    # the bench
    box(-0.42 * k, y0 - 0.85 * k, 0.45 * k, 0.42 * k, y0 - 0.50 * k, 0.52 * k, "piano_black")
    for bx in (-0.40 * k, 0.34 * k):
        for by in (y0 - 0.84 * k, y0 - 0.57 * k):
            box(bx, by, 0, bx + 0.06 * k, by + 0.06 * k, 0.45 * k, "piano_black")
    T = np.array(tris, float)
    c, s = np.cos(np.radians(head)), np.sin(np.radians(head))
    x, y = T[..., 0].copy(), T[..., 1].copy()
    T[..., 0] = center[0] + c * x - s * y
    T[..., 1] = center[1] + s * x + c * y
    T[..., 2] += z
    return T, np.array(mats), np.array(uvs, float)
