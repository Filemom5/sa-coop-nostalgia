"""The house's own decoration, in its model (as the piano): what the game's objects could not make look like the
show's house. The living room (the fireplace and its mantel, the built-in bookshelf, the green sofa, the console table
and its chest behind it, the drum table, the leather and the cream armchairs, the rugs, the desk by the deck's doors),
the posters (ours: in the style of the show's old jazz and car posters) and prints in frames, the kitchen (white
cabinets, the yellow tiled counter and splashback, the hood, the steel fridge, the cart, the round table and its white
chairs) and the family room (the grey sofa, the striped rug, the red ottoman, the armoire, the cane chair).

All in the block's axes (build.py adds it to the house's model and collision), as faces with a material each
("dec_<texture>": textures.py's nch_<texture>) and their texture coordinates where the whole of a texture goes on a
face (a rug, a poster, a cabinet's door; NaN: build.py maps it by where it is). The faces show one side (outward).

The furniture is made at the game's size (its people are not 1.6 times life, as the house is) and a little over."""
import numpy as np

FLOOR = -3.61    # the ground floor (block's axes, z): the house's floor, 5.3 m in the world
RAISED = -3.37   # the piano's raised floor by the deck's doors


class Mesh:
    def __init__(self):
        self.T, self.M, self.U = [], [], []

    def tri(self, a, b, c, mat, uv=None):
        self.T.append([a, b, c])
        self.M.append(mat)
        self.U.append(uv if uv is not None else [[np.nan] * 2] * 3)

    def quad(self, a, b, c, d, mat, uv=None):
        """a, b, c, d counter-clockwise seen from the side the face looks to; uv: the whole texture (or 4 given)"""
        if uv is True:
            uv = [(0, 1), (1, 1), (1, 0), (0, 0)]
        self.tri(a, b, c, mat, None if uv is None else [uv[0], uv[1], uv[2]])
        self.tri(a, c, d, mat, None if uv is None else [uv[0], uv[2], uv[3]])

    def box(self, x0, y0, z0, x1, y1, z1, mat, bottom=False, front=None, top=None):
        """front: a material for the -y face with the whole texture on it; top: the +z face's, its whole texture"""
        x0, x1 = min(x0, x1), max(x0, x1)
        y0, y1 = min(y0, y1), max(y0, y1)
        q = self.quad
        q((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), top or mat, True if top else None)
        if bottom:
            q((x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0), mat)
        q((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), front or mat, True if front else None)
        q((x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1), mat)
        q((x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1), mat)
        q((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1), mat)

    def cyl(self, cx, cy, z0, z1, r, mat, n=14, top=None, r_top=None):
        rt = r if r_top is None else r_top
        a = np.linspace(0, 2 * np.pi, n + 1)
        for i in range(n):
            p0, p1 = (np.cos(a[i]), np.sin(a[i])), (np.cos(a[i + 1]), np.sin(a[i + 1]))
            self.quad((cx + p0[0] * r, cy + p0[1] * r, z0), (cx + p1[0] * r, cy + p1[1] * r, z0),
                      (cx + p1[0] * rt, cy + p1[1] * rt, z1), (cx + p0[0] * rt, cy + p0[1] * rt, z1), mat)
            uv = None
            if top:
                uv = [(0.5, 0.5), (0.5 + p0[0] / 2, 0.5 - p0[1] / 2), (0.5 + p1[0] / 2, 0.5 - p1[1] / 2)]
            self.tri((cx, cy, z1), (cx + p0[0] * rt, cy + p0[1] * rt, z1), (cx + p1[0] * rt, cy + p1[1] * rt, z1),
                     top or mat, uv)

    def bar(self, a, b, r, mat):
        """a square bar from a to b"""
        a, b = np.array(a, float), np.array(b, float)
        d = (b - a) / np.linalg.norm(b - a)
        u = np.cross(d, [0, 0, 1.0]) if abs(d[2]) < 0.9 else np.cross(d, [1.0, 0, 0])
        u /= np.linalg.norm(u)
        v = np.cross(d, u)
        c = [u * r + v * r, -u * r + v * r, -u * r - v * r, u * r - v * r]
        for i in range(4):
            p, q = c[i], c[(i + 1) % 4]
            self.quad(a + q, b + q, b + p, a + p, mat)

    def add(self, other, x, y, z, heading=0.0):
        """another mesh made about its origin (its front to -y) turned by heading (degrees, counter-clockwise: 90
        looks to +x, 180 to +y, 270 to -x) and put at x, y, z"""
        h = np.radians(heading)
        R = np.array([[np.cos(h), -np.sin(h), 0], [np.sin(h), np.cos(h), 0], [0, 0, 1]])
        for t, m, u in zip(other.T, other.M, other.U):
            self.T.append([list(R @ np.array(p) + (x, y, z)) for p in t])
            self.M.append(m)
            self.U.append(u)

    def arrays(self):
        T = np.array(self.T, float).reshape(-1, 3, 3)
        M = np.array(self.M)
        U = np.array(self.U, float).reshape(-1, 3, 2)
        ok = np.linalg.norm(np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]), axis=1) > 1e-7
        return T[ok], M[ok], U[ok]


