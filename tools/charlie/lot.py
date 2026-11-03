"""The lot round the house, as a Malibu beach house has it: the house and its yard up on a raised lot, above the
sand, with a wooden bulkhead toward the sea (a white fence on it, wooden stairs down to the sand), the house's own
part over the beach on wooden pilings, and toward the road a stucco wall with a terracotta cap, its gate for the
cars at the top of a ramp up from the street. All in the block's axes (build.py adds it to the house's model and
collision), as faces with a material each:

  site_paving  the lot's top          site_stucco  the walls toward the road and the neighbours
  site_timber  the bulkhead, stairs   site_under   the shadow under the house, between the pilings
  site_wood    pilings, beams         site_white   the fence
  site_cap     the walls' cap         site_green   the gate's leaves

The lot is made on a grid (CELL m): each cell is the house (its floor is there), the ramp, the lot's top, or outside;
the walls go along the edges between them."""
import numpy as np

CELL = 0.25


class Mesh:
    def __init__(self):
        self.tris, self.mats = [], []

    def quad(self, a, b, c, d, mat):
        """a, b, c, d counter-clockwise seen from the side the face looks to"""
        self.tris += [[a, b, c], [a, c, d]]
        self.mats += [mat, mat]

    def wall(self, p, q, z0, z1, mat):
        """a vertical face from p to q (x, y), looking to the right of p->q... to its left: from outside"""
        (x0, y0), (x1, y1) = p, q
        self.quad((x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1), mat)

    def box(self, x0, y0, z0, x1, y1, z1, mat, bottom=False):
        q = self.quad
        q((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), mat)
        if bottom:
            q((x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0), mat)
        q((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), mat)
        q((x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1), mat)
        q((x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1), mat)
        q((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1), mat)

    def arrays(self):
        """without the faces of no area"""
        T, M = np.array(self.tris, float).reshape(-1, 3, 3), np.array(self.mats)
        ok = np.linalg.norm(np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]), axis=1) > 1e-6
        return T[ok], M[ok]


def raster(tris, x0, y0, w, h, zlo, zhi):
    """cells under faces looking up whose height is in [zlo, zhi] (the house's floor)"""
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    grid = np.zeros((h, w), bool)
    for t in tris:
        n = np.cross(t[1] - t[0], t[2] - t[0])
        if np.linalg.norm(n) < 1e-9 or abs(n[2]) / np.linalg.norm(n) < 0.9 or not (zlo <= t[:, 2].mean() <= zhi):
            continue
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        if abs(d) < 1e-9:
            continue
        i0, i1 = int(max(0, (min(ay, by, qy) - y0) / CELL - 1)), int(min(h, (max(ay, by, qy) - y0) / CELL + 2))
        j0, j1 = int(max(0, (min(ax, bx, qx) - x0) / CELL - 1)), int(min(w, (max(ax, bx, qx) - x0) / CELL + 2))
        X, Y = cx[i0:i1, j0:j1], cy[i0:i1, j0:j1]
        u = ((X - ax) * (qy - ay) - (qx - ax) * (Y - ay)) / d
        v = ((bx - ax) * (Y - ay) - (X - ax) * (by - ay)) / d
        grid[i0:i1, j0:j1] |= (u >= -0.02) & (v >= -0.02) & (u + v <= 1.02)
    return grid


def runs(mask):
    """the True runs of a 1-d mask: [(start, end)]"""
    out, start = [], None
    for i, m in enumerate(list(mask) + [False]):
        if m and start is None:
            start = i
        elif not m and start is not None:
            out.append((start, i))
            start = None
    return out


