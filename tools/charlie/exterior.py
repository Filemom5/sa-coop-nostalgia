"""The house's outside, to look less of one colour, as the reference pictures: a stone base along the walls of the
ground floor (the lot's level), terracotta sills under the windows and dark wood lintels over them and over the doors,
wooden shutters by the windows toward the street, a dark wood beam at the upper floor's level, stone pilasters along the lot's wall to the street, flower beds by
the house's front with bushes in flower, bougainvillea up the pergola's posts and the court's walls, and the sidewalk
where the lot was (it went 4.3 m back: the game's sidewalk does not reach the old wall everywhere).

All in the block's axes, as decor.py (its Mesh): build.py adds it to the model and the collision."""
import numpy as np

from decor import Mesh, face

CELL = 0.25


def _normals(T):
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    return n / np.maximum(np.linalg.norm(n, axis=1), 1e-9)[:, None]


def covered_map(house, mats, zf):
    """where the house has a ceiling or a floor over the ground floor's rooms (0.25 m cells): inside"""
    n = _normals(house)
    sel = (np.abs(n[:, 2]) > 0.3) & (house[:, :, 2].min(1) > zf + 2.2)  # flat or sloped (the vaulted ceilings)
    P = house[sel]
    x0, y0 = house[..., 0].min() - 2, house[..., 1].min() - 2
    w = int((house[..., 0].max() + 2 - x0) / CELL) + 1
    h = int((house[..., 1].max() + 2 - y0) / CELL) + 1
    grid = np.zeros((h, w), bool)
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    for t in P:
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        if abs(d) < 1e-9:
            continue
        i0, i1 = int((min(ay, by, qy) - y0) / CELL), int((max(ay, by, qy) - y0) / CELL) + 1
        j0, j1 = int((min(ax, bx, qx) - x0) / CELL), int((max(ax, bx, qx) - x0) / CELL) + 1
        sx, sy = cx[i0:i1, j0:j1], cy[i0:i1, j0:j1]
        u = ((sx - ax) * (qy - ay) - (qx - ax) * (sy - ay)) / d
        v = ((bx - ax) * (sy - ay) - (sx - ax) * (by - ay)) / d
        grid[i0:i1, j0:j1] |= (u >= -0.02) & (v >= -0.02) & (u + v <= 1.02)

    def inside(x, y):
        i, j = int((y - y0) / CELL), int((x - x0) / CELL)
        return 0 <= i < h and 0 <= j < w and bool(grid[i, j])
    return inside


def cut(T, z):
    """each face's segment at height z: [(i, p, q)]"""
    out = []
    for i, t in enumerate(T):
        dz = t[:, 2] - z
        if (dz > 0).all() or (dz < 0).all():
            continue
        pts = []
        for k in range(3):
            a, b, za, zb = t[k], t[(k + 1) % 3], dz[k], dz[(k + 1) % 3]
            if (za > 0) != (zb > 0):
                pts.append(a + (b - a) * (za / (za - zb)))
        if len(pts) == 2:
            out.append((i, pts[0][:2], pts[1][:2]))
    return out


def outer_walls(house, mats, zf, z, inside, solid=True):
    """the house's outside walls at height z: {(nx, ny, offset): [(from, to) along the wall]} (offset: n . p; along:
    the wall's right seen from outside), merged"""
    n3 = _normals(house)
    walls = {}
    for i, p, q in cut(house, z):
        m = mats[i]
        if solid and ("Glass" in m or m in ("door_leaf", "garage_door", "rail_iron", "piano_black", "piano_keys")):
            continue
        n = n3[i, :2]
        if abs(n3[i, 2]) > 0.2 or np.hypot(*n) < 0.5:
            continue
        n = n / np.hypot(*n)
        if abs(n[0]) > 0.98 or abs(n[1]) > 0.98:
            n = np.round(n)
        mid = (p + q) / 2
        if inside(*(mid - n * 0.6)) and not inside(*(mid + n * 1.0)):
            pass
        elif inside(*(mid + n * 0.6)) and not inside(*(mid - n * 1.0)):
            n = -n
        else:
            continue
        right = np.array([n[1], -n[0]])
        key = (round(n[0], 2), round(n[1], 2), round(float(mid @ n), 2))
        a, b = sorted((float(p @ right), float(q @ right)))
        walls.setdefault(key, []).append((a, b))
    for k, v in walls.items():
        v.sort()
        merged = [list(v[0])]
        for a, b in v[1:]:
            if a <= merged[-1][1] + 0.04:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        walls[k] = [(a, b) for a, b in merged if b - a > 0.15]
    return walls


