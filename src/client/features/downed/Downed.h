#pragma once

namespace coop::downed {

// After CPad::UpdatePads (Patches.cpp): a downed player does nothing but look around; near a downed friend the enter key
// (F, the pad's triangle) revives instead of taking a car.
void AfterPadUpdate();

}  // namespace coop::downed
