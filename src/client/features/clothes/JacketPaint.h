#pragma once
// The jackets' paint (JacketPaint.cpp, data in ClothesData.h): pure functions, no game in them (tested on the PC that
// builds the mod: tools/clothes).
#include <cstddef>
#include <cstdint>

namespace coop::clothes {

// One of our jackets over the game's texture of that name ("coach", "coachsemi": 256 x 256, B G R X rows of `stride`
// bytes): the design, shaded by the original's folds. False (pixels left as they are): not one of ours, or not the
// game's own picture (another mod's).
bool Paint(const char* texture, uint8_t* pixels, int width, int height, int stride);

// the wardrobe's names of the two (their GXT keys COACHA, COACHB), else null
const char* NameFor(const char* gxtKey);

}  // namespace coop::clothes
