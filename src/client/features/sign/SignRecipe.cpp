// The sign's models made from the game's own (the recipe: SignData.h, from tools/sign/recipe.py).
//
// A DFF is a tree of chunks (type, size, version). The sign's are a clump with one frame, one geometry and one atomic;
// only the geometry changes: its vertices (positions, day and night colours, texture coordinates) and triangles are
// built by the recipe's operations, in order:
//   copy: some of the original's triangles (with their vertices), moved, some vertices set to a height (frames' feet);
//   new:  vertices and triangles of ours (new letters), each vertex's colours a weighted mix of the original's ones.
// The rest is the original's: the frame, the atomic, the materials (or, for the LOD, an untextured one) and the
// geometry's extensions, with the triangles' mesh and the night colours made anew. The recipe says which original it
// is for (vertex and triangle counts, CRC-32 of the geometry): another one is left as it is.
#include "features/sign/SignRecipe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "features/sign/SignData.h"

namespace coop::sign {

namespace {

constexpr uint32_t kStruct = 0x01, kExtension = 0x03, kMaterial = 0x07, kMatList = 0x08, kGeometry = 0x0F;
constexpr uint32_t kClump = 0x10, kGeomList = 0x1A, kBinMesh = 0x50E, kNightColours = 0x253F2F9;
constexpr uint32_t kTriStrip = 0x01, kNative = 0x01000000;

template <typename T>
T Get(const uint8_t* p) {
    T v;
    memcpy(&v, p, sizeof v);
    return v;
}

struct Chunk {
    uint32_t type = 0, size = 0, ver = 0;
    size_t data = 0;  // offset of its payload
};

// the chunks in [from, to) of buf
template <typename F>
bool ForChunks(const uint8_t* buf, size_t from, size_t to, F&& f) {
    for (size_t off = from; off + 12 <= to;) {
        Chunk c{Get<uint32_t>(buf + off), Get<uint32_t>(buf + off + 4), Get<uint32_t>(buf + off + 8), off + 12};
        if (c.size > to - c.data) return false;
        if (!f(c)) return true;
        off = c.data + c.size;
    }
    return true;
}

bool Find(const uint8_t* buf, size_t from, size_t to, uint32_t type, Chunk& out) {
    bool found = false;
    ForChunks(buf, from, to, [&](const Chunk& c) {
        if (c.type != type) return true;
        out = c;
        found = true;
        return false;
    });
    return found;
}

// the original geometry
struct Src {
    uint32_t flags = 0, nt = 0, nv = 0;
    const uint8_t* prelit = nullptr;
    const uint8_t* uv = nullptr;
    const uint8_t* tris = nullptr;  // per triangle: vertex 2, vertex 1, material, vertex 3 (u16)
    const uint8_t* verts = nullptr;
    const uint8_t* night = nullptr;
    Chunk structChunk, matList;
    std::vector<Chunk> ext;
};

bool ReadGeometry(const uint8_t* buf, const Chunk& geo, Src& s) {
    size_t end = geo.data + geo.size;
    if (!Find(buf, geo.data, end, kStruct, s.structChunk) || s.structChunk.size < 16) return false;
    const uint8_t* p = buf + s.structChunk.data;
    const uint8_t* pend = p + s.structChunk.size;
    s.flags = Get<uint32_t>(p);
    s.nt = Get<uint32_t>(p + 4);
    s.nv = Get<uint32_t>(p + 8);
    uint32_t morphs = Get<uint32_t>(p + 12);
    uint32_t texSets = (s.flags >> 16) & 0xFF;
    if ((s.flags & kNative) || morphs != 1 || texSets != 1 || !(s.flags & 0x08)) return false;
    p += 16;
    size_t need = size_t(s.nv) * 4 + size_t(s.nv) * 8 + size_t(s.nt) * 8 + 24 + size_t(s.nv) * 12;
    if (size_t(pend - p) < need) return false;
    s.prelit = p, p += s.nv * 4;
    s.uv = p, p += s.nv * 8;
    s.tris = p, p += s.nt * 8;
    if (Get<uint32_t>(p + 16) != 1) return false;  // has vertices
    s.verts = p + 24;
    size_t after = s.structChunk.data + s.structChunk.size;
    if (!Find(buf, after, end, kMatList, s.matList)) return false;
    Chunk ext;
    if (!Find(buf, s.matList.data + s.matList.size, end, kExtension, ext)) return false;
    ForChunks(buf, ext.data, ext.data + ext.size, [&](const Chunk& c) {
        s.ext.push_back(c);
        if (c.type == kNightColours && c.size >= 4 + s.nv * 4 && Get<uint32_t>(buf + c.data)) s.night = buf + c.data + 4;
        return true;
    });
    return true;
}

struct Vertex {
    float x, y, z, u, v;
    uint8_t day[4], night[4];
};

struct Recipe {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;
    template <typename T>
    T Take() {
        if (size_t(end - p) < sizeof(T)) {
            ok = false;
            return T{};
        }
        T v = Get<T>(p);
        p += sizeof(T);
        return v;
    }
    void Skip(size_t n) {
        if (size_t(end - p) < n) ok = false, n = end - p;
        p += n;
    }
};

uint8_t Mix(const std::vector<std::pair<uint16_t, float>>& w, const uint8_t* colours, int k) {
    float sum = 0;
    for (auto& [src, wt] : w) sum += wt * colours[src * 4 + k];
    long v = std::lrint(sum);
    return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

// builds the vertices and triangles (a, b, c, material) of one model from the recipe (r at its operations)
bool Build(Recipe& r, uint32_t ops, const Src& s, std::vector<Vertex>& out, std::vector<std::array<uint16_t, 4>>& tris) {
    auto srcTri = [&](uint32_t t) {  // a, b, c, material of the original's triangle t
        const uint8_t* q = s.tris + t * 8;
        return std::array<uint16_t, 4>{Get<uint16_t>(q + 2), Get<uint16_t>(q), Get<uint16_t>(q + 6), Get<uint16_t>(q + 4)};
    };
    auto srcVertex = [&](uint32_t v) {
        Vertex o;
        memcpy(&o.x, s.verts + v * 12, 12);
        memcpy(&o.u, s.uv + v * 8, 8);
        memcpy(o.day, s.prelit + v * 4, 4);
        if (s.night)
            memcpy(o.night, s.night + v * 4, 4);
        else
            memcpy(o.night, o.day, 4);
        return o;
    };
    for (uint32_t i = 0; i < ops && r.ok; ++i) {
        uint8_t kind = r.Take<uint8_t>();
        r.Skip(3);
        if (kind == 0) {  // copy
            float dx = r.Take<float>(), dy = r.Take<float>(), dz = r.Take<float>();
            uint32_t n = r.Take<uint32_t>();
            std::vector<uint16_t> list(n);
            for (auto& t : list) t = r.Take<uint16_t>();
            r.Skip((n * 2 + 3) / 4 * 4 - n * 2);
            uint32_t nfix = r.Take<uint32_t>();
            std::vector<std::pair<uint16_t, float>> fix(nfix);
            for (auto& f : fix) {
                f.first = r.Take<uint16_t>();
                r.Skip(2);
                f.second = r.Take<float>();
            }
            if (!r.ok) return false;
            std::vector<int32_t> remap(s.nv, -1);
            std::vector<uint16_t> used;
            for (uint16_t t : list) {
                if (t >= s.nt) return false;
                auto tri = srcTri(t);
                for (int k = 0; k < 3; ++k) {
                    if (tri[k] >= s.nv) return false;
                    if (remap[tri[k]] < 0) remap[tri[k]] = 0, used.push_back(tri[k]);
                }
            }
            std::sort(used.begin(), used.end());
            for (uint16_t v : used) {
                remap[v] = (int32_t)out.size();
                Vertex o = srcVertex(v);
                o.x += dx, o.y += dy, o.z += dz;
                for (auto& [fv, z] : fix)
                    if (fv == v) o.z = z;
                out.push_back(o);
            }
            for (uint16_t t : list) {
                auto tri = srcTri(t);
                tris.push_back({(uint16_t)remap[tri[0]], (uint16_t)remap[tri[1]], (uint16_t)remap[tri[2]], tri[3]});
            }
        } else if (kind == 1) {  // new
            uint32_t base = (uint32_t)out.size();
            uint32_t nv = r.Take<uint32_t>();
            for (uint32_t v = 0; v < nv && r.ok; ++v) {
                Vertex o;
                o.x = r.Take<float>(), o.y = r.Take<float>(), o.z = r.Take<float>();
                o.u = r.Take<float>(), o.v = r.Take<float>();
                uint32_t nw = r.Take<uint32_t>();
                std::vector<std::pair<uint16_t, float>> w(nw);
                for (auto& x : w) {
                    x.first = r.Take<uint16_t>();
                    r.Skip(2);
                    x.second = r.Take<float>();
                    if (x.first >= s.nv) return false;
                }
                for (int k = 0; k < 4; ++k) {
                    o.day[k] = Mix(w, s.prelit, k);
                    o.night[k] = s.night ? Mix(w, s.night, k) : o.day[k];
                }
                out.push_back(o);
            }
            uint32_t nt = r.Take<uint32_t>();
            for (uint32_t t = 0; t < nt && r.ok; ++t) {
                std::array<uint16_t, 4> tri{};
                for (auto& x : tri) x = r.Take<uint16_t>();
                for (int k = 0; k < 3; ++k) {
                    if (tri[k] >= nv) return false;
                    tri[k] = (uint16_t)(tri[k] + base);
                }
                tris.push_back(tri);
            }
        } else {
            return false;
        }
        if (out.size() > 0xFFFF) return false;
    }
    return r.ok;
}

// ---------------------------------------------------------------- writing
struct Writer {
    std::vector<uint8_t> b;
    template <typename T>
    void Put(T v) {
        size_t n = b.size();
        b.resize(n + sizeof v);
        memcpy(b.data() + n, &v, sizeof v);
    }
    void Bytes(const void* p, size_t n) { b.insert(b.end(), (const uint8_t*)p, (const uint8_t*)p + n); }
    size_t Open(uint32_t type, uint32_t ver) {  // a chunk's header: its size is set by Close
        Put(type), Put(uint32_t(0)), Put(ver);
        return b.size();
    }
    void Close(size_t at) {
        uint32_t size = uint32_t(b.size() - at);
        memcpy(b.data() + at - 8, &size, 4);
    }
};

void WriteGeometry(Writer& w, const uint8_t* buf, const Src& s, const std::vector<Vertex>& v,
                   const std::vector<std::array<uint16_t, 4>>& tris, uint32_t ver, const uint8_t* colour) {
    size_t geo = w.Open(kGeometry, ver);
    size_t st = w.Open(kStruct, ver);
    w.Put(uint32_t(s.flags & ~kTriStrip)), w.Put(uint32_t(tris.size())), w.Put(uint32_t(v.size())), w.Put(uint32_t(1));
    for (auto& x : v) w.Bytes(x.day, 4);
    for (auto& x : v) w.Put(x.u), w.Put(x.v);
    for (auto& t : tris) w.Put(t[1]), w.Put(t[0]), w.Put(t[3]), w.Put(t[2]);
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (auto& x : v) {
        const float p[3] = {x.x, x.y, x.z};
        for (int k = 0; k < 3; ++k) lo[k] = std::fmin(lo[k], p[k]), hi[k] = std::fmax(hi[k], p[k]);
    }
    float c[3] = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2}, rad = 0;
    for (auto& x : v)
        rad = std::fmax(rad, std::sqrt((x.x - c[0]) * (x.x - c[0]) + (x.y - c[1]) * (x.y - c[1]) + (x.z - c[2]) * (x.z - c[2])));
    w.Put(c[0]), w.Put(c[1]), w.Put(c[2]), w.Put(rad), w.Put(uint32_t(1)), w.Put(uint32_t(0));
    for (auto& x : v) w.Put(x.x), w.Put(x.y), w.Put(x.z);
    w.Close(st);
    if (colour) {  // one untextured material of that colour
        size_t ml = w.Open(kMatList, ver);
        size_t mls = w.Open(kStruct, ver);
        w.Put(int32_t(1)), w.Put(int32_t(-1));
        w.Close(mls);
        size_t m = w.Open(kMaterial, ver);
        size_t ms = w.Open(kStruct, ver);
        w.Put(int32_t(0)), w.Bytes(colour, 4), w.Put(int32_t(0)), w.Put(int32_t(0));
        w.Put(1.0f), w.Put(0.0f), w.Put(1.0f);
        w.Close(ms);
        w.Close(w.Open(kExtension, ver));
        w.Close(m);
        w.Close(ml);
    } else {
        w.Bytes(buf + s.matList.data - 12, s.matList.size + 12);
    }
    size_t ext = w.Open(kExtension, ver);
    // the triangles by material (a list: the original's were strips)
    size_t bm = w.Open(kBinMesh, ver);
    uint32_t materials = 0;
    for (auto& t : tris) materials = std::max<uint32_t>(materials, t[3] + 1u);
    uint32_t meshes = 0;
    for (uint32_t m = 0; m < materials; ++m)
        for (auto& t : tris)
            if (t[3] == m) {
                ++meshes;
                break;
            }
    w.Put(uint32_t(0)), w.Put(meshes), w.Put(uint32_t(tris.size() * 3));
    for (uint32_t m = 0; m < materials; ++m) {
        uint32_t n = 0;
        for (auto& t : tris) n += t[3] == m ? 3 : 0;
        if (!n) continue;
        w.Put(n), w.Put(m);
        for (auto& t : tris)
            if (t[3] == m) w.Put(uint32_t(t[0])), w.Put(uint32_t(t[1])), w.Put(uint32_t(t[2]));
    }
    w.Close(bm);
    for (auto& c : s.ext) {
        if (c.type == kBinMesh) continue;
        if (c.type == kNightColours) {
            size_t nc = w.Open(kNightColours, c.ver);
            w.Put(uint32_t(1));
            for (auto& x : v) w.Bytes(x.night, 4);
            w.Close(nc);
            continue;
        }
        w.Bytes(buf + c.data - 12, c.size + 12);
    }
    w.Close(ext);
    w.Close(geo);
}

// the recipe of a model: r at its operations
bool FindRecipe(uint32_t modelId, Recipe& r, uint32_t& nv, uint32_t& nt, uint32_t& crc, const uint8_t*& colour,
                uint32_t& ops) {
    r = Recipe{data::kRecipe, data::kRecipe + sizeof data::kRecipe};
    if (memcmp(r.p, "NSR1", 4)) return false;
    r.Skip(4);
    uint32_t models = r.Take<uint32_t>();
    for (uint32_t m = 0; m < models && r.ok; ++m) {
        uint32_t id = r.Take<uint32_t>();
        nv = r.Take<uint32_t>(), nt = r.Take<uint32_t>(), crc = r.Take<uint32_t>();
        uint8_t hasColour = r.Take<uint8_t>();
        colour = hasColour ? r.p : nullptr;
        r.Skip(7);
        ops = r.Take<uint32_t>();
        if (id == modelId) return r.ok;
        // skip this model's operations
        std::vector<Vertex> v;
        std::vector<std::array<uint16_t, 4>> t;
        for (uint32_t i = 0; i < ops && r.ok; ++i) {
            uint8_t kind = r.Take<uint8_t>();
            r.Skip(3);
            if (kind == 0) {
                r.Skip(12);
                uint32_t n = r.Take<uint32_t>();
                r.Skip((n * 2 + 3) / 4 * 4);
                r.Skip(r.Take<uint32_t>() * 8);
            } else {
                uint32_t n = r.Take<uint32_t>();
                for (uint32_t k = 0; k < n && r.ok; ++k) {
                    r.Skip(20);
                    r.Skip(r.Take<uint32_t>() * 8);
                }
                r.Skip(r.Take<uint32_t>() * 8);
            }
        }
    }
    return false;
}

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if ((*a | 0x20) != (*b | 0x20)) return false;
    return *a == *b;
}

}  // namespace

