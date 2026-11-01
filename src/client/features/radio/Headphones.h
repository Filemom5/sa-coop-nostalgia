#pragma once
#include <cstdint>

namespace coop::headphones {

// The headphones (Headphones.cpp): a station of the radio heard on foot.
// They play now: on, our player out of any vehicle with a radio, and the radio started for them.
bool Playing();
// The player put them on (the key), wherever he is now.
bool On();
// They are on our player's head now: on, out of any vehicle with a radio (Headset.cpp draws them).
bool Worn();
// Another player has them on, or took them off (G_RADIO OP_HEADSET, Radio.cpp): his copy wears them here (Headset.cpp),
// in the colours he picked (0 the first, as a game before the colours sends nothing).
void RemoteHeadset(uint8_t id, bool on, uint8_t colour);
// The headphones' key went down (keys/headphones in coop.ini, H): a tap puts them on or takes them off, held down the
// mouse wheel turns their dial. The key still reaches the game (in a car it is the horn).
void KeyDown();
// Any other key went down: a cheat being typed (HESOYAM) is no tap on the headphones' key.
void OtherKey();
// After CPad::UpdatePads: while the key is held, the mouse wheel turns the dial and the weapons stay as they are.
void AfterPadUpdate();

}  // namespace coop::headphones
