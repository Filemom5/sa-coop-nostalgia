#pragma once

namespace coop::wheel {

// The weapon wheel (WeaponWheel.cpp): its key went down (keys/weapon_wheel in coop.ini, Q). On foot it opens; true then.
bool KeyDown();
// After CPad::UpdatePads: while the wheel is open the mouse moves its pointer, not the camera, and neither the mouse
// wheel nor the fire button reaches the weapons.
void AfterPadUpdate();

}  // namespace coop::wheel
