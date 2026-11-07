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

# the game's objects' collision boxes (x0, y0, x1, y1 about their origin, from coop\lote.txt): many are not centred
# on it (a bed's runs 0.8 to 3.5 m ahead of it), so a piece's place in the drawing is its box's middle
BOX = {1735: (-0.42, -0.48, 0.45, 0.46), 1742: (-0.49, -0.49, 0.95, -0.12), 1745: (-0.49, 0.82, 1.50, 3.49),
       1746: (-0.50, -0.50, 0.50, 0.50), 1754: (-0.50, -0.50, 0.50, 0.50), 1761: (-0.42, -0.47, 2.44, 0.50),
       1762: (-0.23, -0.49, 1.21, 0.49), 1766: (-0.35, -0.48, 2.40, 0.50), 1786: (-0.49, -0.49, 0.68, 0.07),
       1798: (-0.49, 0.73, 1.50, 3.49), 1802: (-0.41, 0.97, 1.47, 3.47), 1806: (-0.27, -0.14, 0.29, 0.42),
       1811: (-0.37, -0.34, 0.39, 0.35), 1814: (-0.37, 0.05, 1.34, 0.97), 1819: (-0.07, -0.07, 1.10, 1.10),
       2030: (-0.85, -0.84, 0.86, 0.84), 2094: (-0.48, 0.86, 1.50, 1.50), 2096: (-0.25, -0.27, 0.25, 0.45),
       2165: (-0.47, -0.41, 1.47, 0.49), 2299: (-0.46, 0.98, 1.47, 3.49), 2306: (-0.49, 0.85, 0.49, 1.49),
       2311: (-0.45, -0.47, 1.97, 0.48), 2328: (-0.46, 0.72, 0.49, 1.50), 2514: (-0.20, -0.08, 0.21, 0.51),
       2516: (-0.47, -0.48, 1.76, 0.49), 2518: (0.20, 0.04, 0.83, 0.52), 1645: (-0.40, -1.16, 0.41, 1.13)}

PIECES = [
    # ---- the living room (sunken), the piano's raised floor by the deck's doors
    # ---- the deck
    (2030, "the deck's round table", 900, 822, 0, "ground", 0),
    (1811, "a chair", 868, 790, 135, "ground", 0),
    (1811, "a chair", 932, 790, 225, "ground", 0),
    (1811, "a chair", 868, 854, 45, "ground", 0),
    (1811, "a chair", 932, 854, 315, "ground", 0),
    (2096, "a rocking chair", 780, 800, 180, "ground", 0),
    # ---- the kitchen: the counter along the wall, turning at the corner; the round table
    # ---- the family room
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
    (1819, "a small table", 362, 272, 0, "upper", 0),
    (2516, "the bath", 178, 438, 0, "upper", 0),
    (2514, "the toilet", 268, 395, 270, "upper", 0),
    (2518, "the basin", 150, 290, 90, "upper", 0),
    (1645, "a lounge chair on the balcony", 530, 356, 270, "upper", 0),
    (2241, "a plant", 330, 418, 0, "upper", 0),
]

