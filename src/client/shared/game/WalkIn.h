// A house of the map that is its own interior (features/charlie: Charlie Harper's), for the group's homes
// (features/home, /casa marcar): one walks in through its own doors, its save disk inside. Set by the feature that
// makes the house, read by the homes.
#pragma once

namespace game::walkin {

struct House {
    bool (*inPlace)() = nullptr;              // it stands in this game's map
    bool (*onLot)(float x, float y) = nullptr;  // (world) on its lot
    float doorX = 0, doorY = 0, doorZ = 0, heading = 0;  // before its front door, facing it (heading in degrees)
    float diskX = 0, diskY = 0, diskZ = 0;              // the save disk, inside
};

void Set(const House& house);
const House* Get();  // nullptr: no such house

}  // namespace game::walkin
