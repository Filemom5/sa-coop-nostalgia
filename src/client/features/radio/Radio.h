#pragma once

namespace coop::radio {

// Our player turned the car radio's dial: CAERadioTrackManager::CheckForStationRetune heard its key (+1 the next
// station, -1 the last one). Called from the key's hook in Players.cpp, before the game moves the dial.
void StationKey(int delta);

}  // namespace coop::radio
