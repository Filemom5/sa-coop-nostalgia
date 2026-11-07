"""Changes to the SketchUp model's walls, made on its parts (its own axes) before the doors open, given in the block's
axes (where build.py puts the house: x east, y north, z up from the block's origin; the upper floor at z 0.87):

  * the upper floor's corridor to Charlie's room wider: its west wall (the closet's) 0.62 m over, its door with it;
  * the court's window (on the wall at the back of the court, the front door's court) in the wall's middle.

The house's faces show both sides (build.py), so what is added here needs no winding."""
import numpy as np

SCALE = 1.6
EPS = 2e-3


class Axes:
    """the block's axes to the model's: plan = (block - off) / SCALE; model x, y = mid - plan (turned round)"""

    def __init__(self, off, mid):
        self.off, self.mid = np.asarray(off, float), np.asarray(mid, float)

    def p(self, b):
        plan = (np.asarray(b, float) - self.off) / SCALE
        return np.array([self.mid[0] - plan[0], self.mid[1] - plan[1], plan[2]])

    def d(self, db):
        db = np.asarray(db, float) / SCALE
        return np.array([-db[0], -db[1], db[2]])

    def box(self, lo, hi):
        a, b = self.p(lo), self.p(hi)
        return np.minimum(a, b), np.maximum(a, b)


def _inside(P, lo, hi):
    return np.all((P >= lo - EPS) & (P <= hi + EPS), axis=-1)


def stretch(parts, ax, lo, hi, d):
    """the vertices in the box (block's axes) moved by d: the faces they are of follow (a wall moved over)"""
    lo, hi = ax.box(lo, hi)
    dm = ax.d(d)
    out, n = [], 0
    for path, mat, T in parts:
        m = _inside(T, lo, hi)
        if m.any():
            T = T.copy()
            T[m] += dm
            n += int(m.sum())
        out.append((path, mat, T))
    return out, n


def move(parts, ax, lo, hi, d):
    """the faces wholly in the box moved by d (a window: its frame, glass, sill and the jambs' faces)"""
    lo, hi = ax.box(lo, hi)
    dm = ax.d(d)
    out, n = [], 0
    for path, mat, T in parts:
        m = _inside(T, lo, hi).all(1)
        if m.any():
            T = T.copy()
            T[m] += dm
            n += int(m.sum())
        out.append((path, mat, T))
    return out, n


def _clip(poly, k, v, keep_below):
    """Sutherland-Hodgman: the polygon's part with coordinate k below (or above) v"""
    out = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i + 1) % len(poly)]
        ina = (a[k] <= v) if keep_below else (a[k] >= v)
        inb = (b[k] <= v) if keep_below else (b[k] >= v)
        if ina:
            out.append(a)
        if ina != inb:
            t = (v - a[k]) / (b[k] - a[k])
            out.append(a + (b - a) * t)
    return out


def _fan(poly):
    return [[poly[0], poly[i], poly[i + 1]] for i in range(1, len(poly) - 1)] if len(poly) >= 3 else []


def cut_hole(parts, ax, y_planes, x0, x1, z0, z1):
    """a rectangular hole (x0..x1, z0..z1 in the block's axes) in the walls' faces on the planes y = each of
    y_planes (block): each face lying on one, less the rectangle"""
    a, b = ax.p((x0, y_planes[0], z0)), ax.p((x1, y_planes[0], z1))
    u0, u1, w0, w1 = min(a[0], b[0]), max(a[0], b[0]), min(a[2], b[2]), max(a[2], b[2])
    ys = [ax.p((0, y, 0))[1] for y in y_planes]
    out, n = [], 0
    for path, mat, T in parts:
        on = np.zeros(len(T), bool)
        for y in ys:
            on |= np.all(np.abs(T[..., 1] - y) < EPS, axis=1)
        if not on.any():
            out.append((path, mat, T))
            continue
        keep = [t for t in T[~on]]
        for t in T[on]:
            if t[:, 0].max() <= u0 or t[:, 0].min() >= u1 or t[:, 2].max() <= w0 or t[:, 2].min() >= w1:
                keep.append(t)
                continue
            n += 1
            poly = [p for p in t]
            pieces = [_clip(poly, 0, u0, True), _clip(poly, 0, u1, False)]
            mid = _clip(_clip(poly, 0, u0, False), 0, u1, True)
            if mid:
                pieces += [_clip(mid, 2, w0, True), _clip(mid, 2, w1, False)]
            for pc in pieces:
                for tri in _fan(pc):
                    tri = np.array(tri)
                    if np.linalg.norm(np.cross(tri[1] - tri[0], tri[2] - tri[0])) > 1e-8:
                        keep.append(tri)
        if keep:
            out.append((path, mat, np.array(keep).reshape(-1, 3, 3)))
    return out, n


def patch(parts, ax, y_planes, x0, x1, z0, z1, mat_of):
    """the wall's faces over a rectangle on each plane (where a hole was): a new part, the plane's material"""
    out = list(parts)
    for y, mat in zip(y_planes, mat_of):
        a, b, c, d = (ax.p((x0, y, z0)), ax.p((x1, y, z0)), ax.p((x1, y, z1)), ax.p((x0, y, z1)))
        out.append(("edits/patch", mat, np.array([[a, b, c], [a, c, d]])))
    return out


def plane_material(parts, ax, y, x, z):
    """the material of the wall's face on plane y (block) about (x, z)"""
    p = ax.p((x, y, z))
    for path, mat, T in parts:
        on = np.all(np.abs(T[..., 1] - p[1]) < EPS, axis=1)
        if on.any():
            return mat
    return "material"


# ---------------------------------------------------------------- the changes
CORRIDOR = dict(lo=(19.55, -13.72, 0.75), hi=(19.92, -7.48, 6.0), d=(-0.62, 0, 0))
CORRIDORS = [(20.55, -10.0, 1.0)]  # the upstairs corridor's middle: its doors open away from it (details.open_doors)
COURT_WINDOW = dict(x=(17.92, 19.52), z=(-2.36, -0.54), planes=(-3.84, -4.32), d=0.66)


def apply(parts, off, mid):
    ax = Axes(off, mid)
    parts, n1 = stretch(parts, ax, CORRIDOR["lo"], CORRIDOR["hi"], CORRIDOR["d"])
    (x0, x1), (z0, z1), planes, dx = COURT_WINDOW["x"], COURT_WINDOW["z"], COURT_WINDOW["planes"], COURT_WINDOW["d"]
    mats = [plane_material(parts, ax, y, x0 - 0.3, (z0 + z1) / 2) for y in planes]
    parts, n2 = move(parts, ax, (x0 - 0.02, min(planes) - 0.08, z0 - 0.05), (x1 + 0.02, max(planes) + 0.08, z1 + 0.05),
                     (dx, 0, 0))
    parts, n3 = cut_hole(parts, ax, planes, x0 + dx, x1 + dx, z0, z1)
    parts = patch(parts, ax, planes, x0, x0 + dx, z0, z1, mats)
    print("edits: the corridor's wall over (%d vertices), the court's window %.2f m to the middle (%d faces, %d cut)" %
          (n1, dx, n2, n3))
    return parts
