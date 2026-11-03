#pragma once
// Models and collisions of ours in place of the game's, as the game loads them (features/sign: the NOSTALGIA sign;
// features/charlie: Charlie Harper's house). The model: CFileLoader::LoadAtomicFile gets a memory stream of the model's
// file (from gta3.img); a swap gives the file to read instead. The collision: CFileLoader::LoadCollisionModelVer3 (at
// the start, for the bounds, and whenever the area's collision loads) reads a swap's in place of the game's.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace game {

// The model file (DFF) to read for a model (its id) in place of the game's one (`dff`, `size`); empty: the game's.
using ModelSwap = std::vector<uint8_t> (*)(uint32_t modelId, const uint8_t* dff, size_t size);

// A collision as LoadCollisionModelVer3 reads it (after its 32-byte header): ours (and its size) in place of the
// game's one of that name, or null.
using CollisionSwap = const uint8_t* (*)(const char* name, const uint8_t* data, uint32_t size, uint32_t* ourSize);

// From a module's OnInstallHooks (the first one installs the hooks). The swaps are asked in the order they came.
void AddModelSwap(ModelSwap swap);
void AddCollisionSwap(CollisionSwap swap);

}  // namespace game
