#pragma once

namespace coop::roof {

// The hold key (roof_lock) went down. True when the key was ours to take.
bool Key();
// After CPad::UpdatePads (Patches.cpp): holding on to a car, the player does not walk; a jump lets go.
void AfterPadUpdate();

}  // namespace coop::roof