def face(m, p0, p1, z0, z1, n, mat, uv=None):
    """a vertical face from p0 to p1 (x, y), z0 to z1, looking n (x, y): its texture's left on the viewer's left"""
    a, b = np.array(p0, float), np.array(p1, float)
    right = np.array([n[1], -n[0]])            # the viewer's right, looking at it
    if (b - a) @ right < 0:
        a, b = b, a
    m.quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1), mat, uv)


# ---------------------------------------------------------------- pieces (made about their origin, front to -y)
def legs(m, w, d, h, r=0.03, mat="dec_wood", inset=0.06):
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (w / 2 - inset), sy * (d / 2 - inset)
            m.box(x - r, y - r, 0, x + r, y + r, h, mat)


def sofa(w, d, fabric, cushions=3, pillows=("dec_ottoman", "dec_beige"), frame=None):
    m = Mesh()
    fr = frame or fabric
    arm, back, seat = 0.24, 0.24, 0.46
    legs(m, w, d, 0.1, 0.03, "dec_darkwood", 0.08)
    m.box(-w / 2, -d / 2, 0.1, w / 2, d / 2, 0.36, fr)                              # the base
    m.box(-w / 2, d / 2 - back, 0.36, w / 2, d / 2, 0.92, fr)                       # the back
    for s in (-1, 1):                                                               # the arms
        m.box(s * w / 2 - (arm if s > 0 else 0), -d / 2, 0.36, s * w / 2 + (arm if s < 0 else 0), d / 2 - back, 0.64,
              fr)
    cw = (w - 2 * arm) / cushions
    for i in range(cushions):                                                       # the seat's and the back's cushions
        x0 = -w / 2 + arm + i * cw
        m.box(x0 + 0.01, -d / 2 + 0.02, 0.36, x0 + cw - 0.01, d / 2 - back, seat, fabric)
        m.box(x0 + 0.02, d / 2 - back - 0.16, seat, x0 + cw - 0.02, d / 2 - back, 0.84, fabric)
    for i, s in enumerate((-1, 1)):                                                 # two pillows at the arms
        x = s * (w / 2 - arm - 0.26)
        m.box(x - 0.2, d / 2 - back - 0.28, seat, x + 0.2, d / 2 - back - 0.14, seat + 0.38, pillows[i % len(pillows)])
    return m


def armchair(fabric, w=1.0, d=0.95, frame=None):
    """one cushion; a cane chair: its frame of cane"""
    return sofa(w, d, fabric, cushions=1, pillows=("dec_beige",), frame=frame)


def console(w, d, h, mat="dec_wood", drawers=4):
    m = Mesh()
    legs(m, w, d, h - 0.04, 0.035, mat, 0.05)
    m.box(-w / 2, -d / 2, h - 0.04, w / 2, d / 2, h, mat)                          # the top
    m.box(-w / 2 + 0.05, -d / 2 + 0.03, h - 0.2, w / 2 - 0.05, d / 2 - 0.03, h - 0.04, mat)  # the apron
    for i in range(drawers):                                                        # the drawers' knobs
        x = -w / 2 + (i + 0.5) * w / drawers
        m.box(x - 0.025, -d / 2 + 0.0, h - 0.13, x + 0.025, -d / 2 + 0.03, h - 0.1, "dec_copper")
    return m