def build(house_tris, floor, lot, ramp, gate_x, street_z=3.62, sand_z=3.15, beach_y=-12.4, stairs_x=()):
    """house_tris: the house's faces (block's axes); floor: its floor's height (block's axes); lot: (x0, y0, x1, y1);
    ramp: (x0, x1, y_top, y_street) the drive up from the street (north) to the lot; gate_x: the gate's sides on the
    street wall; beach_y: south of it, the lot is beach; stairs_x: where wooden stairs go down to the sand"""
    m = Mesh()
    stairs = []  # (x0, x1, y) the beach stairs, for their ramps in the collision
    X0, Y0, X1, Y1 = lot
    w, h = int(round((X1 - X0) / CELL)), int(round((Y1 - Y0) / CELL))
    house = raster(house_tris, X0, Y0, w, h, floor - 0.3, floor + 0.5)
    house = house | np.roll(house, 1, 0) & np.roll(house, -1, 0)  # fill one-cell cracks
    cy = Y0 + (np.arange(h) + 0.5) * CELL
    cx = X0 + (np.arange(w) + 0.5) * CELL
    CX, CY = np.meshgrid(cx, cy)
    rx0, rx1, ry_top, ry_street = ramp
    is_ramp = (CX > rx0) & (CX < rx1) & (CY > ry_top) & ~house
    top = ~house & ~is_ramp & (CY > beach_y)
    kind = np.zeros((h, w), int)  # 0 outside, 1 house, 2 lot's top, 3 ramp
    kind[house], kind[top], kind[is_ramp] = 1, 2, 3
    zt = floor - 0.03  # the lot's top, a little under the house's floor
    ramp_z = lambda y: zt + (street_z - zt) * np.clip((y - ry_top) / (ry_street - ry_top), 0, 1)

    # the lot's top: rows of cells, merged
    for i in range(h):
        for a, b in runs(kind[i] == 2):
            x0_, x1_ = X0 + a * CELL, X0 + b * CELL
            y0_, y1_ = Y0 + i * CELL, Y0 + (i + 1) * CELL
            m.quad((x0_, y0_, zt), (x1_, y0_, zt), (x1_, y1_, zt), (x0_, y1_, zt), "site_paving")
    # the ramp: one slope per column band
    for i in range(h):
        for a, b in runs(kind[i] == 3):
            x0_, x1_ = X0 + a * CELL, X0 + b * CELL
            y0_, y1_ = Y0 + i * CELL, Y0 + (i + 1) * CELL
            m.quad((x0_, y0_, ramp_z(y0_)), (x1_, y0_, ramp_z(y0_)), (x1_, y1_, ramp_z(y1_)), (x0_, y1_, ramp_z(y1_)),
                   "site_paving")

    # the edges between kinds, as runs along each boundary line: (p, q, inside kind, outside kind, outward normal)
    walls = []
    none = np.zeros(w, int)
    for i in range(h + 1):  # between rows i-1 and i: along x
        below = kind[i - 1] if i > 0 else none
        above = kind[i] if i < h else none
        y = Y0 + i * CELL
        for k_in, k_out in ((2, 0), (1, 0), (3, 0), (2, 3)):
            for a_, b_ in runs((below == k_in) & (above == k_out)):   # looks north
                walls.append(((X0 + b_ * CELL, y), (X0 + a_ * CELL, y), k_in, k_out, (0, 1)))
            for a_, b_ in runs((above == k_in) & (below == k_out)):   # looks south
                walls.append(((X0 + a_ * CELL, y), (X0 + b_ * CELL, y), k_in, k_out, (0, -1)))
    nonec = np.zeros(h, int)
    for j in range(w + 1):  # between columns j-1 and j: along y
        left = kind[:, j - 1] if j > 0 else nonec
        right = kind[:, j] if j < w else nonec
        x = X0 + j * CELL
        for k_in, k_out in ((2, 0), (1, 0), (3, 0), (2, 3)):
            for a_, b_ in runs((left == k_in) & (right == k_out)):    # looks east
                walls.append(((x, Y0 + a_ * CELL), (x, Y0 + b_ * CELL), k_in, k_out, (1, 0)))
            for a_, b_ in runs((right == k_in) & (left == k_out)):    # looks west
                walls.append(((x, Y0 + b_ * CELL), (x, Y0 + a_ * CELL), k_in, k_out, (-1, 0)))

    fence, piles = [], []
    for (p, q, k_in, k_out, n) in walls:
        beach = n == (0, -1) and p[1] <= beach_y + CELL * 1.01
        if k_in == 2:
            if k_out == 3:  # the lot's side over the ramp
                (ax, ay), (bx, by) = p, q
                za, zb = float(ramp_z(ay)), float(ramp_z(by))
                m.quad((ax, ay, za), (bx, by, zb), (bx, by, zt), (ax, ay, zt), "site_stucco")
                continue
            m.wall(p, q, sand_z - 0.3, zt, "site_timber" if beach else "site_stucco")
            if beach:
                fence.append((p, q))
            elif not (n == (0, 1) and gate_x[0] - 0.01 < (p[0] + q[0]) / 2 < gate_x[1] + 0.01):
                parapet(m, p, q, n, zt, 1.0)  # toward the road and the neighbours
        elif k_in == 1:  # the house over the beach: the shadow under it, pilings along it
            m.wall(p, q, sand_z - 0.3, floor - 0.28, "site_under")
            piles.append((p, q, n))
        elif k_in == 3:  # the ramp's sides (not its end on the street)
            if n == (0, 1):
                continue
            (ax, ay), (bx, by) = p, q
            za, zb = float(ramp_z(ay)), float(ramp_z(by))
            m.quad((ax, ay, sand_z - 0.3), (bx, by, sand_z - 0.3), (bx, by, zb), (ax, ay, za), "site_stucco")
            parapet(m, p, q, n, None, 1.0, ramp_z=ramp_z)

    # pilings: every 2.4 m along the house's edges over the beach, a little out of the shadow; a beam on them
    for p, q, n in piles:
        L = np.hypot(q[0] - p[0], q[1] - p[1])
        if L < 0.3:
            continue
        for k in range(int(L / 2.4) + 1):
            t = min(k * 2.4 + 0.2, L - 0.2) / L if L > 0.4 else 0.5
            x, y = p[0] + (q[0] - p[0]) * t + n[0] * 0.12, p[1] + (q[1] - p[1]) * t + n[1] * 0.12
            m.box(x - 0.18, y - 0.18, sand_z - 0.3, x + 0.18, y + 0.18, floor - 0.62, "site_wood")
        xa, xb = sorted((p[0], q[0]))
        ya, yb = sorted((p[1], q[1]))
        m.box(xa + min(n[0], 0) * 0.3, ya + min(n[1], 0) * 0.3, floor - 0.62,
              xb + max(n[0], 0) * 0.3, yb + max(n[1], 0) * 0.3, floor - 0.28, "site_wood", bottom=True)

    # the fence on the bulkhead, with the stairs' gaps; the stairs down to the sand
    for p, q in fence:
        xa, xb = sorted((p[0], q[0]))
        y = p[1]
        where = [sx for sx in stairs_x if xa + 1.0 < sx < xb - 1.0] or ([(xa + xb) / 2] if xb - xa >= 6 else [])
        gaps = sorted((sx - 0.9, sx + 0.9) for sx in where)
        cur = xa
        for g0, g1 in gaps + [(xb, xb)]:
            if g0 - cur > 0.2:
                white_fence(m, cur, g0, y, zt)
            cur = g1
        for g0, g1 in gaps:
            beach_stairs(m, g0 + 0.1, g1 - 0.1, y, zt, sand_z)
            stairs.append((g0 + 0.1, g1 - 0.1, y))

    # the street's gate: pillars, the leaves open against the wall's inside
    gx0, gx1 = gate_x
    ys = Y1
    for gx in (gx0, gx1):
        m.box(gx - 0.35, ys - 0.7, street_z - 0.2, gx + 0.35, ys, zt + 1.35, "site_stucco")
        m.box(gx - 0.42, ys - 0.77, zt + 1.35, gx + 0.42, ys + 0.07, zt + 1.5, "site_cap")
        m.box(gx - 0.12, ys - 0.47, zt + 1.5, gx + 0.12, ys - 0.23, zt + 1.85, "site_lamp")
    leaf = (gx1 - gx0 - 0.7) / 2
    for x, s in ((gx0 + 0.35, 1), (gx1 - 0.35, -1)):  # open inward (south), along the ramp's sides
        m.box(x if s > 0 else x - 0.08, ys - 0.7 - leaf, float(ramp_z(ys - 0.7 - leaf)),
              x + 0.08 if s > 0 else x, ys - 0.7, float(ramp_z(ys - 0.7)) + 1.7, "site_green")
    T, M = m.arrays()
    col = [ramp_faces(a, b, y, zt, sand_z) for a, b, y in stairs]
    return T, M, (np.concatenate(col) if col else np.zeros((0, 3, 3))), kind


