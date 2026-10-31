// Downed instead of dead, as in Left 4 Dead: a player whose wound would kill him falls and lies wounded on the ground
// for 25 s. A friend who holds the enter key (F, the pad's triangle) beside him for 4 s brings him back up with some
// health (the medics' CPR, then the game's get-up); nobody in time, or everyone down, and he dies as before (the hospital).
//
// The wound that would kill is caught where the game decides it (CPedDamageResponseCalculator::ComputeWillKillPed): the
// player keeps 1 HP instead and goes down after the frame. Not in a vehicle, in the water (drowning) or a cutscene, and
// only with another player in the game to come and help. Lying down the player is proof against everything, and the pad
// only turns the camera. Near a downed friend the enter key belongs to the revive and takes no car.
//
// G_DOWNED, reliable: u8 op, u8 player (255 none), i32 value.
//  * OP_DOWN: the sender is down, value its ms left (again every 3 s while down).
//  * OP_UP: the sender is up again, player the one who revived it.
//  * OP_GONE: the sender is no longer down without a revive (it died, was busted, left the game).
//  * OP_REVIVE_START / OP_REVIVE_CANCEL / OP_REVIVE_DONE: the sender begins, gives up or finishes reviving that player.
#include "features/downed/Downed.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>

#include "common/api/protocol/Protocol.h"
#include "common/lib/buffer/Buffer.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Anim.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/chat/Chat.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::downed {

