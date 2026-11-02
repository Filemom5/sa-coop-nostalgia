#pragma once
// Basketball for the group at any hoop of the map (Basketball.cpp, docs/basquete.md): /basquete.
#include <string>

namespace coop::basket {

// Y or N on an invitation to a game: true when there was one
bool Key(bool yes);
// the invitation in the middle of the screen (hint: the keys), empty when there is none
std::string Text(std::string& hint);

}  // namespace coop::basket
