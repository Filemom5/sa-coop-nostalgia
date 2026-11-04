"""What the SketchUp model lacks or has shut, in its own axes (metres, z up from the ground floor):

  * open_doors(): the doors' leaves back, each turned open 90 degrees on the hinge side that has room (no wall where
    it swings), so one walks in; the garage's two doors rolled up under its ceiling, open for the cars;
  * railing(): the wrought-iron railing of the living room's stairs, on their open side (the model has none), in the
    model's plan (build.py's plan: turned round, unscaled)."""
import numpy as np

GARAGE = ("SketchUp/group_64", "SketchUp/group_65")  # its two doors (Cladding_Siding_Tan), 2.35 x 2.0 m


def _wall_points(parts, skip, z=1.0):
    """points every 5 cm on the walls (vertical faces) crossing height z, not of the groups in skip"""
    pts = []
    for path, mat, T in parts:
        if path in skip or mat.startswith("Laura") or mat == "material_0" or "Glass" in mat:
            continue
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
        ln = np.linalg.norm(n, axis=1)
        ok = (ln > 1e-9) & (np.abs(n[:, 2]) < 0.1 * np.maximum(ln, 1e-9)) & (T[..., 2].min(1) < z) & (T[..., 2].max(1) > z)
        for t in T[ok]:
            a = t[:, :2]
            i = np.argmax(np.linalg.norm(a[:, None] - a[None], axis=2).ravel())
            p, q = a[i // 3], a[i % 3]
            k = int(np.linalg.norm(q - p) / 0.05) + 2
            pts.append(p + (q - p) * np.linspace(0, 1, k)[:, None])
    return np.concatenate(pts)


def open_doors(parts, doors, sliding):
    """parts with the doors' leaves (doors, but not the sliding ones) turned open and renamed (path + '/open', material
    'door_leaf': the door finder no longer takes them for doors), the garage's doors rolled up ('garage_door')"""
    groups = {}
    for path, mat, T in parts:
        if path in doors and path not in sliding:
            groups.setdefault(path, []).append(T)
    walls = _wall_points(parts, set(groups) | set(GARAGE))
    turn = {}
    for path, Ts in groups.items():
        P = np.concatenate(Ts).reshape(-1, 3)
        lo, hi = P.min(0), P.max(0)
        along = 0 if hi[0] - lo[0] > hi[1] - lo[1] else 1  # the wall's way
        thin = 1 - along
        mid_thin = (lo[thin] + hi[thin]) / 2
        width = hi[along] - lo[along]
        best = None
        for hinge_end in (0, 1):
            h = [0.0, 0.0]
            h[along] = lo[along] if hinge_end == 0 else hi[along]
            h[thin] = mid_thin
            d = 1.0 if hinge_end == 0 else -1.0  # from the hinge into the leaf, along the wall
            for s in (1.0, -1.0):                # where it swings to
                q = np.array(h)
                tip = q.copy()
                tip[thin] += s * width
                seg = q + (tip - q) * np.linspace(0.12, 1.0, 20)[:, None]
                side = seg.copy()
                side[:, along] += d * 0.06
                hits = sum(int((np.linalg.norm(walls - p, axis=1) < 0.07).any()) for p in np.concatenate([seg, side]))
                if best is None or hits < best[0]:
                    best = (hits, h, d, s)
        _, h, d, s = best
        dvec = np.zeros(2)
        dvec[along] = d
        svec = np.zeros(2)
        svec[thin] = s
        ang = np.arctan2(svec[1], svec[0]) - np.arctan2(dvec[1], dvec[0])
        turn[path] = (np.array(h), ang)
    out = []
    for path, mat, T in parts:
        if path in turn:
            h, ang = turn[path]
            c, s_ = np.cos(ang), np.sin(ang)
            T = T.copy()
            x, y = T[..., 0] - h[0], T[..., 1] - h[1]
            T[..., 0] = h[0] + c * x - s_ * y
            T[..., 1] = h[1] + s_ * x + c * y
            out.append((path + "/open", "door_leaf" if "Glass" not in mat else mat, T))
        elif path in GARAGE:
            T = T.copy()
            y0, top = T[..., 1].min(), T[..., 2].max()
            y, z = T[..., 1] - y0, T[..., 2]
            T[..., 1] = y0 + 0.05 + (top - z)   # up and in, under the ceiling
            T[..., 2] = top - 0.02 + y
            out.append((path + "/open", "garage_door", T))
        else:
            out.append((path, mat, T))
    print("doors: %d leaves opened, the garage's %d doors rolled up" % (len(turn), len(GARAGE)))
    return out


def _bar(a, b, r):
    """a square bar from a to b (3d), half-width r: 8 faces"""
    a, b = np.array(a, float), np.array(b, float)
    d = b - a
    d /= np.linalg.norm(d)
    u = np.cross(d, [0, 0, 1.0]) if abs(d[2]) < 0.9 else np.cross(d, [1.0, 0, 0])
    u /= np.linalg.norm(u)
    v = np.cross(d, u)
    c = [u * r + v * r, -u * r + v * r, -u * r - v * r, u * r - v * r]
    out = []
    for i in range(4):
        p, q = c[i], c[(i + 1) % 4]
        out += [[a + p, b + p, b + q], [a + p, b + q, a + q]]
    return out


def railing(plan, normals, flight_box, side_x, top_y, bottom_y, scale):
    """the stairs' railing along x = side_x (plan) from bottom_y to top_y: balusters on the steps' noses, a hand rail
    over them, a post at each end; 1 m high in the game (the game's people are not 1.6 times life)"""
    (x0, y0, z0), (x1, y1, z1) = flight_box
    up = normals[:, 2] > 0.95
    inb = np.all((plan >= np.array([x0, y0, z0]) - 1e-3) & (plan <= np.array([x1, y1, z1]) + 1e-3), axis=(1, 2))
    near = (plan[..., 0].min(1) < side_x + 0.05) & (plan[..., 0].max(1) > side_x - 0.05)
    treads = []
    for i in np.where(up & inb & near)[0]:
        treads.append((plan[i, :, 1].min(), plan[i, :, 1].max(), plan[i, :, 2].mean()))

    def z_at(y):
        hs = [z for a, b, z in treads if a - 1e-3 <= y <= b + 1e-3]
        return max(hs) if hs else None

    h = 1.0 / scale
    x = side_x + 0.03
    tris = []
    ys = np.arange(min(top_y, bottom_y), max(top_y, bottom_y) + 1e-6, 0.16)
    # the rail straight, along the flight's slope: a line through the steps' noses (the landings out of the fit),
    # as the reference's; it was the treads' middles, which made it zigzag where the treads' depths differ
    steps = [(a, b, z) for a, b, z in treads if b - a < 0.6]
    noses = np.array([((b if top_y < bottom_y else a), z) for a, b, z in steps])
    k, c = np.polyfit(noses[:, 0], noses[:, 1], 1)
    lo_z, hi_z = noses[:, 1].min(), noses[:, 1].max()
    line = lambda y: float(np.clip(k * y + c, lo_z, hi_z)) + h
    rail = []
    for y in ys:
        z = z_at(y)
        if z is None:
            continue
        top = line(y)
        tris += _bar((x, y, z), (x, y, top), 0.012)
        mid = z + (top - z) * 0.5  # a knot halfway up each baluster, as the iron's twists
        tris += _bar((x, y, mid - 0.06), (x, y, mid + 0.06), 0.028)
        rail.append((y, top))
    for y in (ys[0], ys[-1]):
        z = z_at(y)
        if z is not None:
            tris += _bar((x, y, z), (x, y, line(y) + 0.08), 0.035)
            tris += _bar((x, y, line(y) + 0.08), (x, y, line(y) + 0.14), 0.05)
    for (ya, za), (yb, zb) in zip(rail, rail[1:]):  # the hand rail (straight: the points are on the line), a thin one under it
        tris += _bar((x, ya, za), (x, yb, zb), 0.025)
        tris += _bar((x, ya, za - 0.12), (x, yb, zb - 0.12), 0.012)
    T = np.array(tris, float)
    print("railing: %d balusters" % len(rail))
    return T, np.array(["rail_iron"] * len(T))
