// Paints the game's jacket textures as the mod does and compares with export.py's (out/*_ours.bgra).
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
int main() {
    int bad = 0;
    for (const char* name : {"coach", "coachsemi"}) {
        auto txd = Read(std::string("orig/") + name + ".txd");
        // the level's bytes: after the texture native's struct header (88 bytes) and its size
        size_t at = 0;
        for (size_t i = 0; i + 4 < txd.size(); ++i)
            if (!memcmp(&txd[i], name, strlen(name)) && txd[i + strlen(name)] == 0) { at = i; break; }
        at += 32 + 32 + 16 + 4;  // name, mask, format..flags, the level's size
        std::vector<uint8_t> px(txd.begin() + at, txd.begin() + at + 256 * 256 * 4);
        bool ok = coop::clothes::Paint(name, px.data(), 256, 256, 1024);
        auto ref = Read(std::string("out/") + name + "_ours.bgra");
        int diff = 0, worst = 0;
        for (size_t i = 0; i < px.size() && i < ref.size(); ++i) {
            int d = abs(px[i] - ref[i]);
            if (d) diff++;
            if (d > worst) worst = d;
        }
        printf("%s: painted %d, %d bytes differ (worst %d)\n", name, ok, diff, worst);
        if (!ok || worst > 1) bad++;
        auto again = px;
        if (coop::clothes::Paint(name, again.data(), 256, 256, 1024)) { printf("  painted twice!\n"); bad++; }
    }
    printf("%s / %s\n", coop::clothes::NameFor("COACHA"), coop::clothes::NameFor("coachb"));
    return bad;
}
