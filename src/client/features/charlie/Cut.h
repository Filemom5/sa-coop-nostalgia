#pragma once
// Cutting a part off one of the game's own models as it loads: santahouse02_law2, the block east of Charlie's house,
// loses its two houses west of the beach safe house (the house's lot goes there), the safe house stays as it is.
// What lies west of a vertical plane (x = cutX, in the model's own axes) goes; the faces across it are cut along it,
// their new corners getting everything of the old ones in between (place, texture, colours of day and night, normal).
// Pure functions, no game in them (tools/charlie/rwedit.py does the same, to look at).
#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop::charlie {

// A DFF of one geometry (the map's buildings): the same file, its geometry cut (triangle lists, its 2d effects west of
// the cut gone). Empty when it is not such a file.
std::vector<uint8_t> CutModel(const uint8_t* dff, size_t size, float cutX);

// A COL3 collision as CFileLoader::LoadCollisionModelVer3 reads it (after its 32-byte header): its faces cut, its boxes
// and spheres kept by their middle. Empty when it is not one.
std::vector<uint8_t> CutCollision(const uint8_t* body, uint32_t size, float cutX);

}  // namespace coop::charlie
