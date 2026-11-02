// Basketball for the group (docs/basquete.md, phase 1): at any hoop of the map, /basquete asks the players near it, and
// those who say yes play to 11 (or what was asked), in two teams; /basquete treino practises alone.
//
//  * the court: the nearest hoop (the four models the game's own minigame, BBALL, runs at); its rim is the hoop's level
//    design point 0 (GET_LEVEL_DESIGN_COORDS_FOR_OBJECT, as BBALL), the court lies away from the pole;
//  * the judge: the game of who asked. It keeps the score, who has the ball and what each team must do (take the ball
//    outside the arc after it changed hands), and sends that to all (OP_STATE). Its word is final;
//  * the holder's game moves its player as BBALL does: the game's basketball animations (BSKTBALL) walk and run it,
//    turned to the stick; Sprint held and let go at the jump's top shoots, Sprint running at the rim dunks. The other
//    games see its copy play the same animations (Anims replicates BSKTBALL);
//  * the ball: every game has one (BBALL_INGAME). In someone's hands it hangs on that player's ped and plays the "_O"
//    twin of its animation at the same time (as BBALL). A shot goes out as a record (OP_SHOT: from where, in or out, a
//    seed) and every game flies and bounces the ball the same way (BallPath); the first who reaches a loose ball asks the
//    judge for it (OP_CLAIM);
//  * BBALL itself keeps away while a game is on here: it ends at once with $7712 set (the variable it checks).
#include "features/basketball/Basketball.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "common/api/protocol/Protocol.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "features/basketball/BallPath.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Anim.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::basket {

namespace {
using namespace game;
using game::script::Cmd;

enum : uint8_t { OP_INVITE = 1, OP_ANSWER = 2, OP_START = 3, OP_STATE = 4, OP_SHOT = 5, OP_CLAIM = 6, OP_LEAVE = 7, OP_END = 8 };
// what the judge's state has to tell (a banner on every screen)
enum : uint8_t { EV_NONE = 0, EV_START = 1, EV_SCORE = 2, EV_NOCLEAR = 3, EV_OUT = 4, EV_REBOUND = 5, EV_MISS = 6 };
constexpr uint8_t kNobody = 0xFF;

constexpr float kHoopReach = 25.0f;    // /basquete this near a hoop
constexpr float kHoopSeen = 120.0f;    // one farther: "come closer"
constexpr float kInviteReach = 30.0f;  // who is asked: this near its rim
constexpr float kWatchReach = 80.0f;   // the ball and the score are shown this near
constexpr float kOut = 12.0f;          // the holder this far from the rim (flat): out
constexpr float kGrab = 1.1f;          // a loose ball this near (flat) and this low: caught
constexpr float kGrabHeight = 1.7f;
constexpr float kTopOfKey = 7.0f;      // where a team takes the ball after a basket
constexpr uint64_t kAnswerMs = 15000, kStateEveryMs = 2000, kHandAfterMs = 1200, kEndAfterMs = 2500, kBannerMs = 2600;
constexpr uint32_t kTeamColour[2] = {0xFF4D9BFF, 0xFFFF5A5A};  // blue, red
const char* const kHoops[] = {"BSKBALL_LAX", "BSKBALLHUB_LAX01", "VGSXREFBBALLNET", "VGSXREFBBALLNET2"};
const char* const kBlock = "BSKTBALL";
constexpr uintptr_t kNoBasketball = 7712;  // $7712: BBALL ends at once while it is not 0
constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------- the game's animations
// CJ's basketball animations and the ball's twin of each (BBALL plays one on the ball with the other on CJ, the same time)
struct Pair {
    const char* name;
    bool loop;
    void* ped = nullptr;
    void* ball = nullptr;
};
enum : int { A_IDLE, A_IDLE2, A_IDLELOOP, A_WALK_START, A_WALK, A_WALKSTOP_L, A_WALKSTOP_R, A_RUN, A_SKIDSTOP_L, A_SKIDSTOP_R,
             A_PICKUP, A_JUMP_SHOT, A_JUMP_CANCEL, A_DNK_LNCH, A_DNK_GLI, A_DNK, A_DNK_LND, A_COUNT };
Pair g_anims[A_COUNT] = {
    {"BBALL_IDLE", false},       {"BBALL_IDLE2", false},      {"BBALL_IDLELOOP", true},  {"BBALL_WALK_START", false},
    {"BBALL_WALK", true},        {"BBALL_WALKSTOP_L", false}, {"BBALL_WALKSTOP_R", false}, {"BBALL_RUN", true},
    {"BBALL_SKIDSTOP_L", false}, {"BBALL_SKIDSTOP_R", false}, {"BBALL_PICKUP", false},   {"BBALL_JUMP_SHOT", false},
    {"BBALL_JUMP_CANCEL", false}, {"BBALL_DNK_LNCH", false},  {"BBALL_DNK_GLI", false},  {"BBALL_DNK", false},
    {"BBALL_DNK_LND", false},
};
bool g_animsReady = false;

int BlockIndex() { return Call<0x4D3990, int>(kBlock); }  // CAnimManager::GetAnimationBlockIndex

bool ResolveAnims() {
    if (g_animsReady) return true;
    int idx = BlockIndex();
    if (idx < 0 || !anim::BlockLoaded(idx)) return false;
    void* b = (void*)anim::Block(idx);
    for (Pair& p : g_anims) {
        p.ped = Call<0x4D42F0, void*>(p.name, b);  // CAnimManager::GetAnimation
        p.ball = Call<0x4D42F0, void*>((std::string(p.name) + "_O").c_str(), b);
        if (!p.ped) return false;
    }
    g_animsReady = true;
    return true;
}

// the strongest basketball animation a ped plays and its time (seconds), else -1
int PedAnim(CPed* ped, float& t) {
    int best = -1;
    float amount = -1.0f;  // one just blending in (amount 0) counts
    t = 0.0f;
    if (!ped || !g_animsReady) return -1;
    for (void* a = anim::First(ped->RwObject()); a; a = anim::Next(a)) {
        if (anim::BlendDelta(a) < 0.0f || anim::BlendAmount(a) <= amount) continue;
        void* h = anim::Hierarchy(a);
        for (int i = 0; i < A_COUNT; ++i)
            if (g_anims[i].ped == h) best = i, amount = anim::BlendAmount(a), t = anim::CurrentTime(a);
    }
    return best;
}
float Length(int i) { return g_anims[i].ped ? anim::TotalTime(g_anims[i].ped) : 1.0f; }

// ---------------------------------------------------------------- small helpers
int ModelId(const char* name) {
    int id = -1;
    Call<0x4C5940, void*>(name, &id);  // CModelInfo::GetModelInfo
    return id;
}
int BallModel() {
    static int id = ModelId("BBALL_INGAME");
    return id;
}

V3 ToV3(const CVector& p) { return {p.x, p.y, p.z}; }
float Deg(float rad) { return rad * 180.0f / kPi; }
float AngleDiff(float to, float from) { return std::remainder(to - from, 360.0f); }
float Wrap(float deg) { return std::fmod(std::fmod(deg, 360.0f) + 360.0f, 360.0f); }  // 0..360, as SET_CHAR_HEADING wants
// a heading (degrees, the game's: 0 north, turning left) to look from `from` toward `to`
float HeadingTo(const V3& from, const V3& to) { return Deg(std::atan2(-(to.x - from.x), to.y - from.y)); }
int32_t& NoBasketball() { return Ref<int32_t>(addr::ScriptSpace + kNoBasketball); }
std::string Name(uint8_t id) { return id == Net().MyId() || !Net().Online() ? Cfg().nickname : Net().PeerName(id); }
uint32_t Random32() { return ((uint32_t)rand() << 16) ^ (uint32_t)rand() ^ (uint32_t)NowMs(); }
float Random01() { return (Random32() >> 8) * (1.0f / 16777216.0f); }

CPed* PedOf(uint8_t id) {
    if (id == Net().MyId() || !Net().Online()) return FindPlayerPed(0);
    const RemotePlayer* rp = Players::Instance().Get(id);
    return rp && rp->ped && IsPedValid(rp->ped) ? rp->ped : nullptr;
}
bool PosOf(uint8_t id, V3& out) {
    if (id == Net().MyId() || !Net().Online()) {
        CPlayerPed* me = FindPlayerPed(0);
        if (!me) return false;
        out = ToV3(me->GetPosition());
        return true;
    }
    const RemotePlayer* rp = Players::Instance().Get(id);
    if (!rp || !rp->hasState || NowMs() - rp->stateRecvMs > 3000) return false;
    out = ToV3(rp->state.pos);
    return true;
}

// ---------------------------------------------------------------- the match, as every game sees it
struct Match {
    bool on = false;
    bool practice = false;
    bool playing = false;  // ours is one of its players (else we watch)
    uint32_t id = 0;
    uint8_t judge = kNobody;
    Court court;
    int target = 11;
    std::map<uint8_t, uint8_t> team;  // player -> 0 blue, 1 red
    int score[2] = {};
    bool clear[2] = {};  // that team must take the ball outside the arc before its shot counts
    uint8_t holder = kNobody;
    uint16_t seq = 0;     // the judge's state number
    uint16_t shotNo = 0;  // shots seen
    // the ball in the air or loose
    bool flying = false;
    BallPath path;
    uint64_t shotMs = 0;
    uint8_t shooter = kNobody;
    bool made = false, dunk = false;
    // practice: shots and baskets
    int tries = 0, baskets = 0;
    bool over = false;
    uint64_t overMs = 0;
} g_m;

// /basquete comandos: the list in the middle of the screen, till asked again or a minute on
uint64_t g_helpUntil = 0;
constexpr uint64_t kHelpMs = 60000;

struct Banner {
    std::string text;
    uint32_t colour = 0xFFFFFFFF;
    uint64_t until = 0;
} g_banner;
void Show(const std::string& text, uint32_t colour = 0xFFFFFFFF, uint64_t ms = kBannerMs) {
    g_banner = {text, colour, NowMs() + ms};
}

// the judge's own bookkeeping (in the judge's game only)
struct Judge {
    uint64_t rimAtMs = 0;  // the shot in the air reaches the rim then
    bool rimDone = true;
    int points = 0;
    bool counts = false;
    uint8_t shooterTeam = 0;
    bool claimable = false;  // a loose ball: the first claim takes it
    uint8_t handTo = kNobody;
    uint64_t handAtMs = 0;
    uint64_t stateMs = 0;
    uint8_t ev = EV_NONE, evWho = kNobody, evValue = 0;
    uint64_t endAtMs = 0;
} g_j;

// an invitation of ours, or one to us
struct Invite {
    uint32_t id = 0;
    uint8_t from = kNobody;
    Court court;
    int target = 11;
    uint64_t until = 0;
    std::vector<uint8_t> asked, yes, answered;
} g_inviting, g_invited;

bool IsJudge() { return g_m.on && (g_m.practice || g_m.judge == Net().MyId()); }
uint8_t Me() { return Net().Online() ? Net().MyId() : 0; }
bool Watching() {
    if (!g_m.on) return false;
    CPlayerPed* me = FindPlayerPed(0);
    return me && (g_m.playing || Flat(ToV3(me->GetPosition()), g_m.court.rim) < kWatchReach);
}

void SendRaw(Writer& w) {
    if (Net().Online() && !g_m.practice) Net().Send(proto::CH_RELIABLE, w);
}

void WriteCourt(Writer& w, const Court& c) {
    w.F32(c.rim.x), w.F32(c.rim.y), w.F32(c.rim.z), w.F32(c.fx), w.F32(c.fy), w.F32(c.floorZ);
}
Court ReadCourt(Reader& r) {
    Court c;
    c.rim.x = r.F32(), c.rim.y = r.F32(), c.rim.z = r.F32(), c.fx = r.F32(), c.fy = r.F32(), c.floorZ = r.F32();
    return c;
}

// ---------------------------------------------------------------- the hoops
// the nearest hoop to p within reach: its court (the rim, the court's way, the floor)
bool FindCourt(const V3& p, Court& out, float& far) {
    far = 0.0f;
    static std::vector<int> models;
    if (models.empty())
        for (const char* n : kHoops)
            if (int id = ModelId(n); id > 0) models.push_back(id);
    CPoolHeader* pool = ObjectPool();
    CObject* best = nullptr;
    float bestD = kHoopSeen;
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* o = (CObject*)(pool->objects + (size_t)i * kObjectPoolElemSize);
        if (std::find(models.begin(), models.end(), (int)o->ModelIndex()) == models.end()) continue;
        float d = Flat(ToV3(o->GetPosition()), p);
        if (d < bestD) bestD = d, best = o;
    }
    if (!best) return false;
    if (bestD > kHoopReach) {
        far = bestD;
        return false;
    }
    int h = GetObjectRef(best);
    float x = 0, y = 0, z = 0;
    Cmd(0x080A).I(h).I(0).Out(&x).Out(&y).Out(&z).Run();  // GET_LEVEL_DESIGN_COORDS_FOR_OBJECT
    Court c;
    c.rim = {x, y, z};
    V3 pole = ToV3(best->GetPosition());
    float dx = x - pole.x, dy = y - pole.y, len = std::hypot(dx, dy);
    // the court is away from the pole; a model with the rim over its origin: its own forward (+y)
    float ox = 0, oy = 0, oz = 0;
    Cmd(0x0400).I(h).F(0.0f).F(1.0f).F(0.0f).Out(&ox).Out(&oy).Out(&oz).Run();  // GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS
    float fx = ox - pole.x, fy = oy - pole.y, flen = std::hypot(fx, fy);
    if (len > 0.2f) c.fx = dx / len, c.fy = dy / len;
    else if (flen > 0.01f) c.fx = fx / flen, c.fy = fy / flen;
    if (std::fabs(c.rim.z - pole.z) > 8.0f || (c.rim.x == 0 && c.rim.y == 0)) {
        LOGW("Basketball: hoop model %d at %.1f,%.1f,%.1f gave no rim (%.1f,%.1f,%.1f)", (int)best->ModelIndex(), pole.x, pole.y,
             pole.z, x, y, z);
        return false;
    }
    float gz = 0;
    V3 on{c.rim.x + c.fx * 3.0f, c.rim.y + c.fy * 3.0f, c.rim.z + 1.0f};
    Cmd(0x02CE).F(on.x).F(on.y).F(on.z).Out(&gz).Run();  // GET_GROUND_Z_FOR_3D_COORD
    c.floorZ = (gz > c.rim.z - 4.5f && gz < c.rim.z - 1.5f) ? gz : c.rim.z - 3.05f;
    LOGI("Basketball: hoop model %d, rim %.2f,%.2f,%.2f, court toward %.2f,%.2f (own +y %.2f,%.2f), floor %.2f (ground %.2f)",
         (int)best->ModelIndex(), x, y, z, c.fx, c.fy, flen > 0 ? fx / flen : 0, flen > 0 ? fy / flen : 0, c.floorZ, gz);
    out = c;
    return true;
}