def _pt(key, s):
    """the point at s along the wall key (nx, ny, offset)"""
    nx, ny, off = key
    n = np.array([nx, ny])
    right = np.array([ny, -nx])
    return n * off + right * s


def stone_base(m, walls, zf, height=0.4):  # under the windows (their sills at 0.48)
    for key, runs in walls.items():
        n = np.array(key[:2])
        for a, b in runs:
            p0, p1 = _pt(key, a), _pt(key, b)
            o0, o1 = p0 + n * 0.07, p1 + n * 0.07
            lo, hi = np.minimum.reduce([p0, p1, o0, o1]), np.maximum.reduce([p0, p1, o0, o1])
            m.box(lo[0], lo[1], zf - 0.3, hi[0], hi[1], zf + height, "dec_stone")
            c0, c1 = p0 + n * 0.11, p1 + n * 0.11                                  # its cap, a little out
            lo, hi = np.minimum.reduce([p0, p1, c0, c1]), np.maximum.reduce([p0, p1, c0, c1])
            m.box(lo[0], lo[1], zf + height, hi[0], hi[1], zf + height + 0.08, "dec_stone")


def windows(house, mats, inside):
    """the house's windows and glass doors seen from outside: [(n, centre along, half width, z0, z1, wall offset)]"""
    n3 = _normals(house)
    glass = np.array(["Glass" in m for m in mats]) & (np.abs(n3[:, 2]) < 0.2)
    found = []
    for i in np.where(glass)[0]:
        t = house[i]
        n = n3[i, :2] / np.hypot(*n3[i, :2])
        n = np.round(n) if (abs(n[0]) > 0.98 or abs(n[1]) > 0.98) else n
        c = t.mean(0)
        if inside(*(c[:2] - n * 0.8)) and not inside(*(c[:2] + n * 0.8)):
            pass
        elif inside(*(c[:2] + n * 0.8)) and not inside(*(c[:2] - n * 0.8)):
            n = -n
        else:
            continue
        right = np.array([n[1], -n[0]])
        s = t[:, :2] @ right
        found.append([n, s.min(), s.max(), t[:, 2].min(), t[:, 2].max(), float(c[:2] @ n)])
    # the panes of one window together: the same way, over each other along, and their planes within 0.4 m
    groups = []
    for f in found:
        for g in groups:
            if np.allclose(g[0], f[0]) and abs(g[5] - f[5]) < 0.4 and f[1] < g[2] + 0.15 and f[2] > g[1] - 0.15 and \
                    f[3] < g[4] + 0.3 and f[4] > g[3] - 0.3:
                g[1], g[2], g[3], g[4], g[5] = min(g[1], f[1]), max(g[2], f[2]), min(g[3], f[3]), max(g[4], f[4]), \
                    max(g[5], f[5])
                break
        else:
            groups.append(list(f))
    return groups


def wall_face(walls, n, s0, s1, z, plane):
    """the outside wall's plane by a window (the nearest, out from its glass), else the glass's plane + 0.24"""
    best = None
    for key, runs in walls.items():
        if not np.allclose(key[:2], n, atol=0.02):
            continue
        if not (plane - 0.05 < key[2] < plane + 0.7):
            continue
        if any(a < s1 + 0.3 and b > s0 - 0.3 for a, b in runs):
            if best is None or key[2] < best:
                best = key[2]
    return best if best is not None else plane + 0.24


