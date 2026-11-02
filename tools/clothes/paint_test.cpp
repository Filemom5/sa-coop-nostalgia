// Paints the game's jacket textures as the mod does and compares with export.py's: the 256 x 256 texture as it loads
// (out/*_ours.bgra) and the dressed player's in HD with its mipmaps (out/*_hd.bgra).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "features/clothes/JacketPaint.h"
static std::vector<uint8_t> Read(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static int Compare(const char* what, const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    int diff = 0, worst = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        int d = abs(a[i] - b[i]);
        if (d) diff++;
        if (d > worst) worst = d;
    }
    printf("  %s: %d bytes differ (worst %d)%s\n", what, diff, worst, a.size() != b.size() ? ", sizes differ" : "");
    return a.size() != b.size() ? 99 : worst;
}
int main() {
    using namespace coop::clothes;
    int bad = 0;
    for (int j = 0; j < Count(); ++j) {
        const char* name = Texture(j);
        auto txd = Read(std::string("orig/") + name + ".txd");
        // the level's bytes: after the texture native's struct header (88 bytes) and its size
        size_t at = 0;
        for (size_t i = 0; i + 4 < txd.size(); ++i)
            if (!memcmp(&txd[i], name, strlen(name)) && txd[i + strlen(name)] == 0) { at = i; break; }
        at += 32 + 32 + 16 + 4;  // name, mask, format..flags, the level's size
        std::vector<uint8_t> px(txd.begin() + at, txd.begin() + at + 256 * 256 * 4);
        std::vector<uint8_t> none(256 * 256 * 4), hd0(1024 * 1024 * 4);
        if (PaintHd(j, none.data(), 1024, hd0.data())) { printf("%s: HD before it was painted!\n", name); bad++; }
        bool ok = Paint(name, px.data(), 256, 256, 1024);
        printf("%s: painted %d\n", name, ok);
        if (!ok || Compare("256", px, Read(std::string("out/") + name + "_ours.bgra")) > 1) bad++;
        auto again = px;
        if (Paint(name, again.data(), 256, 256, 1024)) { printf("  painted twice!\n"); bad++; }
        // HD over the painted texture (the dressed player's is made from it), and its mipmaps
        std::vector<uint8_t> hd(1024 * 1024 * 4), all;
        if (!PaintHd(j, px.data(), 1024, hd.data())) { printf("  no HD!\n"); bad++; continue; }
        all = hd;
        for (int size = 1024; size > 1; size /= 2) {
            std::vector<uint8_t> half(size_t(size / 2) * (size / 2) * 4);
            HalfSize(hd.data(), size, half.data());
            all.insert(all.end(), half.begin(), half.end());
            hd.swap(half);
        }
        if (Compare("HD and mipmaps", all, Read(std::string("out/") + name + "_hd.bgra")) > 1) bad++;
    }
    printf("%s / %s / %s\n", NameFor("COACHA"), NameFor("coachb"), NameFor("HODJCK1"));
    return bad;
}
