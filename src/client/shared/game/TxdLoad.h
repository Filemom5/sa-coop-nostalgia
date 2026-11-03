#pragma once
// A texture dictionary (TXD) of the game has just loaded (CTxdStore::LoadTxd, or FinishLoadTxd for a big one read in
// two goes): features/clothes paints its jackets over the game's textures there, features/charlie adds the house's.
// The models that use the dictionary load after it, so what is done here is what they find.

namespace game {

// index: the TXD's slot (CTxdStore); dict: its RwTexDictionary
using TxdLoaded = void (*)(int index, void* dict);

// From a module's OnInstallHooks (the first one installs the hooks).
void AddTxdLoaded(TxdLoaded callback);

}  // namespace game