namespace {
using namespace game;
using namespace game::script;

enum : uint8_t { OP_DOWN = 1, OP_UP = 2, OP_GONE = 3, OP_REVIVE_START = 4, OP_REVIVE_CANCEL = 5, OP_REVIVE_DONE = 6 };
constexpr uint8_t kNobody = 0xFF;

constexpr int32_t kBleedOutMs = 25000;
constexpr uint64_t kReviveMs = 4000;
constexpr uint64_t kResendMs = 3000;
constexpr uint64_t kAnimCheckMs = 1000;
constexpr float kReviveRange = 1.8f;    // metres between the reviver and the downed one (flat)
constexpr float kReviveHeight = 1.6f;
constexpr float kRevivedHealth = 0.3f;  // of the maximum
constexpr uint64_t kFreshMs = 3000;     // a player whose state came this recently is in the game
constexpr int kWeaponDrowning = 53;
constexpr uint32_t kProofBits = CPhysical::PF_BULLET_PROOF | CPhysical::PF_FIRE_PROOF | CPhysical::PF_COLLISION_PROOF |
                                CPhysical::PF_MELEE_PROOF | CPhysical::PF_EXPLOSION_PROOF;

// our player
struct Down {
    bool on = false;
    int32_t leftMs = 0;       // to bleeding out (stands still while someone revives)
    uint64_t lastTickMs = 0;
    uint64_t sentMs = 0;
    uint64_t animMs = 0;
    uint8_t reviver = kNobody;
    uint32_t proofsWere = 0;  // the proof bits before going down
};
Down g_down;
bool g_downPending = false;   // the wound of this frame: down after it
bool g_allowDeath = false;    // our own bleeding out kills for real

// the other players who are down, by their own word
struct Other {
    int32_t leftMs = 0;
    uint64_t recvMs = 0;
    uint8_t reviver = kNobody;
    uint64_t animMs = 0;
};
std::map<uint8_t, Other> g_others;

// our reviving
uint8_t g_target = kNobody;   // the downed friend in reach (at the last frame)
bool g_enterHeld = false;     // the enter key, taken from the pad near a downed friend
uint8_t g_reviving = kNobody;
uint64_t g_reviveStartMs = 0;

void Send(uint8_t op, uint8_t player, int32_t value = 0) {
    if (!Net().Online()) return;
    Writer w(proto::G_DOWNED);
    w.U8(op);
    w.U8(player);
    w.I32(value);
    Net().Send(proto::CH_RELIABLE, w);
}

bool Dead(CPed* p) { return p->Health() <= 0.0f || p->PedState() == 54 || p->PedState() == 55; }  // DIE, DEAD
bool CutsceneRunning() { return Ref<uint8_t>(0xB5F851) != 0; }  // CCutsceneMgr::ms_running

// the other players in the game with us (their states come)
int OthersInGame() {
    int n = 0;
    uint64_t now = NowMs();
    for (const auto& [id, rp] : Players::Instance().All())
        if (rp.hasState && rp.ped && now - rp.stateRecvMs < kFreshMs) ++n;
    return n;
}

bool EveryoneElseDown() {
    uint64_t now = NowMs();
    for (const auto& [id, rp] : Players::Instance().All())
        if (rp.hasState && rp.ped && now - rp.stateRecvMs < kFreshMs && !g_others.count(id)) return false;
    return true;
}

// an IFP block in memory, else asked for (Anims.cpp does the same)
bool BlockReady(const char* block) {
    int idx = Call<0x4D3990, int>(block);  // CAnimManager::GetAnimationBlockIndex
    if (idx < 0) return false;
    if (anim::BlockLoaded(idx)) return true;
    RequestModel(anim::kIfpBase + idx, 2);
    return false;
}

// lying wounded: Sweet's own (SWEET), or the ground's after a fall (PED, always in memory) until that one is here
void LieDown(int handle) {
    if (BlockReady("sweet")) {
        if (!Cmd(0x0611).I(handle).S("Sweet_injuredloop").Run())  // IS_CHAR_PLAYING_ANIM
            Cmd(0x0812).I(handle).S("Sweet_injuredloop").S("SWEET").F(4.0f).I(1).I(0).I(0).I(1).I(-1).Run();  // TASK_PLAY_ANIM_NON_INTERRUPTABLE
    } else if (!Cmd(0x0611).I(handle).S("FLOOR_hit").Run()) {
        Cmd(0x0812).I(handle).S("FLOOR_hit").S("PED").F(4.0f).I(1).I(0).I(0).I(1).I(-1).Run();
    }
}

void GetUpAnim(int handle) {
    Cmd(0x0792).I(handle).Run();  // CLEAR_CHAR_TASKS_IMMEDIATELY: off the wounded loop
    Cmd(0x0605).I(handle).S("getup").S("PED").F(4.0f).I(0).I(0).I(0).I(0).I(-1).Run();  // TASK_PLAY_ANIM, once
}

void EndDown(CPlayerPed* me) {
    if (me) {
        me->PhysicalFlags() = (me->PhysicalFlags() & ~kProofBits) | g_down.proofsWere;
        Cmd(0x0792).I(GetPedRef(me)).Run();  // CLEAR_CHAR_TASKS_IMMEDIATELY
    }
    g_down = Down{};
}

void GoDown(CPlayerPed* me) {
    g_down = Down{};
    g_down.on = true;
    g_down.leftMs = kBleedOutMs;
    g_down.lastTickMs = g_down.sentMs = NowMs();
    g_down.proofsWere = me->PhysicalFlags() & kProofBits;
    me->PhysicalFlags() |= kProofBits;
    me->Health() = std::max(me->Health(), 1.0f);
    LieDown(GetPedRef(me));
    g_down.animMs = NowMs();
    Send(OP_DOWN, kNobody, g_down.leftMs);
    ui::TheChat().AddSystem(TR("Вы ранены! Ждите, пока вас поднимут.", "You are down! Wait for a friend to revive you."));
    LOGI("Downed: our player is down (%d s to bleed out)", kBleedOutMs / 1000);
}

void Revived(CPlayerPed* me, uint8_t by) {
    int handle = GetPedRef(me);
    EndDown(me);
    me->Health() = std::max(me->Health(), me->MaxHealth() * kRevivedHealth);
    GetUpAnim(handle);
    Send(OP_UP, by);
    ui::TheChat().AddSystem(Format(TR("%s поднял вас.", "%s revived you."), Net().PeerName(by).c_str()));
    LOGI("Downed: revived by player %d", by);
}

void BleedOut(CPlayerPed* me, const char* why) {
    int handle = GetPedRef(me);
    EndDown(me);
    Send(OP_GONE, kNobody);
    LOGI("Downed: our player dies (%s)", why);
    g_allowDeath = true;
    me->Health() = 0.0f;
    Cmd(0x05BE).I(handle).Run();  // TASK_DIE
    g_allowDeath = false;
}

// --- the wound that would kill -------------------------------------------------------------------------------------

using WillKillFn = void(__fastcall*)(void* calc, void* edx, CPed* ped, void* rsp, bool speak);
WillKillFn g_origWillKill = nullptr;
void __fastcall HookWillKill(void* calc, void* edx, CPed* ped, void* rsp, bool speak) {
    float before = ped ? ped->Health() : 0.0f;
    bool dyingBefore = ped && Dead(ped);  // bleeding out already (TASK_DIE): a later shot must not bring it back
    g_origWillKill(calc, edx, ped, rsp, speak);
    CPlayerPed* me = FindPlayerPed(0);
    if (!ped || ped != me || g_allowDeath || dyingBefore) return;
    bool& healthZero = Field<bool>(rsp, 8);   // CPedDamageResponse::m_bHealthZero
    bool& forceDeath = Field<bool>(rsp, 9);   // ::m_bForceDeath
    if (!healthZero) return;
    int weapon = Field<int32_t>(calc, 0x0C);  // CPedDamageResponseCalculator::m_weaponType
    bool canGoDown = g_down.on || (Net().Online() && !me->InVehicle() && weapon != kWeaponDrowning && !CutsceneRunning() &&
                                   PlayerInfos()[0].playerState == 0 && OthersInGame() > 0);
    if (!canGoDown) return;
    healthZero = forceDeath = false;
    Field<float>(rsp, 0) = std::max(0.0f, before - 1.0f);  // m_fDamageHealth
    ped->Health() = 1.0f;
    if (!g_down.on) g_downPending = true;
}

// --- messages ------------------------------------------------------------------------------------------------------

void Handle(uint8_t from, Reader& r) {
    uint8_t op = r.U8();
    uint8_t player = r.U8();
    int32_t value = r.I32();
    if (!r.Ok() || from == Net().MyId()) return;
    uint64_t now = NowMs();
    RemotePlayer* rp = Players::Instance().Get(from);
    switch (op) {
        case OP_DOWN: {
            bool fresh = !g_others.count(from);
            Other& o = g_others[from];
            o.leftMs = value;
            o.recvMs = now;
            if (fresh) {
                ui::TheChat().AddSystem(Format(TR("%s ранен! Подойдите и удерживайте F, чтобы поднять.",
                                                  "%s is down! Go and hold F to revive them."),
                                               Net().PeerName(from).c_str()));
                if (rp && rp->ped) LieDown(rp->pedHandle), o.animMs = now;
            }
            break;
        }
        case OP_UP:
            g_others.erase(from);
            if (rp && rp->ped) GetUpAnim(rp->pedHandle);
            if (player != Net().MyId())
                ui::TheChat().AddSystem(Format(TR("%s поднял игрока %s.", "%s revived %s."), Net().PeerName(player).c_str(),
                                               Net().PeerName(from).c_str()));
            break;
        case OP_GONE:
            g_others.erase(from);
            break;
        case OP_REVIVE_START:
        case OP_REVIVE_CANCEL:
            if (auto it = g_others.find(player); it != g_others.end())
                it->second.reviver = op == OP_REVIVE_START ? from : kNobody;
            if (player != Net().MyId() || !g_down.on) break;
            if (op == OP_REVIVE_START) g_down.reviver = from;
            else if (g_down.reviver == from) g_down.reviver = kNobody;
            break;
        case OP_REVIVE_DONE:
            if (player == Net().MyId() && g_down.on && core::IsPlayerReady()) Revived(FindPlayerPed(0), from);
            break;
    }
}

// --- reviving a friend ---------------------------------------------------------------------------------------------

void StopReviving(bool tell) {
    if (g_reviving == kNobody) return;
    if (tell) Send(OP_REVIVE_CANCEL, g_reviving);
    if (CPlayerPed* me = FindPlayerPed(0)) Cmd(0x0792).I(GetPedRef(me)).Run();  // CLEAR_CHAR_TASKS_IMMEDIATELY: off the CPR
    g_reviving = kNobody;
}

// the downed friend nearest to our player, in reach
uint8_t TargetInReach(CPlayerPed* me) {
    uint8_t best = kNobody;
    float bestDist = kReviveRange;
    CVector at = me->GetPosition();
    for (const auto& [id, o] : g_others) {
        RemotePlayer* rp = Players::Instance().Get(id);
        if (!rp || !rp->ped) continue;
        CVector d = rp->ped->GetPosition() - at;
        float dist = d.Length2D();
        if (dist < bestDist && std::fabs(d.z) < kReviveHeight) best = id, bestDist = dist;
    }
    return best;
}

class Downed : public Module {
public:
    const char* Name() const override { return "Downed"; }

