// The headphones on the head. The game has no model of them, so ours (HeadsetModel.h, made by tools/headset_model.py:
// a band and two cups, dark with pink outsides) is drawn at the head bone of each player who wears them, the way the
// game draws a weapon at a hand (CVisibilityPlugins::RenderWeaponPedsForPC): the bone's matrix into the model's frame,
// lit as the ped, rendered. Ours comes right after that one, in the same render state: the frame's call of it
// (in the scene's render, after RenderScene) is redirected, not the function, which SilentPatch replaces with its own.
//
// Putting them on or taking them off plays the night vision goggles' animation on the upper body (GOGGLES,
// goggles_put_on: both hands to the head), and the headset shows or goes as the hands get there. In a vehicle with a
// radio they are off the head (its radio plays). The other players are told (G_RADIO OP_HEADSET, Radio.cpp) and see them
// on our copy.
//
// /fone x y z rx ry rz: moves the model on the head (metres along the head bone's axes) and turns it (degrees about
// them), to fit it in a test; /fone alone shows the values in use.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "features/radio/HeadsetModel.h"
#include "features/radio/Headphones.h"
#include "features/radio/Radio.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Anim.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/chat/Chat.h"

namespace coop::headphones {

namespace {
using namespace game;
using namespace game::script;

constexpr uintptr_t kCallRenderWeaponPeds = 0x53EAC4;  // the frame's call of CVisibilityPlugins::RenderWeaponPedsForPC
constexpr int kBoneHead = 5;                       // BONE_HEAD
constexpr uint32_t kCombinePreConcat = 1;          // rwCOMBINEPRECONCAT
constexpr uint64_t kHandsUpMs = 700;               // the hands reach the head this far into the animation
constexpr uint64_t kTellEveryMs = 5000;            // while worn, the others are told again this often
constexpr uint64_t kRemoteFreshMs = 12000;         // a copy's headset not told of again for this long goes
constexpr uint64_t kAnimWaitMs = 1500;             // the animation's block not here by then: no animation
constexpr float kDrawDistance = 80.0f;

struct RwMemory {
    const uint8_t* start;
    uint32_t length;
};
struct RwV3d {
    float x, y, z;
};
struct Fit {
    float x = 0, y = 0, z = 0;     // metres, along the head bone's axes
    float rx = 0, ry = 0, rz = 0;  // degrees, about them
};

void* g_model = nullptr;  // RpClump
bool g_modelFailed = false;
Fit g_fit;
bool g_wasOn = false;
uint64_t g_showFromMs = 0;   // put on: the headset shows from then (the hands at the head)
uint64_t g_hideFromMs = 0;   // taken off: it goes then
bool g_shown = false;        // on our player's head now
bool g_told = false;         // the others have it as shown
uint64_t g_tellMs = 0;
bool g_animPending = false;  // the animation waits for its block
uint64_t g_animAskMs = 0;
std::map<uint8_t, uint64_t> g_remote;  // the players whose copies wear them, and when that was last told

using VoidFn = void(__cdecl*)();
VoidFn g_origRender = nullptr;

void* Model() {
    if (g_model || g_modelFailed) return g_model;
    RwMemory memory{kHeadsetDff, (uint32_t)sizeof(kHeadsetDff)};
    void* stream = Call<0x7ECEF0, void*>(3, 1, (const void*)&memory);  // RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD)
    if (stream) {
        if (Call<0x7ED2D0, int>(stream, 0x10u, (uint32_t*)nullptr, (uint32_t*)nullptr))  // RwStreamFindChunk(rwID_CLUMP)
            g_model = Call<0x74B420, void*>(stream);                                        // RpClumpStreamRead
        Call<0x7ECE20, int>(stream, (void*)nullptr);                                       // RwStreamClose
    }
    if (g_model) {
        LOGI("Headphones: the headset's model is ready");
    } else {
        g_modelFailed = true;
        LOGW("Headphones: the headset's model could not be read");
    }
    return g_model;
}

// the headset at a ped's head, lit as the ped
void DrawOn(CPed* ped) {
    void* clump = ped ? ped->RwObject() : nullptr;
    if (!clump || !ped->IsVisible()) return;
    CPlayerPed* me = FindPlayerPed(0);
    if (me && me != ped) {
        CVector a = ped->GetPosition(), b = me->GetPosition();
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        if (dx * dx + dy * dy + dz * dz > kDrawDistance * kDrawDistance) return;
    }
    if (!CallMethod<0x534540, bool>(ped)) return;  // CEntity::GetIsOnScreen: its bones are this frame's
    void* hierarchy = Call<0x734A40, void*>(clump);  // GetAnimHierarchyFromSkinClump
    if (!hierarchy) return;
    int index = Call<0x7C51A0, int>(hierarchy, kBoneHead);  // RpHAnimIDGetIndex
    uint8_t* bones = Call<0x7C5120, uint8_t*>(hierarchy);   // RpHAnimHierarchyGetMatrixArray
    if (index < 0 || !bones) return;
    uint8_t* frame = Field<uint8_t*>(Model(), 4);  // RpClumpGetFrame: the clump's parent
    uint8_t* matrix = frame + 0x10;                // RwFrameGetMatrix: its modelling matrix
    std::memcpy(matrix, bones + 64 * index, 64);   // the head bone's, in the world
    RwV3d move{g_fit.x, g_fit.y, g_fit.z};
    Call<0x7F2450, void*>(matrix, &move, kCombinePreConcat);  // RwMatrixTranslate
    static const RwV3d kAxes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float turns[3] = {g_fit.rx, g_fit.ry, g_fit.rz};
    for (int i = 0; i < 3; ++i)
        if (turns[i] != 0) Call<0x7F1FD0, void*>(matrix, &kAxes[i], turns[i], kCombinePreConcat);  // RwMatrixRotate
    Call<0x7F0910, void*>(frame);    // RwFrameUpdateObjects
    bool lit = CallMethod<0x553F00, bool>(ped);  // CPed::SetupLighting
    Call<0x749B20, void*>(Model());             // RpClumpRender
    CallMethod<0x5533B0>(ped, (uint8_t)lit);    // CPed::RemoveLighting(bool): it takes what SetupLighting gave (ret 4)
}

void __cdecl HookRenderWeaponPeds() {
    g_origRender();
    if ((!g_shown && g_remote.empty()) || !core::IsPlayerReady() || !Model()) return;
    if (g_shown) DrawOn(FindPlayerPed(0));
    for (const auto& [id, ms] : g_remote) {
        RemotePlayer* rp = Players::Instance().Get(id);
        if (rp && rp->ped) DrawOn(rp->ped);
    }
}

// an IFP block in memory, else asked for
bool BlockReady(const char* block) {
    int idx = Call<0x4D3990, int>(block);  // CAnimManager::GetAnimationBlockIndex
    if (idx < 0) return false;
    if (anim::BlockLoaded(idx)) return true;
    RequestModel(anim::kIfpBase + idx, 2);
    return false;
}

// both hands to the head, on the upper body only: walking goes on
void PlayHandsToHead() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me || me->InVehicle()) return;
    if (!BlockReady("goggles")) {
        if (!g_animPending) g_animAskMs = NowMs();
        g_animPending = true;
        return;
    }
    g_animPending = false;
    Cmd(0x0A1A).I(GetPedRef(me)).S("goggles_put_on").S("GOGGLES").F(4.0f).I(0).I(0).I(0).I(0).I(-1).Run();  // TASK_PLAY_ANIM_SECONDARY
}

