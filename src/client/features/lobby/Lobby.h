#pragma once
// The group's open servers, without a code to type: the host's game says on a channel of its own (ntfy.sh, as the
// invite codes' signalling) that its server is open, with its code, its name and how many play; the F9 menu of the
// others lists them with a button to join.
#include <string>
#include <vector>

namespace coop::lobby {

struct Server {
    std::string name, code;
    int players = 0, max = 0;
};

// the servers open now (ours aside). The list is fetched in the background while something asks for it (the F9 menu,
// every frame it shows): empty at first.
std::vector<Server> OpenServers();

}  // namespace coop::lobby