// ---------------------------------------------------------------- the ball here
struct Ball {
    int handle = 0;
    int on = -1;     // the ped it hangs on (handle), -1 free
    int anim = -1;   // the twin it plays
} g_ball;
bool g_modelAsked = false;

bool Assets() {
    if (!g_modelAsked) {
        Cmd(0x04ED).S(kBlock).Run();  // REQUEST_ANIMATION
        RequestModel(BallModel());
        g_modelAsked = true;
    }
    if (!IsModelLoaded(BallModel())) {
        RequestModel(BallModel());
        return false;
    }
    return ResolveAnims();
}

void BallDelete() {
    if (g_ball.handle) Cmd(0x0108).I(g_ball.handle).Run();  // DELETE_OBJECT
    g_ball = {};
}
// A script's object gets its model (RwObject) from the streaming a frame or so after CREATE_OBJECT (CObject::Create
// with no RwObject), and loses it again out of its draw distance: an animation command on it before or after read the
// clump's plugin data from null (RpAnimBlendClumpGetAssociation, 0x4D687F: the crash of the first /basquete, 02.11).
// BBALL waits a frame (WAIT 0) between the two. Ours: the clump is there, a clump.
bool HasClump(int handle) {
    CObject* o = handle ? GetObject(handle) : nullptr;
    auto* rw = o ? (const uint8_t*)o->RwObject() : nullptr;
    return rw && rw[0] == 2;  // rpCLUMP
}

