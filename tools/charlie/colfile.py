"""COL3 files of the game: one model's collision read out of a .col (read_col, parse) and written (write)."""
import struct, numpy as np
def read_col(path, want):
    b = open(path, 'rb').read(); p = 0
    while p + 8 <= len(b):
        four = b[p:p + 4]
        size = struct.unpack_from('<I', b, p + 4)[0]
        name = b[p + 8:p + 30].split(b'\0')[0].decode()
        if name.lower() == want.lower():
            return parse(b[p:p + 8 + size])
        p += 8 + size
def parse(e):
    assert e[:4] == b'COL3', e[:4]
    q = 4  # offsets from here
    bounds = struct.unpack_from('<10f', e, 32)
    ns, nb, nf, nl, pad, flags = struct.unpack_from('<HHHBBI', e, 72)
    offs = struct.unpack_from('<6I', e, 84)
    sh = struct.unpack_from('<3I', e, 108)
    S = [struct.unpack_from('<4f4B', e, q + offs[0] + 20 * i) for i in range(ns)]
    B = [struct.unpack_from('<6f4B', e, q + offs[1] + 28 * i) for i in range(nb)]
    F = [struct.unpack_from('<3H2B', e, q + offs[4] + 8 * i) for i in range(nf)]
    nv = (max(max(f[:3]) for f in F) + 1) if F else 0
    V = np.frombuffer(e, '<i2', nv * 3, q + offs[3]).reshape(-1, 3).astype(float) / 128
    return dict(bounds=bounds, spheres=S, boxes=B, faces=F, verts=V, flags=flags, shadow=sh, name=e[8:30].split(b'\0')[0].decode())
def write(name, model_id, spheres, boxes, verts, faces, flags=2):
    verts = np.asarray(verts, float)
    pts = [verts] + [np.array([[s[0] - s[3], s[1] - s[3], s[2] - s[3]], [s[0] + s[3], s[1] + s[3], s[2] + s[3]]]) for s in spheres] + \
          [np.array([b[:3], b[3:6]]) for b in boxes]
    P = np.concatenate([p for p in pts if len(p)])
    vmin, vmax = P.min(0), P.max(0); ctr = (vmin + vmax) / 2; rad = float(np.linalg.norm(P - ctr, axis=1).max())
    q = np.round(verts * 128).astype('<i2')
    head = 28 + 40 + 12 + 24 + 12
    so = head; bo = so + 20 * len(spheres); vo = bo + 28 * len(boxes)
    vsize = len(q) * 6; vsize += (-vsize) % 4
    fo = vo + vsize
    body = struct.pack('<10f', *vmin, *vmax, *ctr, rad)
    fl = (flags | 2) if len(faces) else (flags & ~2)
    body += struct.pack('<HHHBBI', len(spheres), len(boxes), len(faces), 0, 0, fl)
    body += struct.pack('<6I', so if spheres else 0, bo if boxes else 0, 0, vo, fo, 0) + struct.pack('<3I', 0, 0, 0)
    data = b''.join(struct.pack('<4f4B', *s) for s in spheres) + b''.join(struct.pack('<6f4B', *b) for b in boxes)
    data += q.tobytes().ljust(vsize, b'\0')
    data += b''.join(struct.pack('<3H2B', *f) for f in faces)
    payload = name.encode().ljust(22, b'\0') + struct.pack('<H', model_id) + body + data
    return b'COL3' + struct.pack('<I', len(payload)) + payload
