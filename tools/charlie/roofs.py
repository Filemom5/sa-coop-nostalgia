"""The house's roofs, as the reference images have them: hipped, of terracotta tiles, over every flat roof of the
SketchUp model (it has flat ones but over the living room), but the two terraces of the upper floor, which keep their
floor and get a stucco wall with a terracotta cap round them. In the block's axes (build.py adds them to the model).

Each flat roof is a region of cells at one height with nothing above it; its hipped roof is, at each point, as high
over its eaves as the point is far from the region's edge (the farthest one can go along x or y: for a region of
straight walls that is the roof whose slopes all rise alike). It is drawn as the hipped roofs of the region's
largest rectangles, one over the other (the highest is seen). The eaves stand out from the walls where the air is
open, not against a higher part of the house."""
import numpy as np
from scipy import ndimage

import lot

CELL = 0.5
SLOPE = np.tan(np.radians(21))
OVERHANG = 0.5


def _cover(tris, x0, y0, w, h, pick):
    """per cell, the highest value pick(face) of the faces over the cell's middle (-inf: none)"""
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    out = np.full((h, w), -np.inf)
    for t in tris:
        v = pick(t)
        if v is None:
            continue
        (ax, ay), (bx, by), (qx, qy) = t[:, :2]
        d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay)
        i0, i1 = int(max(0, (min(ay, by, qy) - y0) / CELL - 1)), int(min(h, (max(ay, by, qy) - y0) / CELL + 2))
        j0, j1 = int(max(0, (min(ax, bx, qx) - x0) / CELL - 1)), int(min(w, (max(ax, bx, qx) - x0) / CELL + 2))
        if i0 >= i1 or j0 >= j1:
            continue
        X, Y = cx[i0:i1, j0:j1], cy[i0:i1, j0:j1]
        if abs(d) < 1e-9:  # a wall: the cells along it
            a, b = t[:, :2].min(0), t[:, :2].max(0)
            m = (X > a[0] - CELL / 2) & (X < b[0] + CELL / 2) & (Y > a[1] - CELL / 2) & (Y < b[1] + CELL / 2)
        else:
            u = ((X - ax) * (qy - ay) - (qx - ax) * (Y - ay)) / d
            vv = ((bx - ax) * (Y - ay) - (X - ax) * (by - ay)) / d
            m = (u >= -0.02) & (vv >= -0.02) & (u + vv <= 1.02)
        sub = out[i0:i1, j0:j1]
        sub[m] = np.maximum(sub[m], v)
    return out


def _rectangles(region):
    """the region's maximal rectangles (cells: i0, i1, j0, j1, end exclusive), at least 2 cells each way"""
    d = ndimage.distance_transform_cdt(np.pad(region, 1), metric="chessboard")[1:-1, 1:-1]
    h, w = region.shape
    seen = set()
    for i, j in zip(*np.where(region)):
        r = d[i, j] - 1
        i0, i1, j0, j1 = max(i - r, 0), min(i + r + 1, h), max(j - r, 0), min(j + r + 1, w)
        grown = True
        while grown:  # x first, then y, as far as the region goes
            grown = False
            if j0 > 0 and region[i0:i1, j0 - 1].all():
                j0, grown = j0 - 1, True
            if j1 < w and region[i0:i1, j1].all():
                j1, grown = j1 + 1, True
        grown = True
        while grown:
            grown = False
            if i0 > 0 and region[i0 - 1, j0:j1].all():
                i0, grown = i0 - 1, True
            if i1 < h and region[i1, j0:j1].all():
                i1, grown = i1 + 1, True
        if i1 - i0 >= 2 and j1 - j0 >= 2:
            seen.add((i0, i1, j0, j1))
    rects = sorted(seen)
    return [r for r in rects if not any(o != r and o[0] <= r[0] and o[1] >= r[1] and o[2] <= r[2] and o[3] >= r[3]
                                        for o in rects)]


def _hip(m, x0, y0, x1, y1, z, eaves):
    """a hipped roof over the rectangle from height z; eaves: (west, south, east, north) stand out"""
    W, D = x1 - x0, y1 - y0
    if W >= D:
        r, yc = D / 2, (y0 + y1) / 2
        zr = z + SLOPE * r
        a, b = (x0 + r, yc, zr), (x1 - r, yc, zr)
        m.quad((x0, y0, z), (x1, y0, z), b, a, "roof_tile")
        m.quad((x1, y1, z), (x0, y1, z), a, b, "roof_tile")
        m.tris += [[(x0, y1, z), (x0, y0, z), a], [(x1, y0, z), (x1, y1, z), b]]
    else:
        r, xc = W / 2, (x0 + x1) / 2
        zr = z + SLOPE * r
        a, b = (xc, y0 + r, zr), (xc, y1 - r, zr)
        m.quad((x1, y0, z), (x1, y1, z), b, a, "roof_tile")
        m.quad((x0, y1, z), (x0, y0, z), a, b, "roof_tile")
        m.tris += [[(x0, y0, z), (x1, y0, z), a], [(x1, y1, z), (x0, y1, z), b]]
    m.mats += ["roof_tile", "roof_tile"]
    o, dz = OVERHANG, OVERHANG * SLOPE
    w_, s_, e_, n_ = (o if e else 0.0 for e in eaves)
    if eaves[1]:
        m.quad((x0 - w_, y0 - o, z - dz), (x1 + e_, y0 - o, z - dz), (x1, y0, z), (x0, y0, z), "roof_tile")
    if eaves[3]:
        m.quad((x1 + e_, y1 + o, z - dz), (x0 - w_, y1 + o, z - dz), (x0, y1, z), (x1, y1, z), "roof_tile")
    if eaves[0]:
        m.quad((x0 - o, y1 + n_, z - dz), (x0 - o, y0 - s_, z - dz), (x0, y0, z), (x0, y1, z), "roof_tile")
    if eaves[2]:
        m.quad((x1 + o, y0 - s_, z - dz), (x1 + o, y1 + n_, z - dz), (x1, y1, z), (x1, y0, z), "roof_tile")