bool BallMake(const V3& at) {
    if (g_ball.handle && GetObject(g_ball.handle)) return true;
    g_ball = {};
    if (!Assets()) return false;
    int h = 0;
    Cmd(0x029B).I(BallModel()).F(at.x).F(at.y).F(at.z).Out(&h).Run();  // CREATE_OBJECT_NO_OFFSET
    if (!h) return false;
    Cmd(0x0382).I(h).I(0).Run();  // SET_OBJECT_COLLISION: it would knock the players about
    g_ball.handle = h;
    return true;
}
// in a player's hands: on its ped, the twin of its animation at the same time
void BallHeld(CPed* ped) {
    if (!ped) return BallDelete();
    int pedRef = GetPedRef(ped);
    // in another one's hands: a fresh one (the last pose is in its frames). Not one waiting for its model: made anew
    // every frame, it never got one, and stood in CJ's middle unattached (02.11)
    if (g_ball.handle && g_ball.on >= 0 && g_ball.on != pedRef) BallDelete();
    if (!BallMake(ToV3(ped->GetPosition()))) return;
    if (!HasClump(g_ball.handle)) return;  // its model comes next frame
    if (g_ball.on != pedRef) {
        Cmd(0x069B).I(g_ball.handle).I(pedRef).F(0.0f).F(0.5f).F(-1.0f).F(0.0f).F(0.0f).F(0.0f).Run();  // ATTACH_OBJECT_TO_CHAR, as BBALL
        g_ball.on = pedRef;
        g_ball.anim = -1;
    }
    float t = 0.0f;
    int a = PedAnim(ped, t);
    if (a < 0 || !g_anims[a].ball) a = A_IDLELOOP, t = 0.0f;  // the copy not playing one yet: the ball in the hand
    const Pair& p = g_anims[a];
    std::string twin = std::string(p.name) + "_O";
    if (g_ball.anim != a) {
        // PLAY_OBJECT_ANIM: as BBALL, a loop's twin loops, another holds its last frame
        Cmd(0x075A).I(g_ball.handle).S(twin.c_str()).S(kBlock).F(1000.0f).I(p.loop ? 1 : 0).I(p.loop ? 0 : 1).Run();
        g_ball.anim = a;
    }
    float len = anim::TotalTime(p.ball);
    float part = len > 0.0f ? std::min(t / len, 1.0f) : 0.0f;
    if (p.loop) part = std::fmod(part, 1.0f);
    Cmd(0x083A).I(g_ball.handle).S(twin.c_str()).F(part).Run();  // SET_OBJECT_ANIM_CURRENT_TIME
}
void BallAt(const V3& p) {
    if (g_ball.handle && g_ball.on >= 0) BallDelete();
    if (!BallMake(p)) return;
    Cmd(0x01BC).I(g_ball.handle).F(p.x).F(p.y).F(p.z).Run();  // SET_OBJECT_COORDINATES
}

// ---------------------------------------------------------------- our player with the ball
enum class Move { None, Idle, Walk, Run, Stop, Shoot, Dunk, Pickup };
struct Mine {
    Move move = Move::None;
    uint64_t sinceMs = 0;
    float heading = 0.0f;  // degrees
    // the jump shot: Fire held since the jump (let go at the top), or tapped (pressed again at the top)
    bool held = false;
    bool released = false, gone = false;
    float releasePart = -1.0f;
    int timing = -1;
    uint64_t timingMs = 0;
    // the dunk: CJ rides an invisible object to the rim, as BBALL
    int carrier = 0;
    V3 dunkFrom, dunkTo, dunkAt, landAt;
    int dunkStage = 0;
    bool padSet = false;
    uint64_t claimMs = 0;
    uint16_t claimedShot = 0;
} g_me;

int CjRef() { return GetPedRef(FindPlayerPed(0)); }

// TASK_PLAY_ANIM_NON_INTERRUPTABLE with BBALL's arguments for each
// the task starts on the next frame and its animation blends in after: asked again only when it has had the time
uint64_t g_playMs = 0;
void Play(int a, float blend, bool loop, bool lock, bool keep) {
    g_playMs = NowMs();
    Cmd(0x0812).I(CjRef()).S(g_anims[a].name).S(kBlock).F(blend).I(loop).I(lock).I(lock).I(keep).I(-1).Run();
}
bool Playing(int a, float* part = nullptr) {
    float t = 0.0f;
    int now = PedAnim(FindPlayerPed(0), t);
    if (part) *part = now == a ? t / std::max(Length(a), 0.01f) : 0.0f;
    return now == a;
}

void Pad(bool hold) {
    CPad* pad = GetPad(0);
    if (!pad || hold == g_me.padSet) return;
    pad->bDisablePlayerEnterCar = hold, pad->bDisablePlayerFireWeapon = hold, pad->bDisablePlayerJump = hold,
    pad->bDisablePlayerDuck = hold, pad->bDisablePlayerCycleWeapon = hold;
    g_me.padSet = hold;
}

void DropCarrier() {
    if (!g_me.carrier) return;
    Cmd(0x0465).I(CjRef()).Run();          // DETACH_CHAR_FROM_CAR: from the object too
    Cmd(0x0108).I(g_me.carrier).Run();     // DELETE_OBJECT
    g_me.carrier = 0;
}

// the ball left our hands, or the game ended: CJ is ours again
void LetGo(bool now) {
    DropCarrier();
    if (g_me.move != Move::None && FindPlayerPed(0)) Cmd(now ? 0x0792 : 0x0687).I(CjRef()).Run();  // CLEAR_CHAR_TASKS(_IMMEDIATELY)
    g_me.move = Move::None;
    Pad(false);
}

void Become(bool pickUp) {
    Pad(true);
    Cmd(0x01B9).I(CjRef()).I(0).Run();  // SET_CURRENT_CHAR_WEAPON: fists
    g_me.heading = Deg(FindPlayerPed(0)->GetHeading());
    if (pickUp) {
        Play(A_PICKUP, 4.0f, false, false, true);
        g_me.move = Move::Pickup;
    } else {
        Play(A_IDLELOOP, 4.0f, true, false, false);
        g_me.move = Move::Idle;
    }
    g_me.sinceMs = NowMs();
}

void SendShot(const V3& from, bool made, bool dunk, uint32_t seed, int points);

void StartShot() {
    CPlayerPed* cj = FindPlayerPed(0);
    g_me.heading = HeadingTo(ToV3(cj->GetPosition()), g_m.court.rim);
    Cmd(0x0173).I(CjRef()).F(Wrap(g_me.heading)).Run();  // SET_CHAR_HEADING: at the rim
    Play(A_JUMP_SHOT, 10000.0f, false, false, false);
    g_me.move = Move::Shoot;
    g_me.sinceMs = NowMs();
    g_me.released = g_me.gone = false;
    g_me.held = true;
    g_me.releasePart = -1.0f;
}

// let go this early, Fire was a tap to jump: the next press is the release (as the players took it, 02.11: every shot was
// let go 0.33 before the top, and "Bad")
constexpr float kTap = 0.12f;

void UpdateShot(bool fire, bool firePressed) {
    float part = 0.0f;
    if (!Playing(A_JUMP_SHOT, &part)) part = (NowMs() - g_me.sinceMs) / 1000.0f / std::max(Length(A_JUMP_SHOT), 0.01f);
    if (!g_me.released) {
        if (g_me.held && !fire) {
            if (part >= kTap) g_me.released = true, g_me.releasePart = part;
            else g_me.held = false;
        } else if (!g_me.held && firePressed) {
            g_me.released = true, g_me.releasePart = part;
        }
    }
    if (!g_me.gone && part >= kPeak && (g_me.released || part >= kPeak + 0.1f)) {
        float error = g_me.released ? std::fabs(g_me.releasePart - kPeak) : 1.0f;
        CPlayerPed* cj = FindPlayerPed(0);
        float x = 0, y = 0, z = 0;
        Cmd(0x04C4).I(CjRef()).F(0.04f).F(0.694f).F(1.75f).Out(&x).Out(&y).Out(&z).Run();  // GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS
        float dist = Flat(ToV3(cj->GetPosition()), g_m.court.rim);
        bool made = Random01() < Chance(dist, error);
        g_me.gone = true;
        g_me.timing = error >= 1.0f ? 0 : Timing(error);
        g_me.timingMs = NowMs();
        LOGI("Basketball: shot from %.1f m, released %.2f off the top: %s", dist, error, made ? "in" : "out");
        SendShot({x, y, z}, made, false, Random32(), Points(dist));
    }
    if (g_me.gone && (part >= 0.98f || NowMs() - g_me.sinceMs > 2500)) LetGo(false);
}

bool DunkOk() {
    CPlayerPed* cj = FindPlayerPed(0);
    V3 p = ToV3(cj->GetPosition());
    const Court& c = g_m.court;
    float d = Flat(p, c.rim);
    float front = (p.x - c.rim.x) * c.fx + (p.y - c.rim.y) * c.fy;
    float facing = std::fabs(AngleDiff(HeadingTo(p, c.rim), Deg(cj->GetHeading())));
    return d > 1.4f && d < 3.6f && front > 0.3f && facing < 60.0f;
}