def chest():
    m = Mesh()
    m.box(-0.32, -0.2, 0, 0.32, 0.2, 0.3, "dec_cane")
    m.box(-0.34, -0.22, 0.3, 0.34, 0.22, 0.36, "dec_lightwood")
    for x in (-0.2, 0.2):
        m.box(x - 0.02, -0.225, 0.0, x + 0.02, 0.225, 0.37, "dec_wood")
    return m


def chair_x(mat="dec_white"):
    """a kitchen chair: white, its back an X"""
    m = Mesh()
    w, d = 0.46, 0.46
    legs(m, w, d, 0.46, 0.022, mat, 0.03)
    m.box(-w / 2, -d / 2, 0.44, w / 2, d / 2, 0.49, mat)
    for s in (-1, 1):
        m.box(s * (w / 2 - 0.03) - 0.022, d / 2 - 0.05, 0.49, s * (w / 2 - 0.03) + 0.022, d / 2 - 0.006, 1.02, mat)
    m.box(-w / 2 + 0.01, d / 2 - 0.05, 0.96, w / 2 - 0.01, d / 2 - 0.006, 1.04, mat)
    m.bar((-w / 2 + 0.05, d / 2 - 0.03, 0.52), (w / 2 - 0.05, d / 2 - 0.03, 0.94), 0.016, mat)
    m.bar((w / 2 - 0.05, d / 2 - 0.03, 0.52), (-w / 2 + 0.05, d / 2 - 0.03, 0.94), 0.016, mat)
    return m


def office_chair():
    m = Mesh()
    m.cyl(0, 0, 0, 0.06, 0.3, "dec_black", n=5)
    m.cyl(0, 0, 0.06, 0.42, 0.03, "dec_black", n=6)
    m.box(-0.25, -0.24, 0.42, 0.25, 0.24, 0.5, "dec_black")
    m.box(-0.23, 0.2, 0.5, 0.23, 0.26, 1.05, "dec_black")
    return m


def rug(m, cx, cy, z, w, d, tex, heading=0):
    """a rug, flat: w along x, d along y (turned by 90: the other way)"""
    if heading % 180:
        w, d = d, w
    x0, y0, x1, y1 = cx - w / 2, cy - d / 2, cx + w / 2, cy + d / 2
    z1 = z + 0.012
    uv = [(0, 1), (1, 1), (1, 0), (0, 0)] if heading % 180 == 0 else [(0, 0), (0, 1), (1, 1), (1, 0)]
    m.quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), tex, uv)
    for (ax, ay), (bx, by) in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        m.quad((ax, ay, z), (bx, by, z), (bx, by, z1), (ax, ay, z1), tex)


def framed(m, x, y, zc, nx, ny, w, h, tex, frame=0.05, depth=0.04):
    """a poster or a print in a dark wood frame, on the wall at (x, y) looking (nx, ny)"""
    n = np.array([nx, ny, 0.0])
    along = np.cross([0, 0, 1.0], n)        # to the right, seen from the front
    c = np.array([x, y, zc])
    for (a0, a1, b0, b1) in ((-w / 2 - frame, w / 2 + frame, h / 2, h / 2 + frame),
                             (-w / 2 - frame, w / 2 + frame, -h / 2 - frame, -h / 2),
                             (-w / 2 - frame, -w / 2, -h / 2, h / 2), (w / 2, w / 2 + frame, -h / 2, h / 2)):
        p = [c + along * a0 + [0, 0, b0], c + along * a1 + [0, 0, b1]]
        lo = np.minimum(p[0], p[1]) + np.minimum(n * 0.0, n * depth)
        hi = np.maximum(p[0], p[1]) + np.maximum(n * 0.0, n * depth)
        m.box(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], "dec_wood")
    f = c + n * (depth * 0.6)
    a, b = f - along * w / 2 - [0, 0, h / 2], f + along * w / 2 - [0, 0, h / 2]
    m.quad(tuple(a), tuple(b), tuple(b + [0, 0, h]), tuple(a + [0, 0, h]), tex, True)
    back = c + n * 0.005                     # its back on the wall
    m.quad(tuple(back + along * (w / 2 + frame) - [0, 0, h / 2 + frame]),
           tuple(back - along * (w / 2 + frame) - [0, 0, h / 2 + frame]),
           tuple(back - along * (w / 2 + frame) + [0, 0, h / 2 + frame]),
           tuple(back + along * (w / 2 + frame) + [0, 0, h / 2 + frame]), "dec_wood")