void ShowFit() {
    ui::TheChat().AddSystem(Format("/fone %.3f %.3f %.3f %.0f %.0f %.0f", g_fit.x, g_fit.y, g_fit.z, g_fit.rx, g_fit.ry, g_fit.rz));
}

class Headset : public Module {
public:
    const char* Name() const override { return "Headset"; }
    void OnInstallHooks() override {
        if (!mem::RedirectCallFn(kCallRenderWeaponPeds, &HookRenderWeaponPeds, &g_origRender))
            LOGW("Headphones: the scene's render is not as expected: no headset on the head");
    }
    void OnSessionEnd() override {
        g_remote.clear();
        g_told = false;
    }
    void OnPeerLeave(uint8_t id) override { g_remote.erase(id); }

    void OnPostProcess() override {
        uint64_t now = NowMs();
        for (auto it = g_remote.begin(); it != g_remote.end();)
            it = now - it->second > kRemoteFreshMs ? g_remote.erase(it) : std::next(it);
        bool on = On();
        if (on != g_wasOn) {  // the key: the hands go to the head, the headset comes or goes when they get there
            g_wasOn = on;
            CPlayerPed* me = FindPlayerPed(0);
            bool afoot = me && !me->InVehicle();
            if (afoot) PlayHandsToHead();
            if (on) g_showFromMs = afoot ? now + kHandsUpMs : now;
            else g_hideFromMs = afoot ? now + kHandsUpMs : now;
        }
        if (g_animPending) {
            if (now - g_animAskMs > kAnimWaitMs) g_animPending = false;
            else PlayHandsToHead();
        }
        CPlayerPed* me = FindPlayerPed(0);
        bool shown = Worn() ? now >= g_showFromMs : (now < g_hideFromMs && me && !me->InVehicle() && core::IsPlayerReady());
        if (shown != g_shown) g_shown = shown;
        if (!Net().Online()) {
            g_told = false;
            return;
        }
        if (g_shown != g_told || (g_shown && now - g_tellMs >= kTellEveryMs)) {
            radio::SendHeadset(g_shown);
            g_told = g_shown;
            g_tellMs = now;
        }
    }

    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "fone") return false;
        if (args.size() >= 3) {
            float* fields[6] = {&g_fit.x, &g_fit.y, &g_fit.z, &g_fit.rx, &g_fit.ry, &g_fit.rz};
            for (size_t i = 0; i < args.size() && i < 6; ++i) *fields[i] = (float)std::atof(args[i].c_str());
        }
        ShowFit();
        return true;
    }

    static Headset& Instance() {
        static Headset m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Headset::Instance()); }
} g_reg;
}  // namespace

void RemoteHeadset(uint8_t id, bool on) {
    if (on) g_remote[id] = NowMs();
    else g_remote.erase(id);
}

}  // namespace coop::headphones