void StartDunk() {
    CPlayerPed* cj = FindPlayerPed(0);
    V3 p = ToV3(cj->GetPosition());
    const Court& c = g_m.court;
    float d = std::max(Flat(p, c.rim), 0.01f);
    g_me.dunkFrom = p;
    // BBALL's: under the rim by 1.9 m, a centimetre toward where it came from
    g_me.dunkTo = {c.rim.x + (p.x - c.rim.x) / d * 0.01f, c.rim.y + (p.y - c.rim.y) / d * 0.01f, c.rim.z - 1.9f};
    g_me.heading = HeadingTo(p, c.rim);
    int h = 0;
    Cmd(0x029B).I(BallModel()).F(p.x).F(p.y).F(p.z).Out(&h).Run();  // CREATE_OBJECT_NO_OFFSET
    if (!h) return StartShot();
    Cmd(0x0177).I(h).F(Wrap(g_me.heading)).Run();  // SET_OBJECT_HEADING
    Cmd(0x0382).I(h).I(0).Run();             // SET_OBJECT_COLLISION
    Cmd(0x0750).I(h).I(0).Run();             // SET_OBJECT_VISIBLE
    Cmd(0x0550).I(h).I(1).Run();             // FREEZE_OBJECT_POSITION
    Cmd(0x04F4).I(CjRef()).I(h).F(0.0f).F(0.0f).F(0.0f).I(0).F(360.0f).I(0).Run();  // ATTACH_CHAR_TO_OBJECT
    g_me.carrier = h;
    g_me.dunkAt = p;
    Play(A_DNK_LNCH, 4.0f, false, true, true);
    g_me.move = Move::Dunk;
    g_me.dunkStage = 0;
    g_me.sinceMs = NowMs();
    LOGI("Basketball: dunk from %.1f m", d);
}

// BBALL's dunk: the launch, a glide to the rim (the carrier at 3.25 m/s: 0.065 m a frame), hanging there while the ball
// goes down through the net, and the landing on the carrier back down to the court in front of the rim
void UpdateDunk() {
    uint64_t now = NowMs();
    float since = (now - g_me.sinceMs) / 1000.0f;
    auto glide = [&](const V3& to) {
        if (!g_me.carrier) return true;
        float step = 3.25f * TimeStep() * 0.02f;
        V3& p = g_me.dunkAt;
        float dx = to.x - p.x, dy = to.y - p.y, dz = to.z - p.z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
        bool there = d <= std::max(step, 0.07f);
        p = there ? to : V3{p.x + dx / d * step, p.y + dy / d * step, p.z + dz / d * step};
        Cmd(0x01BC).I(g_me.carrier).F(p.x).F(p.y).F(p.z).Run();  // SET_OBJECT_COORDINATES
        return there;
    };
    switch (g_me.dunkStage) {
    case 0:  // the launch
        if (since >= Length(A_DNK_LNCH)) Play(A_DNK_GLI, 1000.0f, false, false, true), g_me.dunkStage = 1, g_me.sinceMs = now;
        return;
    case 1:  // the glide
        if (glide(g_me.dunkTo) || since > 2.0f) Play(A_DNK, 1000.0f, false, false, true), g_me.dunkStage = 2, g_me.sinceMs = now;
        return;
    case 2:  // on the rim: the ball leaves the hands into the net (BBALL_DNK_O's end)
        if (since >= 0.09f) {
            g_me.dunkStage = 3;
            SendShot(g_m.court.rim, true, true, Random32(), 2);
        }
        return;
    case 3:  // hanging
        if (since >= Length(A_DNK)) {
            const Court& c = g_m.court;
            g_me.landAt = {c.rim.x + c.fx * 0.9f, c.rim.y + c.fy * 0.9f, c.floorZ + 1.0f};
            Play(A_DNK_LND, 1000.0f, false, false, false);
            g_me.dunkStage = 4;
            g_me.sinceMs = now;
        }
        return;
    default:  // the landing
        glide(g_me.landAt);
        if (since >= Length(A_DNK_LND)) LetGo(false);
        return;
    }
}

// our player has the ball: BBALL's moves
void UpdateMine() {
    CPlayerPed* cj = FindPlayerPed(0);
    CPad* pad = GetPad(0);
    if (!cj || !pad) return;
    const CControllerState& now = pad->NewState;
    const CControllerState& was = pad->OldState;
    bool typing = ui::TheChat().IsOpen() || MenuActive();
    // Fire (the left mouse button: CPad::GetWeapon's ButtonCircle) shoots; Sprint (Space: ButtonCross) runs
    bool fire = !typing && now.ButtonCircle != 0;
    bool pressed = fire && was.ButtonCircle == 0;
    bool sprint = !typing && now.ButtonCross != 0;
    float sx = typing ? 0.0f : now.LeftStickX / 128.0f, sy = typing ? 0.0f : -now.LeftStickY / 128.0f;
    float mag = std::min(std::hypot(sx, sy), 1.0f);
    bool run = sprint;
    switch (g_me.move) {
    case Move::Shoot:
        return UpdateShot(fire, pressed);
    case Move::Dunk:
        return UpdateDunk();
    case Move::Pickup:
        if ((NowMs() - g_me.sinceMs > 400 && !Playing(A_PICKUP)) || NowMs() - g_me.sinceMs > 1900)
            Play(A_IDLELOOP, 4.0f, true, false, false), g_me.move = Move::Idle;
        return;
    case Move::Stop:
        if (NowMs() - g_me.sinceMs < 250) return;
        if (mag <= 0.3f) {
            bool stopping = Playing(A_WALKSTOP_L) || Playing(A_WALKSTOP_R) || Playing(A_SKIDSTOP_L) || Playing(A_SKIDSTOP_R);
            if (stopping && NowMs() - g_me.sinceMs < 600) return;
            Play(A_IDLELOOP, 4.0f, true, false, false);
            g_me.move = Move::Idle;
            return;
        }
        g_me.move = Move::Idle;  // and on below: off again
        break;
    default:
        break;
    }
    if (pressed) return g_me.move == Move::Run && DunkOk() ? StartDunk() : StartShot();
    if (mag > 0.3f) {
        Move want = run ? Move::Run : Move::Walk;
        // again when it did not take (the task replaced, CJ knocked over)
        if (g_me.move != want || (NowMs() - g_playMs > 600 && !Playing(run ? A_RUN : A_WALK))) {
            Play(run ? A_RUN : A_WALK, 8.0f, true, true, false);
            g_me.move = want;
        }
        // to the stick, from the camera's view: a tenth of the way a frame (BBALL)
        CVector f = cam::Front();
        float fl = std::max(std::hypot(f.x, f.y), 0.001f), fx = f.x / fl, fy = f.y / fl;
        float dx = fx * sy + fy * sx, dy = fy * sy - fx * sx;
        float want_h = Deg(std::atan2(-dx, dy));
        g_me.heading += AngleDiff(want_h, g_me.heading) * std::min(1.0f, 0.1f * TimeStep());
        g_me.heading = Wrap(g_me.heading);
        Cmd(0x0173).I(CjRef()).F(g_me.heading).Run();  // SET_CHAR_HEADING
        return;
    }
    if (g_me.move == Move::Walk || g_me.move == Move::Run) {
        float part = 0.0f;
        bool walk = g_me.move == Move::Walk;
        Playing(walk ? A_WALK : A_RUN, &part);
        int stop = walk ? (std::fmod(part, 1.0f) < 0.5f ? A_WALKSTOP_L : A_WALKSTOP_R) : (std::fmod(part, 1.0f) < 0.5f ? A_SKIDSTOP_L : A_SKIDSTOP_R);
        Play(stop, 6.0f, false, true, true);
        g_me.move = Move::Stop;
        g_me.sinceMs = NowMs();
        return;
    }
    if (g_me.move == Move::Idle && NowMs() - g_playMs > 600 && !Playing(A_IDLELOOP)) Play(A_IDLELOOP, 4.0f, true, false, false);
}

// ---------------------------------------------------------------- the judge
void ApplyState(uint16_t seq, uint16_t shots, uint8_t s0, uint8_t s1, uint8_t holder, uint8_t bits, uint8_t ev, uint8_t who,
                uint8_t value);

void SendState() {
    if (!IsJudge()) return;
    Writer w(proto::G_BASKET);
    w.U8(OP_STATE), w.U32(g_m.id), w.U16(++g_m.seq), w.U16(g_m.shotNo);
    w.U8((uint8_t)g_m.score[0]), w.U8((uint8_t)g_m.score[1]), w.U8(g_m.holder);
    uint8_t bits = (uint8_t)((g_m.clear[0] ? 1 : 0) | (g_m.clear[1] ? 2 : 0) | (g_m.over ? 4 : 0));
    w.U8(bits);
    w.U8(g_j.ev), w.U8(g_j.evWho), w.U8(g_j.evValue);
    SendRaw(w);
    // ours too, by the same way
    ApplyState(g_m.seq, g_m.shotNo, (uint8_t)g_m.score[0], (uint8_t)g_m.score[1], g_m.holder, bits, g_j.ev, g_j.evWho, g_j.evValue);
    g_j.ev = EV_NONE;
    g_j.stateMs = NowMs();
}

void Event(uint8_t ev, uint8_t who, uint8_t value) { g_j.ev = ev, g_j.evWho = who, g_j.evValue = value; }

