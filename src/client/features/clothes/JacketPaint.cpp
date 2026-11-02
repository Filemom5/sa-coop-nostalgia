// The jackets' paint: our design (ClothesData.h, from tools/clothes: 1024 x 1024) over the game's texture of the jacket,
// shaded by the game's own folds: its brightness (5 x 5 median, so that the cloth's grain goes) against its mean gives
// each texel a shade, 0.55 to 1.2 (the original's white bits, a shirt under it: none), taken by the design as strong as
// its alpha. Texels the design leaves out (alpha 0) stay the game's. Only over the game's own picture (CRC-32 of its
// pixels).
//
// The game's texture is 256 x 256: every 4th texel of the design goes on it (Paint). The game makes the dressed
// player's texture from it (CClothesBuilder), and PaintHd makes that one anew at 1024 x 1024: it scaled up (bilinear),
// the whole design over it, the folds scaled up too. In whole numbers, as tools/clothes/export.py does.
#include "features/clothes/JacketPaint.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "features/clothes/ClothesData.h"

namespace coop::clothes {

namespace {

constexpr int kSize = kGameSize, kBig = kHdSize;

struct Jacket {
    const char* texture;
    uint32_t crc;
    const unsigned char* design;
    size_t designSize;
    const char* gxtKey;
    const char* name;
};

const Jacket kJackets[] = {
    {"coach", data::kCoachCrc, data::kCoachDesign, sizeof data::kCoachDesign, "COACHA", "Corta-vento Matunas"},
    {"coachsemi", data::kCoachsemiCrc, data::kCoachsemiDesign, sizeof data::kCoachsemiDesign, "COACHB",
     "Corta-vento Nostalgia"},
    {"hoodjackbeige", data::kHoodjackbeigeCrc, data::kHoodjackbeigeDesign, sizeof data::kHoodjackbeigeDesign, "HODJCK1",
     "Jaqueta Sunset Nostalgia"},
};
constexpr int kCount = (int)(sizeof kJackets / sizeof kJackets[0]);

// each jacket's folds as Paint found them on the game's texture: 1024 for a shade of 1.0 (empty: not painted)
std::vector<uint16_t> g_folds[kCount];

uint32_t Crc32(const uint8_t* data, size_t size, uint32_t c = 0xFFFFFFFFu) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c;
}

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if ((*a | 0x20) != (*b | 0x20)) return false;
    return *a == *b;
}

int Find(const char* texture) {
    for (int i = 0; i < kCount; ++i)
        if (texture && SameName(texture, kJackets[i].texture)) return i;
    return -1;
}

// the design, RGBA 1024 x 1024, from its runs (count, R, G, B, A)
bool Unpack(const Jacket& j, std::vector<uint8_t>& rgba) {
    rgba.assign(size_t(kBig) * kBig * 4, 0);
    size_t at = 0;
    for (size_t i = 0; i + 5 <= j.designSize; i += 5) {
        size_t n = j.design[i];
        if (at + n * 4 > rgba.size()) return false;
        for (size_t k = 0; k < n; ++k, at += 4) memcpy(&rgba[at], &j.design[i + 1], 4);
    }
    return at == rgba.size();
}

// 4 times as big, bilinear at the texel centres (edges clamped): the 4 texels of one sit 3/8 and 1/8 before it, 1/8 and
// 3/8 after it, so the weights are eighths
struct Up4 {
    int lo[kBig], w[kBig];  // the first source texel and its weight (the next one has 8 - w)
    Up4() {
        static const int kW[4] = {3, 1, 7, 5};
        for (int x = 0; x < kBig; ++x) {
            int i = x >> 2, l = (x & 3) < 2 ? i - 1 : i;
            lo[x] = l;
            w[x] = kW[x & 3];
        }
    }
    static int Clamp(int i) { return std::clamp(i, 0, kSize - 1); }
};
const Up4& Weights() {
    static const Up4 u;
    return u;
}

}  // namespace

int Count() { return kCount; }

const char* Texture(int jacket) { return jacket >= 0 && jacket < kCount ? kJackets[jacket].texture : nullptr; }