def build(house_tris, floor, levels, terraces):
    """levels: [(lo, hi)] the flat roofs' heights over the floor (block's axes); terraces: [(x0, y0, x1, y1)] the
    upper floor's terraces (not roofed). Returns (tris, materials)"""
    P = house_tris.reshape(-1, 3)
    x0, y0 = np.floor(P[:, 0].min()) - 1, np.floor(P[:, 1].min()) - 1
    w, h = int((P[:, 0].max() + 1 - x0) / CELL) + 1, int((P[:, 1].max() + 1 - y0) / CELL) + 1
    n = np.cross(house_tris[:, 1] - house_tris[:, 0], house_tris[:, 2] - house_tris[:, 0])
    ln = np.linalg.norm(n, axis=1)
    up = n[:, 2] > 0.95 * np.maximum(ln, 1e-9)
    tops = _cover(house_tris[up], x0, y0, w, h, lambda t: t[:, 2].mean())
    hmax = _cover(house_tris, x0, y0, w, h, lambda t: t[:, 2].max())
    cx, cy = np.meshgrid(x0 + (np.arange(w) + 0.5) * CELL, y0 + (np.arange(h) + 0.5) * CELL)
    in_terrace = np.zeros((h, w), bool)
    for a, b, c, d in terraces:
        in_terrace |= (cx > a) & (cx < c) & (cy > b) & (cy < d)
    m = lot.Mesh()
    n_roofs = 0
    for lo, hi in levels:
        flat = (tops > floor + lo) & (tops < floor + hi) & (hmax < floor + hi + 0.15)
        lab, k = ndimage.label(flat & ~in_terrace)
        for c in range(1, k + 1):
            region = lab == c
            if region.sum() * CELL * CELL < 4.0:
                continue
            z = float(np.median(tops[region])) + 0.02
            for i0, i1, j0, j1 in _rectangles(region):
                ax, bx, ay, by = x0 + j0 * CELL, x0 + j1 * CELL, y0 + i0 * CELL, y0 + i1 * CELL

                def open_(ii, jj):  # the cells just out of a side: not the region, nothing higher
                    ii, jj = np.clip(ii, 0, h - 1), np.clip(jj, 0, w - 1)
                    out = ~region[ii, jj] & (hmax[ii, jj] < z + 0.3)
                    return out.mean() > 0.7

                eaves = (open_(np.arange(i0, i1), np.full(i1 - i0, j0 - 1)), open_(np.full(j1 - j0, i0 - 1), np.arange(j0, j1)),
                         open_(np.arange(i0, i1), np.full(i1 - i0, j1)), open_(np.full(j1 - j0, i1), np.arange(j0, j1)))
                _hip(m, ax, ay, bx, by, z, eaves)
                n_roofs += 1
    # the terraces: a wall round their open edges
    walls = 0
    for lo, hi in levels[:1]:
        flat = (tops > floor + lo) & (tops < floor + hi) & (hmax < floor + hi + 0.15) & in_terrace
        z = float(np.median(tops[flat])) if flat.any() else floor + lo
        for i in range(h + 1):
            below = flat[i - 1] if i > 0 else np.zeros(w, bool)
            above = flat[i] if i < h else np.zeros(w, bool)
            for inside, outside, nrm in ((below, above, (0, 1)), (above, below, (0, -1))):
                ii = min(i, h - 1) if nrm == (0, 1) else max(i - 1, 0)
                for a_, b_ in lot.runs(inside & ~outside & (hmax[ii] < z + 0.3)):
                    p, q = (x0 + a_ * CELL, y0 + i * CELL), (x0 + b_ * CELL, y0 + i * CELL)
                    if nrm == (0, 1):
                        p, q = q, p
                    lot.parapet(m, p, q, nrm, z, 1.0)
                    walls += 1
        for j in range(w + 1):
            left = flat[:, j - 1] if j > 0 else np.zeros(h, bool)
            right = flat[:, j] if j < w else np.zeros(h, bool)
            for inside, outside, nrm in ((left, right, (1, 0)), (right, left, (-1, 0))):
                jj = min(j, w - 1) if nrm == (1, 0) else max(j - 1, 0)
                for a_, b_ in lot.runs(inside & ~outside & (hmax[:, jj] < z + 0.3)):
                    p, q = (x0 + j * CELL, y0 + a_ * CELL), (x0 + j * CELL, y0 + b_ * CELL)
                    if nrm == (-1, 0):
                        p, q = q, p
                    lot.parapet(m, p, q, nrm, z, 1.0)
                    walls += 1
    T, M = m.arrays()
    print("roofs: %d hipped (of rectangles), %d walls round the terraces, %d faces" % (n_roofs, walls, len(T)))
    return T, M