// a player of `team` for the ball: the one nearest the top of the key
uint8_t PickFrom(int team) {
    uint8_t best = kNobody;
    float bestD = 1e9f;
    const Court& c = g_m.court;
    V3 top{c.rim.x + c.fx * kTopOfKey, c.rim.y + c.fy * kTopOfKey, c.rim.z};
    for (auto& [id, t] : g_m.team) {
        if (t != team) continue;
        V3 p;
        float d = PosOf(id, p) ? Flat(p, top) : 1e8f;
        if (d < bestD) bestD = d, best = id;
    }
    return best;
}

void EndMatch(const char* why);

void JudgeTick() {
    if (!IsJudge() || g_m.over) {
        if (IsJudge() && g_m.over && NowMs() >= g_j.endAtMs) EndMatch("over");
        return;
    }
    uint64_t now = NowMs();
    // the shot reaches the rim: it counts (or not)
    if (g_m.flying && !g_j.rimDone && now >= g_j.rimAtMs) {
        g_j.rimDone = true;
        int team = g_j.shooterTeam;
        if (g_m.practice) {
            if (g_m.made) g_m.score[0] += g_j.points, ++g_m.baskets, Event(EV_SCORE, g_m.shooter, (uint8_t)(g_m.dunk ? 9 : g_j.points));
            g_j.claimable = true;
        } else if (g_m.made && g_j.counts) {
            g_m.score[team] += g_j.points;
            Event(EV_SCORE, g_m.shooter, (uint8_t)(g_m.dunk ? 9 : g_j.points));
            if (g_m.score[team] >= g_m.target) {
                g_m.over = true;
                g_j.endAtMs = now + kEndAfterMs + 4000;
                LOGI("Basketball: team %d wins %d-%d", team, g_m.score[0], g_m.score[1]);
            } else {
                g_j.handTo = PickFrom(1 - team);
                g_j.handAtMs = now + kHandAfterMs;
            }
        } else if (g_m.made) {
            Event(EV_NOCLEAR, g_m.shooter, 0);
            g_j.handTo = PickFrom(1 - team);
            g_j.handAtMs = now + kHandAfterMs;
        } else {
            g_j.claimable = true;
            Event(EV_MISS, g_m.shooter, 0);
        }
        SendState();
    }
    if (g_j.handTo != kNobody && now >= g_j.handAtMs) {
        g_m.holder = g_j.handTo;
        g_m.flying = false;
        auto it = g_m.team.find(g_m.holder);
        if (it != g_m.team.end()) g_m.clear[it->second] = true;
        g_j.handTo = kNobody;
        g_j.claimable = false;
        SendState();
    }
    // the holder: clear of the arc, or out
    if (g_m.holder != kNobody && !g_m.practice) {
        V3 p;
        auto it = g_m.team.find(g_m.holder);
        if (it != g_m.team.end() && PosOf(g_m.holder, p)) {
            int team = it->second;
            const Court& c = g_m.court;
            float d = Flat(p, c.rim), front = (p.x - c.rim.x) * c.fx + (p.y - c.rim.y) * c.fy;
            if (d > kOut || front < -0.8f) {
                Event(EV_OUT, g_m.holder, 0);
                uint8_t to = PickFrom(1 - team);
                g_m.holder = to;
                if (to != kNobody) g_m.clear[1 - team] = true;
                SendState();
            } else if (g_m.clear[team] && d > kThreeArc) {
                g_m.clear[team] = false;
                SendState();
            }
        }
    }
    if (now - g_j.stateMs >= kStateEveryMs) SendState();
}

void JudgeShot(uint8_t from, const Shot& s, int points) {
    if (!IsJudge() || g_m.over) return;
    auto it = g_m.team.find(from);
    if (it == g_m.team.end()) return;
    g_j.shooterTeam = it->second;
    g_j.points = points;
    g_j.counts = !g_m.clear[it->second];
    g_j.rimAtMs = g_m.shotMs + (uint64_t)(g_m.path.RimTime() * 1000.0f);
    g_j.rimDone = false;
    g_j.claimable = false;
    g_j.handTo = kNobody;
    if (g_m.practice) ++g_m.tries;
    (void)s;
    SendState();
}

// a player left the game (or the session): the judge goes on without it, and ends it when a team is empty
void JudgeRemove(uint8_t who) {
    auto it = g_m.team.find(who);
    if (!IsJudge() || it == g_m.team.end()) return;
    int team = it->second;
    g_m.team.erase(it);
    ui::TheChat().AddSystem(Format(TR("%s вышел из игры в баскетбол.", "%s left the basketball game."), Name(who).c_str()));
    bool empty = true;
    for (auto& [pid, t] : g_m.team) empty = empty && t != team;
    if (empty) return EndMatch("a team is empty");
    if (g_m.holder == who) g_m.holder = PickFrom(team);
    SendState();
}

void JudgeClaim(uint8_t who, uint16_t shot) {
    if (!IsJudge() || g_m.over || !g_j.claimable || g_m.holder != kNobody || shot != g_m.shotNo) return;
    auto it = g_m.team.find(who);
    if (it == g_m.team.end()) return;
    g_m.holder = who;
    g_m.flying = false;
    g_j.claimable = false;
    if (!g_m.practice) {
        bool defence = it->second != g_j.shooterTeam;
        g_m.clear[it->second] = defence;
        Event(EV_REBOUND, who, defence ? 1 : 0);
    }
    SendState();
}

// ---------------------------------------------------------------- every game
void Cleanup() {
    LetGo(true);
    BallDelete();
    if (g_m.on) {
        NoBasketball() = 0;
        Cmd(0x04EF).S(kBlock).Run();  // REMOVE_ANIMATION
        MarkModelNotNeeded(BallModel());
    }
    g_modelAsked = false;
    g_animsReady = false;  // the block may go: found again next time
    g_m = Match{};
    g_j = Judge{};
}

void EndMatch(const char* why) {
    if (!g_m.on) return;
    if (IsJudge() && !g_m.practice) {
        Writer w(proto::G_BASKET);
        w.U8(OP_END), w.U32(g_m.id), w.U8((uint8_t)g_m.score[0]), w.U8((uint8_t)g_m.score[1]);
        SendRaw(w);
    }
    LOGI("Basketball: the game ends (%s), %d-%d", why, g_m.score[0], g_m.score[1]);
    Cleanup();
}

void Begin(uint32_t id, uint8_t judge, const Court& c, int target, bool practice, const std::map<uint8_t, uint8_t>& teams,
           uint8_t holder) {
    if (g_m.on) Cleanup();
    g_m.on = true;
    g_m.id = id;
    g_m.judge = judge;
    g_m.court = c;
    g_m.target = target;
    g_m.practice = practice;
    g_m.team = teams;
    g_m.playing = teams.count(Me()) != 0;
    g_m.holder = holder;
    auto it = teams.find(holder);
    if (it != teams.end() && !practice) g_m.clear[it->second] = true;
    NoBasketball() = 1;
    Assets();
    if (g_m.playing) {
        std::string blue, red;
        for (auto& [pid, t] : teams) (t ? red : blue) += (t ? red : blue).empty() ? Name(pid) : ", " + Name(pid);
        ui::TheChat().AddSystem(practice ? TR("Тренировка. Все команды: /basquete comandos. Выйти: /basquete sair.",
                                              "Practice. All the commands: /basquete comandos. Leave with /basquete sair.")
                                         : Format(TR("Баскетбол до %d очков: синие (%s) против красных (%s). Команды: /basquete comandos.",
                                                     "Basketball to %d: blue (%s) against red (%s). Commands: /basquete comandos."),
                                                  target, blue.c_str(), red.c_str()));
        Show(practice ? TR("Тренировка", "Practice") : TR("Баскетбол!", "Basketball!"), 0xFFFFC864);
    }
    LOGI("Basketball: game %08X on, judge %u, to %d, %zu players%s", id, judge, target, teams.size(), practice ? " (practice)" : "");
}

void SendShot(const V3& from, bool made, bool dunk, uint32_t seed, int points) {
    Shot s;
    s.from = from, s.made = made, s.dunk = dunk, s.seed = seed;
    uint16_t no = (uint16_t)(g_m.shotNo + 1);
    Writer w(proto::G_BASKET);
    w.U8(OP_SHOT), w.U32(g_m.id), w.U16(no), w.U8(Me());
    w.F32(from.x), w.F32(from.y), w.F32(from.z), w.U8((uint8_t)((made ? 1 : 0) | (dunk ? 2 : 0))), w.U32(seed), w.U8((uint8_t)points);
    SendRaw(w);
    // ours at once
    g_m.shotNo = no;
    g_m.flying = true;
    g_m.path = BallPath(g_m.court, s);
    g_m.shotMs = NowMs();
    g_m.shooter = Me();
    g_m.made = made, g_m.dunk = dunk;
    g_m.holder = kNobody;
    JudgeShot(Me(), s, points);
}