# ---------------------------------------------------------------- the rooms
def living(m):
    z = FLOOR
    # the fireplace on the west wall (x 2.26, looking east), the mantel, the hearth; the poster over it
    wx, cy = 2.26, -5.2
    m.box(wx, cy - 1.4, z, wx + 0.62, cy + 1.4, z + 4.2, "dec_cream")                 # the chimney breast
    m.box(wx + 0.62, cy - 0.62, z + 0.25, wx + 0.66, cy + 0.62, z + 1.3, "dec_black")  # the firebox
    m.box(wx + 0.62, cy - 0.85, z + 1.3, wx + 0.72, cy + 0.85, z + 1.42, "dec_cream")  # its lintel
    m.box(wx + 0.5, cy - 1.6, z + 1.78, wx + 0.92, cy + 1.6, z + 1.92, "dec_wood")   # the mantel shelf
    m.box(wx + 0.62, cy - 1.0, z, wx + 1.5, cy + 1.0, z + 0.14, "dec_tile")           # the hearth
    for s in (-1, 1):                                                                  # the logs
        m.bar((wx + 0.9, cy + s * 0.15 - 0.3, z + 0.3), (wx + 0.9, cy + s * 0.15 + 0.3, z + 0.32), 0.07, "dec_darkwood")
    framed(m, wx + 0.66, cy, z + 2.65, 1, 0, 0.66, 0.88, "dec_post3")
    m.cyl(wx + 0.75, cy - 1.2, z + 1.92, z + 2.3, 0.1, "dec_white", n=8, r_top=0.06)  # a vase on the mantel
    # the built-in bookshelf beside it (the wall's niche toward the deck)
    y0, y1 = -9.6, -7.2
    m.box(wx, y0, z, wx + 0.4, y0 + 0.08, z + 3.2, "dec_cream")
    m.box(wx, y1 - 0.08, z, wx + 0.4, y1, z + 3.2, "dec_cream")
    m.box(wx, y0, z + 3.12, wx + 0.42, y1, z + 3.24, "dec_wood")
    for k, sz in enumerate((0.0, 0.62, 1.22, 1.82, 2.42)):
        m.box(wx, y0 + 0.08, z + sz, wx + 0.4, y1 - 0.08, z + sz + 0.06, "dec_wood")
        if k < 4:  # a row of books on each shelf (some with a gap), their spines out
            ya, yb = y0 + 0.1, y1 - 0.1 - (0.6 if k % 2 else 0.0)
            hb = 0.42
            x1 = wx + 0.36
            m.box(wx + 0.05, ya, z + sz + 0.06, x1 - 0.03, yb, z + sz + 0.06 + hb, "dec_black")
            u = (yb - ya) / 0.8
            face(m, (x1 + 0.002, ya), (x1 + 0.002, yb), z + sz + 0.06, z + sz + 0.06 + hb, (1, 0), "dec_books",
                 [(0, 1), (u, 1), (u, 0), (0, 0)])
    m.cyl(wx + 0.2, y1 - 0.4, z + 1.88, z + 2.1, 0.12, "dec_copper", n=8)             # a bowl, a small plant
    m.cyl(wx + 0.2, y1 - 0.4, z + 0.68, z + 0.86, 0.1, "dec_tile", n=8)
    m.box(wx + 0.1, y1 - 0.5, z + 0.86, wx + 0.3, y1 - 0.3, z + 1.1, "dec_leaves")
    # the seating: the green sofa facing the fireplace, the drum table, the armchairs, the console table and its chest
    rug(m, 5.6, -5.2, z, 4.4, 3.4, "dec_rug1", heading=90)
    m.add(sofa(3.0, 1.05, "dec_sofa"), 7.0, -5.2, z, 270)
    m.add(console(2.8, 0.45, 0.8), 7.95, -5.2, z, 90)
    m.add(chest(), 7.95, -4.5, z + 0.8, 90)
    m.cyl(5.0, -5.2, z, z + 0.46, 0.56, "dec_darkwood", n=16, top="dec_darkwood")
    m.cyl(5.0, -5.2, z + 0.46, z + 0.48, 0.58, "dec_black", n=16)
    m.add(armchair("dec_leather", 1.05, 1.0), 4.2, -7.5, z, 162)
    m.add(armchair("dec_beige", 1.0, 0.95), 4.4, -2.9, z, 10)
    m.cyl(7.2, -3.2, z, z + 0.58, 0.28, "dec_cane", n=10, top="dec_cane")              # the wicker side table
    m.cyl(3.6, -2.0, z, z + 0.04, 0.2, "dec_black", n=8)                               # a floor lamp by the chair
    m.cyl(3.6, -2.0, z + 0.04, z + 1.7, 0.025, "dec_black", n=6)
    m.cyl(3.6, -2.0, z + 1.55, z + 1.95, 0.3, "dec_beige", n=10, r_top=0.2)
    rug(m, 12.4, -3.0, z, 2.4, 3.2, "dec_rug3", heading=0)                            # by the stairs' foot
    # the entry by the front door: its rug, a bench, a coat stand
    rug(m, 13.4, -0.4, z, 2.6, 1.5, "dec_rug2", heading=0)
    m.add(console(1.4, 0.4, 0.5, drawers=0), 11.0, 0.95, z, 180)
    m.cyl(16.0, -1.0, z, z + 0.05, 0.22, "dec_black", n=8)
    m.cyl(16.0, -1.0, z + 0.05, z + 1.85, 0.03, "dec_darkwood", n=6)
    for a in range(4):
        r = np.radians(a * 90 + 45)
        m.bar((16.0, -1.0, z + 1.7), (16.0 + 0.2 * np.cos(r), -1.0 + 0.2 * np.sin(r), z + 1.82), 0.015, "dec_darkwood")
    # the piano's raised floor: its rug; the desk by the deck's doors, its chair and computer
    rug(m, 9.8, -11.2, RAISED, 3.2, 2.4, "dec_rug5", heading=0)
    zr = RAISED
    m.add(console(1.5, 0.75, 0.78, drawers=2), 4.6, -12.55, zr, 180)
    m.box(4.35, -12.75, zr + 0.78, 4.85, -12.7, zr + 1.15, "dec_black")
    m.box(4.55, -12.72, zr + 0.78, 4.65, -12.62, zr + 0.85, "dec_black")
    m.box(4.3, -12.45, zr + 0.78, 4.8, -12.3, zr + 0.8, "dec_white")
    m.add(office_chair(), 4.6, -11.8, zr, 0)
    # the posters by the stairs (on the wall they go up along, x 15.1, looking west), the prints on the north wall
    framed(m, 15.1, -8.7, -0.9, -1, 0, 0.72, 0.96, "dec_post1")
    framed(m, 15.1, -10.4, 0.2, -1, 0, 0.72, 0.96, "dec_post2")
    framed(m, 3.8, 1.32, z + 2.3, 0, -1, 0.6, 0.8, "dec_print1")
    framed(m, 5.2, 1.32, z + 2.3, 0, -1, 0.6, 0.8, "dec_print2")
    framed(m, 12.0, 1.32, z + 2.3, 0, -1, 0.66, 0.88, "dec_post4")


