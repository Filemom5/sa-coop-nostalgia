#include "shared/game/ModelSwap.h"

#include "common/lib/log/Log.h"
#include "shared/game/Game.h"
#include "shared/lib/memory/Memory.h"

namespace game {

namespace {

using LoadAtomicFn = bool(__cdecl*)(void* stream, uint32_t modelId);
using LoadColFn = void(__cdecl*)(const uint8_t* data, uint32_t size, void* colModel, const char* name);
LoadAtomicFn g_origLoadAtomic = nullptr;
LoadColFn g_origLoadCol = nullptr;
std::vector<ModelSwap> g_models;
std::vector<CollisionSwap> g_cols;

constexpr int kStreamMemory = 3, kStreamRead = 1;  // rwSTREAMMEMORY, rwSTREAMREAD
struct RwMemory {
    const uint8_t* start;
    uint32_t length;
};

bool __cdecl HookLoadAtomicFile(void* stream, uint32_t modelId) {
    if (stream && *(int32_t*)stream == kStreamMemory) {
        auto* s = (uint8_t*)stream;  // RwStream: type, access, position, then the memory's position, size, block
        uint32_t pos = *(uint32_t*)(s + 12), size = *(uint32_t*)(s + 16);
        const uint8_t* block = *(const uint8_t**)(s + 20);
        if (block && pos < size) {
            for (ModelSwap swap : g_models) {
                std::vector<uint8_t> ours = swap(modelId, block + pos, size - pos);
                if (ours.empty()) continue;
                RwMemory memory{ours.data(), (uint32_t)ours.size()};
                void* mine = Call<0x7ECEF0, void*>(kStreamMemory, kStreamRead, (const void*)&memory);  // RwStreamOpen
                if (!mine) break;
                bool ok = g_origLoadAtomic(mine, modelId);
                Call<0x7ECE20, int>(mine, (void*)&memory);  // RwStreamClose
                LOGI("ModelSwap: model %u, ours (%zu bytes) %s", modelId, ours.size(), ok ? "loaded" : "not loaded");
                if (ok) return true;
                break;
            }
        }
    }
    return g_origLoadAtomic(stream, modelId);
}

void __cdecl HookLoadCollisionModelVer3(const uint8_t* data, uint32_t size, void* colModel, const char* name) {
    for (CollisionSwap swap : g_cols) {
        uint32_t ourSize = 0;
        if (const uint8_t* ours = swap(name, data, size, &ourSize)) {
            g_origLoadCol(ours, ourSize, colModel, name);
            return;
        }
    }
    g_origLoadCol(data, size, colModel, name);
}

void Install() {
    static bool done = false;
    if (done) return;
    done = true;
    coop::mem::HookFn(0x5371F0, &HookLoadAtomicFile, &g_origLoadAtomic, "CFileLoader::LoadAtomicFile");
    coop::mem::HookFn(0x537CE0, &HookLoadCollisionModelVer3, &g_origLoadCol, "CFileLoader::LoadCollisionModelVer3");
}

}  // namespace

void AddModelSwap(ModelSwap swap) {
    Install();
    g_models.push_back(swap);
}

void AddCollisionSwap(CollisionSwap swap) {
    Install();
    g_cols.push_back(swap);
}

}  // namespace game
