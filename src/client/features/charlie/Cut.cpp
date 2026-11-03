#include "features/charlie/Cut.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <utility>

namespace coop::charlie {

namespace {

uint32_t Rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t Rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
float RdF(const uint8_t* p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

struct Out {
    std::vector<uint8_t> b;
    void Raw(const void* p, size_t n) { b.insert(b.end(), (const uint8_t*)p, (const uint8_t*)p + n); }
    void U32(uint32_t v) { Raw(&v, 4); }
    void U16(uint16_t v) { Raw(&v, 2); }
    void U8(uint8_t v) { b.push_back(v); }
    void F(float v) { Raw(&v, 4); }
    size_t Open(uint32_t type, uint32_t ver) {  // a chunk; Close puts its size
        U32(type), U32(0), U32(ver);
        return b.size();
    }
    void Close(size_t at) {
        uint32_t size = (uint32_t)(b.size() - at);
        memcpy(&b[at - 8], &size, 4);
    }
};

struct Chunk {
    uint32_t type = 0, size = 0, ver = 0;
    size_t at = 0;  // its header
    size_t data() const { return at + 12; }
    size_t end() const { return at + 12 + size; }
};

// the chunks in [p, end)
std::vector<Chunk> Children(const uint8_t* b, size_t p, size_t end) {
    std::vector<Chunk> out;
    while (p + 12 <= end) {
        Chunk c{Rd32(b + p), Rd32(b + p + 4), Rd32(b + p + 8), p};
        if (c.end() > end) return {};
        out.push_back(c);
        p = c.end();
    }
    return out;
}

// cutting a triangle (by its corners' distances d, >= 0 stays) into the polygon that stays; new corners from Mid
template <typename MidFn>
int Clip(const uint32_t v[3], const float d[3], uint32_t poly[4], MidFn mid) {
    int n = 0;
    for (int k = 0; k < 3; ++k) {
        int j = (k + 1) % 3;
        if (d[k] >= 0) poly[n++] = v[k];
        if ((d[k] >= 0) != (d[j] >= 0) && d[k] != 0 && d[j] != 0 && n < 4) poly[n++] = mid(v[k], v[j], d[k] / (d[k] - d[j]));
    }
    return n;
}

}  // namespace

std::vector<uint8_t> CutModel(const uint8_t* b, size_t size, float cutX) {
    std::vector<Chunk> top = Children(b, 0, size);
    if (top.empty() || top[0].type != 0x10) return {};
    const Chunk clump = top[0];
    std::vector<Chunk> parts = Children(b, clump.data(), clump.end());
    auto gl = std::find_if(parts.begin(), parts.end(), [](const Chunk& c) { return c.type == 0x1A; });
    if (gl == parts.end()) return {};
    std::vector<Chunk> gparts = Children(b, gl->data(), gl->end());
    if (gparts.size() != 2 || gparts[0].type != 0x01 || Rd32(b + gparts[0].data()) != 1 || gparts[1].type != 0x0F) return {};
    const Chunk geo = gparts[1];
    std::vector<Chunk> geoParts = Children(b, geo.data(), geo.end());
    if (geoParts.empty() || geoParts[0].type != 0x01) return {};
    // the geometry's struct
    const uint8_t* s = b + geoParts[0].data();
    size_t ssize = geoParts[0].size;
    if (ssize < 16) return {};
    uint32_t flags = Rd32(s);
    int nt = (int)Rd32(s + 4), nv = (int)Rd32(s + 8), nm = (int)Rd32(s + 12);
    if (nm != 1 || nt < 0 || nv <= 0) return {};
    int ntex = (flags >> 16) & 0xFF;
    if (!ntex) ntex = (flags & 0x80) ? 2 : ((flags & 0x04) ? 1 : 0);
    bool prelit = flags & 0x08;
    size_t q = 16;
    size_t need = q + (prelit ? nv * 4u : 0) + size_t(ntex) * nv * 8 + size_t(nt) * 8 + 24;
    if (need > ssize) return {};
    const uint8_t* pre = prelit ? s + q : nullptr;
    q += prelit ? nv * 4u : 0;
    const uint8_t* uv = s + q;
    q += size_t(ntex) * nv * 8;
    const uint8_t* tri = s + q;
    q += size_t(nt) * 8;
    uint32_t hasVerts = Rd32(s + q + 16), hasNormals = Rd32(s + q + 20);
    q += 24;
    if (!hasVerts || q + nv * 12u * (hasNormals ? 2 : 1) > ssize) return {};
    const uint8_t* pos = s + q;
    const uint8_t* nrm = hasNormals ? s + q + nv * 12u : nullptr;
    // the night colours, if there
    const uint8_t* night = nullptr;
    std::vector<Chunk> ext;
    const Chunk* extChunk = nullptr;
    for (const Chunk& c : geoParts)
        if (c.type == 0x03) extChunk = &c;
    if (extChunk) ext = Children(b, extChunk->data(), extChunk->end());
    for (const Chunk& c : ext)
        if (c.type == 0x253F2F9 && c.size >= 4 + nv * 4u && Rd32(b + c.data())) night = b + c.data() + 4;
    // every vertex's values in a row: position, prelit, uvs, normal, night
    const int K = 3 + (prelit ? 4 : 0) + 2 * ntex + (nrm ? 3 : 0) + (night ? 4 : 0);
    std::vector<float> v(size_t(nv) * K);
    for (int i = 0; i < nv; ++i) {
        float* o = &v[size_t(i) * K];
        int k = 0;
        for (int c = 0; c < 3; ++c) o[k++] = RdF(pos + i * 12 + c * 4);
        if (pre)
            for (int c = 0; c < 4; ++c) o[k++] = pre[i * 4 + c];
        for (int t = 0; t < ntex; ++t)
            for (int c = 0; c < 2; ++c) o[k++] = RdF(uv + (size_t(t) * nv + i) * 8 + c * 4);
        if (nrm)
            for (int c = 0; c < 3; ++c) o[k++] = RdF(nrm + i * 12 + c * 4);
        if (night)
            for (int c = 0; c < 4; ++c) o[k++] = night[i * 4 + c];
    }
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> mids;
    auto mid = [&](uint32_t i, uint32_t j, float t) {
        auto key = std::make_pair(std::min(i, j), std::max(i, j));
        auto it = mids.find(key);
        if (it != mids.end()) return it->second;
        size_t n = v.size() / K;
        v.resize(v.size() + K);
        for (int k = 0; k < K; ++k) v[n * K + k] = v[size_t(i) * K + k] + (v[size_t(j) * K + k] - v[size_t(i) * K + k]) * t;
        mids[key] = (uint32_t)n;
        return (uint32_t)n;
    };
    std::vector<uint32_t> faces;  // a, b, c, material
    for (int t = 0; t < nt; ++t) {
        const uint8_t* f = tri + t * 8;
        uint32_t c[3] = {Rd16(f + 2), Rd16(f), Rd16(f + 6)};  // v1, v2, v3 (the file: v2, v1, material, v3)
        uint32_t mat = Rd16(f + 4);
        if (c[0] >= (uint32_t)nv || c[1] >= (uint32_t)nv || c[2] >= (uint32_t)nv) return {};
        float d[3];
        for (int k = 0; k < 3; ++k) d[k] = v[size_t(c[k]) * K] - cutX;
        uint32_t poly[4];
        int n = Clip(c, d, poly, mid);
        for (int k = 1; k + 1 < n; ++k) faces.insert(faces.end(), {poly[0], poly[k], poly[k + 1], mat});
    }
    // the vertices still used
    size_t total = v.size() / K;
    std::vector<int64_t> remap(total, -1);
    std::vector<uint32_t> keep;
    for (size_t f = 0; f < faces.size(); f += 4)
        for (int k = 0; k < 3; ++k)
            if (remap[faces[f + k]] < 0) remap[faces[f + k]] = (int64_t)keep.size(), keep.push_back(faces[f + k]);
    if (keep.empty() || keep.size() >= 65536) return {};
    for (size_t f = 0; f < faces.size(); f += 4)
        for (int k = 0; k < 3; ++k) faces[f + k] = (uint32_t)remap[faces[f + k]];
    const int nv2 = (int)keep.size(), nt2 = (int)(faces.size() / 4);
    auto val = [&](int i, int k) { return v[size_t(keep[i]) * K + k]; };
    auto byte = [](float f) { return (uint8_t)std::clamp((int)std::lround(f), 0, 255); };

    Out o;
    size_t cl = o.Open(0x10, clump.ver);
    for (const Chunk& c : parts) {
        if (c.type != 0x1A) {
            o.Raw(b + c.at, 12 + c.size);
            continue;
        }
        size_t g = o.Open(0x1A, c.ver);
        o.Raw(b + gparts[0].at, 12 + gparts[0].size);
        size_t gc = o.Open(0x0F, geo.ver);
        size_t st = o.Open(0x01, geoParts[0].ver);
        o.U32(flags & ~1u);  // lists, not strips
        o.U32(nt2), o.U32(nv2), o.U32(1);
        int k = 3;
        if (pre) {
            for (int i = 0; i < nv2; ++i)
                for (int c2 = 0; c2 < 4; ++c2) o.U8(byte(val(i, k + c2)));
            k += 4;
        }
        for (int t = 0; t < ntex; ++t, k += 2)
            for (int i = 0; i < nv2; ++i) o.F(val(i, k)), o.F(val(i, k + 1));
        for (size_t f = 0; f < faces.size(); f += 4)
            o.U16((uint16_t)faces[f + 1]), o.U16((uint16_t)faces[f]), o.U16((uint16_t)faces[f + 3]), o.U16((uint16_t)faces[f + 2]);
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (int i = 0; i < nv2; ++i)
            for (int c2 = 0; c2 < 3; ++c2) lo[c2] = std::min(lo[c2], val(i, c2)), hi[c2] = std::max(hi[c2], val(i, c2));
        float ctr[3] = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2}, rad = 0;
        for (int i = 0; i < nv2; ++i)
            rad = std::max(rad, std::hypot(val(i, 0) - ctr[0], val(i, 1) - ctr[1], val(i, 2) - ctr[2]));
        o.F(ctr[0]), o.F(ctr[1]), o.F(ctr[2]), o.F(rad), o.U32(1), o.U32(nrm ? 1 : 0);
        for (int i = 0; i < nv2; ++i) o.F(val(i, 0)), o.F(val(i, 1)), o.F(val(i, 2));
        if (nrm) {
            for (int i = 0; i < nv2; ++i) {
                float x = val(i, k), y = val(i, k + 1), z = val(i, k + 2), l = std::max(std::hypot(x, y, z), 1e-6f);
                o.F(x / l), o.F(y / l), o.F(z / l);
            }
            k += 3;
        }
        o.Close(st);
        for (size_t p = 1; p < geoParts.size(); ++p)
            if (geoParts[p].type != 0x03) o.Raw(b + geoParts[p].at, 12 + geoParts[p].size);
        size_t ex = o.Open(0x03, extChunk ? extChunk->ver : geo.ver);
        for (const Chunk& e : ext) {
            if (e.type == 0x50E) {  // the meshes: anew, lists, by material
                size_t m = o.Open(0x50E, e.ver);
                std::map<uint32_t, std::vector<uint32_t>> byMat;
                for (size_t f = 0; f < faces.size(); f += 4)
                    byMat[faces[f + 3]].insert(byMat[faces[f + 3]].end(), {faces[f], faces[f + 1], faces[f + 2]});
                o.U32(0), o.U32((uint32_t)byMat.size()), o.U32((uint32_t)faces.size() / 4 * 3);
                for (auto& [mat, idx] : byMat) {
                    o.U32((uint32_t)idx.size()), o.U32(mat);
                    for (uint32_t i : idx) o.U32(i);
                }
                o.Close(m);
            } else if (e.type == 0x253F2F9 && night) {
                size_t m = o.Open(e.type, e.ver);
                o.U32(1);
                for (int i = 0; i < nv2; ++i)
                    for (int c2 = 0; c2 < 4; ++c2) o.U8(byte(val(i, k + c2)));
                o.Close(m);
            } else if (e.type == 0x253F2F8 && e.size >= 4) {  // the 2d effects: those east of the cut
                const uint8_t* d = b + e.data();
                uint32_t n = Rd32(d);
                size_t p = 4;
                std::vector<std::pair<size_t, size_t>> kept;
                for (uint32_t i = 0; i < n && p + 20 <= e.size; ++i) {
                    uint32_t len = 20 + Rd32(d + p + 16);
                    if (p + len > e.size) break;
                    if (RdF(d + p) >= cutX) kept.push_back({p, len});
                    p += len;
                }
                size_t m = o.Open(e.type, e.ver);
                o.U32((uint32_t)kept.size());
                for (auto& [at, len] : kept) o.Raw(d + at, len);
                o.Close(m);
            } else {
                o.Raw(b + e.at, 12 + e.size);
            }
        }
        o.Close(ex);
        o.Close(gc);
        o.Close(g);
    }
    o.Close(cl);
    return o.b;
}

std::vector<uint8_t> CutCollision(const uint8_t* body, uint32_t size, float cutX) {
    constexpr uint32_t kHead = 88, kBase = 28;  // the header after the name; offsets count from 28 bytes before it
    if (size < kHead) return {};
    uint16_t ns = Rd16(body + 40), nb = Rd16(body + 42), nf = Rd16(body + 44);
    uint32_t flags = Rd32(body + 48);
    uint32_t offS = Rd32(body + 52), offB = Rd32(body + 56), offV = Rd32(body + 64), offF = Rd32(body + 68);
    auto at = [&](uint32_t off, size_t len) -> const uint8_t* {
        if (off < kBase || off - kBase + len > size) return nullptr;
        return body + off - kBase;
    };
    const uint8_t* S = ns ? at(offS, ns * 20u) : nullptr;
    const uint8_t* B = nb ? at(offB, nb * 28u) : nullptr;
    const uint8_t* F = nf ? at(offF, nf * 8u) : nullptr;
    if ((ns && !S) || (nb && !B) || (nf && !F)) return {};
    uint32_t nverts = 0;
    for (uint16_t i = 0; i < nf; ++i)
        for (int k = 0; k < 3; ++k) nverts = std::max<uint32_t>(nverts, Rd16(F + i * 8 + k * 2) + 1u);
    const uint8_t* V = nverts ? at(offV, nverts * 6u) : nullptr;
    if (nverts && !V) return {};
    std::vector<float> v(size_t(nverts) * 3);
    for (uint32_t i = 0; i < nverts; ++i)
        for (int k = 0; k < 3; ++k) v[i * 3 + k] = (int16_t)Rd16(V + i * 6 + k * 2) / 128.0f;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> mids;
    auto mid = [&](uint32_t i, uint32_t j, float t) {
        auto key = std::make_pair(std::min(i, j), std::max(i, j));
        auto it = mids.find(key);
        if (it != mids.end()) return it->second;
        uint32_t n = (uint32_t)(v.size() / 3);
        for (int k = 0; k < 3; ++k) v.push_back(v[i * 3 + k] + (v[j * 3 + k] - v[i * 3 + k]) * t);
        mids[key] = n;
        return n;
    };
    std::vector<uint32_t> faces;  // a, b, c, material | light << 8
    for (uint16_t i = 0; i < nf; ++i) {
        const uint8_t* f = F + i * 8;
        uint32_t c[3] = {Rd16(f), Rd16(f + 2), Rd16(f + 4)};
        float d[3];
        for (int k = 0; k < 3; ++k) d[k] = v[c[k] * 3] - cutX;
        uint32_t poly[4];
        int n = Clip(c, d, poly, mid);
        for (int k = 1; k + 1 < n; ++k) faces.insert(faces.end(), {poly[0], poly[k], poly[k + 1], (uint32_t)(f[6] | (f[7] << 8))});
    }
    std::vector<int64_t> remap(v.size() / 3, -1);
    std::vector<uint32_t> keep;
    for (size_t f = 0; f < faces.size(); f += 4)
        for (int k = 0; k < 3; ++k)
            if (remap[faces[f + k]] < 0) remap[faces[f + k]] = (int64_t)keep.size(), keep.push_back(faces[f + k]);
    if (keep.size() >= 65536 || faces.size() / 4 >= 65536) return {};
    std::vector<const uint8_t*> spheres, boxes;
    for (uint16_t i = 0; i < ns; ++i)
        if (RdF(S + i * 20) >= cutX) spheres.push_back(S + i * 20);
    for (uint16_t i = 0; i < nb; ++i)
        if ((RdF(B + i * 28) + RdF(B + i * 28 + 12)) / 2 >= cutX) boxes.push_back(B + i * 28);

    Out o;
    o.Raw(body, 40);  // its bounds: as they were (the block's place in the world stays)
    uint32_t nverts2 = (uint32_t)keep.size(), nf2 = (uint32_t)(faces.size() / 4);
    o.U16((uint16_t)spheres.size()), o.U16((uint16_t)boxes.size()), o.U16((uint16_t)nf2), o.U8(0), o.U8(0);
    o.U32((flags & ~(0x08u | 0x10u)) | (nf2 ? 0x02u : 0u));  // no face groups, no shadow mesh
    uint32_t pS = kBase + kHead, pB = pS + (uint32_t)spheres.size() * 20, pV = pB + (uint32_t)boxes.size() * 28;
    uint32_t vsize = (nverts2 * 6 + 3) & ~3u, pF = pV + vsize;
    o.U32(spheres.empty() ? 0 : pS), o.U32(boxes.empty() ? 0 : pB), o.U32(0), o.U32(pV), o.U32(pF), o.U32(0);
    o.U32(0), o.U32(0), o.U32(0);
    for (const uint8_t* p : spheres) o.Raw(p, 20);
    for (const uint8_t* p : boxes) o.Raw(p, 28);
    for (uint32_t i : keep)
        for (int k = 0; k < 3; ++k) o.U16((uint16_t)(int16_t)std::clamp(std::lround(v[i * 3 + k] * 128.0f), -32768l, 32767l));
    while (o.b.size() < pF - kBase) o.U8(0);
    for (size_t f = 0; f < faces.size(); f += 4) {
        o.U16((uint16_t)remap[faces[f]]), o.U16((uint16_t)remap[faces[f + 1]]), o.U16((uint16_t)remap[faces[f + 2]]);
        o.U8((uint8_t)(faces[f + 3] & 0xFF)), o.U8((uint8_t)(faces[f + 3] >> 8));
    }
    return o.b;
}

}  // namespace coop::charlie
