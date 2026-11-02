#pragma once
// The jackets' paint (JacketPaint.cpp, data in ClothesData.h): pure functions, no game in them (tested on the PC that
// builds the mod: tools/clothes).
#include <cstddef>
#include <cstdint>

namespace coop::clothes {

constexpr int kGameSize = 256;  // the game's textures of the jackets
constexpr int kHdSize = 1024;   // ours on the dressed player

// how many jackets, and each one's texture in the game ("coach", "coachsemi", "hoodjackbeige")
int Count();
const char* Texture(int jacket);

// One of our jackets over the game's texture of that name (256 x 256, B G R X rows of `stride` bytes): the design,
// shaded by the original's folds. False (pixels left as they are): not one of ours, or not the game's own picture
// (another mod's). Keeps the folds for PaintHd.
bool Paint(const char* texture, uint8_t* pixels, int width, int height, int stride);

// The dressed player's texture in HD: base, the game's 256 x 256 one made from that jacket's (B G R X rows of
// baseStride bytes), scaled up 4 times and the design over it at 1024 x 1024, shaded by the folds Paint kept: out,
// 1024 x 1024 x 4 bytes, B G R X. False: Paint never painted that jacket (another mod's picture).
bool PaintHd(int jacket, const uint8_t* base, int baseStride, uint8_t* out);

// the next mipmap: size x size B G R X -> size / 2 x size / 2, each texel the mean of 4 (rounded)
void HalfSize(const uint8_t* in, int size, uint8_t* out);

// the wardrobe's names of ours (their GXT keys COACHA, COACHB, HODJCK1), else null
const char* NameFor(const char* gxtKey);

}  // namespace coop::clothes