def kitchen(m):
    z = FLOOR
    D, H, T = 0.64, 0.9, 0.06          # the counters: depth, height to the top, the top's thickness
    # along the back wall (y -17.32, looking north) from the west wall to the fridge; along the west wall (x -7.5)
    runs = [((-7.5, -17.32), (1, 0), 5.55, (0, 1)), ((-7.5, -16.68), (0, 1), 2.7, (1, 0))]
    for (sx, sy), (ax, ay), L, (nx, ny) in runs:
        A, N = np.array([ax, ay], float), np.array([nx, ny], float)
        S = np.array([sx, sy], float)
        n_doors = max(1, int(round(L / 0.55)))
        dw = L / n_doors
        p0, p1 = S, S + A * L
        lo, hi = np.minimum(p0, p1 + N * D), np.maximum(p0, p1 + N * D)
        lo2, hi2 = np.minimum(p0, p1 + N * (D - 0.05)), np.maximum(p0, p1 + N * (D - 0.05))
        m.box(lo2[0], lo2[1], z, hi2[0], hi2[1], z + 0.1, "dec_darkwood")             # the toe kick
        m.box(lo[0], lo[1], z + 0.1, hi[0], hi[1], z + H - T, "dec_white")              # the cabinets
        for i in range(n_doors):                                                       # their doors
            f0 = S + A * (i * dw) + N * (D + 0.004)
            f1 = S + A * ((i + 1) * dw) + N * (D + 0.004)
            face(m, f0, f1, z + 0.13, z + H - T - 0.02, N, "dec_cabinet", True)
        q1 = p1 + N * (D + 0.04)
        lo, hi = np.minimum(p0, q1), np.maximum(p0, q1)
        m.box(lo[0], lo[1], z + H - T, hi[0], hi[1], z + H, "dec_ytile")               # the tiled top
        face(m, p0 + N * 0.01, p1 + N * 0.01, z + H, z + H + 0.62, N, "dec_ytile")     # the splashback
    # the upper cabinets on the back wall, the hood between them over the stove
    for x0, x1 in ((-7.5, -5.4), (-3.8, -1.95)):
        n = max(1, int(round((x1 - x0) / 0.55)))
        dw = (x1 - x0) / n
        for i in range(n):
            a, b = x0 + i * dw, x0 + (i + 1) * dw
            m.box(a, -17.32, z + 1.75, b, -16.94, z + 2.6, "dec_white")
            face(m, (a, -16.936), (b, -16.936), z + 1.78, z + 2.57, (0, 1), "dec_cabinet", True)
        m.box(x0 - 0.02, -17.32, z + 2.6, x1 + 0.02, -16.9, z + 2.68, "dec_white")
    m.box(-5.4, -17.32, z + 1.75, -3.8, -16.85, z + 2.68, "dec_copper")                # the hood
    m.box(-5.0, -17.31, z + 2.0, -4.2, -16.84, z + 2.4, "dec_black")
    m.box(-5.15, -17.2, z + H, -4.05, -16.75, z + H + 0.03, "dec_black")              # the cooktop
    m.box(-7.45, -15.5, z + H, -6.95, -14.8, z + H + 0.005, "dec_steel")              # the sink, its tap
    m.cyl(-7.38, -15.15, z + H, z + H + 0.35, 0.02, "dec_steel", n=6)
    m.bar((-7.38, -15.15, z + H + 0.35), (-7.15, -15.15, z + H + 0.3), 0.015, "dec_steel")
    # the fridge: steel, at the back wall's east end
    m.box(-1.94, -17.32, z, -0.98, -16.58, z + 1.95, "dec_steel")
    m.box(-1.94, -16.585, z + 1.18, -0.98, -16.57, z + 1.2, "dec_black")
    m.box(-1.12, -16.57, z + 0.5, -1.08, -16.53, z + 1.1, "dec_steel")
    m.box(-1.12, -16.57, z + 1.3, -1.08, -16.53, z + 1.8, "dec_steel")
    # the cart (an island of light wood), the round table, its white chairs, the navy rug
    m.add(console(1.05, 0.6, 0.92, "dec_lightwood", drawers=1), -7.18, -13.42, z, 90)   # in the corner, as the show's
    m.box(-7.43, -13.9, z + 0.3, -6.93, -12.94, z + 0.33, "dec_lightwood")
    m.box(-7.5, -14.26, z + 1.55, -7.12, -13.66, z + 2.68, "dec_white")               # the tall glass cabinet
    face(m, (-7.116, -14.22), (-7.116, -13.7), z + 1.58, z + 2.65, (1, 0), "dec_cabinet", True)
    rug(m, -3.6, -11.6, z, 3.0, 2.6, "dec_rug4")
    m.cyl(-3.6, -11.6, z, z + 0.06, 0.34, "dec_white", n=12)
    m.cyl(-3.6, -11.6, z + 0.06, z + 0.72, 0.09, "dec_white", n=10)
    m.cyl(-3.6, -11.6, z + 0.72, z + 0.78, 0.72, "dec_bluegrey", n=20, top="dec_bluegrey")
    for k in range(4):
        a = np.radians(45 + 90 * k)
        cx, cy = -3.6 + np.cos(a) * 0.95, -11.6 + np.sin(a) * 0.95
        m.add(chair_x(), cx, cy, z, np.degrees(np.arctan2(-np.cos(a), np.sin(a))))  # facing the table
        px, py = -3.6 + np.cos(a) * 0.45, -11.6 + np.sin(a) * 0.45
        m.box(px - 0.17, py - 0.13, z + 0.78, px + 0.17, py + 0.13, z + 0.785, "dec_white")   # the place mats
    framed(m, -7.5, -13.7, z + 2.1, 1, 0, 0.6, 0.8, "dec_post4")