    void OnInstallHooks() override {
        Net().SetHandler(proto::G_DOWNED, &Handle);
        mem::HookFn(0x4B3210, &HookWillKill, &g_origWillKill, "CPedDamageResponseCalculator::ComputeWillKillPed");
    }
    void OnSessionEnd() override {
        if (g_down.on && core::IsPlayerReady()) EndDown(FindPlayerPed(0));
        g_down = Down{};
        g_others.clear();
        g_reviving = g_target = kNobody;
        g_downPending = false;
    }
    void OnGameShutdown() override {
        g_down = Down{};
        g_others.clear();
        g_reviving = g_target = kNobody;
        g_downPending = false;
    }
    void OnPeerLeave(uint8_t id) override {
        g_others.erase(id);
        if (g_reviving == id) StopReviving(false);
        if (g_down.reviver == id) g_down.reviver = kNobody;
    }
    void OnPeerResync(uint8_t id) override { g_others.erase(id); }

    void OnPostProcess() override {
        if (!core::IsPlayerReady()) return;
        CPlayerPed* me = FindPlayerPed(0);
        uint64_t now = NowMs();
        // the others who are down: their copies lie wounded here (and stop when their word stops coming)
        for (auto it = g_others.begin(); it != g_others.end();) {
            RemotePlayer* rp = Players::Instance().Get(it->first);
            if (now - it->second.recvMs > kResendMs * 3 || !rp) {
                it = g_others.erase(it);
                continue;
            }
            if (rp->ped && now - it->second.animMs > kAnimCheckMs) {
                LieDown(rp->pedHandle);
                BlockReady("medic");  // the CPR, ready before anyone kneels down
                it->second.animMs = now;
            }
            ++it;
        }

        if (g_downPending) {
            g_downPending = false;
            if (!g_down.on && !Dead(me)) GoDown(me);
        }
        if (g_down.on) {
            UpdateDown(me, now);
            g_target = kNobody;
            return;
        }
        UpdateReviving(me, now);
    }

