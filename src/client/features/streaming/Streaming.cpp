// The memory the game keeps the world it streams in (models and textures of the buildings, cars and people around)
// in: CStreaming::ms_memoryAvailable. CStreaming::Init2 sets 50 MB (stream.ini's "memory" is read before and lost), a
// size of 2004. When it is full, the game drops what it has to load the rest, and loads late: buildings and roads that
// come in late, cars and people that vanish, blurred textures. With four players, 500 people and cars (the limit
// adjuster) and bigger textures (cars' paint jobs), it is full all the time.
//
// So it is set higher once the game set its own: 512 MB when the game may use 4 GB (gta_sa.exe marked large address
// aware, on a 64-bit Windows) and the computer has the memory, less otherwise. [game] streaming_memory in coop.ini (MB)
// sets it by hand (0: as here).
#include <windows.h>

#include <algorithm>
#include <cstdint>

#include "common/lib/log/Log.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop {

namespace {
using namespace game;

constexpr uintptr_t kMemoryAvailable = 0x8A5A80;  // CStreaming::ms_memoryAvailable (bytes)

uint32_t WantedMb() {
    int mb = Cfg().streamingMemoryMb;
    if (mb <= 0) {
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof ms;
        GlobalMemoryStatusEx(&ms);
        bool wide = ms.ullTotalVirtual > (3ull << 30);  // 4 GB for the game, not 2
        mb = !wide ? 256 : ms.ullTotalPhys >= (6ull << 30) ? 512 : 384;
    }
    return (uint32_t)std::clamp(mb, 50, 1024);
}

void Apply() {
    uint32_t want = WantedMb() << 20;
    uint32_t& available = Ref<uint32_t>(kMemoryAvailable);
    if (available >= want) return;
    LOGI("Streaming: memory for the world %u MB (the game set %u MB)", want >> 20, available >> 20);
    available = want;
}

using InitFn = void(__cdecl*)();
InitFn g_origInit2 = nullptr;
void __cdecl HookInit2() {
    g_origInit2();
    Apply();
}

class Streaming : public Module {
public:
    const char* Name() const override { return "Streaming"; }
    void OnInstallHooks() override { mem::HookFn(0x5B8AD0, &HookInit2, &g_origInit2, "CStreaming::Init2"); }
    void OnGameStarted() override { Apply(); }
    static Streaming& Instance() {
        static Streaming m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Streaming::Instance()); }
} g_reg;
}  // namespace

}  // namespace coop