def family(m):
    z = FLOOR
    rug(m, -3.6, -4.2, z, 3.4, 4.0, "dec_rug3", heading=90)
    m.add(sofa(2.7, 1.0, "dec_grey", pillows=("dec_beige", "dec_grey")), -3.6, -6.3, z, 180)
    m.box(-4.35, -3.75, z + 0.1, -2.85, -2.95, z + 0.48, "dec_ottoman")              # the red ottoman, its legs
    legs_m = Mesh()
    legs(legs_m, 1.5, 0.8, 0.1, 0.03, "dec_darkwood", 0.06)
    m.add(legs_m, -3.6, -3.35, z, 0)
    # the armoire on the north wall, the cane chair, a sideboard, the prints over it, a plant
    m.box(-8.62, -4.75, z, -7.92, -2.95, z + 2.45, "dec_wood")                        # the armoire (west wall)
    m.box(-8.62, -4.81, z + 2.45, -7.88, -2.89, z + 2.58, "dec_wood")
    for y0, y1 in ((-4.65, -3.88), (-3.82, -3.05)):
        m.box(-7.92, y0, z + 1.0, -7.9, y1, z + 2.3, "dec_darkwood")
        m.box(-7.92, y0, z + 0.15, -7.9, y1, z + 0.9, "dec_darkwood")
    # the big TV facing the sofa, on a low stand against the north wall
    m.add(console(2.2, 0.5, 0.55, "dec_darkwood", drawers=4), -3.6, 1.05, z, 180)
    m.box(-4.6, 0.98, z + 0.55, -2.6, 1.1, z + 0.6, "dec_black")
    m.box(-3.68, 1.0, z + 0.6, -3.52, 1.08, z + 0.72, "dec_black")
    m.box(-4.75, 1.02, z + 0.72, -2.45, 1.12, z + 2.02, "dec_black")
    face(m, (-4.68, 1.017), (-2.52, 1.017), z + 0.78, z + 1.96, (0, -1), "dec_screen", True)
    m.add(armchair("dec_beige", 0.95, 0.9, frame="dec_cane"), -6.4, -2.0, z, 63)
    m.add(console(1.6, 0.5, 0.75, drawers=3), -1.2, 1.05, z, 180)
    framed(m, -1.7, 1.32, z + 2.1, 0, -1, 0.5, 0.66, "dec_print1")
    framed(m, -0.7, 1.32, z + 2.1, 0, -1, 0.5, 0.66, "dec_print2")


