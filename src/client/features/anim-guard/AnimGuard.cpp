// A guard against a crash of the game itself: a ped (or any animated clump) left with no animation at all.
//
// RpAnimBlendClumpUpdateAnimations (0x4D34F0) lists the first node of every animation that plays (AnimBlendFrameUpdate
// Data: i32 flags, then the nodes, ended by a null) and hands the list to a frame callback through
// CAnimBlendClumpData::ForAllFrames (0x4CF190), from five places: FrameUpdateCallBackSkinned and NonSkinned, their
// offscreen kinds and the one with velocity extraction. Every one of them reads the list's first node without looking,
// so an empty list crashed the game: 0x4D1750 (velocity extraction, the crash of 30.10), and the same at 0x4D1B34,
// 0x4D2C62 and 0x4D2EF7. With no animation there is nothing to move: the bones keep their pose for that frame, and
// whoever left the ped bare (a mirror's ended action, Peds.cpp) gives it its stance again.
#include <cstdint>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop {

namespace {

// CAnimBlendClumpData::ForAllFrames(callback, data), thiscall
using ForAllFramesFn = void(__fastcall*)(void* clumpData, void* edx, void* callback, void* data);

// its calls in RpAnimBlendClumpUpdateAnimations
constexpr uintptr_t kSites[] = {0x4D3664, 0x4D367A, 0x4D36D0, 0x4D36EB, 0x4D3706};
constexpr int kSiteCount = sizeof(kSites) / sizeof(kSites[0]);
ForAllFramesFn g_orig[kSiteCount] = {};

uint32_t g_skipped = 0;
uint64_t g_loggedMs = 0;

template <int N>
void __fastcall HookFrames(void* clumpData, void* edx, void* callback, void* data) {
    if (!game::Field<void*>(data, 4)) {  // AnimBlendFrameUpdateData::nodes[0]: no animation plays
        ++g_skipped;
        return;
    }
    g_orig[N](clumpData, edx, callback, data);
}

template <int N>
bool Redirect() {
    return mem::RedirectCallFn(kSites[N], &HookFrames<N>, &g_orig[N]);
}

class AnimGuard : public Module {
public:
    const char* Name() const override { return "AnimGuard"; }

    void OnInstallHooks() override {
        bool ok[kSiteCount] = {Redirect<0>(), Redirect<1>(), Redirect<2>(), Redirect<3>(), Redirect<4>()};
        int n = 0;
        for (bool b : ok) n += b ? 1 : 0;
        if (n == kSiteCount) LOGI("AnimGuard: frame updates of clumps with no animation are skipped");
        else LOGW("AnimGuard: %d of %d frame update calls guarded (another mod changed the rest?)", n, kSiteCount);
    }

    void OnPostProcess() override {
        if (!g_skipped) return;
        uint64_t now = NowMs();
        if (now - g_loggedMs < 10000) return;
        LOGW("AnimGuard: skipped %u frame updates of a ped with no animation (the game would have crashed)", g_skipped);
        g_skipped = 0;
        g_loggedMs = now;
    }

    static AnimGuard& Instance() {
        static AnimGuard m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&AnimGuard::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop
