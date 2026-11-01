"""RenderWare files of GTA SA, read for study: DFF (clump, frames, geometry, materials), TXD (D3D9 textures) and COL3.

Only what the sign needs; numpy for the textures."""
import struct

import numpy as np

CHUNK_STRUCT, CHUNK_STRING, CHUNK_EXTENSION = 0x01, 0x02, 0x03
CHUNK_TEXTURE, CHUNK_MATERIAL, CHUNK_MATLIST = 0x06, 0x07, 0x08
CHUNK_FRAMELIST, CHUNK_GEOMETRY, CHUNK_CLUMP, CHUNK_ATOMIC = 0x0E, 0x0F, 0x10, 0x14
CHUNK_TEXNATIVE, CHUNK_TEXDICT, CHUNK_GEOMLIST = 0x15, 0x16, 0x1A
CHUNK_BINMESH, CHUNK_NIGHTCOLS, CHUNK_2DFX, CHUNK_FRAMENAME = 0x50E, 0x253F2F9, 0x253F2F8, 0x253F2FE


def chunks(data, off=0, end=None):
    end = len(data) if end is None else end
    while off + 12 <= end:
        t, size, ver = struct.unpack_from("<III", data, off)
        yield t, off + 12, size, ver
        off += 12 + size


def find(data, off, end, kind):
    for t, o, s, v in chunks(data, off, end):
        if t == kind:
            return o, s, v
    return None


class Material:
    def __init__(self):
        self.color = (255, 255, 255, 255)
        self.texture = None
        self.mask = None
        self.surf = (1.0, 1.0, 1.0)
        self.raw = b""  # the whole chunk, as read


class Geometry:
    def __init__(self):
        self.flags = 0
        self.verts = None      # (n, 3)
        self.normals = None
        self.prelit = None     # (n, 4) uint8
        self.night = None      # (n, 4) uint8, the extra vertex colours
        self.uvs = []          # [(n, 2)]
        self.tris = None       # (m, 4): a, b, c, material
        self.materials = []
        self.sphere = None
        self.ext = []          # (type, bytes) of its extension
        self.raw = b""


def read_material(data, off, end):
    m = Material()
    m.raw = data[off - 12:end]
    so, ss, _ = find(data, off, end, CHUNK_STRUCT)
    flags, r, g, b, a, _unused, textured = struct.unpack_from("<iBBBBii", data, so)
    m.color = (r, g, b, a)
    if ss >= 28:
        m.surf = struct.unpack_from("<fff", data, so + 16)
    if textured:
        to, ts, _ = find(data, so + ss, end, CHUNK_TEXTURE)
        strs = [(o, s) for t, o, s, v in chunks(data, to, to + ts) if t == CHUNK_STRING]
        m.texture = data[strs[0][0]:strs[0][0] + strs[0][1]].split(b"\0")[0].decode()
        m.mask = data[strs[1][0]:strs[1][0] + strs[1][1]].split(b"\0")[0].decode() if len(strs) > 1 else ""
    return m


