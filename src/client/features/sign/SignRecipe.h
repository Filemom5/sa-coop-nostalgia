#pragma once
// The sign's recipe (SignRecipe.cpp, data in SignData.h): pure functions, no game in them (they are tested on the PC that
// builds the mod, on the game's own files: tools/sign).
#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop::sign {

// One of the sign's models (its id): the DFF to load in place of the game's one (VineSign1_cunte01, VineSign1_cunte,
// LODVineSign1_cunte), made from it. Empty when the model is not one of them or not the game's own (another mod's).
std::vector<uint8_t> Compose(uint32_t modelId, const uint8_t* dff, size_t size);
bool IsSignModel(uint32_t modelId);

// A collision model as CFileLoader::LoadCollisionModelVer3 reads it (after its 32-byte header): ours in place of the
// sign's two (when they are the game's own), else null.
const uint8_t* Collision(const char* name, const uint8_t* data, uint32_t size, uint32_t* ourSize);

uint32_t Crc32(const uint8_t* data, size_t size);

}  // namespace coop::sign
