#include "shared/game/WalkIn.h"

namespace game::walkin {

namespace {
House g_house;
bool g_set = false;
}  // namespace

void Set(const House& house) {
    g_house = house;
    g_set = true;
}

const House* Get() { return g_set ? &g_house : nullptr; }

}  // namespace game::walkin
