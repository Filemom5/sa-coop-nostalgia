#pragma once
// Each player's own houses and cars (Home.cpp): /casa, /home.
#include <string>

namespace coop::home {

// Y or N on a purchase or a removal waiting for it: true when there was one
bool Key(bool yes);
// that question in the middle of the screen (hint: the keys), empty when there is none
std::string Text(std::string& hint);

}  // namespace coop::home
