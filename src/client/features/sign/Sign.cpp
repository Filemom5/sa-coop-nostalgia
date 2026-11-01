// NOSTALGIA in place of VINEWOOD: the sign on the hill (models VineSign1_cunte01, the letters' fronts; VineSign1_cunte,
// their backs and frames; LODVineSign1_cunte, far away) is made anew from the game's own as they load, by the recipe
// in SignData.h (SignRecipe.cpp; made by tools/sign from the game's files): N, O and I are the game's own letters, moved;
// S, T, A, L and G are ours in the same typeface, with the same textures, frames and colours of day and night. The
// collision of the two is ours too. Everyone with the mod sees it; another mod's sign is left as it is.
//
// The models: CFileLoader::LoadAtomicFile gets a memory stream of the model's file (from gta3.img); for the sign's,
// it gets one of ours instead. The collision: CFileLoader::LoadCollisionModelVer3 reads ours in place of theirs (at
// the start, for the bounds, and whenever the area's collision loads).
#include <cstdint>
#include <vector>

#include "common/lib/log/Log.h"
#include "features/sign/SignRecipe.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop::sign {

namespace {
using namespace game;

using LoadAtomicFn = bool(__cdecl*)(void* stream, uint32_t modelId);
using LoadColFn = void(__cdecl*)(const uint8_t* data, uint32_t size, void* colModel, const char* name);
LoadAtomicFn g_origLoadAtomic = nullptr;
LoadColFn g_origLoadCol = nullptr;

constexpr int kStreamMemory = 3, kStreamRead = 1;  // rwSTREAMMEMORY, rwSTREAMREAD
struct RwMemory {
    const uint8_t* start;
    uint32_t length;
};

bool __cdecl HookLoadAtomicFile(void* stream, uint32_t modelId) {
    if (IsSignModel(modelId) && stream && *(int32_t*)stream == kStreamMemory) {
        auto* s = (uint8_t*)stream;  // RwStream: type, access, position, then the memory's position, size, block
        uint32_t pos = *(uint32_t*)(s + 12), size = *(uint32_t*)(s + 16);
        const uint8_t* block = *(const uint8_t**)(s + 20);
        if (block && pos < size) {
            std::vector<uint8_t> ours = Compose(modelId, block + pos, size - pos);
            if (!ours.empty()) {
                RwMemory memory{ours.data(), (uint32_t)ours.size()};
                void* mine = Call<0x7ECEF0, void*>(kStreamMemory, kStreamRead, (const void*)&memory);  // RwStreamOpen
                if (mine) {
                    bool ok = g_origLoadAtomic(mine, modelId);
                    Call<0x7ECE20, int>(mine, (void*)&memory);  // RwStreamClose
                    LOGI("Sign: NOSTALGIA (model %u, %s)", modelId, ok ? "loaded" : "not loaded");
                    if (ok) return true;
                }
            }
        }
    }
    return g_origLoadAtomic(stream, modelId);
}

void __cdecl HookLoadCollisionModelVer3(const uint8_t* data, uint32_t size, void* colModel, const char* name) {
    uint32_t ourSize = 0;
    if (const uint8_t* ours = Collision(name, data, size, &ourSize)) {
        g_origLoadCol(ours, ourSize, colModel, name);
        return;
    }
    g_origLoadCol(data, size, colModel, name);
}

class Sign : public Module {
public:
    const char* Name() const override { return "Sign"; }
    void OnInstallHooks() override {
        mem::HookFn(0x5371F0, &HookLoadAtomicFile, &g_origLoadAtomic, "CFileLoader::LoadAtomicFile");
        mem::HookFn(0x537CE0, &HookLoadCollisionModelVer3, &g_origLoadCol, "CFileLoader::LoadCollisionModelVer3");
    }
    static Sign& Instance() {
        static Sign m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Sign::Instance()); }
} g_reg;
}  // namespace

}  // namespace coop::sign
