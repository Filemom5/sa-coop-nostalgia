// The jackets' paint: our design (ClothesData.h, from tools/clothes) over the game's texture of the jacket, shaded by
// the game's own folds: its brightness (5 x 5 median, so that the cloth's grain goes) against its mean gives each texel
// a shade, 0.55 to 1.2 (the original's white bits, a shirt under it: none), taken by the design as strong as its alpha.
// Texels the design leaves out (alpha 0) stay the game's. Only over the game's own picture (CRC-32 of its pixels).
#include "features/clothes/JacketPaint.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "features/clothes/ClothesData.h"

namespace coop::clothes {

namespace {

constexpr int kSize = 256;

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
};

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

// the design, RGBA, from its runs (count, R, G, B, A)
bool Unpack(const Jacket& j, std::vector<uint8_t>& rgba) {
    rgba.assign(size_t(kSize) * kSize * 4, 0);
    size_t at = 0;
    for (size_t i = 0; i + 5 <= j.designSize; i += 5) {
        size_t n = j.design[i];
        if (at + n * 4 > rgba.size()) return false;
        for (size_t k = 0; k < n; ++k, at += 4) memcpy(&rgba[at], &j.design[i + 1], 4);
    }
    return at == rgba.size();
}

}  // namespace

bool Paint(const char* texture, uint8_t* pixels, int width, int height, int stride) {
    if (!texture || !pixels || width != kSize || height != kSize || stride < kSize * 4) return false;
    const Jacket* j = nullptr;
    for (auto& x : kJackets)
        if (SameName(texture, x.texture)) j = &x;
    if (!j) return false;
    uint32_t c = 0xFFFFFFFFu;
    for (int y = 0; y < kSize; ++y) c = Crc32(pixels + size_t(y) * stride, kSize * 4, c);
    if ((c ^ 0xFFFFFFFFu) != j->crc) return false;
    std::vector<uint8_t> d;
    if (!Unpack(*j, d)) return false;
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
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const uint8_t* s = &d[(y * kSize + x) * 4];
            if (!s[3]) continue;
            float ratio = med[y * kSize + x] / mean;
            ratio = ratio > 1.6f ? 1.0f : std::clamp(ratio, 0.55f, 1.2f);
            float k = s[3] / 255.0f, shade = 1 + (ratio - 1) * k;
            uint8_t* p = pixels + size_t(y) * stride + x * 4;
            for (int ch = 0; ch < 3; ++ch) p[2 - ch] = (uint8_t)std::clamp(s[ch] * shade, 0.0f, 255.0f);
            p[3] = 255;
        }
    return true;
}

const char* NameFor(const char* gxtKey) {
    if (!gxtKey) return nullptr;
    for (auto& j : kJackets)
        if (SameName(gxtKey, j.gxtKey)) return j.name;
    return nullptr;
}

}  // namespace coop::clothes