    void UpdateDown(CPlayerPed* me, uint64_t now) {
        // gone another way: killed by what the proofs let through, busted, put in a car by a script
        if (Dead(me) || PlayerInfos()[0].playerState != 0 || me->InVehicle()) {
            EndDown(me);
            Send(OP_GONE, kNobody);
            LOGI("Downed: no longer down (dead, busted or in a vehicle)");
            return;
        }
        if (g_down.reviver != kNobody && !Players::Instance().Get(g_down.reviver)) g_down.reviver = kNobody;
        if (g_down.reviver == kNobody) g_down.leftMs -= (int32_t)(now - g_down.lastTickMs);
        g_down.lastTickMs = now;
        if (g_down.leftMs <= 0) return BleedOut(me, "nobody came in time");
        if (!Net().Online() || OthersInGame() == 0) return BleedOut(me, "nobody left to help");
        if (g_down.reviver == kNobody && EveryoneElseDown()) return BleedOut(me, "everyone is down");
        me->Health() = std::max(me->Health(), 1.0f);
        if (now - g_down.animMs > kAnimCheckMs) {
            LieDown(GetPedRef(me));
            g_down.animMs = now;
        }
        if (now - g_down.sentMs > kResendMs) {
            Send(OP_DOWN, kNobody, g_down.leftMs);
            g_down.sentMs = now;
        }
    }

    void UpdateReviving(CPlayerPed* me, uint64_t now) {
        bool able = !Dead(me) && !me->InVehicle() && !CutsceneRunning();
        g_target = able ? TargetInReach(me) : kNobody;
        if (g_reviving != kNobody) {
            if (!g_enterHeld || g_target != g_reviving) return StopReviving(true);
            if (now - g_reviveStartMs >= kReviveMs) {
                Send(OP_REVIVE_DONE, g_reviving);
                LOGI("Downed: we revived player %d", g_reviving);
                StopReviving(false);
            }
            return;
        }
        if (g_target == kNobody || !g_enterHeld) return;
        g_reviving = g_target;
        g_reviveStartMs = now;
        Send(OP_REVIVE_START, g_reviving);
        // kneel down to the friend: the medics' CPR when its IFP is here
        if (RemotePlayer* rp = Players::Instance().Get(g_reviving); rp && rp->ped) {
            CVector d = rp->ped->GetPosition() - me->GetPosition();
            me->CurrentRotation() = me->AimingRotation() = std::atan2(-d.x, d.y);
        }
        if (BlockReady("medic"))
            Cmd(0x0605).I(GetPedRef(me)).S("CPR").S("MEDIC").F(4.0f).I(1).I(0).I(0).I(0).I(-1).Run();  // TASK_PLAY_ANIM, looped
        LOGI("Downed: reviving player %d", g_reviving);
    }

