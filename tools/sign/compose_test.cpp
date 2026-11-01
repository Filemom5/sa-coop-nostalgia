// Applies the sign's recipe to the game's own files, as the mod does: tools/sign test.
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <iterator>
#include <vector>
#include "features/sign/SignRecipe.h"
static std::vector<uint8_t> Read(const char* p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char** argv) {
    struct { unsigned id; const char* in; const char* out; } m[] = {
        {13831, "orig/vinesign1_cunte01.dff", "test/vinesign1_cunte01.dff"},
        {13722, "orig/vinesign1_cunte.dff", "test/vinesign1_cunte.dff"},
        {13759, "orig/lodvinesign1_cunte.dff", "test/lodvinesign1_cunte.dff"},
    };
    int bad = 0;
    for (auto& x : m) {
        auto in = Read(x.in);
        auto out = coop::sign::Compose(x.id, in.data(), in.size());
        printf("%u: %zu -> %zu bytes\n", x.id, in.size(), out.size());
        if (out.empty()) { bad++; continue; }
        std::ofstream(x.out, std::ios::binary).write((const char*)out.data(), out.size());
        auto wrong = coop::sign::Compose(x.id == 13831 ? 13722 : 13831, in.data(), in.size());
        if (!wrong.empty()) { printf("  composed for the wrong model!\n"); bad++; }
    }
    // the collision: the originals' (in the area's files), as LoadCollisionModelVer3 gets them
    int found = 0;
    for (int i = 1; i <= 6; ++i) {
        auto b = Read(("orig/lahills_" + std::to_string(i) + ".col").c_str());
        for (size_t off = 0; off + 32 <= b.size() && !memcmp(&b[off], "COL", 3);) {
            unsigned size;
            memcpy(&size, &b[off + 4], 4);
            const char* name = (const char*)&b[off + 8];
            unsigned ours = 0;
            if (coop::sign::Collision(name, &b[off + 32], size - 24, &ours)) printf("%s: ours, %u bytes\n", name, ours), found++;
            off += 8 + size;
        }
    }
    if (found != 2) printf("collision: %d of 2\n", found), bad++;
    return bad;
}