def window_trims(m, house, mats, zf, inside):
    groups = windows(house, mats, inside)
    walls_mid = outer_walls(house, mats, zf, zf + 2.0, inside)
    n_sill = n_shut = 0
    for n, s0, s1, z0, z1, plane in groups:
        if s1 - s0 < 0.3:
            continue
        n = np.array(n)
        wp = wall_face(walls_mid, n, s0, s1, (z0 + z1) / 2, plane)
        key = (n[0], n[1], wp)
        w = s1 - s0
        a, b = _pt(key, s0 - 0.12), _pt(key, s1 + 0.12)
        if z0 > zf + 0.35:  # a window: a terracotta sill under it
            o = n * 0.16
            lo, hi = np.minimum.reduce([a, b, a + o, b + o]), np.maximum.reduce([a, b, a + o, b + o])
            m.box(lo[0], lo[1], z0 - 0.12, hi[0], hi[1], z0 - 0.02, "dec_cap")
            n_sill += 1
        o = n * 0.08                       # a dark wood lintel over it
        lo, hi = np.minimum.reduce([a - n * 0.0, b, a + o, b + o]), np.maximum.reduce([a, b, a + o, b + o])
        m.box(lo[0], lo[1], z1 + 0.02, hi[0], hi[1], z1 + 0.2, "dec_wood")
        # the windows toward the street (and the court) on the ground floor: shutters open beside them
        if n[1] > 0.9 and z0 > zf + 0.35 and z1 < zf + 4.0 and w < 2.2:
            for side, (sa, sb) in ((-1, (s0 - 0.08 - w / 2, s0 - 0.08)), (1, (s1 + 0.08, s1 + 0.08 + w / 2))):
                p, q = _pt(key, sa) + n * 0.04, _pt(key, sb) + n * 0.04
                face(m, p, q, z0, z1, n, "dec_shutter", True)
                face(m, q - n * 0.035, p - n * 0.035, z0, z1, -n, "dec_shutter")
            n_shut += 1
    return n_sill, n_shut, len(groups)


def bush(m, x, y, z, size, rng, mat="dec_bloom"):
    """a bush: a few boxes of leaves, flowers in them"""
    for _ in range(4):
        dx, dy = rng.uniform(-0.3, 0.3, 2) * size
        s = rng.uniform(0.45, 0.7) * size
        h = rng.uniform(0.5, 0.9) * size
        m.box(x + dx - s / 2, y + dy - s / 2, z, x + dx + s / 2, y + dy + s / 2, z + h, mat if rng.random() < 0.6
              else "dec_leaves")


def vine(m, x, y, z0, z1, n, rng):
    """a climber up a wall or a post: leaves and flowers in clumps, wandering"""
    z = z0
    off = 0.0
    right = np.array([n[1], -n[0]])
    while z < z1:
        s = rng.uniform(0.25, 0.45)
        off += rng.uniform(-0.15, 0.15)
        c = np.array([x, y]) + right * off + np.array(n) * (s / 2)
        m.box(c[0] - s / 2, c[1] - s / 2, z, c[0] + s / 2, c[1] + s / 2, z + s * 1.2,
              "dec_bloom" if rng.random() < 0.55 else "dec_leaves")
        z += s * 0.9
    # its top: spreading along
    for k in range(5):
        s = rng.uniform(0.25, 0.4)
        c = np.array([x, y]) + right * (off + (k - 2) * 0.35) + np.array(n) * (s / 2)
        m.box(c[0] - s / 2, c[1] - s / 2, z1 - rng.uniform(0, 0.4), c[0] + s / 2, c[1] + s / 2, z1 + s,
              "dec_bloom" if rng.random() < 0.6 else "dec_leaves")


