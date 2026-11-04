"""The lot round the house, as a Malibu beach house has it: the house and its yard up on a raised lot, above the
sand, the house's own part over the beach on wooden pilings (open under it), and round the yard a stucco wall with a
terracotta cap: toward the road, the neighbours and the beach. From the street a drive up for the cars (its green
gate's leaves open along it, on to the garage's doors) and steps for the people, a green gate at their top, iron
lanterns on the gates' pillars. All in the block's axes (build.py adds it to the house's model and collision), as
faces with a material each:

  site_paving  the lot's top          site_stucco  the walls
  site_tile    the steps              site_cap     the walls' cap
  site_wood    pilings, beams         site_green   the gates' leaves
  site_lamp    the lanterns' iron     site_glow    their glass

The lot is made on a grid (CELL m): each cell is the house (its floor is there), a ramp, the lot's top, or outside;
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


def build(house_tris, floor, lot, ramps, beach_y, street_z=3.62, sand_z=3.15):
    """house_tris: the house's faces (block's axes); floor: its floor's height (block's axes); lot: (x0, y0, x1, y1);
    ramps: [(x0, x1, y_top, style)] up from the street (the lot's north edge) to the lot: style "drive" (a slope for
    the cars, its gate's leaves open along its sides) or "steps" (the people's way in, its gate at the top);
    beach_y(x): south of it the lot is beach (the house over it on pilings). Returns the faces and their materials,
    the collision's own faces (the stairs' slopes, the skirt under the house) and the cells' kinds"""
    m = Mesh()
    col = []  # collision only
    X0, Y0, X1, Y1 = lot
    w, h = int(round((X1 - X0) / CELL)), int(round((Y1 - Y0) / CELL))
    house = raster(house_tris, X0, Y0, w, h, floor - 0.3, floor + 0.5)
    house = house | np.roll(house, 1, 0) & np.roll(house, -1, 0)  # fill one-cell cracks
    cy = Y0 + (np.arange(h) + 0.5) * CELL
    cx = X0 + (np.arange(w) + 0.5) * CELL
    CX, CY = np.meshgrid(cx, cy)
    which = np.full((h, w), -1)  # the ramp a cell is on
    for k, (rx0, rx1, ry_top, style) in enumerate(ramps):
        which[(CX > rx0) & (CX < rx1) & (CY > ry_top) & ~house] = k
    is_ramp = which >= 0
    by = np.vectorize(beach_y)(CX)
    top = ~house & ~is_ramp & (CY > by)
    kind = np.zeros((h, w), int)  # 0 outside, 1 house, 2 lot's top, 3 ramp
    kind[house], kind[top], kind[is_ramp] = 1, 2, 3
    zt = floor - 0.03  # the lot's top, a little under the house's floor

    def ramp_of(x):
        for r in ramps:
            if r[0] - 0.01 <= x <= r[1] + 0.01:
                return r
        return ramps[0]

    def ramp_z(x, y):
        rx0, rx1, ry_top, style = ramp_of(x)
        return float(zt + (street_z - zt) * np.clip((y - ry_top) / (Y1 - ry_top), 0, 1))

    # the lot's top: rows of cells, merged
    for i in range(h):
        for a, b in runs(kind[i] == 2):
            x0_, x1_ = X0 + a * CELL, X0 + b * CELL
            y0_, y1_ = Y0 + i * CELL, Y0 + (i + 1) * CELL
            m.quad((x0_, y0_, zt), (x1_, y0_, zt), (x1_, y1_, zt), (x0_, y1_, zt), "site_paving")
    # the ramps: the drive a slope, the steps steps (and a slope in the collision)
    for rx0, rx1, ry_top, style in ramps:
        if style == "drive":
            for i in range(h):
                for a, b in runs(kind[i] == 3):
                    x0_, x1_ = X0 + a * CELL, X0 + b * CELL
                    if not (rx0 - 0.01 <= (x0_ + x1_) / 2 <= rx1 + 0.01):
                        continue
                    y0_, y1_ = Y0 + i * CELL, Y0 + (i + 1) * CELL
                    m.quad((x0_, y0_, ramp_z(x0_, y0_)), (x1_, y0_, ramp_z(x0_, y0_)), (x1_, y1_, ramp_z(x0_, y1_)),
                           (x0_, y1_, ramp_z(x0_, y1_)), "site_paving")
        else:
            n = int(round((Y1 - ry_top) / 0.30))
            r = (zt - street_z) / n
            for k in range(n):  # k = 0 at the street
                m.box(rx0, Y1 - (k + 1) * 0.30, street_z - 0.3, rx1, Y1 - k * 0.30, street_z + (k + 1) * r, "site_tile")
            a, b, c, d = (rx0, Y1, street_z), (rx1, Y1, street_z), (rx1, ry_top, zt), (rx0, ry_top, zt)
            col += [[a, c, b], [a, d, c]]

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

    piles = []
    for (p, q, k_in, k_out, n) in walls:
        street = n == (0, 1) and abs(p[1] - Y1) < 0.01
        if k_in == 2:
            if k_out == 3:  # the lot's side over a ramp: down to it (the steps: to the street), a wall on top
                if n[1] != 0:
                    continue  # its end: the ramp's top is the lot's
                (ax, ay), (bx, by_) = p, q
                steps = ramp_of(ax - n[0] * 0.1)[3] == "steps"
                za = street_z - 0.3 if steps else ramp_z(ax - n[0] * 0.1, ay)
                zb = street_z - 0.3 if steps else ramp_z(ax - n[0] * 0.1, by_)
                m.quad((ax, ay, za), (bx, by_, zb), (bx, by_, zt), (ax, ay, zt), "site_stucco")
                if min(ay, by_) < Y1 - 0.8:
                    parapet(m, p, q, n, zt, 1.0)
                continue
            m.wall(p, q, sand_z - 0.3, zt, "site_stucco")
            if not (street and any(r[0] - 0.01 < (p[0] + q[0]) / 2 < r[1] + 0.01 for r in ramps)):
                parapet(m, p, q, n, zt, 1.0)  # toward the road, the neighbours and the beach
        elif k_in == 1:  # the house over the beach: pilings along it; under it a skirt (collision only)
            (ax, ay), (bx, by_) = p, q
            col += [[(ax, ay, sand_z - 0.3), (bx, by_, sand_z - 0.3), (bx, by_, floor - 0.2)],
                    [(ax, ay, sand_z - 0.3), (bx, by_, floor - 0.2), (ax, ay, floor - 0.2)]]
            piles.append((p, q, n))
        elif k_in == 3:  # the ramps' sides (not their ends on the street)
            if n == (0, 1):
                continue
            (ax, ay), (bx, by_) = p, q
            za, zb = ramp_z(ax - n[0] * 0.1, ay), ramp_z(ax - n[0] * 0.1, by_)
            m.quad((ax, ay, sand_z - 0.3), (bx, by_, sand_z - 0.3), (bx, by_, zb), (ax, ay, za), "site_stucco")
            parapet(m, p, q, n, None, 1.0, ramp_z=lambda y, x=ax - n[0] * 0.1: ramp_z(x, y))

    # pilings: along the house's edges over the beach (every 2.4 m) and in rows under it (3.2 m); a beam on the edges
    for p, q, n in piles:
        L = np.hypot(q[0] - p[0], q[1] - p[1])
        if L < 0.3:
            continue
        for k in range(int(L / 2.4) + 1):
            t = min(k * 2.4 + 0.2, L - 0.2) / L if L > 0.4 else 0.5
            x, y = p[0] + (q[0] - p[0]) * t - n[0] * 0.25, p[1] + (q[1] - p[1]) * t - n[1] * 0.25
            m.box(x - 0.18, y - 0.18, sand_z - 0.3, x + 0.18, y + 0.18, floor - 0.62, "site_wood")
        xa, xb = sorted((p[0], q[0]))
        ya, yb = sorted((p[1], q[1]))
        m.box(xa - max(n[0], 0) * 0.3, ya - max(n[1], 0) * 0.3, floor - 0.62,
              xb - min(n[0], 0) * 0.3, yb - min(n[1], 0) * 0.3, floor - 0.28, "site_wood", bottom=True)
    over = house & (CY < by - 0.3)
    inner = over.copy()
    for _ in range(3):  # 0.75 m from the edges
        inner = inner & np.roll(inner, 1, 0) & np.roll(inner, -1, 0) & np.roll(inner, 1, 1) & np.roll(inner, -1, 1)
    for gx in np.arange(np.ceil(X0 / 3.2) * 3.2, X1, 3.2):
        for gy in np.arange(np.ceil(Y0 / 3.2) * 3.2, Y1, 3.2):
            j, i = int((gx - X0) / CELL), int((gy - Y0) / CELL)
            if 0 <= i < h and 0 <= j < w and inner[i, j]:
                m.box(gx - 0.16, gy - 0.16, sand_z - 0.3, gx + 0.16, gy + 0.16, floor - 0.3, "site_wood")

    # the gates: pillars with lanterns at the street on both sides of each way in
    lamps = []
    for rx0, rx1, ry_top, style in ramps:
        for gx in (rx0 - 0.35, rx1 + 0.35):
            m.box(gx - 0.35, Y1 - 0.7, street_z - 0.2, gx + 0.35, Y1, zt + 1.35, "site_stucco")
            m.box(gx - 0.42, Y1 - 0.77, zt + 1.35, gx + 0.42, Y1 + 0.07, zt + 1.5, "site_cap")
            lantern(m, gx, Y1 - 0.35, zt + 1.5)
            lamps.append((gx, Y1 - 0.35, zt + 1.9))
        if style == "drive":  # the leaves open inward, along the ramp's sides
            leaf = (rx1 - rx0) / 2
            for x, s in ((rx0, 1), (rx1, -1)):
                y_far = Y1 - 0.8 - leaf
                m.box(x if s > 0 else x - 0.08, y_far, ramp_z(x + s * 0.1, y_far), x + 0.08 if s > 0 else x, Y1 - 0.8,
                      ramp_z(x + s * 0.1, y_far) + 1.9, "site_green")
        else:  # at the top of the steps: posts, the leaves open into the yard
            leaf = (rx1 - rx0) / 2
            for x, s in ((rx0, 1), (rx1, -1)):
                m.box(x - 0.15, ry_top - 0.15, zt, x + 0.15, ry_top + 0.15, zt + 2.0, "site_stucco")
                m.box(x if s > 0 else x - 0.08, ry_top - 0.15 - leaf, zt + 0.05, x + 0.08 if s > 0 else x, ry_top - 0.15,
                      zt + 1.8, "site_green")
    T, M = m.arrays()
    return T, M, np.array(col, float).reshape(-1, 3, 3), kind, lamps


def lantern(m, x, y, z):
    """a black iron lantern on a pillar (its light: a 2dfx of the model)"""
    m.box(x - 0.1, y - 0.1, z, x + 0.1, y + 0.1, z + 0.06, "site_lamp")
    m.box(x - 0.13, y - 0.13, z + 0.06, x + 0.13, y + 0.13, z + 0.45, "site_glow")
    m.box(x - 0.17, y - 0.17, z + 0.45, x + 0.17, y + 0.17, z + 0.52, "site_lamp")
    m.box(x - 0.05, y - 0.05, z + 0.52, x + 0.05, y + 0.05, z + 0.62, "site_lamp")


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
