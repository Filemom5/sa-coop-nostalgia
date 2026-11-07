#pragma once
#include <string>
#include <vector>

namespace coop::mods {

// The mods in the game's modloader folder (each a folder there; the ones whose name starts with a dot are modloader's own),
// on or off: off is its name in modloader.ini's [Profiles.Default.IgnoreMods], as modloader itself reads it at the game's
// start. A change takes effect when the game starts again. Locked: the ones the game needs (_ESSENTIALS: SilentPatch,
// the widescreen fix...) stay on.
struct Mod {
    std::string name;
    bool on = true;
    bool locked = false;
};
std::vector<Mod> List();
bool Toggle(const std::string& name, std::string& error);  // on <-> off
bool Changed();                                            // since the game started: a restart applies it

}  // namespace coop::mods