def parapet(m, p, q, n, z, height, ramp_z=None):
    """a wall of 30 cm on top of the lot's edge from p to q (looking along n), its cap of terracotta"""
    (ax, ay), (bx, by) = p, q
    nx, ny = n
    ix, iy = -nx * 0.3, -ny * 0.3  # inward
    za = z if z is not None else float(ramp_z(ay))
    zb = z if z is not None else float(ramp_z(by))
    pa, pb = (ax, ay), (bx, by)
    qa, qb = (ax + ix, ay + iy), (bx + ix, by + iy)
    m.quad((pa[0], pa[1], za), (pb[0], pb[1], zb), (pb[0], pb[1], zb + height), (pa[0], pa[1], za + height), "site_stucco")
    m.quad((qb[0], qb[1], zb), (qa[0], qa[1], za), (qa[0], qa[1], za + height), (qb[0], qb[1], zb + height), "site_stucco")
    # the cap, a little wider
    ox, oy = nx * 0.05, ny * 0.05
    c0 = (pa[0] + ox, pa[1] + oy)
    c1 = (pb[0] + ox, pb[1] + oy)
    c2 = (qb[0] - ox, qb[1] - oy)
    c3 = (qa[0] - ox, qa[1] - oy)
    ha, hb = za + height + 0.12, zb + height + 0.12
    m.quad((c0[0], c0[1], ha), (c1[0], c1[1], hb), (c2[0], c2[1], hb), (c3[0], c3[1], ha), "site_cap")
    m.quad((c0[0], c0[1], ha - 0.12), (c1[0], c1[1], hb - 0.12), (c1[0], c1[1], hb), (c0[0], c0[1], ha), "site_cap")
    m.quad((c2[0], c2[1], hb - 0.12), (c3[0], c3[1], ha - 0.12), (c3[0], c3[1], ha), (c2[0], c2[1], hb), "site_cap")