bool Paint(const char* texture, uint8_t* pixels, int width, int height, int stride) {
    int ji = Find(texture);
    if (ji < 0 || !pixels || width != kSize || height != kSize || stride < kSize * 4) return false;
    const Jacket& j = kJackets[ji];
    uint32_t c = 0xFFFFFFFFu;
    for (int y = 0; y < kSize; ++y) c = Crc32(pixels + size_t(y) * stride, kSize * 4, c);
    if ((c ^ 0xFFFFFFFFu) != j.crc) return false;
    std::vector<uint8_t> big;
    if (!Unpack(j, big)) return false;
    std::vector<uint8_t> d(size_t(kSize) * kSize * 4);  // every 4th texel (the 3rd of each 4)
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) memcpy(&d[(y * kSize + x) * 4], &big[((4 * y + 2) * kBig + 4 * x + 2) * 4], 4);
    // brightness, its 5 x 5 median (edges clamped), its mean under the design
    std::vector<uint8_t> lum(kSize * kSize), med(kSize * kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const uint8_t* p = pixels + size_t(y) * stride + x * 4;
            lum[y * kSize + x] = (uint8_t)(p[2] * 0.299f + p[1] * 0.587f + p[0] * 0.114f);
        }
    double sum = 0;
    int count = 0;
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            uint8_t v[25];
            int n = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    v[n++] = lum[std::clamp(y + dy, 0, kSize - 1) * kSize + std::clamp(x + dx, 0, kSize - 1)];
            std::nth_element(v, v + 12, v + 25);
            med[y * kSize + x] = v[12];
            if (d[(y * kSize + x) * 4 + 3]) sum += v[12], ++count;
        }
    float mean = std::max((float)(sum / std::max(count, 1)), 1.0f);
    std::vector<uint16_t>& folds = g_folds[ji];
    folds.assign(size_t(kSize) * kSize, 1024);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            float ratio = med[y * kSize + x] / mean;
            ratio = ratio > 1.6f ? 1.0f : std::clamp(ratio, 0.55f, 1.2f);
            folds[y * kSize + x] = (uint16_t)(int)(ratio * 1024.0f + 0.5f);
            const uint8_t* s = &d[(y * kSize + x) * 4];
            if (!s[3]) continue;
            float k = s[3] / 255.0f, shade = 1 + (ratio - 1) * k;
            uint8_t* p = pixels + size_t(y) * stride + x * 4;
            for (int ch = 0; ch < 3; ++ch) p[2 - ch] = (uint8_t)std::clamp(s[ch] * shade, 0.0f, 255.0f);
            p[3] = 255;
        }
    return true;
}

bool PaintHd(int jacket, const uint8_t* base, int baseStride, uint8_t* out) {
    if (jacket < 0 || jacket >= kCount || !base || !out || baseStride < kSize * 4) return false;
    const std::vector<uint16_t>& folds = g_folds[jacket];
    if (folds.size() != size_t(kSize) * kSize) return false;
    std::vector<uint8_t> d;
    if (!Unpack(kJackets[jacket], d)) return false;
    const Up4& u = Weights();
    for (int y = 0; y < kBig; ++y) {
        int y0 = Up4::Clamp(u.lo[y]), y1 = Up4::Clamp(u.lo[y] + 1), wy = u.w[y];
        const uint8_t *r0 = base + size_t(y0) * baseStride, *r1 = base + size_t(y1) * baseStride;
        const uint16_t *f0 = &folds[size_t(y0) * kSize], *f1 = &folds[size_t(y1) * kSize];
        for (int x = 0; x < kBig; ++x) {
            int x0 = Up4::Clamp(u.lo[x]), x1 = Up4::Clamp(u.lo[x] + 1), wx = u.w[x];
            int w00 = wy * wx, w01 = wy * (8 - wx), w10 = (8 - wy) * wx, w11 = (8 - wy) * (8 - wx);
            uint8_t* o = out + (size_t(y) * kBig + x) * 4;
            const uint8_t* s = &d[(size_t(y) * kBig + x) * 4];
            if (s[3]) {
                int64_t r = (f0[x0] * w00 + f0[x1] * w01 + f1[x0] * w10 + f1[x1] * w11 + 32) >> 6;
                int64_t f = 1024 * 255 + (r - 1024) * s[3];
                for (int ch = 0; ch < 3; ++ch) o[2 - ch] = (uint8_t)std::clamp<int64_t>((s[ch] * f + 130560) / 261120, 0, 255);
            } else {
                for (int ch = 0; ch < 3; ++ch)
                    o[ch] = (uint8_t)((r0[x0 * 4 + ch] * w00 + r0[x1 * 4 + ch] * w01 + r1[x0 * 4 + ch] * w10 +
                                       r1[x1 * 4 + ch] * w11 + 32) >> 6);
            }
            o[3] = 255;
        }
    }
    return true;
}

void HalfSize(const uint8_t* in, int size, uint8_t* out) {
    int half = size / 2;
    for (int y = 0; y < half; ++y)
        for (int x = 0; x < half; ++x)
            for (int ch = 0; ch < 4; ++ch) {
                const uint8_t* a = in + (size_t(2 * y) * size + 2 * x) * 4 + ch;
                out[(size_t(y) * half + x) * 4 + ch] = (uint8_t)((a[0] + a[4] + a[size * 4] + a[size * 4 + 4] + 2) >> 2);
            }
}

const char* NameFor(const char* gxtKey) {
    if (!gxtKey) return nullptr;
    for (auto& j : kJackets)
        if (SameName(gxtKey, j.gxtKey)) return j.name;
    return nullptr;
}

}  // namespace coop::clothes