void ApplyState(uint16_t seq, uint16_t shots, uint8_t s0, uint8_t s1, uint8_t holder, uint8_t bits, uint8_t ev, uint8_t who,
                uint8_t value) {
    bool stale = shots < g_m.shotNo;  // sent before the judge had our latest shot: its holder is old
    if (!IsJudge()) g_m.seq = seq;
    g_m.score[0] = s0, g_m.score[1] = s1;
    g_m.clear[0] = bits & 1, g_m.clear[1] = (bits & 2) != 0;
    g_m.over = (bits & 4) != 0;
    if (!stale) {
        if (holder != kNobody) g_m.flying = false;
        g_m.holder = holder;
    }
    if (!g_m.playing && !Watching()) return;
    switch (ev) {
    case EV_SCORE:
        if (value == 9) Show(Format(TR("Данк! %s", "Dunk! %s"), Name(who).c_str()), 0xFFFFC864);
        else Show(Format(TR("+%d: %s!", "+%d: %s!"), value, Name(who).c_str()), 0xFFFFC864);
        break;
    case EV_NOCLEAR:
        Show(TR("Не засчитано: сначала выведите мяч за дугу", "No points: take the ball outside the arc first"), 0xFFFF8C3C);
        break;
    case EV_OUT:
        Show(TR("Аут", "Out of bounds"), 0xFFFF8C3C);
        break;
    case EV_REBOUND:
        Show(Format(TR("Подбор: %s", "Rebound: %s"), Name(who).c_str()), 0xFFFFFFFF, 1500);
        break;
    default:
        break;
    }
    // the winning basket's banner gives way to the result
    if (g_m.over && !g_m.overMs) {
        g_m.overMs = NowMs();
        int w = s0 > s1 ? 0 : 1;
        Show(Format(TR("Победа %s: %d - %d", "%s win %d-%d"), w ? TR("красных", "Red") : TR("синих", "Blue"), std::max(s0, s1),
                    std::min(s0, s1)),
             kTeamColour[w], 6000);
    }
}

void Handle(uint8_t from, Reader& r) {
    uint8_t op = r.U8();
    uint32_t id = r.U32();
    if (!r.Ok()) return;
    switch (op) {
    case OP_INVITE: {
        Court c = ReadCourt(r);
        int target = r.U8();
        CPlayerPed* me = FindPlayerPed(0);
        if (!r.Ok() || !me || g_m.on || !core::IsPlayerReady() || me->InVehicle() || me->AreaCode() != 0) return;
        if (Flat(ToV3(me->GetPosition()), c.rim) > kInviteReach) return;
        g_invited = Invite{};
        g_invited.id = id, g_invited.from = from, g_invited.court = c, g_invited.target = target;
        g_invited.until = NowMs() + kAnswerMs;
        ui::TheChat().AddSystem(Format(TR("%s зовёт играть в баскетбол до %d очков: Y - да, N - нет.",
                                          "%s asks you to play basketball to %d: Y yes, N no."),
                                       Net().PeerName(from).c_str(), target));
        return;
    }
    case OP_ANSWER: {
        bool yes = r.U8() != 0;
        if (!r.Ok() || id != g_inviting.id || NowMs() > g_inviting.until) return;
        if (std::find(g_inviting.answered.begin(), g_inviting.answered.end(), from) != g_inviting.answered.end()) return;
        g_inviting.answered.push_back(from);
        if (yes) g_inviting.yes.push_back(from);
        else ui::TheChat().AddSystem(Format(TR("%s отказался.", "%s said no."), Net().PeerName(from).c_str()));
        return;
    }
    case OP_START: {
        Court c = ReadCourt(r);
        int target = r.U8(), n = r.U8();
        std::map<uint8_t, uint8_t> teams;
        for (int i = 0; i < n && i < 8; ++i) {
            uint8_t pid = r.U8(), t = r.U8();
            teams[pid] = t & 1;
        }
        uint8_t holder = r.U8();
        if (!r.Ok()) return;
        Begin(id, from, c, target, false, teams, holder);
        return;
    }
    default:
        break;
    }
    if (!g_m.on || id != g_m.id) return;
    switch (op) {
    case OP_STATE: {
        uint16_t seq = r.U16(), shots = r.U16();
        uint8_t s0 = r.U8(), s1 = r.U8(), holder = r.U8(), bits = r.U8(), ev = r.U8(), who = r.U8(), value = r.U8();
        if (r.Ok() && from == g_m.judge) ApplyState(seq, shots, s0, s1, holder, bits, ev, who, value);
        return;
    }
    case OP_SHOT: {
        uint16_t no = r.U16();
        uint8_t shooter = r.U8();
        Shot s;
        s.from.x = r.F32(), s.from.y = r.F32(), s.from.z = r.F32();
        uint8_t bits = r.U8();
        s.seed = r.U32();
        int points = r.U8();
        if (!r.Ok() || shooter != from) return;
        s.made = bits & 1, s.dunk = (bits & 2) != 0;
        g_m.shotNo = std::max(g_m.shotNo, no);
        g_m.flying = true;
        g_m.path = BallPath(g_m.court, s);
        g_m.shotMs = NowMs();
        g_m.shooter = shooter;
        g_m.made = s.made, g_m.dunk = s.dunk;
        g_m.holder = kNobody;
        JudgeShot(shooter, s, points);
        return;
    }
    case OP_CLAIM: {
        uint16_t shot = r.U16();
        if (r.Ok()) JudgeClaim(from, shot);
        return;
    }
    case OP_LEAVE:
        JudgeRemove(from);
        return;
    case OP_END:
        if (from == g_m.judge) {
            if (!g_m.over) Show(TR("Игра окончена", "Game over"), 0xFFFFFFFF, 3000);
            EndMatch("the judge ended it");
        }
        return;
    default:
        return;
    }
}

// ---------------------------------------------------------------- the invitation
void StartInvited() {
    if (g_inviting.yes.empty()) {
        ui::TheChat().AddSystem(TR("Никто рядом не согласился.", "Nobody nearby said yes."));
        g_inviting = Invite{};
        return;
    }
    // the teams in turn: us blue, the first yes red, and so on; an odd last one watches
    std::map<uint8_t, uint8_t> teams;
    std::vector<uint8_t> order{Me()};
    order.insert(order.end(), g_inviting.yes.begin(), g_inviting.yes.end());
    if (order.size() > 6) order.resize(6);
    if (order.size() % 2) order.pop_back();
    for (size_t i = 0; i < order.size(); ++i) teams[order[i]] = (uint8_t)(i % 2);
    uint8_t holder = order[1];  // red starts with the ball
    Writer w(proto::G_BASKET);
    w.U8(OP_START), w.U32(g_inviting.id);
    WriteCourt(w, g_inviting.court);
    w.U8((uint8_t)g_inviting.target), w.U8((uint8_t)teams.size());
    for (auto& [pid, t] : teams) w.U8(pid), w.U8(t);
    w.U8(holder);
    Net().Send(proto::CH_RELIABLE, w);
    Begin(g_inviting.id, Me(), g_inviting.court, g_inviting.target, false, teams, holder);
    g_inviting = Invite{};
    SendState();
}

void InviteTick() {
    if (!g_inviting.id) return;
    bool all = !g_inviting.asked.empty();
    for (uint8_t a : g_inviting.asked)
        all = all && std::find(g_inviting.answered.begin(), g_inviting.answered.end(), a) != g_inviting.answered.end();
    if (all || NowMs() > g_inviting.until) StartInvited();
}