uint32_t Crc32(const uint8_t* data, size_t size) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool IsSignModel(uint32_t modelId) { return modelId == 13722 || modelId == 13831 || modelId == 13759; }

std::vector<uint8_t> Compose(uint32_t modelId, const uint8_t* buf, size_t size) {
    Recipe r{nullptr, nullptr};
    uint32_t nv, nt, crc, ops;
    const uint8_t* colour;
    if (!IsSignModel(modelId) || !FindRecipe(modelId, r, nv, nt, crc, colour, ops)) return {};
    Chunk clump, list, geo;
    if (!Find(buf, 0, size, kClump, clump)) return {};
    size_t clumpEnd = clump.data + clump.size;
    if (!Find(buf, clump.data, clumpEnd, kGeomList, list)) return {};
    if (!Find(buf, list.data, list.data + list.size, kGeometry, geo)) return {};
    Src s;
    if (!ReadGeometry(buf, geo, s) || s.nv != nv || s.nt != nt) return {};
    if (Crc32(buf + s.structChunk.data, s.structChunk.size) != crc) return {};
    std::vector<Vertex> v;
    std::vector<std::array<uint16_t, 4>> tris;
    if (!Build(r, ops, s, v, tris) || v.empty()) return {};
    Writer w;
    size_t c = w.Open(kClump, clump.ver);
    bool ok = ForChunks(buf, clump.data, clumpEnd, [&](const Chunk& ch) {
        if (ch.type != kGeomList) {
            w.Bytes(buf + ch.data - 12, ch.size + 12);
            return true;
        }
        size_t gl = w.Open(kGeomList, ch.ver);
        size_t gls = w.Open(kStruct, ch.ver);
        w.Put(int32_t(1));
        w.Close(gls);
        WriteGeometry(w, buf, s, v, tris, geo.ver, colour);
        w.Close(gl);
        return true;
    });
    if (!ok) return {};
    w.Close(c);
    return std::move(w.b);
}

const uint8_t* Collision(const char* name, const uint8_t* buf, uint32_t size, uint32_t* ourSize) {
    if (!name || !buf) return nullptr;
    if (SameName(name, "VineSign1_cunte01") && size == data::kColLettersSize &&
        Crc32(buf, size) == data::kColLettersCrc) {
        *ourSize = sizeof data::kColLetters;
        return data::kColLetters;
    }
    if (SameName(name, "VineSign1_cunte") && size == data::kColFramesSize && Crc32(buf, size) == data::kColFramesCrc) {
        *ourSize = sizeof data::kColFrames;
        return data::kColFrames;
    }
    return nullptr;
}

}  // namespace coop::sign
