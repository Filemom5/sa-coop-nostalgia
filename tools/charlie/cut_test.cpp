// Cuts the east block's model and collision as the mod does (features/charlie/Cut.cpp), from the game's own files:
//   g++ -std=c++17 -O2 -I../../src/client cut_test.cpp ../../src/client/features/charlie/Cut.cpp -o cut_test
//   ./cut_test santahouse02_law2.dff law2_3.col out/
// writes out/santahouse02_law2.dff (to look at with preview tools) and out/santahouse02_law2.colbody
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "features/charlie/Cut.h"

static std::vector<uint8_t> Read(const char* p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char** argv) {
    if (argc < 4) return printf("cut_test model.dff file.col outdir/\n"), 2;
    const float cut = 308.0f - 307.265625f;  // build.py's EAST_CUT, in the block's axes
    std::vector<uint8_t> dff = Read(argv[1]), col = Read(argv[2]);
    std::vector<uint8_t> m = coop::charlie::CutModel(dff.data(), dff.size(), cut);
    printf("model: %zu bytes -> %zu\n", dff.size(), m.size());
    std::ofstream(std::string(argv[3]) + "santahouse02_law2.dff", std::ios::binary).write((const char*)m.data(), m.size());
    for (size_t p = 0; p + 32 <= col.size();) {
        uint32_t size;
        memcpy(&size, &col[p + 4], 4);
        if (!strncmp((const char*)&col[p + 8], "santahouse02_law2", 22)) {
            std::vector<uint8_t> c = coop::charlie::CutCollision(&col[p + 32], size - 24, cut);
            printf("collision: %u bytes -> %zu\n", size - 24, c.size());
            std::ofstream(std::string(argv[3]) + "santahouse02_law2.colbody", std::ios::binary).write((const char*)c.data(), c.size());
        }
        p += 8 + size;
    }
    return m.empty();
}
