#pragma once

namespace coop::headphones {

// The headphones (Headphones.cpp): a station of the radio heard on foot.
// They play now: on, our player out of any vehicle with a radio, and the radio started for them.
bool Playing();
// The headphones' key went down (keys/headphones in coop.ini, H): a tap puts them on or takes them off, held down the
// mouse wheel turns their dial. The key still reaches the game (in a car it is the horn).
void KeyDown();
// Any other key went down: a cheat being typed (HESOYAM) is no tap on the headphones' key.
void OtherKey();
// After CPad::UpdatePads: while the key is held, the mouse wheel turns the dial and the weapons stay as they are.
void AfterPadUpdate();

}  // namespace coop::headphones