# the decoration: the game's objects by the house's place (the block's axes, x and y of the old block's place for the
# house's: build.py moves them with the house), not the drawing's: (model, what, x, y, where, floor, heading or the
# wall's direction). where: "floor" (on the house's floor), "wall" (a picture, on the wall that way), "lot" (outside)
PLANTS = (948, 949, 2001, 2010, 2241, 2244, 2245, 2247, 2251)
FRAMES = (2257, 2259, 2261, 2263, 2266, 2268, 2270, 2276, 2281, 2283, 2286)
DECOR = [
    # the living room (its pictures, rugs, the fireplace...: decor.py): plants in its corners and by the stairs' foot
    (2010, "a plant", 15.4, 7.7, "floor", "ground", 0), (2241, "a plant", 12.6, -0.6, "floor", "ground", 0),
    (2011, "a palm", 2.9, 7.7, "floor", "ground", 0), (2245, "a plant", 12.6, -5.5, "floor", "ground", 0),
    # the kitchen: Charlie's surfboard at the back, on the wall to the left (by the deck's glass door)
    (2404, "Charlie's surfboard", 1.66, -9.94, "floor", "ground", 270),
    # the hall by the front door and the stairs
    (FRAMES[5], "a picture", 18.0, 2.0, "wall", "ground", 0), (2244, "a plant", 17.2, 3.0, "floor", "ground", 0),
    # the west wing's rooms
    (FRAMES[6], "a picture", -5.0, 4.0, "wall", "ground", 90), (FRAMES[7], "a picture", -5.0, -8.0, "wall", "ground", 270),
    (2818, "a rug", -5.0, 2.0, "floor", "ground", 0), (2842, "a rug", -5.0, -7.0, "floor", "ground", 0),
    (2247, "a plant", -7.8, 7.6, "floor", "ground", 0),
    # the family room
    (FRAMES[8], "a picture", 25.0, 5.0, "wall", "ground", 90), (2251, "a plant", 22.8, 8.6, "floor", "ground", 0),
    (2010, "a plant in the family room", 0.3, 7.6, "floor", "ground", 0),
    # upstairs: Charlie's room
    (FRAMES[9], "a picture", 26.0, -5.0, "wall", "upper", 90), (FRAMES[10], "a picture", 26.0, -5.0, "wall", "upper", 270),
    (2847, "a rug", 26.0, -5.0, "floor", "upper", 0),
    # the street's terrace upstairs: a table, chairs, plants
    (2030, "the terrace's table", 25.0, 5.0, "floor", "upper", 0),
    (1811, "a chair", 24.0, 4.0, "floor", "upper", 315), (1811, "a chair", 26.0, 6.0, "floor", "upper", 135),
    (948, "a plant", 18.0, 8.4, "floor", "upper", 0), (949, "a plant", 29.6, 8.4, "floor", "upper", 0),
    # outside: plants along the walk, by the street's wall and the house; the court; the drive; the east yard
] + [(PLANTS[i % len(PLANTS)], "a plant by the wall", x, 11.4, "lot", "", 0) for i, x in
     enumerate((-11.0, -6.0, -1.0, 4.0, 9.0, 14.0, 24.0, 28.0, 33.0, 38.0))] + \
    [(949, "a plant in the court", 17.1, 6.0, "lot", "", 0), (948, "a plant in the court", 21.3, 6.0, "lot", "", 0),
     (2010, "a plant by the side door", -9.9, -8.0, "lot", "", 0), (2001, "a plant by the side door", -9.9, 0.0, "lot", "", 0),
     (2011, "a palm on the drive", -21.4, -4.0, "lot", "", 0), (2011, "a palm on the drive", -21.4, -12.0, "lot", "", 0),
     (1645, "a lounge chair", 36.0, -9.0, "lot", "", 0), (1645, "a lounge chair", 38.5, -9.0, "lot", "", 0),
     (2030, "the yard's table", 37.0, -1.5, "lot", "", 0), (1811, "a chair", 36.0, -2.5, "lot", "", 315),
     (1811, "a chair", 38.0, -0.5, "lot", "", 135), (2241, "a plant", 41.0, 11.0, "lot", "", 0)]

PIANO = (945, 960, 56.5)  # the drawing's x, y; its heading in the game (its keyboard's side to -y)
FLIP = {1811}  # these have their front to +y (the chairs)


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
    """the cells of a piece's footprint (plan metres; the piece's size is the game's, so / scale), 5 cm round it"""
    res = RASTER["res"]
    hx, hy = size[0] / 2 / scale + 0.03, size[1] / 2 / scale + 0.03  # all of it, and a little round it
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
        xy, head = to_plan(u, v, floor), heading(facing + (180 if model in FLIP else 0))
        size = SIZE.get(model, (1.0, 1.0, 1.0))
        coff = np.zeros(2)
        if model in BOX:
            b = BOX[model]
            size = (b[2] - b[0], b[3] - b[1], size[2])
            coff = np.array([(b[0] + b[2]) / 2, (b[1] + b[3]) / 2])
        if height == 0 and size[2] > 0.05:  # rugs and things on others: no check
            best = None
            for r in np.arange(0, 1.61, 0.05):
                for a in np.arange(0, 360, 30 if r else 360):
                    p = xy + r * np.array([np.cos(np.radians(a)), np.sin(np.radians(a))])
                    if not footprint(p, size, head, scale) & wall[floor] and \
                            floor_z(plan, normals, mats, p, floor) is not None:
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
        c, s_ = np.cos(np.radians(head)), np.sin(np.radians(head))  # its origin: the box's middle less its offset
        ox, oy = xy - np.array([c * coff[0] - s_ * coff[1], s_ * coff[0] + c * coff[1]]) / scale
        out.append((model, what, ox, oy, z, head, height))
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
    # the lid's stick: from the case's rim on the treble side up to the lid's underside near its edge
    sx0, sy = outline[:, 0].max() - 0.05 * k, outline[:, 1].min() + 0.30 * k
    dx = 1.34 * k  # where along the lid (from its hinge) it holds it
    sx1, sz1 = left + dx * np.cos(a), zt + dx * np.sin(a) - 0.03
    d = 0.015 * k
    for (ax_, az), (bx_, bz) in (((sx0 - d, zt), (sx1 - d, sz1)), ((sx0 + d, zt), (sx1 + d, sz1))):
        quad((ax_, sy - d, az), (bx_, sy - d, bz), (bx_, sy + d, bz), (ax_, sy + d, az), "piano_black")
        quad((ax_, sy + d, az), (bx_, sy + d, bz), (bx_, sy - d, bz), (ax_, sy - d, az), "piano_black")
    for yy in (sy - d, sy + d):
        quad((sx0 - d, yy, zt), (sx0 + d, yy, zt), (sx1 + d, yy, sz1), (sx1 - d, yy, sz1), "piano_black")
        quad((sx0 + d, yy, zt), (sx0 - d, yy, zt), (sx1 - d, yy, sz1), (sx1 + d, yy, sz1), "piano_black")
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