def read_geometry(data, off, size):
    g = Geometry()
    g.raw = data[off - 12:off + size]
    end = off + size
    so, ss, ver = find(data, off, end, CHUNK_STRUCT)
    p = so
    g.flags, nt, nv, nm = struct.unpack_from("<Iiii", data, p)
    p += 16
    flags = g.flags & 0xFFFF
    ntex = (g.flags >> 16) & 0xFF or (2 if flags & 0x80 else 1 if flags & 0x04 else 0)
    if not g.flags & 0x01000000:  # not native
        if flags & 0x08:
            g.prelit = np.frombuffer(data, np.uint8, nv * 4, p).reshape(nv, 4).copy()
            p += nv * 4
        for _ in range(ntex):
            g.uvs.append(np.frombuffer(data, np.float32, nv * 2, p).reshape(nv, 2).copy())
            p += nv * 8
        t = np.frombuffer(data, np.uint16, nt * 4, p).reshape(nt, 4)
        g.tris = np.stack([t[:, 1], t[:, 0], t[:, 3], t[:, 2]], 1).astype(np.int32)  # v1, v2, v3, material
        p += nt * 8
    for _ in range(nm):
        g.sphere = struct.unpack_from("<ffff", data, p)
        hv, hn = struct.unpack_from("<ii", data, p + 16)
        p += 24
        if hv:
            g.verts = np.frombuffer(data, np.float32, nv * 3, p).reshape(nv, 3).copy()
            p += nv * 12
        if hn:
            g.normals = np.frombuffer(data, np.float32, nv * 3, p).reshape(nv, 3).copy()
            p += nv * 12
    mo, ms, _ = find(data, so + ss, end, CHUNK_MATLIST)
    lo, ls, _ = find(data, mo, mo + ms, CHUNK_STRUCT)
    count = struct.unpack_from("<i", data, lo)[0]
    refs = struct.unpack_from("<%di" % count, data, lo + 4)
    mats = [read_material(data, o, o + s) for t, o, s, v in chunks(data, lo + ls, mo + ms) if t == CHUNK_MATERIAL]
    it = iter(mats)
    g.materials = [next(it) if r < 0 else None for r in refs]
    g.materials = [m if m is not None else g.materials[r] for m, r in zip(g.materials, refs)]
    eo, es, _ = find(data, mo + ms, end, CHUNK_EXTENSION)
    for t, o, s, v in chunks(data, eo, eo + es):
        g.ext.append((t, data[o:o + s]))
        if t == CHUNK_NIGHTCOLS:
            has = struct.unpack_from("<I", data, o)[0]
            if has:
                g.night = np.frombuffer(data, np.uint8, nv * 4, o + 4).reshape(nv, 4).copy()
    return g


class Clump:
    def __init__(self):
        self.frames = []    # (rot 3x3 rows, pos, parent, name)
        self.geoms = []
        self.atomics = []   # (frame, geometry, flags)


def read_dff(data):
    c = Clump()
    co, cs, ver = find(data, 0, len(data), CHUNK_CLUMP)
    end = co + cs
    fo, fs, _ = find(data, co, end, CHUNK_FRAMELIST)
    so, ss, _ = find(data, fo, fo + fs, CHUNK_STRUCT)
    n = struct.unpack_from("<i", data, so)[0]
    for i in range(n):
        v = struct.unpack_from("<12fiI", data, so + 4 + i * 56)
        c.frames.append([np.array(v[0:9]).reshape(3, 3), np.array(v[9:12]), v[12], ""])
    exts = [(o, s) for t, o, s, v in chunks(data, so + ss, fo + fs) if t == CHUNK_EXTENSION]
    for i, (o, s) in enumerate(exts):
        for t, o2, s2, v in chunks(data, o, o + s):
            if t == CHUNK_FRAMENAME:
                c.frames[i][3] = data[o2:o2 + s2].decode(errors="replace")
    go, gs, _ = find(data, co, end, CHUNK_GEOMLIST)
    for t, o, s, v in chunks(data, go, go + gs):
        if t == CHUNK_GEOMETRY:
            c.geoms.append(read_geometry(data, o, s))
    for t, o, s, v in chunks(data, co, end):
        if t == CHUNK_ATOMIC:
            ao, _, _ = find(data, o, o + s, CHUNK_STRUCT)
            c.atomics.append(struct.unpack_from("<iii", data, ao))
    return c


def frame_matrix(clump, i):
    """the frame's matrix to the clump's root: x' = R x + p (rows of R are its right, up, at)"""
    rot, pos, parent, _ = clump.frames[i]
    m = np.eye(4)
    m[:3, :3] = rot.T
    m[:3, 3] = pos
    return frame_matrix(clump, parent) @ m if parent >= 0 else m


# ---------------------------------------------------------------- textures
def _565(c):
    r = ((c >> 11) & 31) * 255 // 31
    g = ((c >> 5) & 63) * 255 // 63
    b = (c & 31) * 255 // 31
    return np.stack([r, g, b], -1).astype(np.int32)