def slab(m, p0, p1, w, t, mat):
    """a board from p0 to p1 (in a plane x = const), w wide (x), t thick"""
    p0, p1 = np.array(p0, float), np.array(p1, float)
    d = p1 - p0
    u = np.array([1.0, 0, 0])
    v = np.cross(d, u)
    v /= np.linalg.norm(v)
    c = lambda p, a, b: tuple(p + u * a * w / 2 + v * b * t / 2)
    for p in (p0, p1):
        pass
    corners = [(a, b) for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for i in range(4):
        a0, b0 = corners[i]
        a1, b1 = corners[(i + 1) % 4]
        m.quad(c(p0, a0, b0), c(p0, a1, b1), c(p1, a1, b1), c(p1, a0, b0), mat)
    m.quad(c(p0, -1, 1), c(p0, 1, 1), c(p0, 1, -1), c(p0, -1, -1), mat)
    m.quad(c(p1, -1, -1), c(p1, 1, -1), c(p1, 1, 1), c(p1, -1, 1), mat)


def deck_chair():
    """the show's deck chair: a wooden lounger, its back leaning, green cushions; its foot to -y"""
    m = Mesh()
    w = 0.66
    for s in (-1, 1):                                       # the side frames, the armrests, the legs
        x = s * (w / 2 + 0.03)
        m.box(x - 0.03, -0.75, 0.0, x + 0.03, -0.69, 0.42, "dec_lightwood")
        m.box(x - 0.03, 0.25, 0.0, x + 0.03, 0.31, 0.55, "dec_lightwood")
        m.box(x - 0.04, -0.8, 0.28, x + 0.04, 0.4, 0.34, "dec_lightwood")
        m.box(x - 0.06, -0.2, 0.55, x + 0.06, 0.35, 0.6, "dec_lightwood")
    m.box(-w / 2, -0.8, 0.3, w / 2, 0.3, 0.36, "dec_lightwood")
    m.box(-w / 2 + 0.02, -0.78, 0.36, w / 2 - 0.02, 0.28, 0.44, "dec_sofa")    # the seat's cushion
    slab(m, (0, 0.3, 0.36), (0, 0.75, 1.05), w, 0.06, "dec_lightwood")          # the back, leaning
    slab(m, (0, 0.24, 0.42), (0, 0.66, 1.06), w - 0.04, 0.08, "dec_sofa")
    return m


def deck(m):
    z = RAISED
    for x in (4.4, 6.2):
        m.add(deck_chair(), x, -16.4, z, 0)
    m.box(5.05, -16.65, z + 0.38, 5.55, -16.15, z + 0.42, "dec_lightwood")       # the little table between
    for sx in (5.1, 5.5):
        for sy in (-16.6, -16.2):
            m.box(sx - 0.02, sy - 0.02, z, sx + 0.02, sy + 0.02, z + 0.38, "dec_lightwood")
    m.cyl(3.2, -15.2, z, z + 0.6, 0.38, "dec_tile", n=10, r_top=0.46)              # a big pot, a palm in it
    m.cyl(3.2, -15.2, z + 0.6, z + 1.6, 0.06, "dec_wood", n=6)
    for k in range(6):
        a = np.radians(k * 60)
        m.bar((3.2, -15.2, z + 1.55), (3.2 + 0.8 * np.cos(a), -15.2 + 0.8 * np.sin(a), z + 1.2), 0.08, "dec_leaves")


def bathroom(m):
    """Charlie's bathroom upstairs: terracotta on its floor, as the show's (its walls: build.py's BATH)"""
    z = 0.87 + 0.012
    for x0, y0, x1, y1 in ((31.25, -7.75, 34.6, -1.7), (34.6, -6.95, 36.55, -1.7)):
        m.quad((x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z), "dec_tile")


def build():
    """the decoration's faces, materials and texture coordinates (NaN: by where they are)"""
    m = Mesh()
    living(m)
    kitchen(m)
    family(m)
    deck(m)
    bathroom(m)
    T, M, U = m.arrays()
    print("decor: %d faces (living room, kitchen, family room)" % len(T))
    return T, M, U


def prints(specs):
    """the framed prints where the game's frames were (build.py's decorate: x, y, z of the middle, the wall's normal)"""
    m = Mesh()
    texs = ("dec_print1", "dec_post2", "dec_print2", "dec_post1", "dec_post3")
    for i, (x, y, zc, nx, ny) in enumerate(specs):
        framed(m, x, y, zc, nx, ny, 0.6, 0.8, texs[i % len(texs)])
    return m.arrays()