def white_fence(m, x0, x1, y, z):
    """posts every 1.8 m, two rails, a cap rail, balusters every 15 cm, on the bulkhead's edge (y), facing south"""
    yi = y + 0.12
    for k in range(int((x1 - x0) / 1.8) + 2):
        x = min(x0 + k * 1.8, x1)
        m.box(x - 0.07, yi - 0.07, z, x + 0.07, yi + 0.07, z + 1.1, "site_white")
    m.box(x0, yi - 0.05, z + 1.0, x1, yi + 0.05, z + 1.08, "site_white")
    m.box(x0, yi - 0.03, z + 0.12, x1, yi + 0.03, z + 0.2, "site_white")
    x = x0 + 0.1
    while x < x1 - 0.05:
        m.box(x - 0.025, yi - 0.025, z + 0.2, x + 0.025, yi + 0.025, z + 1.0, "site_white")
        x += 0.22


def beach_stairs(m, x0, x1, y, z_top, z_sand):
    """wooden stairs from the lot's edge (y) down to the sand, toward the south"""
    rise = z_top - z_sand
    n = max(3, int(np.ceil(rise / 0.2)))
    r, run = rise / n, 0.32
    for k in range(n):
        zt = z_top - (k + 1) * r + r  # the tread's top
        y0 = y - (k + 1) * run
        m.box(x0, y0, zt - 0.06, x1, y0 + run, zt, "site_timber", bottom=True)
    # the stringers and the hand rails
    L = n * run
    for x in (x0 - 0.08, x1):
        m.quad((x, y, z_top), (x, y - L, z_sand), (x, y - L, z_sand + 0.3), (x, y, z_top + 0.3), "site_wood")
        m.quad((x + 0.08, y - L, z_sand), (x + 0.08, y, z_top), (x + 0.08, y, z_top + 0.3), (x + 0.08, y - L, z_sand + 0.3), "site_wood")
        m.box(x, y - L, z_sand, x + 0.08, y - L + 0.08, z_sand + 1.0, "site_white")
        m.quad((x, y, z_top + 0.9), (x, y - L, z_sand + 0.9), (x, y - L, z_sand + 0.98), (x, y, z_top + 0.98), "site_white")
        m.quad((x + 0.08, y - L, z_sand + 0.9), (x + 0.08, y, z_top + 0.9), (x + 0.08, y, z_top + 0.98), (x + 0.08, y - L, z_sand + 0.98), "site_white")


def ramp_faces(x0, x1, y, z_top, z_sand):
    """the stairs' collision: one slope"""
    rise = z_top - z_sand
    n = max(3, int(np.ceil(rise / 0.2)))
    L = n * 0.32
    a, b, c, d = (x0, y, z_top), (x1, y, z_top), (x1, y - L, z_sand), (x0, y - L, z_sand)
    return np.array([[a, c, b], [a, d, c]], float)