def lot_details(m, lot, ramps, zf, street_z, court, front_y):
    """the pilasters on the street's wall, the beds by the house's front, the climbers, the sidewalk's strip"""
    rng = np.random.default_rng(7)
    X0, Y0, X1, Y1 = lot
    zt = zf - 0.03
    # stone pilasters on the street's wall (its outside, looking north), every 4.5 m, not on the ways in
    for x in np.arange(X0 + 0.4, X1, 4.5):
        if any(r[0] - 1.2 < x < r[1] + 1.2 for r in ramps):
            continue
        m.box(x - 0.3, Y1 - 0.02, street_z - 0.3, x + 0.3, Y1 + 0.12, zt + 1.15, "dec_stone")
        m.box(x - 0.36, Y1 - 0.34, zt + 1.12, x + 0.36, Y1 + 0.18, zt + 1.24, "dec_stone")
    # the sidewalk where the old block was: just under the game's own (it shows only where the game's has a gap)
    walk_z = 3.30 - 8.906
    m.quad((X0 - 1.0, Y1, walk_z), (X1 + 1.0, Y1, walk_z), (X1 + 1.0, Y1 + 4.6, walk_z), (X0 - 1.0, Y1 + 4.6, walk_z),
           "dec_walk")
    # flower beds along the house's front (between the walk and the house), stone edged, bushes in flower
    for xa, xb in ((-7.5, 4.0), (6.0, 15.5), (23.0, 33.0)):
        y0, y1 = front_y + 0.05, front_y + 0.95
        m.box(xa, y0, zt, xb, y1, zt + 0.4, "dec_stone")
        m.box(xa + 0.1, y0 + 0.1, zt + 0.4, xb - 0.1, y1 - 0.1, zt + 0.42, "dec_soil")
        for x in np.arange(xa + 0.6, xb - 0.3, 1.3):
            bush(m, x, (y0 + y1) / 2, zt + 0.42, rng.uniform(0.7, 1.0), rng)
    # bougainvillea up the pergola's posts and the court's walls
    cx0, cy0, cx1, cy1 = court
    for x in (cx0 + 0.6, cx1 - 0.6):
        vine(m, x - 0.14, cy1 + 0.36, zt, zt + 3.6, (-1, 0), rng)
    for y in (cy0 + 0.8, cy0 + 3.0):
        vine(m, cx0 + 0.02, y, zt, zt + 3.2, (1, 0), rng)
        vine(m, cx1 - 0.02, y, zt, zt + 3.2, (-1, 0), rng)


def storey_band(m, house, mats, zf, inside, storey=4.48):
    """a dark wood beam along the outside walls where they go on up to the upper floor, at its level"""
    low = outer_walls(house, mats, zf, zf + storey - 0.4, inside)
    high = outer_walls(house, mats, zf, zf + storey + 0.5, inside)
    n = 0
    for key, runs in low.items():
        for k2, runs2 in high.items():
            if k2[:2] != key[:2] or abs(k2[2] - key[2]) > 0.05:
                continue
            for a, b in runs:
                for c, d in runs2:
                    s0, s1 = max(a, c), min(b, d)
                    if s1 - s0 < 0.5:
                        continue
                    nn = np.array(key[:2])
                    p0, p1 = _pt(key, s0), _pt(key, s1)
                    lo = np.minimum.reduce([p0, p1, p0 + nn * 0.1, p1 + nn * 0.1])
                    hi = np.maximum.reduce([p0, p1, p0 + nn * 0.1, p1 + nn * 0.1])
                    m.box(lo[0], lo[1], zf + storey - 0.12, hi[0], hi[1], zf + storey + 0.12, "dec_wood", bottom=True)
                    n += 1
    return n


def build(house, mats, zf, lot, ramps, street_z, court, front_y):
    m = Mesh()
    inside = covered_map(house, mats, zf)
    walls = outer_walls(house, mats, zf, zf + 0.45, inside)
    stone_base(m, walls, zf)
    n_sill, n_shut, n_win = window_trims(m, house, mats, zf, inside)
    n_band = storey_band(m, house, mats, zf, inside)
    lot_details(m, lot, ramps, zf, street_z, court, front_y)
    T, M, U = m.arrays()
    print("exterior: stone base on %d walls, %d windows (%d sills, %d with shutters), %d beams, %d faces" %
          (len(walls), n_win, n_sill, n_shut, n_band, len(T)))
    return T, M, U