bool Command(const std::vector<std::string>& args) {
    std::string a = args.empty() ? "" : args[0];
    for (char& ch : a) ch = (char)tolower((unsigned char)ch);
    if (a == "comandos" || a == "ajuda" || a == "help" || a == "commands") {
        g_helpUntil = NowMs() < g_helpUntil ? 0 : NowMs() + kHelpMs;
        return true;
    }
    if (a == "sair" || a == "leave" || a == "exit") {
        if (!g_m.on) {
            ui::TheChat().AddError(TR("Вы не в игре в баскетбол.", "You are not in a basketball game."));
            return true;
        }
        if (IsJudge()) {
            Show(TR("Игра окончена", "Game over"), 0xFFFFFFFF, 3000);
            EndMatch("the judge left");
        } else {
            Writer w(proto::G_BASKET);
            w.U8(OP_LEAVE), w.U32(g_m.id);
            SendRaw(w);
            EndMatch("left");
        }
        return true;
    }
    CPlayerPed* me = FindPlayerPed(0);
    if (!core::IsPlayerReady() || !me) return true;
    if (g_m.on) {
        ui::TheChat().AddError(TR("Игра уже идёт. Выйти: /basquete sair.", "A game is on already. Leave with /basquete sair."));
        return true;
    }
    if (me->InVehicle()) {
        ui::TheChat().AddError(TR("Сначала выйдите из машины.", "Get out of the vehicle first."));
        return true;
    }
    if (core::IsOnMission()) {
        ui::TheChat().AddError(TR("Не во время миссии.", "Not during a mission."));
        return true;
    }
    Court c;
    float far = 0.0f;
    if (me->AreaCode() != 0 || !FindCourt(ToV3(me->GetPosition()), c, far)) {
        if (far > 0.0f)
            ui::TheChat().AddError(Format(TR("Подойдите ближе к кольцу (до него %.0f м).", "Come closer to the hoop (it is %.0f m away)."), far));
        else ui::TheChat().AddError(TR("Рядом нет баскетбольного кольца.", "There is no basketball hoop near you."));
        return true;
    }
    if (a == "treino" || a == "practice" || !Net().Online()) {
        std::map<uint8_t, uint8_t> teams{{Me(), 0}};
        Begin(Random32(), Me(), c, 0, true, teams, Me());
        return true;
    }
    int target = atoi(a.c_str());
    target = target > 0 ? std::clamp(target, 5, 31) : 11;
    Invite inv;
    inv.id = Random32() | 1u;
    inv.from = Me(), inv.court = c, inv.target = target, inv.until = NowMs() + kAnswerMs;
    for (auto& [pid, rp] : Players::Instance().All()) {
        V3 p;
        if (rp.hasState && rp.state.area == 0 && PosOf(pid, p) && Flat(p, c.rim) <= kInviteReach) inv.asked.push_back(pid);
    }
    if (inv.asked.empty()) {
        ui::TheChat().AddError(TR("Рядом нет других игроков. Тренировка: /basquete treino.",
                                  "No other players near the hoop. Practise alone with /basquete treino."));
        return true;
    }
    g_inviting = inv;
    Writer w(proto::G_BASKET);
    w.U8(OP_INVITE), w.U32(inv.id);
    WriteCourt(w, c);
    w.U8((uint8_t)target);
    Net().Send(proto::CH_RELIABLE, w);
    ui::TheChat().AddSystem(Format(TR("Приглашение в баскетбол до %d очков отправлено игрокам рядом.",
                                      "Basketball to %d: the players near the hoop are asked."),
                                   target));
    LOGI("Basketball: invitation %08X to %zu players, to %d", inv.id, inv.asked.size(), target);
    return true;
}

// ---------------------------------------------------------------- each frame
void Tick() {
    if (g_inviting.id) InviteTick();
    if (g_invited.id && NowMs() > g_invited.until) g_invited = Invite{};
    if (!g_m.on) return;
    if (!core::IsPlayerReady()) return;
    NoBasketball() = 1;
    bool watch = Watching();
    if (!Assets()) return;
    JudgeTick();
    if (!g_m.on) return;
    uint8_t me = Me();
    CPlayerPed* cj = FindPlayerPed(0);
    // our player with the ball, or not any more
    bool mine = g_m.playing && g_m.holder == me;
    if (mine && g_me.move == Move::None) {
        bool resting = g_m.shotNo && NowMs() - g_m.shotMs > (uint64_t)(g_m.path.RestTime() * 1000.0f);
        Become(resting);
    }
    if (!mine && g_me.move != Move::None && g_me.move != Move::Shoot && g_me.move != Move::Dunk) LetGo(false);
    if (g_me.move != Move::None) UpdateMine();
    // the ball
    if (!watch) return BallDelete();
    if (g_m.holder != kNobody && !g_m.flying) {
        BallHeld(PedOf(g_m.holder));
    } else if (g_m.flying || g_m.shotNo) {
        float t = (NowMs() - g_m.shotMs) / 1000.0f;
        // the dunk's ball stays in the dunker's hands till it goes through
        BallAt(g_m.path.At(t));
        // a loose ball within reach: ask the judge for it
        if (g_m.playing && g_m.holder == kNobody && cj && t >= g_m.path.RimTime() && !(g_m.made && !g_m.practice)) {
            V3 b = g_m.path.At(t), p = ToV3(cj->GetPosition());
            if (Flat(b, p) < kGrab && g_m.path.Height(t) < kGrabHeight &&
                (g_me.claimedShot != g_m.shotNo || NowMs() - g_me.claimMs > 600)) {
                g_me.claimedShot = g_m.shotNo;
                g_me.claimMs = NowMs();
                if (IsJudge()) JudgeClaim(me, g_m.shotNo);
                else {
                    Writer w(proto::G_BASKET);
                    w.U8(OP_CLAIM), w.U32(g_m.id), w.U16(g_m.shotNo);
                    SendRaw(w);
                }
            }
        }
    }
}

// ---------------------------------------------------------------- the screen
// /basquete comandos: what to type and which keys, in a dark box in the middle
void RenderHelp() {
    struct Row {
        const char* key;
        const char* what;
    };
    const Row chat[] = {
        {"/basquete", TR("позвать игроков у кольца (до 11 очков)", "ask the players near the hoop (to 11 points)")},
        {"/basquete 21", TR("до 21 очка (любое число от 5 до 31)", "to 21 points (any number from 5 to 31)")},
        {"/basquete treino", TR("тренировка в одиночку", "practise alone")},
        {"/basquete sair", TR("выйти из игры", "leave the game")},
        {"/basquete comandos", TR("показать или скрыть этот список", "show or hide this list")},
        {"Y / N", TR("принять или отклонить приглашение", "say yes or no to an invitation")},
    };
    const Row keys[] = {
        {"W A S D", TR("идти с мячом (дриблинг)", "walk with the ball (dribbling)")},
        {TR("Пробел", "Space"), TR("бежать с мячом", "run with the ball")},
        {TR("Левая кнопка мыши", "Left mouse button"),
         TR("бросок: нажмите для прыжка, отпустите у отметки", "shoot: press to jump, let go at the mark")},
        {"", TR("или нажмите и нажмите ещё раз у отметки", "or tap it, and press again at the mark")},
        {TR("ЛКМ на бегу у кольца", "Left mouse running at the rim"), TR("данк (лицом к кольцу, до 3,5 м)", "dunk (facing the rim, up to 3.5 m)")},
        {TR("Свободный мяч", "Loose ball"), TR("подбор: подойдите к нему", "rebound: walk into it")},
    };
    const char* const rules[] = {
        TR("2 очка из-под дуги, 3 из-за дуги.", "2 points inside the arc, 3 from outside it."),
        TR("После попадания или подбора в защите выведите мяч за дугу перед броском.",
           "After a basket or a defensive rebound, take the ball outside the arc before you shoot."),
        TR("Мяч дальше 12 м от кольца - аут, мяч у соперника.", "The ball over 12 m from the rim is out: the other team's."),
    };
    auto& g = ui::Gfx();
    float s = g.Scale(), lh = g.LineHeight(ui::FONT_SMALL), th = g.LineHeight(ui::FONT_MENU);
    float keyW = 0.0f, whatW = 0.0f;
    for (const Row* list : {chat, keys})
        for (size_t i = 0; i < 6; ++i) {
            keyW = std::max(keyW, g.TextWidth(ui::FONT_SMALL, list[i].key));
            whatW = std::max(whatW, g.TextWidth(ui::FONT_SMALL, list[i].what));
        }
    for (const char* r : rules) whatW = std::max(whatW, g.TextWidth(ui::FONT_SMALL, r) - keyW - 24 * s);
    float pad = 26 * s, w = keyW + 24 * s + whatW + 2 * pad;
    float h = pad * 2 + th + 10 * s + (6 + 6) * lh + 3 * lh + 3 * (lh + 8 * s);
    float x = (g.Width() - w) * 0.5f, y = (g.Height() - h) * 0.5f;
    g.Rect(x, y, w, h, ui::Argb(205, 0, 0, 0));
    g.RectOutline(x, y, w, h, ui::Argb(120, 255, 200, 100), 1.5f * s);
    float cy = y + pad;
    g.Text(ui::FONT_MENU, g.Width() * 0.5f, cy, TR("БАСКЕТБОЛ - КОМАНДЫ", "BASKETBALL - COMMANDS"), 0xFFFFC864,
           ui::TEXT_CENTER | ui::TEXT_SHADOW);
    cy += th + 10 * s;
    auto section = [&](const char* title) {
        g.Text(ui::FONT_SMALL, x + pad, cy, title, 0xFF8CB4FF, ui::TEXT_SHADOW);
        cy += lh + 8 * s;
    };
    auto rows = [&](const Row* list) {
        for (size_t i = 0; i < 6; ++i) {
            g.Text(ui::FONT_SMALL, x + pad, cy, list[i].key, 0xFFFFFFFF, ui::TEXT_SHADOW);
            g.Text(ui::FONT_SMALL, x + pad + keyW + 24 * s, cy, list[i].what, 0xFFC8C8D2, ui::TEXT_SHADOW);
            cy += lh;
        }
    };
    section(TR("В чате", "In the chat"));
    rows(chat);
    section(TR("С мячом", "With the ball"));
    rows(keys);
    section(TR("Правила", "Rules"));
    for (const char* r : rules) {
        g.Text(ui::FONT_SMALL, x + pad, cy, r, 0xFFC8C8D2, ui::TEXT_SHADOW);
        cy += lh;
    }
}

