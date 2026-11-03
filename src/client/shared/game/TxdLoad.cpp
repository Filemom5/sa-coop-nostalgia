#include "shared/game/TxdLoad.h"

#include <vector>

#include "shared/game/Game.h"
#include "shared/lib/memory/Memory.h"

namespace game {

namespace {

constexpr uintptr_t kTxdPool = 0xC8800C;  // CTxdStore::ms_pTxdPool

std::vector<TxdLoaded> g_callbacks;

void Loaded(int index) {
    auto* pool = Ref<CPoolHeader*>(kTxdPool);
    if (!pool || index < 0 || index >= pool->size || pool->byteMap[index] < 0) return;
    void* dict = Ref<void*>((uintptr_t)pool->objects + (uintptr_t)index * 0xC);  // TxdDef::m_pRwDictionary
    if (!dict) return;
    for (TxdLoaded callback : g_callbacks) callback(index, dict);
}

using LoadTxdFn = bool(__cdecl*)(int index, void* stream);
LoadTxdFn g_origLoadTxd = nullptr, g_origFinishLoadTxd = nullptr;
bool __cdecl HookLoadTxd(int index, void* stream) {
    bool ok = g_origLoadTxd(index, stream);
    if (ok) Loaded(index);
    return ok;
}
bool __cdecl HookFinishLoadTxd(int index, void* stream) {
    bool ok = g_origFinishLoadTxd(index, stream);
    if (ok) Loaded(index);
    return ok;
}

}  // namespace

void AddTxdLoaded(TxdLoaded callback) {
    if (g_callbacks.empty()) {
        coop::mem::HookFn(0x731DD0, &HookLoadTxd, &g_origLoadTxd, "CTxdStore::LoadTxd");
        coop::mem::HookFn(0x731E40, &HookFinishLoadTxd, &g_origFinishLoadTxd, "CTxdStore::FinishLoadTxd");
    }
    g_callbacks.push_back(callback);
}

}  // namespace game