    void OnRender() override {
        auto& g = ui::Gfx();
        float s = g.Scale(), cx = g.Width() * 0.5f, y = g.Height() * 0.70f;
        uint64_t now = NowMs();
        if (g_down.on) {
            g.Text(ui::FONT_BIG, cx, y, TR("ВЫ РАНЕНЫ", "YOU ARE DOWN"), 0xFFE04040, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            y += g.LineHeight(ui::FONT_BIG);
            std::string line = g_down.reviver != kNobody
                                   ? Format(TR("%s поднимает вас...", "%s is reviving you..."), Net().PeerName(g_down.reviver).c_str())
                                   : Format(TR("Ждите помощи: %d с", "Wait for help: %d s"), std::max(0, g_down.leftMs + 999) / 1000);
            g.Text(ui::FONT_NORMAL, cx, y, line, 0xFFFFFFFF, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            return;
        }
        // the downed copies: a mark over each one on the screen
        for (const auto& [id, o] : g_others) {
            RemotePlayer* rp = Players::Instance().Get(id);
            if (!rp || !rp->ped) continue;
            float sx = 0, sy = 0, depth = 0;
            if (!WorldToScreen(rp->ped->GetPosition() + CVector{0, 0, 0.6f}, sx, sy, depth) || depth <= 0) continue;
            int left = std::max(0, o.leftMs - (int32_t)(now - o.recvMs) + 999) / 1000;
            g.Text(ui::FONT_SMALL, sx, sy, o.reviver != kNobody ? TR("ПОДНИМАЮТ", "REVIVING") : Format(TR("РАНЕН %d с", "DOWN %d s"), left),
                   0xFFFF6060, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
        }
        if (g_reviving != kNobody) {
            float p = std::min(1.0f, (float)(now - g_reviveStartMs) / (float)kReviveMs);
            std::string name = Net().PeerName(g_reviving);
            g.Text(ui::FONT_NORMAL, cx, y, Format(TR("Поднимаете %s...", "Reviving %s..."), name.c_str()), 0xFFFFFFFF,
                   ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            y += g.LineHeight(ui::FONT_NORMAL) + 4 * s;
            float w = 320 * s, h = 12 * s;
            g.Rect(cx - w / 2, y, w, h, 0xA0000000);
            g.Rect(cx - w / 2, y, w * p, h, 0xFF40C040);
        } else if (g_target != kNobody) {
            g.Text(ui::FONT_NORMAL, cx, y, Format(TR("Удерживайте F, чтобы поднять %s", "Hold F to revive %s"),
                                                  Net().PeerName(g_target).c_str()),
                   0xFFFFFFFF, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
        }
    }

    static Downed& Instance() {
        static Downed m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Downed::Instance()); }
} g_reg;
}  // namespace

void AfterPadUpdate() {
    CPad* pad = GetPad(0);
    if (g_down.on) {
        // lying down: the camera only (the right stick; the mouse is not in this state)
        int16_t rx = pad->NewState.RightStickX, ry = pad->NewState.RightStickY;
        pad->NewState = CControllerState{};
        pad->NewState.RightStickX = rx, pad->NewState.RightStickY = ry;
        g_enterHeld = false;
        return;
    }
    if (g_target == kNobody && g_reviving == kNobody) {
        g_enterHeld = false;
        return;
    }
    // beside a downed friend: the enter key revives and takes no car
    g_enterHeld = pad->NewState.ButtonTriangle != 0;
    pad->NewState.ButtonTriangle = 0;
    if (g_reviving != kNobody) pad->NewState.LeftStickX = pad->NewState.LeftStickY = 0;  // kneeling
}

}  // namespace coop::downed