void Render() {
    auto& g = ui::Gfx();
    float s = g.Scale();
    uint64_t now = NowMs();
    if (g_m.on && Watching() && !Cfg().cleanScreen) {
        std::string line, sub;
        if (g_m.practice) {
            line = Format(TR("Тренировка  ·  очки: %d  ·  попаданий %d из %d", "Practice  ·  points %d  ·  made %d of %d"), g_m.score[0],
                          g_m.baskets, g_m.tries);
        } else {
            line = Format(TR("СИНИЕ  %d   x   %d  КРАСНЫЕ", "BLUE  %d   x   %d  RED"), g_m.score[0], g_m.score[1]);
            sub = Format(TR("до %d", "to %d"), g_m.target);
            if (g_m.holder != kNobody) sub += Format(TR("  ·  мяч: %s", "  ·  ball: %s"), Name(g_m.holder).c_str());
            else sub += g_m.flying && (now - g_m.shotMs) / 1000.0f < g_m.path.RimTime() ? TR("  ·  бросок", "  ·  shot in the air")
                                                                                       : TR("  ·  мяч свободен", "  ·  loose ball");
        }
        float lw = g.TextWidth(ui::FONT_MENU, line), sw = sub.empty() ? 0.0f : g.TextWidth(ui::FONT_SMALL, sub);
        float w = std::max(lw, sw) + 40 * s, h = g.LineHeight(ui::FONT_MENU) + (sub.empty() ? 0 : g.LineHeight(ui::FONT_SMALL)) + 18 * s;
        float x = (g.Width() - w) * 0.5f, y = 8 * s;
        g.Rect(x, y, w, h, ui::Argb(170, 8, 10, 18));
        if (!g_m.practice) {
            g.Rect(x, y, 6 * s, h, kTeamColour[0]);
            g.Rect(x + w - 6 * s, y, 6 * s, h, kTeamColour[1]);
        }
        g.Text(ui::FONT_MENU, g.Width() * 0.5f, y + 8 * s, line, 0xFFFFFFFF, ui::TEXT_CENTER | ui::TEXT_SHADOW);
        if (!sub.empty())
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, y + 10 * s + g.LineHeight(ui::FONT_MENU), sub, 0xFFC8C8D2, ui::TEXT_CENTER | ui::TEXT_SHADOW);
        float yy = y + h + 6 * s;
        // what our team must do, and the keys
        auto it = g_m.team.find(Me());
        if (g_m.playing && it != g_m.team.end() && g_m.holder == Me() && g_m.clear[it->second] && !g_m.practice) {
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, yy, TR("Выведите мяч за трёхочковую дугу", "Take the ball outside the 3-point arc"),
                   0xFFFFB450, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            yy += g.LineHeight(ui::FONT_SMALL);
        }
        if (g_m.playing && g_m.holder == Me() && g_me.move != Move::Shoot && g_me.move != Move::Dunk)
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, g.Height() - 40 * s,
                   TR("ЛКМ: бросок  ·  Пробел: бег  ·  ЛКМ на бегу у кольца: данк  ·  /basquete comandos: все команды",
                      "Left mouse: shoot  ·  Space: run  ·  Left mouse running at the rim: dunk  ·  /basquete comandos: all commands"),
                   0xFFDCDCE6, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    }
    // the jump's meter: the top is the mark
    if (g_me.move == Move::Shoot && !g_me.gone) {
        float part = 0.0f;
        if (!Playing(A_JUMP_SHOT, &part)) part = (now - g_me.sinceMs) / 1000.0f / std::max(Length(A_JUMP_SHOT), 0.01f);
        float bw = 320 * s, bh = 16 * s, bx = (g.Width() - bw) * 0.5f, by = g.Height() * 0.72f, full = kPeak + 0.1f;
        g.Rect(bx - 3 * s, by - 3 * s, bw + 6 * s, bh + 6 * s, ui::Argb(170, 0, 0, 0));
        auto at = [&](float p) { return bx + bw * std::clamp(p / full, 0.0f, 1.0f); };
        g.Rect(at(kPeak - 0.07f), by, at(kPeak + 0.07f) - at(kPeak - 0.07f), bh, ui::Argb(110, 255, 210, 60));
        g.Rect(at(kPeak - 0.03f), by, at(kPeak + 0.03f) - at(kPeak - 0.03f), bh, ui::Argb(170, 80, 230, 110));
        g.Rect(bx, by + bh * 0.35f, at(part) - bx, bh * 0.3f, 0xFFFFFFFF);
        g.Rect(at(kPeak) - 1 * s, by - 4 * s, 2 * s, bh + 8 * s, 0xFFFFFFFF);
        g.Text(ui::FONT_SMALL, g.Width() * 0.5f, by + bh + 8 * s,
               g_me.held ? TR("Отпустите ЛКМ у отметки", "Let go of the left mouse button at the mark")
                         : TR("Нажмите ЛКМ у отметки", "Press the left mouse button at the mark"),
               0xFFDCDCE6, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    }
    if (g_me.timing >= 0 && now - g_me.timingMs < 1400) {
        static const uint32_t kc[3] = {0xFFFF6464, 0xFFFFD250, 0xFF50E66E};
        const char* t = g_me.timing == 2 ? TR("Идеально!", "Perfect!") : g_me.timing == 1 ? TR("Хорошо", "Good") : TR("Плохо", "Bad");
        g.Text(ui::FONT_NORMAL, g.Width() * 0.5f, g.Height() * 0.72f - 34 * s, t, kc[g_me.timing], ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    }
    if (now < g_banner.until && !g_banner.text.empty())
        g.Text(ui::FONT_BIG, g.Width() * 0.5f, g.Height() * 0.30f, g_banner.text, g_banner.colour, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    if (now < g_helpUntil) RenderHelp();
}

class Basketball : public Module {
public:
    const char* Name() const override { return "Basketball"; }
    void OnInstallHooks() override { Net().SetHandler(proto::G_BASKET, &Handle); }
    void OnSessionEnd() override {
        if (g_m.on && !g_m.practice) Cleanup();
        g_inviting = g_invited = Invite{};
    }
    void OnGameShutdown() override {
        // the world goes, and the ball and the carrier with it: no commands now
        if (g_m.on) NoBasketball() = 0;
        g_ball = {};
        g_me = Mine{};
        g_m = Match{};
        g_j = Judge{};
        g_modelAsked = false;
        g_animsReady = false;
    }
    void OnPeerLeave(uint8_t id) override {
        if (!g_m.on || g_m.practice) return;
        if (id == g_m.judge) {
            Show(TR("Игра окончена", "Game over"), 0xFFFFFFFF, 3000);
            return EndMatch("the judge is gone");
        }
        JudgeRemove(id);
    }
    void OnPostProcess() override { Tick(); }
    void OnRender() override {
        if (g_m.on || g_me.timing >= 0 || NowMs() < g_banner.until || NowMs() < g_helpUntil) Render();
    }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "basquete" && cmd != "basketball" && cmd != "basket") return false;
        return Command(args);
    }
    static Basketball& Instance() {
        static Basketball m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Basketball::Instance()); }
} g_reg;

}  // namespace

bool Key(bool yes) {
    if (!g_invited.id || NowMs() > g_invited.until) return false;
    Writer w(proto::G_BASKET);
    w.U8(OP_ANSWER), w.U32(g_invited.id), w.U8(yes ? 1 : 0);
    Net().Send(proto::CH_RELIABLE, w);
    LOGI("Basketball: %s to %s's invitation", yes ? "yes" : "no", Net().PeerName(g_invited.from).c_str());
    g_invited = Invite{};
    return true;
}

std::string Text(std::string& hint) {
    if (!g_invited.id || NowMs() > g_invited.until) return "";
    hint = TR("Y - да, N - нет", "Y: yes, N: no");
    return Format(TR("%s зовёт в баскетбол (до %d)", "%s: basketball to %d?"), Net().PeerName(g_invited.from).c_str(), g_invited.target);
}

}  // namespace coop::basket