def _colour_blocks(blocks, dxt1):
    c0 = blocks[:, 0].astype(np.int32) | blocks[:, 1].astype(np.int32) << 8
    c1 = blocks[:, 2].astype(np.int32) | blocks[:, 3].astype(np.int32) << 8
    p0, p1 = _565(c0), _565(c1)
    four = (c0 > c1) | (not dxt1)
    p2 = np.where(four[:, None], (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four[:, None], (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], 1)  # (n, 4, 3)
    alpha3 = np.where(four, 255, 0)
    bits = blocks[:, 4:8].astype(np.uint32)
    idx = bits[:, 0] | bits[:, 1] << 8 | bits[:, 2] << 16 | bits[:, 3] << 24
    sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1)  # (n, 16)
    rgb = np.take_along_axis(pal, sel[:, :, None].astype(np.int64).repeat(3, 2), 1)
    a = np.where((sel == 3) & ~four[:, None], 0, 255)
    return rgb, a


def decode_dxt(buf, w, h, kind):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    bsize = 8 if kind == 1 else 16
    blocks = np.frombuffer(buf, np.uint8, bw * bh * bsize).reshape(-1, bsize)
    if kind == 1:
        rgb, a = _colour_blocks(blocks, True)
    else:
        rgb, _ = _colour_blocks(blocks[:, 8:], False)
        if kind == 3:
            ab = blocks[:, :8]
            nib = np.stack([(ab[:, k // 2] >> (4 * (k % 2))) & 15 for k in range(16)], 1)
            a = nib.astype(np.int32) * 17
        else:
            a0, a1 = blocks[:, 0].astype(np.int32), blocks[:, 1].astype(np.int32)
            bits = np.zeros(len(blocks), np.uint64)
            for k in range(6):
                bits |= blocks[:, 2 + k].astype(np.uint64) << np.uint64(8 * k)
            sel = np.stack([(bits >> np.uint64(3 * k)) & np.uint64(7) for k in range(16)], 1).astype(np.int32)
            eight = a0 > a1
            pal = np.zeros((len(blocks), 8), np.int32)
            pal[:, 0], pal[:, 1] = a0, a1
            for k in range(2, 8):
                pal[:, k] = np.where(eight, ((8 - k) * a0 + (k - 1) * a1) // 7, 0)
            for k in range(2, 6):
                pal[:, k] = np.where(eight, pal[:, k], ((6 - k) * a0 + (k - 1) * a1) // 5)
            pal[:, 6] = np.where(eight, pal[:, 6], 0)
            pal[:, 7] = np.where(eight, pal[:, 7], 255)
            a = np.take_along_axis(pal, sel, 1)
    img = np.concatenate([rgb, a[:, :, None]], 2).reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4)
    return img.reshape(bh * 4, bw * 4, 4)[:h, :w].astype(np.uint8)


class Texture:
    def __init__(self):
        self.name = ""
        self.mask = ""
        self.format = 0
        self.fourcc = 0
        self.w = self.h = 0
        self.alpha = False
        self.rgba = None
        self.raw = b""


def read_txd(data):
    out = {}
    to, ts, _ = find(data, 0, len(data), CHUNK_TEXDICT)
    for t, o, s, v in chunks(data, to, to + ts):
        if t != CHUNK_TEXNATIVE:
            continue
        so, ss, _ = find(data, o, o + s, CHUNK_STRUCT)
        tx = Texture()
        tx.raw = data[o - 12:o + s]
        platform, filt = struct.unpack_from("<II", data, so)
        tx.name = data[so + 8:so + 40].split(b"\0")[0].decode()
        tx.mask = data[so + 40:so + 72].split(b"\0")[0].decode()
        tx.format, tx.fourcc, tx.w, tx.h, depth, levels, rtype, flags = struct.unpack_from("<IIHHBBBB", data, so + 72)
        tx.alpha = bool(flags & 1) if platform == 9 else bool(tx.fourcc)
        p = so + 88
        if tx.format & 0x2000:  # palette 8
            pal = np.frombuffer(data, np.uint8, 1024, p).reshape(256, 4)
            p += 1024
        size = struct.unpack_from("<I", data, p)[0]
        buf = data[p + 4:p + 4 + size]
        fcc = struct.pack("<I", tx.fourcc)
        if fcc == b"DXT1":
            tx.rgba = decode_dxt(buf, tx.w, tx.h, 1)
        elif fcc == b"DXT3":
            tx.rgba = decode_dxt(buf, tx.w, tx.h, 3)
        elif fcc == b"DXT5":
            tx.rgba = decode_dxt(buf, tx.w, tx.h, 5)
        elif tx.format & 0x2000:
            idx = np.frombuffer(buf, np.uint8, tx.w * tx.h).reshape(tx.h, tx.w)
            tx.rgba = pal[idx]
        elif depth == 32:
            bgra = np.frombuffer(buf, np.uint8, tx.w * tx.h * 4).reshape(tx.h, tx.w, 4)
            tx.rgba = bgra[:, :, [2, 1, 0, 3]].copy()
            if not tx.alpha:
                tx.rgba[:, :, 3] = 255
        out[tx.name.lower()] = tx
    return out


# ---------------------------------------------------------------- collision (COL3)
class ColModel:
    def __init__(self):
        self.name = ""
        self.id = 0
        self.bounds = None
        self.spheres = []   # (centre, radius, surface)
        self.boxes = []     # (min, max, surface)
        self.verts = None
        self.faces = []     # (a, b, c, material, light)
        self.shadow_verts = None
        self.shadow_faces = []
        self.raw = b""


def read_col(data):
    out = []
    off = 0
    while off + 8 <= len(data):
        fcc, size = data[off:off + 4], struct.unpack_from("<I", data, off + 4)[0]
        if fcc not in (b"COL3", b"COL2", b"COLL", b"COL4"):
            break
        m = ColModel()
        m.raw = data[off:off + 8 + size]
        m.name = data[off + 8:off + 30].split(b"\0")[0].decode()
        m.id = struct.unpack_from("<H", data, off + 30)[0]
        b = off + 32
        m.bounds = struct.unpack_from("<10f", data, b)
        ns, nb, nf, nl, _, flags = struct.unpack_from("<HHHBBI", data, b + 40)
        osph, obox, olin, overt, oface, oplane = struct.unpack_from("<6I", data, b + 52)
        base = off + 4  # the offsets count from after the fourcc
        for i in range(ns):
            v = struct.unpack_from("<4f4B", data, base + osph + i * 20)
            m.spheres.append((v[0:3], v[3], v[4:8]))
        for i in range(nb):
            v = struct.unpack_from("<6f4B", data, base + obox + i * 28)
            m.boxes.append((v[0:3], v[3:6], v[6:10]))
        faces = [struct.unpack_from("<3HBB", data, base + oface + i * 8) for i in range(nf)]
        m.faces = faces
        nv = max([max(f[0:3]) for f in faces], default=-1) + 1
        if nv:
            m.verts = np.frombuffer(data, np.int16, nv * 3, base + overt).reshape(nv, 3) / 128.0
        if fcc in (b"COL3", b"COL4"):
            nsf, osv, osf = struct.unpack_from("<3I", data, b + 76)
            sf = [struct.unpack_from("<3HBB", data, base + osf + i * 8) for i in range(nsf)]
            m.shadow_faces = sf
            nsv = max([max(f[0:3]) for f in sf], default=-1) + 1
            if nsv:
                m.shadow_verts = np.frombuffer(data, np.int16, nsv * 3, base + osv).reshape(nsv, 3) / 128.0
        out.append(m)
        off += 8 + size
    return out


# ---------------------------------------------------------------- binary IPL (in the IMG)
def read_bnry_ipl(data):
    if data[:4] != b"bnry":
        return []
    ninst = struct.unpack_from("<i", data, 4)[0]
    inst_off = struct.unpack_from("<i", data, 28)[0]
    out = []
    for i in range(ninst):
        v = struct.unpack_from("<7f3i", data, inst_off + i * 40)
        out.append({"pos": v[0:3], "rot": v[3:7], "id": v[7], "interior": v[8], "lod": v[9]})
    return out
