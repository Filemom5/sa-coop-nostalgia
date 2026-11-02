// Basketball for the group (docs/basquete.md): at any hoop of the map, /basquete asks the players near it, and those who
// say yes play street ball as NBA Street plays it (to 21, 1 and 2 points, win by 2; /basquete nba: 2 and 3), in two
// teams; /basquete treino practises alone.
//
//  * the court: the nearest hoop (the four models the game's own minigame, BBALL, runs at); its rim is the hoop's level
//    design point 0 (GET_LEVEL_DESIGN_COORDS_FOR_OBJECT, as BBALL), the court lies away from the pole;
//  * the judge: the game of who asked. It keeps the score, the teams' Gamebreaker meters, the shot clock, who has the ball
//    and what each team must do (take the ball outside the arc after it changed hands), and sends that to all
//    (OP_STATE). Its word is final;
//  * the holder's game moves its player as BBALL does: the game's basketball animations (BSKTBALL) walk and run it,
//    turned to the stick. Its tricks (crossover, spin, showboat, pump fake), passes (to a teammate, the alley-oop, off
//    the backboard, off a defender's head) and shots are its own; a trick can break a near defender's ankles (OP_ANKLE:
//    that game drops its player), and the style of the possession goes with the ball and is banked on a basket. The
//    other games see its copy play the same animations (Anims replicates BSKTBALL and any script's);
//  * a defender's game: a steal asks the holder's game (OP_STEAL: it decides, by what its player is doing, OP_STOLE), a
//    block is a jump everyone hears of (OP_BLOCK: the shooter's game checks for one at the release), the stance slides;
//  * the ball: every game has one (BBALL_INGAME). In someone's hands it hangs on that player's ped and plays the "_O"
//    twin of its animation at the same time (as BBALL). A shot or a pass goes out as a record (OP_SHOT, OP_PASS: from
//    where, where to, a seed) and every game flies and bounces the ball the same way (BallPath); the first who reaches a
//    loose or passed ball asks the judge for it (OP_CLAIM);
//  * the dunk keeps the player's own camera: CJ is frozen and placed on BBALL's dunk path each frame (BBALL's ride on an
//    invisible object, ATTACH_CHAR_TO_OBJECT, put the camera in the first person);
//  * BBALL itself keeps away while a game is on here: it ends at once with $7712 set (the variable it checks).
#include "features/basketball/Basketball.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
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

enum : uint8_t {
    OP_INVITE = 1, OP_ANSWER = 2, OP_START = 3, OP_STATE = 4, OP_SHOT = 5, OP_CLAIM = 6, OP_LEAVE = 7, OP_END = 8,
    OP_PASS = 9, OP_TRICK = 10, OP_ANKLE = 11, OP_STEAL = 12, OP_STOLE = 13, OP_BLOCK = 14,
};
// what the judge's state has to tell (a banner on every screen)
enum : uint8_t {
    EV_NONE = 0, EV_START = 1, EV_SCORE = 2, EV_NOCLEAR = 3, EV_OUT = 4, EV_REBOUND = 5, EV_MISS = 6, EV_CLOCK = 7,
    EV_STEAL = 8, EV_BLOCK = 9, EV_BREAKER = 10,
};
constexpr uint8_t kValDunk = 0x10, kValOop = 0x20;  // EV_SCORE's and EV_BREAKER's value: the points, and these
constexpr uint8_t kNobody = 0xFF;
constexpr uint8_t RULE_NBA = 1;  // 2 and 3 points (else street: 1 and 2)
enum : uint8_t { P_CHEST = 0, P_OOP = 1, P_BOARD = 2, P_HEAD = 3 };  // the passes

constexpr float kHoopReach = 25.0f;    // /basquete this near a hoop
constexpr float kHoopSeen = 120.0f;    // one farther: "come closer"
constexpr float kInviteReach = 30.0f;  // who is asked: this near its rim
constexpr float kWatchReach = 80.0f;   // the ball and the score are shown this near
constexpr float kOut = 12.0f;          // the holder this far from the rim (flat): out
constexpr float kGrab = 1.1f;          // a loose ball this near (flat) and this low: caught
constexpr float kGrabHeight = 1.7f;
constexpr float kCatch = 1.8f;         // a pass to us: this near, this low
constexpr float kCatchHeight = 2.3f;
constexpr float kTopOfKey = 7.0f;      // where a team takes the ball after a basket
constexpr float kStealReach = 1.6f;
constexpr uint64_t kAnswerMs = 15000, kStateEveryMs = 2000, kHandAfterMs = 1200, kEndAfterMs = 2500, kBannerMs = 2600;
constexpr uint64_t kShotClockMs = 24000, kComboMs = 3000;
constexpr int kBreaker = 1000;  // a team's Gamebreaker meter: full
constexpr float kTurboDrain = 0.17f, kTurboRegen = 0.22f;  // a full meter a second
constexpr uint32_t kTeamColour[2] = {0xFF4D9BFF, 0xFFFF5A5A};  // blue, red
const char* const kHoops[] = {"BSKBALL_LAX", "BSKBALLHUB_LAX01", "VGSXREFBBALLNET", "VGSXREFBBALLNET2"};
const char* const kBlock = "BSKTBALL";
const char* const kThrowBlock = "GRENADE";  // WEAPON_throwu, WEAPON_throw: the passes
const char* const kPedBlock = "PED";        // KD_left, KD_right, getup: broken ankles
constexpr uintptr_t kNoBasketball = 7712;   // $7712: BBALL ends at once while it is not 0
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
             A_PICKUP, A_JUMP_SHOT, A_JUMP_CANCEL, A_DNK_LNCH, A_DNK_GLI, A_DNK, A_DNK_LND, A_REACT_SCORE, A_REACT_MISS,
             A_DEF_JUMP, A_DEF_LOOP, A_DEF_STEPL, A_DEF_STEPR, A_COUNT };
Pair g_anims[A_COUNT] = {
    {"BBALL_IDLE", false},       {"BBALL_IDLE2", false},      {"BBALL_IDLELOOP", true},  {"BBALL_WALK_START", false},
    {"BBALL_WALK", true},        {"BBALL_WALKSTOP_L", false}, {"BBALL_WALKSTOP_R", false}, {"BBALL_RUN", true},
    {"BBALL_SKIDSTOP_L", false}, {"BBALL_SKIDSTOP_R", false}, {"BBALL_PICKUP", false},   {"BBALL_JUMP_SHOT", false},
    {"BBALL_JUMP_CANCEL", false}, {"BBALL_DNK_LNCH", false},  {"BBALL_DNK_GLI", false},  {"BBALL_DNK", false},
    {"BBALL_DNK_LND", false},    {"BBALL_REACT_SCORE", false}, {"BBALL_REACT_MISS", false}, {"BBALL_DEF_JUMP_SHOT", false},
    {"BBALL_DEF_LOOP", true},    {"BBALL_DEF_STEPL", false},  {"BBALL_DEF_STEPR", false},
};
bool g_animsReady = false;

int BlockIndex(const char* name) { return Call<0x4D3990, int>(name); }  // CAnimManager::GetAnimationBlockIndex
bool BlockReady(const char* name) {
    int idx = BlockIndex(name);
    return idx >= 0 && anim::BlockLoaded(idx);
}

bool ResolveAnims() {
    if (g_animsReady) return true;
    int idx = BlockIndex(kBlock);
    if (idx < 0 || !anim::BlockLoaded(idx)) return false;
    void* b = (void*)anim::Block(idx);
    for (Pair& p : g_anims) {
        p.ped = Call<0x4D42F0, void*>(p.name, b);  // CAnimManager::GetAnimation
        p.ball = Call<0x4D42F0, void*>((std::string(p.name) + "_O").c_str(), b);  // the defender's and the reactions': none
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
V3 Ahead(float heading) { return {-std::sin(heading * kPi / 180.0f), std::cos(heading * kPi / 180.0f), 0.0f}; }
float Dist(const V3& a, const V3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
// how much `to` lies the way `dir` (flat, unit) points from `from`: 1 straight ahead, 0 aside, -1 behind
float Facing(const V3& from, const V3& dir, const V3& to) {
    float dx = to.x - from.x, dy = to.y - from.y, d = std::hypot(dx, dy);
    return d < 0.05f ? 1.0f : (dx * dir.x + dy * dir.y) / d;
}
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
    int target = 21;
    uint8_t rules = 0;
    std::map<uint8_t, uint8_t> team;  // player -> 0 blue, 1 red
    int score[2] = {};
    bool clear[2] = {};  // that team must take the ball outside the arc before its shot counts
    int style[2] = {};   // the teams' Gamebreaker meters
    int clock = -1;      // the shot clock's seconds when the judge told (clockMs), -1 stopped
    uint64_t clockMs = 0;
    uint8_t holder = kNobody;
    uint16_t seq = 0;     // the judge's state number
    uint16_t shotNo = 0;  // what the ball did: shots, passes and steals seen
    // the ball in the air or loose
    bool flying = false;
    BallPath path;
    uint64_t shotMs = 0;
    uint8_t shooter = kNobody;  // or the passer
    bool made = false, dunk = false, blocked = false;
    // a pass: to whom, an alley-oop (to the rim, for its dunk), the style of the possession it carries
    bool pass = false, oop = false;
    uint8_t passTo = kNobody;
    int carry = 0;
    // practice: shots and baskets
    int tries = 0, baskets = 0;
    bool over = false;
    uint64_t overMs = 0;
} g_m;

// /basquete comandos: the list in the middle of the screen, till asked again or two minutes on
uint64_t g_helpUntil = 0;
constexpr uint64_t kHelpMs = 120000;

struct Banner {
    std::string text;
    uint32_t colour = 0xFFFFFFFF;
    uint64_t until = 0;
} g_banner;
void Show(const std::string& text, uint32_t colour = 0xFFFFFFFF, uint64_t ms = kBannerMs) {
    g_banner = {text, colour, NowMs() + ms};
}
// the others' tricks, small under the score
struct FeedLine {
    std::string text;
    uint32_t colour;
    uint64_t until;
};
std::vector<FeedLine> g_feed;
void Feed(const std::string& text, uint32_t colour) {
    g_feed.push_back({text, colour, NowMs() + 2500});
    if (g_feed.size() > 4) g_feed.erase(g_feed.begin());
}

// the judge's own bookkeeping (in the judge's game only)
struct Judge {
    uint64_t rimAtMs = 0;  // the shot in the air reaches the rim then
    bool rimDone = true;
    int points = 0, style = 0;
    bool counts = false, oopShot = false;
    uint8_t shooterTeam = 0, blocker = kNobody;
    bool claimable = false;  // a loose or passed ball: the first claim takes it
    uint8_t oopTo = kNobody;  // an alley-oop in the air: that one's dunk takes it
    uint8_t handTo = kNobody;
    uint64_t handAtMs = 0;
    uint64_t stateMs = 0;
    int possTeam = -1;  // the shot clock's: whose ball, since when
    uint64_t clockStartMs = 0;
    uint8_t ev = EV_NONE, evWho = kNobody, evValue = 0;
    uint64_t endAtMs = 0;
} g_j;

// an invitation of ours, or one to us
struct Invite {
    uint32_t id = 0;
    uint8_t from = kNobody;
    Court court;
    int target = 21;
    uint8_t rules = 0;
    uint64_t until = 0;
    std::vector<uint8_t> asked, yes, answered;
} g_inviting, g_invited;

bool IsJudge() { return g_m.on && (g_m.practice || g_m.judge == Net().MyId()); }
uint8_t Me() { return Net().Online() ? Net().MyId() : 0; }
int TeamOf(uint8_t id) {
    auto it = g_m.team.find(id);
    return it == g_m.team.end() ? -1 : it->second;
}
bool Opponent(uint8_t id) {
    int a = TeamOf(Me()), b = TeamOf(id);
    return a >= 0 && b >= 0 && a != b;
}
bool Watching() {
    if (!g_m.on) return false;
    CPlayerPed* me = FindPlayerPed(0);
    return me && (g_m.playing || Flat(ToV3(me->GetPosition()), g_m.court.rim) < kWatchReach);
}
// what a basket is worth: street 1 inside the arc, 2 out; NBA 2 and 3
int Value(float dist) {
    bool out = dist > kThreeArc;
    return (g_m.rules & RULE_NBA) ? (out ? 3 : 2) : (out ? 2 : 1);
}
int DunkValue() { return (g_m.rules & RULE_NBA) ? 2 : 1; }
const char* RulesName(uint8_t rules) { return (rules & RULE_NBA) ? TR("правила НБА", "NBA rules") : TR("уличные правила", "street rules"); }

void Handle(uint8_t from, Reader& r);

void SendRaw(Writer& w) {
    if (Net().Online() && !g_m.practice) Net().Send(proto::CH_RELIABLE, w);
}
// to all, and ours at once by the same way
void Deliver(Writer& w) {
    SendRaw(w);
    Reader r(w.Data() + 1, w.Size() - 1);  // past the message's id
    Handle(Me(), r);
}

void WriteCourt(Writer& w, const Court& c) {
    w.F32(c.rim.x), w.F32(c.rim.y), w.F32(c.rim.z), w.F32(c.fx), w.F32(c.fy), w.F32(c.floorZ);
}
Court ReadCourt(Reader& r) {
    Court c;
    c.rim.x = r.F32(), c.rim.y = r.F32(), c.rim.z = r.F32(), c.fx = r.F32(), c.fy = r.F32(), c.floorZ = r.F32();
    return c;
}
void WriteV3(Writer& w, const V3& v) { w.F32(v.x), w.F32(v.y), w.F32(v.z); }
V3 ReadV3(Reader& r) {
    V3 v;
    v.x = r.F32(), v.y = r.F32(), v.z = r.F32();
    return v;
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
// how far in front of the backboard p is (along the court's way)
float Front(const V3& p) {
    const Court& c = g_m.court;
    return (p.x - c.rim.x) * c.fx + (p.y - c.rim.y) * c.fy;
}
// where an alley-oop meets its dunker: just over the rim's front
V3 OopSpot() {
    const Court& c = g_m.court;
    return {c.rim.x + c.fx * 0.45f, c.rim.y + c.fy * 0.45f, c.rim.z + 0.15f};
}
// near enough the rim to fly up to it for an alley-oop
bool OopOk(const V3& p) {
    float d = Flat(p, g_m.court.rim);
    return d > 1.0f && d < 7.5f && Front(p) > 0.4f;
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
        Cmd(0x04ED).S(kBlock).Run();       // REQUEST_ANIMATION
        Cmd(0x04ED).S(kThrowBlock).Run();  // the passes' throws: not needed to play
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
    if (a < 0 || !g_anims[a].ball) a = A_IDLELOOP, t = 0.0f;  // not one with a twin (a pass's throw): the ball in the hand
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

// the blocks' jumps heard lately (a defender's OP_BLOCK): a shot, a dunk or a fake looks at them
struct Jump {
    uint8_t who;
    uint64_t ms;
};
std::vector<Jump> g_jumps;

// ---------------------------------------------------------------- tricks and style
enum : uint8_t { T_CROSS, T_SPIN, T_SHOW, T_FAKE, T_FAKED, T_ANKLE, T_BOARD, T_HEAD, T_OOP, T_DUNK, T_LONG_DUNK, T_TIP,
                 T_PERFECT, T_COUNT };
constexpr int kTrickStyle[T_COUNT] = {60, 90, 40, 30, 80, 150, 120, 160, 150, 60, 120, 100, 40};
const char* TrickName(int k) {
    switch (k) {
    case T_CROSS: return TR("Кроссовер", "Crossover");
    case T_SPIN: return TR("Разворот", "Spin move");
    case T_SHOW: return TR("Шоу с мячом", "Showboat");
    case T_FAKE: return TR("Ложный бросок", "Pump fake");
    case T_FAKED: return TR("Купился!", "Faked out!");
    case T_ANKLE: return TR("Сломал лодыжки!", "Ankle breaker!");
    case T_BOARD: return TR("Пас от щита", "Back 2 Papa");
    case T_HEAD: return TR("Пас от головы", "Off the Heezay");
    case T_OOP: return TR("Аллей-уп", "Alley-oop");
    case T_DUNK: return TR("Данк", "Dunk");
    case T_LONG_DUNK: return TR("Данк издалека", "Long dunk");
    case T_TIP: return TR("Добивание сверху", "Tip slam");
    case T_PERFECT: return TR("Идеальный бросок", "Perfect release");
    default: return "";
    }
}

// ---------------------------------------------------------------- our player with the ball
enum class Move { None, Idle, Walk, Run, Stop, Shoot, Dunk, Pickup, Trick, Show, Fake, Pass };
struct Mine {
    Move move = Move::None;
    uint64_t sinceMs = 0;
    float heading = 0.0f;  // degrees
    int stage = 0;         // of the dunk, the fake
    // the jump shot: Fire held since the jump (let go at the top), or tapped (pressed again at the top)
    bool held = false;
    bool released = false, gone = false;
    float releasePart = -1.0f;
    int timing = -1;
    uint64_t timingMs = 0;
    // the dunk: CJ frozen and placed on BBALL's path each frame, his own camera on him
    bool frozen = false;
    V3 dunkAt, dunkTo, landAt;
    float glide = 3.25f;    // m/s
    uint64_t oopAtMs = 0;   // an alley-oop's: its ball is at the rim then
    int dunkKind = T_DUNK;
    // a trick: the crossover's or the spin's turn
    int trick = -1, side = 1;
    float trickFrom = 0.0f, trickTurn = 0.0f;
    bool ankleDone = false, shows = false;
    // the fake: a defender jumped at it
    bool cancel = false, fooled = false;
    // the pass
    uint8_t passKind = 0, passTo = kNobody, passVia = kNobody;
    float passAt = 0.0f;
    bool passGone = false;
    // the possession's style: banked on a basket; a trick again within kComboMs adds to the combo
    int style = 0, mult = 1, count[T_COUNT] = {};
    uint64_t lastTrickMs = 0;
    std::string popup;
    uint32_t popupColour = 0xFFFFC864;
    uint64_t popupUntil = 0;
    float turbo = 1.0f;
    bool tired = false;  // empty: no running till it is a quarter full
    uint64_t claimMs = 0;
    uint16_t claimedShot = 0;
    bool claimTip = false;
    int react = -1;  // after our shot: its animation, when free
    uint64_t reactUntil = 0;
} g_me;

// our player without the ball
struct Def {
    bool stance = false;
    bool stepping = false;
    uint64_t stepUntil = 0, blockUntil = 0, tipUntil = 0, stealAt = 0;
    int down = 0;  // ankles broken: 1 falling, 2 getting up
    uint64_t downMs = 0;
} g_def;

int CjRef() { return GetPedRef(FindPlayerPed(0)); }
V3 CjPos() {
    CPlayerPed* cj = FindPlayerPed(0);
    return cj ? ToV3(cj->GetPosition()) : V3{};
}
V3 FromCj(float x, float y, float z) {
    float ox = 0, oy = 0, oz = 0;
    Cmd(0x04C4).I(CjRef()).F(x).F(y).F(z).Out(&ox).Out(&oy).Out(&oz).Run();  // GET_OFFSET_FROM_CHAR_IN_WORLD_COORDS
    return {ox, oy, oz};
}
void Face(float heading) {
    g_me.heading = Wrap(heading);
    Cmd(0x0173).I(CjRef()).F(g_me.heading).Run();  // SET_CHAR_HEADING
}

void Popup(const std::string& text, uint32_t colour = 0xFFFFC864) {
    g_me.popup = text, g_me.popupColour = colour, g_me.popupUntil = NowMs() + 1600;
}

// a trick of ours: its style (less each time the same in a possession), times the combo; the others hear of it
void Style(int kind) {
    uint64_t now = NowMs();
    g_me.mult = now - g_me.lastTrickMs < kComboMs ? std::min(g_me.mult + 1, 5) : 1;
    g_me.lastTrickMs = now;
    int n = g_me.count[kind]++;
    int pts = (int)(kTrickStyle[kind] * std::pow(0.7f, (float)n)) * g_me.mult;
    g_me.style = std::min(g_me.style + pts, 9999);
    Popup(g_me.mult > 1 ? Format("%s +%d  x%d", TrickName(kind), pts, g_me.mult) : Format("%s +%d", TrickName(kind), pts));
    Writer w(proto::G_BASKET);
    w.U8(OP_TRICK), w.U32(g_m.id), w.U8((uint8_t)kind), w.U16((uint16_t)pts), w.U8((uint8_t)g_me.mult);
    SendRaw(w);
}

// TASK_PLAY_ANIM_NON_INTERRUPTABLE with BBALL's arguments for each
// the task starts on the next frame and its animation blends in after: asked again only when it has had the time
uint64_t g_playMs = 0;
void PlayFrom(const char* block, const char* name, float blend, bool loop, bool lock, bool keep) {
    g_playMs = NowMs();
    Cmd(0x0812).I(CjRef()).S(name).S(block).F(blend).I(loop).I(lock).I(lock).I(keep).I(-1).Run();
}
void Play(int a, float blend, bool loop, bool lock, bool keep) { PlayFrom(kBlock, g_anims[a].name, blend, loop, lock, keep); }
bool Playing(int a, float* part = nullptr) {
    float t = 0.0f;
    int now = PedAnim(FindPlayerPed(0), t);
    if (part) *part = now == a ? t / std::max(Length(a), 0.01f) : 0.0f;
    return now == a;
}
void ClearTasks(bool now) {
    if (FindPlayerPed(0)) Cmd(now ? 0x0792 : 0x0687).I(CjRef()).Run();  // CLEAR_CHAR_TASKS(_IMMEDIATELY)
}

// what we turned off on the pad: in a game, Fire (a punch: the steal is ours), Jump (the block), Duck (the fake) and the
// weapon wheel; with the ball, getting in a car too. The getters only: the keys themselves are read raw.
uint8_t g_padSet = 0;
void PadSync(bool game, bool ball) {
    CPad* pad = GetPad(0);
    uint8_t want = (uint8_t)((game ? 1 : 0) | (ball ? 2 : 0));
    if (!pad || want == g_padSet) return;
    pad->bDisablePlayerFireWeapon = game;
    pad->bDisablePlayerJump = game;
    pad->bDisablePlayerDuck = game;
    pad->bDisablePlayerCycleWeapon = game;
    pad->bDisablePlayerEnterCar = ball;
    g_padSet = want;
}

// the dunk: frozen in place, no collision (the pole, the board), placed by us
void Hold(bool on) {
    if (on == g_me.frozen || !FindPlayerPed(0)) return;
    int cj = CjRef();
    if (on) {
        Cmd(0x0619).I(cj).I(0).Run();  // SET_CHAR_COLLISION
        Cmd(0x04D7).I(cj).I(1).Run();  // FREEZE_CHAR_POSITION
    } else {
        Cmd(0x04D7).I(cj).I(0).Run();
        Cmd(0x0619).I(cj).I(1).Run();
    }
    g_me.frozen = on;
}
void PlaceCj(const V3& p) {
    CPlayerPed* cj = FindPlayerPed(0);
    if (!cj) return;
    SetEntityPosition(cj, CVector(p.x, p.y, p.z));
    cj->MoveSpeed() = CVector(0.0f, 0.0f, 0.0f);
}

// the ball left our hands, or the game ended: CJ is ours again
void LetGo(bool now) {
    Hold(false);
    if (g_me.move != Move::None) ClearTasks(now);
    g_me.move = Move::None;
}

void StartDunk(int kind, uint64_t oopAtMs);

// the ball is ours: from a pass of our team, the possession's style with it; a tip at the rim goes up again at once
void Become(bool pickUp) {
    bool fromMate = g_m.pass && TeamOf(g_m.shooter) == TeamOf(Me()) && g_m.shooter != Me();
    bool tip = g_me.claimTip && NowMs() - g_me.claimMs < 1500 && !g_m.pass && TeamOf(g_m.shooter) == TeamOf(Me());
    g_me.claimTip = false;
    g_me.style = fromMate ? g_m.carry : (g_m.pass && g_m.shooter == Me() ? g_m.carry : 0);
    g_me.mult = 1;
    std::fill(std::begin(g_me.count), std::end(g_me.count), 0);
    g_def = Def{};
    Cmd(0x01B9).I(CjRef()).I(0).Run();  // SET_CURRENT_CHAR_WEAPON: fists
    g_me.heading = Deg(FindPlayerPed(0)->GetHeading());
    g_me.sinceMs = NowMs();
    if (tip && Flat(CjPos(), g_m.court.rim) < 3.6f) return StartDunk(T_TIP, 0);
    if (pickUp) {
        Play(A_PICKUP, 4.0f, false, false, true);
        g_me.move = Move::Pickup;
    } else {
        Play(A_IDLELOOP, 4.0f, true, false, false);
        g_me.move = Move::Idle;
    }
}

void SendShot(const V3& from, bool made, bool dunk, int points, uint8_t blocker, bool oop);

// an opponent's block that meets our shot: one who jumped within `window` seconds, this near, not behind us; one roll of
// `chance` for it (and that jump is spent)
uint8_t Blocker(float window, float reach, float chance) {
    V3 me = CjPos(), dir = Ahead(g_me.heading);
    uint64_t now = NowMs();
    for (size_t i = 0; i < g_jumps.size(); ++i) {
        const Jump j = g_jumps[i];
        V3 p;
        if (now - j.ms > (uint64_t)(window * 1000.0f) || !Opponent(j.who) || !PosOf(j.who, p)) continue;
        if (Flat(p, me) > reach || Facing(me, dir, p) < -0.2f) continue;
        g_jumps.erase(g_jumps.begin() + (long)i);
        return Random01() < chance ? j.who : kNobody;
    }
    return kNobody;
}
// an opponent close in front: the shot is harder
bool Contested() {
    V3 me = CjPos(), dir = Ahead(g_me.heading);
    for (auto& [pid, t] : g_m.team) {
        V3 p;
        if (Opponent(pid) && PosOf(pid, p) && Flat(p, me) < 1.4f && Facing(me, dir, p) > 0.3f) return true;
    }
    return false;
}

void StartShot() {
    Face(HeadingTo(CjPos(), g_m.court.rim));
    Play(A_JUMP_SHOT, 10000.0f, false, false, false);
    g_me.move = Move::Shoot;
    g_me.sinceMs = NowMs();
    g_me.released = g_me.gone = false;
    g_me.held = true;
    g_me.releasePart = -1.0f;
}

// the pump fake (C), or a jump shot cancelled in the air (C before the release): the game's own jump cancel
void StartFake(bool cancel) {
    if (cancel) {
        Play(A_JUMP_CANCEL, 6.0f, false, false, false);
    } else {
        Face(HeadingTo(CjPos(), g_m.court.rim));
        Play(A_JUMP_SHOT, 10000.0f, false, false, false);
    }
    g_me.cancel = cancel, g_me.fooled = false;
    g_me.stage = cancel ? 1 : 0;
    g_me.move = Move::Fake;
    g_me.sinceMs = NowMs();
    Style(T_FAKE);
}
// true while it goes on
bool UpdateFake() {
    uint64_t now = NowMs();
    float since = (now - g_me.sinceMs) / 1000.0f;
    if (g_me.stage == 0 && since >= 0.22f) Play(A_JUMP_CANCEL, 6.0f, false, false, false), g_me.stage = 1;
    if (!g_me.fooled) {
        V3 me = CjPos();
        for (const Jump& j : g_jumps) {
            V3 p;
            if (j.ms + 200 >= g_me.sinceMs && Opponent(j.who) && PosOf(j.who, p) && Flat(p, me) < 3.0f) {
                g_me.fooled = true;
                Style(T_FAKED);
                break;
            }
        }
    }
    if (since < (g_me.cancel ? Length(A_JUMP_CANCEL) : 0.75f)) return true;
    Play(A_IDLELOOP, 6.0f, true, false, false);
    g_me.move = Move::Idle;
    return false;
}

// let go this early, Fire was a tap to jump: the next press is the release (as the players took it, 02.11: every shot was
// let go 0.33 before the top, and "Bad")
constexpr float kTap = 0.12f;

void UpdateShot(bool fire, bool firePressed, bool cancel) {
    float part = 0.0f;
    if (!Playing(A_JUMP_SHOT, &part)) part = (NowMs() - g_me.sinceMs) / 1000.0f / std::max(Length(A_JUMP_SHOT), 0.01f);
    if (!g_me.gone && !g_me.released && cancel) return StartFake(true);
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
        V3 from = FromCj(0.04f, 0.694f, 1.75f);
        float dist = Flat(CjPos(), g_m.court.rim);
        g_me.gone = true;
        g_me.timing = error >= 1.0f ? 0 : Timing(error);
        g_me.timingMs = NowMs();
        uint8_t blocker = Blocker(0.8f, 1.9f, 0.6f);
        if (blocker != kNobody) {
            LOGI("Basketball: shot from %.1f m blocked by %u", dist, blocker);
            g_me.timing = -1;
            SendShot(from, false, false, 0, blocker, false);
        } else {
            bool contested = Contested();
            float chance = Chance(dist, error) - (contested ? 0.15f : 0.0f);
            bool made = Random01() < chance;
            if (g_me.timing == 2) Style(T_PERFECT);
            LOGI("Basketball: shot from %.1f m, released %.2f off the top%s: %s", dist, error, contested ? ", contested" : "",
                 made ? "in" : "out");
            SendShot(from, made, false, Value(dist), kNobody, false);
        }
    }
    if (g_me.gone && (part >= 0.98f || NowMs() - g_me.sinceMs > 2500)) LetGo(false);
}

// a dunk from here: T_DUNK, T_LONG_DUNK (with turbo, from farther), else -1
int DunkFrom(bool turbo) {
    CPlayerPed* cj = FindPlayerPed(0);
    V3 p = ToV3(cj->GetPosition());
    float d = Flat(p, g_m.court.rim);
    float facing = std::fabs(AngleDiff(HeadingTo(p, g_m.court.rim), Deg(cj->GetHeading())));
    if (d <= 1.4f || Front(p) <= 0.3f || facing >= 60.0f) return -1;
    if (d < 3.6f) return T_DUNK;
    if (turbo && d < 5.5f && g_me.turbo >= 0.3f) return T_LONG_DUNK;
    return -1;
}

void StartDunk(int kind, uint64_t oopAtMs) {
    V3 p = CjPos();
    const Court& c = g_m.court;
    float d = std::max(Flat(p, c.rim), 0.01f);
    // BBALL's: under the rim by 1.9 m, a centimetre toward where it came from
    g_me.dunkTo = {c.rim.x + (p.x - c.rim.x) / d * 0.01f, c.rim.y + (p.y - c.rim.y) / d * 0.01f, c.rim.z - 1.9f};
    g_me.dunkAt = p;
    g_me.dunkKind = kind;
    g_me.oopAtMs = oopAtMs;
    g_me.glide = std::max(3.25f, d / 0.6f);
    if (kind == T_LONG_DUNK) g_me.turbo = std::max(0.0f, g_me.turbo - 0.3f);
    Face(HeadingTo(p, c.rim));
    Cmd(0x01B9).I(CjRef()).I(0).Run();  // fists
    Hold(true);
    Play(A_DNK_LNCH, 4.0f, false, true, true);
    g_me.move = Move::Dunk;
    g_me.stage = 0;
    g_me.sinceMs = NowMs();
    LOGI("Basketball: %s from %.1f m", kind == T_OOP ? "alley-oop" : kind == T_TIP ? "tip slam" : "dunk", d);
}

// BBALL's dunk: the launch, a glide to the rim (3.25 m/s or quicker from farther; an alley-oop's to meet its ball),
// hanging there while the ball goes down through the net, and the landing back down to the court in front of the rim
void UpdateDunk() {
    uint64_t now = NowMs();
    float since = (now - g_me.sinceMs) / 1000.0f;
    bool oop = g_me.oopAtMs != 0;
    const Court& c = g_m.court;
    auto glide = [&](const V3& to, float speed) {
        float step = speed * TimeStep() * 0.02f;
        V3& p = g_me.dunkAt;
        float dx = to.x - p.x, dy = to.y - p.y, dz = to.z - p.z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
        bool there = d <= std::max(step, 0.07f);
        p = there ? to : V3{p.x + dx / d * step, p.y + dy / d * step, p.z + dz / d * step};
        return there;
    };
    auto land = [&]() {
        g_me.landAt = {c.rim.x + c.fx * 0.9f, c.rim.y + c.fy * 0.9f, c.floorZ + 1.0f};
        Play(A_DNK_LND, 1000.0f, false, false, false);
        g_me.stage = 4;
        g_me.sinceMs = now;
    };
    switch (g_me.stage) {
    case 0:  // the launch
        if (since >= Length(A_DNK_LNCH)) {
            Play(A_DNK_GLI, 1000.0f, false, false, true);
            g_me.stage = 1;
            g_me.sinceMs = now;
            if (oop) {
                float left = std::max((int64_t)(g_me.oopAtMs - now), (int64_t)120) / 1000.0f;
                g_me.glide = std::clamp(Dist(g_me.dunkAt, g_me.dunkTo) / left, 3.25f, 12.0f);
            }
        }
        break;
    case 1: {  // the glide
        bool there = glide(g_me.dunkTo, g_me.glide);
        if (oop && now > g_me.oopAtMs + 1500) {
            land();  // its ball never came
            break;
        }
        if ((there && (!oop || now + 40 >= g_me.oopAtMs)) || (!oop && since > 2.0f)) {
            // a defender up at the rim
            uint8_t blocker = Blocker(0.9f, 1.9f, oop ? 0.35f : 0.45f);
            if (blocker != kNobody) {
                LOGI("Basketball: dunk blocked by %u", blocker);
                SendShot(FromCj(0.0f, 0.4f, 1.6f), false, false, 0, blocker, false);
                land();
                break;
            }
            Play(A_DNK, 1000.0f, false, false, true);
            g_me.stage = 2;
            g_me.sinceMs = now;
        }
        break;
    }
    case 2:  // on the rim: the ball leaves the hands into the net (BBALL_DNK_O's end)
        if (since >= 0.09f) {
            g_me.stage = 3;
            Style(g_me.dunkKind);
            SendShot(c.rim, true, true, DunkValue(), kNobody, g_me.dunkKind == T_OOP);
        }
        break;
    case 3:  // hanging
        if (since >= Length(A_DNK)) land();
        break;
    default:  // the landing
        glide(g_me.landAt, 3.25f);
        if (since >= Length(A_DNK_LND)) {
            PlaceCj(g_me.dunkAt);
            return LetGo(false);
        }
        break;
    }
    PlaceCj(g_me.dunkAt);
    Cmd(0x0173).I(CjRef()).F(g_me.heading).Run();  // SET_CHAR_HEADING
}

// the nearest opponent close in front of us (a crossover's victim, a pass off the head), else kNobody
uint8_t OpponentAhead(float reach, float facing, float heading) {
    V3 me = CjPos(), dir = Ahead(heading);
    uint8_t best = kNobody;
    float bestD = reach;
    for (auto& [pid, t] : g_m.team) {
        V3 p;
        if (!Opponent(pid) || !PosOf(pid, p)) continue;
        float d = Flat(p, me);
        if (d < bestD && Facing(me, dir, p) >= facing) best = pid, bestD = d;
    }
    return best;
}

// a crossover or a spin in front of a defender: its ankles may go (less in its stance, never in the air)
void TryAnkles() {
    uint8_t victim = OpponentAhead(2.4f, 0.25f, g_me.trickFrom);
    if (victim == kNobody) return;
    float t = 0.0f;
    int a = PedAnim(PedOf(victim), t);
    if (a == A_DEF_JUMP) return;
    float chance = (g_me.trick == T_SPIN ? 0.32f : 0.22f) + 0.05f * (float)(g_me.mult - 1);
    if (a == A_DEF_LOOP || a == A_DEF_STEPL || a == A_DEF_STEPR) chance *= 0.5f;
    if (Random01() >= chance) return;
    LOGI("Basketball: %s broke %u's ankles", TrickName(g_me.trick), victim);
    Style(T_ANKLE);
    Writer w(proto::G_BASKET);
    w.U8(OP_ANKLE), w.U32(g_m.id), w.U8(victim), w.U8(g_me.side > 0 ? 1 : 0);
    Deliver(w);
}

// Shift on the move: a crossover to the other hand each time (BBALL's skid stops, a jink of 40 degrees); running on
// turbo: a spin move (the run turned once round, ending a little aside)
void StartTrick(bool turbo) {
    bool spin = turbo && g_me.turbo >= 0.2f;
    if (spin) g_me.turbo -= 0.2f;
    g_me.trick = spin ? T_SPIN : T_CROSS;
    g_me.side = -g_me.side;
    g_me.trickFrom = g_me.heading;
    g_me.trickTurn = spin ? g_me.side * 385.0f : g_me.side * 40.0f;
    g_me.ankleDone = false;
    if (spin) Play(A_RUN, 8.0f, true, true, false);
    else Play(g_me.side > 0 ? A_SKIDSTOP_L : A_SKIDSTOP_R, 8.0f, false, true, true);
    g_me.move = Move::Trick;
    g_me.sinceMs = NowMs();
    Style(g_me.trick);
}
// true while it goes on
bool UpdateTrick(bool again, bool turbo, float mag) {
    float t = (NowMs() - g_me.sinceMs) / 1000.0f, len = g_me.trick == T_SPIN ? 0.45f : 0.42f;
    float k = std::min(t / len, 1.0f), ease = k * k * (3.0f - 2.0f * k);
    Face(g_me.trickFrom + g_me.trickTurn * ease);
    if (!g_me.ankleDone && t >= 0.2f) g_me.ankleDone = true, TryAnkles();
    if (again && k >= 0.55f) {
        StartTrick(turbo);
        return true;
    }
    if (k < 1.0f) return true;
    g_me.move = Move::Idle;
    if (mag <= 0.3f) Play(A_IDLELOOP, 6.0f, true, false, false);
    return false;  // on by the stick: walking or running the new way
}

// Shift standing: BBALL's own dribbles in place, one and the other in turn
void StartShow() {
    g_me.shows = !g_me.shows;
    Play(g_me.shows ? A_IDLE : A_IDLE2, 4.0f, false, false, false);
    g_me.move = Move::Show;
    g_me.sinceMs = NowMs();
    Style(T_SHOW);
}

// the pass (right mouse): to the teammate the camera looks most at. With Space: an alley-oop to one near the rim, else
// off the nearest defender's head in front, else off the backboard to ourselves (an alley-oop of our own near the rim)
void StartPass(bool special) {
    V3 me = CjPos();
    CVector f = cam::Front();
    float fl = std::max(std::hypot(f.x, f.y), 0.001f);
    V3 look{f.x / fl, f.y / fl, 0.0f};
    uint8_t mate = kNobody;
    float best = 1e9f;
    V3 matePos;
    for (auto& [pid, t] : g_m.team) {
        V3 p;
        if (pid == Me() || TeamOf(pid) != TeamOf(Me()) || !PosOf(pid, p)) continue;
        float d = Flat(p, me);
        if (d < 0.8f || d > 30.0f) continue;
        float score = (1.0f - Facing(me, look, p)) * 10.0f + d * 0.1f;
        if (score < best) best = score, mate = pid, matePos = p;
    }
    uint8_t opp = OpponentAhead(2.6f, 0.5f, g_me.heading);
    g_me.passVia = kNobody;
    if (special && mate != kNobody && OopOk(matePos)) {
        g_me.passKind = P_OOP, g_me.passTo = mate;
        Face(HeadingTo(me, matePos));
    } else if (special && opp != kNobody) {
        g_me.passKind = P_HEAD, g_me.passTo = mate != kNobody ? mate : Me(), g_me.passVia = opp;
        V3 o;
        if (PosOf(opp, o)) Face(HeadingTo(me, o));
    } else if (!special && mate != kNobody) {
        g_me.passKind = P_CHEST, g_me.passTo = mate;
        Face(HeadingTo(me, matePos));
    } else {
        g_me.passKind = P_BOARD, g_me.passTo = Me();
        Face(HeadingTo(me, g_m.court.rim));
    }
    // GRENADE's throws (underarm for a chest pass): the ball goes at the arm's swing
    bool chest = g_me.passKind == P_CHEST;
    if (BlockReady(kThrowBlock)) {
        PlayFrom(kThrowBlock, chest ? "WEAPON_throwu" : "WEAPON_throw", 8.0f, false, false, false);
        g_me.passAt = chest ? 0.18f : 0.28f;
    } else {
        Play(A_IDLELOOP, 8.0f, true, false, false);
        g_me.passAt = 0.05f;
    }
    g_me.passGone = false;
    g_me.move = Move::Pass;
    g_me.sinceMs = NowMs();
}

void SendPass(uint8_t kind, uint8_t to, bool oop, const V3& from, const V3& via, const V3& dest, float s1, float s2) {
    Writer w(proto::G_BASKET);
    w.U8(OP_PASS), w.U32(g_m.id), w.U16((uint16_t)(g_m.shotNo + 1)), w.U8(Me()), w.U8(kind), w.U8(to), w.U8(oop ? 1 : 0);
    WriteV3(w, from), WriteV3(w, via), WriteV3(w, dest);
    w.F32(s1), w.F32(s2), w.U32(Random32()), w.U16((uint16_t)std::min(g_me.style, 65535));
    g_me.style = 0;
    Deliver(w);
}

void ReleasePass() {
    g_me.passGone = true;
    const Court& c = g_m.court;
    V3 me = CjPos(), from = FromCj(0.2f, 0.45f, 0.3f), via = from, dest, p;
    float s1 = 0.5f, s2 = 0.0f;
    bool oop = false;
    switch (g_me.passKind) {
    case P_CHEST:
        if (!PosOf(g_me.passTo, p)) p = {me.x + Ahead(g_me.heading).x * 5, me.y + Ahead(g_me.heading).y * 5, me.z};
        dest = {p.x, p.y, p.z + 0.25f};
        s1 = std::clamp(0.15f + Dist(from, dest) / 14.0f, 0.25f, 1.0f);
        break;
    case P_OOP:
        if (!PosOf(g_me.passTo, p)) p = me;
        dest = OopSpot();
        s1 = std::clamp(0.3f + Flat(p, c.rim) / 6.5f, 0.75f, 1.6f);
        oop = true;
        Style(T_OOP);
        break;
    case P_HEAD: {
        V3 o;
        if (!PosOf(g_me.passVia, o)) o = {me.x + Ahead(g_me.heading).x, me.y + Ahead(g_me.heading).y, me.z};
        via = {o.x, o.y, o.z + 0.8f};
        if (g_me.passTo != Me() && PosOf(g_me.passTo, p)) dest = {p.x, p.y, p.z + 0.25f};
        else dest = {me.x + Ahead(g_me.heading).x * 0.7f, me.y + Ahead(g_me.heading).y * 0.7f, me.z + 0.25f};
        s1 = std::clamp(Dist(from, via) / 10.0f, 0.15f, 0.4f);
        s2 = std::clamp(0.15f + Dist(via, dest) / 12.0f, 0.3f, 0.9f);
        Style(T_HEAD);
        break;
    }
    default: {  // off the backboard's square, back to us: up for our own alley-oop near the rim, else out in front
        via = {c.rim.x - c.fx * 0.33f, c.rim.y - c.fy * 0.33f, c.rim.z + 0.6f};
        oop = OopOk(me);
        dest = oop ? OopSpot() : V3{c.rim.x + c.fx * 1.9f, c.rim.y + c.fy * 1.9f, c.floorZ + 1.3f};
        s1 = std::clamp(0.2f + Dist(from, via) / 12.0f, 0.35f, 1.0f);
        s2 = oop ? 0.3f : 0.45f;
        Style(T_BOARD);
        break;
    }
    }
    LOGI("Basketball: pass %d to %u%s, %.2f s", g_me.passKind, g_me.passTo, oop ? " (alley-oop)" : "", s1 + s2);
    int style = g_me.style;
    SendPass(g_me.passKind, g_me.passTo, oop, from, via, dest, s1, s2);
    if (oop && g_me.passTo == Me()) {
        StartDunk(T_OOP, NowMs() + (uint64_t)((s1 + s2) * 1000.0f));
        g_me.style = style;  // it comes back to us
    }
}

void UpdatePass() {
    float since = (NowMs() - g_me.sinceMs) / 1000.0f;
    if (!g_me.passGone && since >= g_me.passAt) ReleasePass();
    if (g_me.move == Move::Pass && since >= g_me.passAt + 0.35f) LetGo(false);
}

void UpdateTurbo(bool running) {
    float dt = TimeStep() * 0.02f;
    g_me.turbo = std::clamp(g_me.turbo + (running ? -kTurboDrain : kTurboRegen) * dt, 0.0f, 1.0f);
    if (g_me.turbo <= 0.0f) g_me.tired = true;
    if (g_me.tired && g_me.turbo >= 0.25f) g_me.tired = false;
}

// the stick's way, from the camera's view (a heading)
float StickHeading(float sx, float sy) {
    CVector f = cam::Front();
    float fl = std::max(std::hypot(f.x, f.y), 0.001f), fx = f.x / fl, fy = f.y / fl;
    float dx = fx * sy + fy * sx, dy = fy * sy - fx * sx;
    return Deg(std::atan2(-dx, dy));
}

// our player has the ball: BBALL's moves, and ours
void UpdateMine() {
    CPlayerPed* cj = FindPlayerPed(0);
    CPad* pad = GetPad(0);
    if (!cj || !pad) return;
    const CControllerState& now = pad->NewState;
    const CControllerState& was = pad->OldState;
    bool typing = ui::TheChat().IsOpen() || MenuActive();
    // Fire (the left mouse button: CPad::GetWeapon's ButtonCircle) shoots; Sprint (Space: ButtonCross) runs; Jump
    // (Shift: ButtonSquare) tricks; Duck (C: ShockButtonL) fakes; Aim (the right mouse button: RightShoulder1) passes
    auto key = [&](int16_t n) { return !typing && n != 0; };
    bool fire = key(now.ButtonCircle), pressed = fire && was.ButtonCircle == 0;
    bool sprint = key(now.ButtonCross);
    bool trick = key(now.ButtonSquare) && was.ButtonSquare == 0;
    bool fake = key(now.ShockButtonL) && was.ShockButtonL == 0;
    bool pass = key(now.RightShoulder1) && was.RightShoulder1 == 0;
    float sx = typing ? 0.0f : now.LeftStickX / 128.0f, sy = typing ? 0.0f : -now.LeftStickY / 128.0f;
    float mag = std::min(std::hypot(sx, sy), 1.0f);
    bool run = sprint && !g_me.tired;
    UpdateTurbo(g_me.move == Move::Run);
    switch (g_me.move) {
    case Move::Shoot:
        return UpdateShot(fire, pressed, fake);
    case Move::Dunk:
        return UpdateDunk();
    case Move::Pass:
        return UpdatePass();
    case Move::Pickup:
        if ((NowMs() - g_me.sinceMs > 400 && !Playing(A_PICKUP)) || NowMs() - g_me.sinceMs > 1900)
            Play(A_IDLELOOP, 4.0f, true, false, false), g_me.move = Move::Idle;
        return;
    case Move::Trick:
        if (UpdateTrick(trick, run, mag)) return;
        trick = false;
        break;
    case Move::Fake:
        if (UpdateFake()) return;
        fake = false;
        break;
    case Move::Show: {
        bool busy = NowMs() - g_me.sinceMs < 400 || Playing(g_me.shows ? A_IDLE : A_IDLE2);
        if (mag > 0.3f || pressed || pass || fake || trick || !busy) {
            if (!busy && mag <= 0.3f && !pressed && !pass && !fake && !trick) Play(A_IDLELOOP, 4.0f, true, false, false);
            g_me.move = Move::Idle;
            break;
        }
        return;
    }
    case Move::Stop:
        if (NowMs() - g_me.sinceMs < 250 && !pressed && !pass && !fake && !trick) return;
        if (mag <= 0.3f && !pressed && !pass && !fake && !trick) {
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
    if (pressed) {
        int dunk = g_me.move == Move::Run ? DunkFrom(sprint) : -1;
        return dunk >= 0 ? StartDunk(dunk, 0) : StartShot();
    }
    if (pass) return StartPass(sprint);
    if (fake) return StartFake(false);
    if (trick) return mag > 0.3f ? StartTrick(run && g_me.move == Move::Run) : StartShow();
    if (mag > 0.3f) {
        Move want = run ? Move::Run : Move::Walk;
        // again when it did not take (the task replaced, CJ knocked over)
        if (g_me.move != want || (NowMs() - g_playMs > 600 && !Playing(run ? A_RUN : A_WALK))) {
            Play(run ? A_RUN : A_WALK, 8.0f, true, true, false);
            g_me.move = want;
        }
        // to the stick: a tenth of the way a frame (BBALL)
        float want_h = StickHeading(sx, sy);
        g_me.heading += AngleDiff(want_h, g_me.heading) * std::min(1.0f, 0.1f * TimeStep());
        Face(g_me.heading);
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

// a defender reached for our ball: whether it is gone is ours to say, by what we are doing (a trick or a showboat is
// the moment: NBA Street's counter)
void ResolveSteal(uint8_t thief) {
    if (g_m.holder != Me() || !Opponent(thief)) return;
    V3 p;
    float chance;
    switch (g_me.move) {
    case Move::Shoot: case Move::Dunk: chance = 0.0f; break;
    case Move::Pass: chance = g_me.passGone ? 0.0f : 0.25f; break;
    case Move::Trick: chance = NowMs() - g_me.sinceMs < 250 ? 0.55f : 0.2f; break;
    case Move::Show: chance = 0.55f; break;
    case Move::Pickup: chance = 0.45f; break;
    case Move::Fake: chance = 0.3f; break;
    case Move::Run: chance = 0.12f; break;
    case Move::Walk: chance = 0.22f; break;
    default: chance = 0.3f; break;
    }
    if (!PosOf(thief, p) || Flat(p, CjPos()) > 2.2f) chance = 0.0f;
    bool ok = Random01() < chance;
    LOGI("Basketball: %u reached for the ball (%.0f%%): %s", thief, chance * 100.0f, ok ? "stolen" : "kept");
    Writer w(proto::G_BASKET);
    w.U8(OP_STOLE), w.U32(g_m.id), w.U16((uint16_t)(g_m.shotNo + (ok ? 1 : 0))), w.U8(thief), w.U8(ok ? 1 : 0);
    if (ok) LetGo(true);
    Deliver(w);
}

// ---------------------------------------------------------------- our player without the ball
// broken ankles: down on the floor (KD_*), and up again (getup)
void Fall(bool left) {
    CPlayerPed* cj = FindPlayerPed(0);
    if (!cj || cj->InVehicle() || g_def.down || g_me.move != Move::None) return;
    g_def = Def{};
    g_def.down = 1;
    g_def.downMs = NowMs();
    PlayFrom(kPedBlock, left ? "KD_left" : "KD_right", 8.0f, false, false, true);
    Popup(TR("Лодыжки!", "Your ankles!"), 0xFFFF6464);
}

// Fire near the holder: a steal; Jump: a block (and a tip at the rim); Aim held: the stance, sliding to the stick
void UpdateDefense() {
    CPlayerPed* cj = FindPlayerPed(0);
    CPad* pad = GetPad(0);
    if (!cj || !pad || cj->InVehicle()) return;
    uint64_t now = NowMs();
    UpdateTurbo(false);
    if (g_def.down == 1) {
        if (now - g_def.downMs >= 900) PlayFrom(kPedBlock, "getup", 4.0f, false, false, false), g_def.down = 2, g_def.downMs = now;
        return;
    }
    if (g_def.down == 2) {
        if (now - g_def.downMs >= 1400) g_def.down = 0, ClearTasks(false);
        return;
    }
    const CControllerState& st = pad->NewState;
    const CControllerState& was = pad->OldState;
    bool typing = ui::TheChat().IsOpen() || MenuActive();
    auto key = [&](int16_t n) { return !typing && n != 0; };
    bool steal = key(st.ButtonCircle) && was.ButtonCircle == 0;
    bool block = key(st.ButtonSquare) && was.ButtonSquare == 0;
    bool stance = key(st.RightShoulder1);
    float sx = typing ? 0.0f : st.LeftStickX / 128.0f;
    V3 me = ToV3(cj->GetPosition());
    if (steal && g_m.holder != kNobody && Opponent(g_m.holder) && now >= g_def.stealAt) {
        V3 p;
        if (PosOf(g_m.holder, p) && Flat(p, me) < kStealReach) {
            g_def.stealAt = now + 900;
            Writer w(proto::G_BASKET);
            w.U8(OP_STEAL), w.U32(g_m.id), w.U8(g_m.holder);
            SendRaw(w);
        }
    }
    if (block && now >= g_def.blockUntil) {
        Play(A_DEF_JUMP, 8.0f, false, false, false);
        g_def.blockUntil = now + 1170;
        g_def.tipUntil = now + 700;
        g_def.stance = g_def.stepping = false;
        Writer w(proto::G_BASKET);
        w.U8(OP_BLOCK), w.U32(g_m.id);
        SendRaw(w);
        return;
    }
    if (now < g_def.blockUntil) return;  // in the air
    if (stance) {
        if (!g_def.stance || (g_def.stepping && now >= g_def.stepUntil)) Play(A_DEF_LOOP, 6.0f, true, false, false);
        if (g_def.stepping && now >= g_def.stepUntil) g_def.stepping = false;
        g_def.stance = true;
        // facing the ball
        V3 at = g_m.court.rim;
        if (g_m.holder != kNobody) PosOf(g_m.holder, at);
        else if (g_m.flying) at = g_m.path.At((now - g_m.shotMs) / 1000.0f);
        g_me.heading = Deg(cj->GetHeading());
        float want = HeadingTo(me, at);
        Face(g_me.heading + AngleDiff(want, g_me.heading) * std::min(1.0f, 0.2f * TimeStep()));
        if (!g_def.stepping && std::fabs(sx) > 0.5f) {
            Play(sx > 0 ? A_DEF_STEPR : A_DEF_STEPL, 8.0f, false, true, false);
            g_def.stepping = true;
            g_def.stepUntil = now + 560;
        }
        return;
    }
    if (g_def.stance) {
        g_def.stance = g_def.stepping = false;
        ClearTasks(false);
    }
    // after our shot, when free and the stick let go
    if (g_me.react >= 0 && now < g_me.reactUntil && std::hypot(st.LeftStickX / 128.0f, st.LeftStickY / 128.0f) < 0.3f) {
        Play(g_me.react, 4.0f, false, false, false);
        g_me.react = -1;
    }
    if (now >= g_me.reactUntil) g_me.react = -1;
}

// ---------------------------------------------------------------- the judge
void ApplyState(uint16_t seq, uint16_t shots, uint8_t s0, uint8_t s1, uint8_t holder, uint8_t bits, uint8_t ev, uint8_t who,
                uint8_t value, uint16_t st0, uint16_t st1, uint8_t clock);

uint8_t ClockLeft() {
    if (g_m.practice || g_j.possTeam < 0 || !(g_m.holder != kNobody || (g_m.flying && g_m.pass))) return 255;
    uint64_t gone = NowMs() - g_j.clockStartMs;
    return (uint8_t)(gone >= kShotClockMs ? 0 : (kShotClockMs - gone + 999) / 1000);
}

void SendState() {
    if (!IsJudge()) return;
    Writer w(proto::G_BASKET);
    w.U8(OP_STATE), w.U32(g_m.id), w.U16(++g_m.seq), w.U16(g_m.shotNo);
    w.U8((uint8_t)g_m.score[0]), w.U8((uint8_t)g_m.score[1]), w.U8(g_m.holder);
    uint8_t bits = (uint8_t)((g_m.clear[0] ? 1 : 0) | (g_m.clear[1] ? 2 : 0) | (g_m.over ? 4 : 0));
    w.U8(bits);
    w.U8(g_j.ev), w.U8(g_j.evWho), w.U8(g_j.evValue);
    uint8_t clock = ClockLeft();
    w.U16((uint16_t)g_m.style[0]), w.U16((uint16_t)g_m.style[1]), w.U8(clock);
    SendRaw(w);
    // ours too, by the same way
    ApplyState(g_m.seq, g_m.shotNo, (uint8_t)g_m.score[0], (uint8_t)g_m.score[1], g_m.holder, bits, g_j.ev, g_j.evWho, g_j.evValue,
               (uint16_t)g_m.style[0], (uint16_t)g_m.style[1], clock);
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

// the ball to the other team (out, the shot clock): it must take it outside the arc
void Turnover(int from, uint8_t ev, uint8_t who) {
    Event(ev, who, 0);
    uint8_t to = PickFrom(1 - from);
    g_m.holder = to;
    g_m.flying = false;
    g_j.claimable = false;
    g_j.oopTo = kNobody;
    if (to != kNobody) g_m.clear[1 - from] = true;
    ++g_m.shotNo;  // what the old holder sent before this is old
    SendState();
}

void EndMatch(const char* why);

void JudgeTick() {
    if (!IsJudge() || g_m.over) {
        if (IsJudge() && g_m.over && NowMs() >= g_j.endAtMs) EndMatch("over");
        return;
    }
    uint64_t now = NowMs();
    // the shot reaches the rim: it counts (or not)
    if (g_m.flying && !g_m.pass && !g_j.rimDone && now >= g_j.rimAtMs) {
        g_j.rimDone = true;
        int team = g_j.shooterTeam;
        if (g_m.blocked) {
            g_j.claimable = true;
            Event(EV_BLOCK, g_j.blocker, 0);
        } else if (g_m.made && (g_j.counts || g_m.practice)) {
            int points = g_j.points;
            bool breaker = g_m.style[team] >= kBreaker;
            if (breaker) {
                points += 1;
                if (!g_m.practice) g_m.score[1 - team] = std::max(0, g_m.score[1 - team] - 1);
                g_m.style[team] = 0;
            } else {
                g_m.style[team] = std::min(kBreaker, g_m.style[team] + g_j.style);
            }
            g_m.score[team] += points;
            Event(breaker ? EV_BREAKER : EV_SCORE, g_m.shooter,
                  (uint8_t)(std::min(points, 15) | (g_m.dunk ? kValDunk : 0) | (g_j.oopShot ? kValOop : 0)));
            if (g_m.practice) {
                ++g_m.baskets;
                g_j.claimable = true;
            } else if (g_m.score[team] >= g_m.target && g_m.score[team] - g_m.score[1 - team] >= 2) {
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
        int t = TeamOf(g_m.holder);
        if (t >= 0) g_m.clear[t] = true;
        g_j.handTo = kNobody;
        g_j.claimable = false;
        SendState();
    }
    if (g_m.practice) {
        if (now - g_j.stateMs >= kStateEveryMs) SendState();
        return;
    }
    // the holder: clear of the arc, or out
    if (g_m.holder != kNobody) {
        V3 p;
        int team = TeamOf(g_m.holder);
        if (team >= 0 && PosOf(g_m.holder, p)) {
            float d = Flat(p, g_m.court.rim);
            if (d > kOut || Front(p) < -0.8f) {
                Turnover(team, EV_OUT, g_m.holder);
            } else if (g_m.clear[team] && d > kThreeArc) {
                g_m.clear[team] = false;
                SendState();
            }
        }
    }
    // the shot clock: whose ball, and for how long without a shot
    int poss = g_m.holder != kNobody ? TeamOf(g_m.holder) : (g_m.flying && g_m.pass ? TeamOf(g_m.shooter) : -1);
    if (poss >= 0 && poss != g_j.possTeam) {
        g_j.possTeam = poss;
        g_j.clockStartMs = now;
        SendState();
    }
    if (poss >= 0 && now - g_j.clockStartMs >= kShotClockMs) {
        LOGI("Basketball: the shot clock ran out on team %d", poss);
        g_j.possTeam = 1 - poss;
        g_j.clockStartMs = now;
        Turnover(poss, EV_CLOCK, kNobody);
    }
    if (now - g_j.stateMs >= kStateEveryMs) SendState();
}

// the ball is that one's to shoot or pass: it holds it, or it flies to the rim for that one's alley-oop
bool JudgeAccepts(uint8_t from) {
    return g_m.holder == from || (g_m.holder == kNobody && g_m.flying && g_m.pass && g_j.oopTo == from);
}

void JudgeShot(uint8_t from, int points, int style, bool blocked, uint8_t blocker, bool oop) {
    if (!IsJudge() || g_m.over) return;
    int team = TeamOf(from);
    if (team < 0) return;
    g_j.shooterTeam = (uint8_t)team;
    g_j.points = points;
    g_j.style = style;
    g_j.counts = !g_m.clear[team];
    g_j.blocker = blocker;
    g_j.oopShot = oop;
    g_j.rimAtMs = blocked ? NowMs() : g_m.shotMs + (uint64_t)(g_m.path.RimTime() * 1000.0f);
    g_j.rimDone = false;
    g_j.claimable = false;
    g_j.oopTo = kNobody;
    g_j.handTo = kNobody;
    if (g_m.practice) ++g_m.tries;
    SendState();
}

void JudgePass(uint8_t oopTo) {
    if (!IsJudge() || g_m.over) return;
    g_j.claimable = true;
    g_j.rimDone = true;
    g_j.oopTo = oopTo;
    g_j.handTo = kNobody;
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
    int team = TeamOf(who);
    if (team < 0) return;
    g_m.holder = who;
    g_m.flying = false;
    g_j.claimable = false;
    g_j.oopTo = kNobody;
    if (!g_m.practice) {
        if (g_m.pass) {
            // caught by the other team: intercepted
            if (team != TeamOf(g_m.shooter)) g_m.clear[team] = true, Event(EV_STEAL, who, 1);
        } else {
            bool defence = team != g_j.shooterTeam;
            g_m.clear[team] = defence;
            Event(EV_REBOUND, who, defence ? 1 : 0);
            g_j.possTeam = team;
            g_j.clockStartMs = NowMs();  // a new 24 after a shot
        }
    }
    SendState();
}

void JudgeSteal(uint8_t thief, uint8_t victim, uint16_t no) {
    if (!IsJudge() || g_m.over || g_m.holder != victim) return;
    int team = TeamOf(thief);
    if (team < 0) return;
    g_m.holder = thief;
    g_m.flying = false;
    g_m.shotNo = std::max(g_m.shotNo, no);
    g_m.clear[team] = true;
    Event(EV_STEAL, thief, 0);
    SendState();
}

// ---------------------------------------------------------------- every game
void Cleanup() {
    LetGo(true);
    if (g_def.stance || g_def.down || NowMs() < g_def.blockUntil) ClearTasks(true);
    PadSync(false, false);
    BallDelete();
    if (g_m.on) {
        NoBasketball() = 0;
        Cmd(0x04EF).S(kBlock).Run();  // REMOVE_ANIMATION
        Cmd(0x04EF).S(kThrowBlock).Run();
        MarkModelNotNeeded(BallModel());
    }
    g_modelAsked = false;
    g_animsReady = false;  // the block may go: found again next time
    g_m = Match{};
    g_j = Judge{};
    g_me = Mine{};
    g_def = Def{};
    g_jumps.clear();
    g_feed.clear();
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

void Begin(uint32_t id, uint8_t judge, const Court& c, int target, uint8_t rules, bool practice,
           const std::map<uint8_t, uint8_t>& teams, uint8_t holder) {
    if (g_m.on) Cleanup();
    g_m.on = true;
    g_m.id = id;
    g_m.judge = judge;
    g_m.court = c;
    g_m.target = target;
    g_m.rules = rules;
    g_m.practice = practice;
    g_m.team = teams;
    g_m.playing = teams.count(Me()) != 0;
    g_m.holder = holder;
    auto it = teams.find(holder);
    if (it != teams.end() && !practice) g_m.clear[it->second] = true;
    NoBasketball() = 1;
    Assets();
    if (g_m.playing) {
        if (FindPlayerPed(0)) Cmd(0x01B9).I(CjRef()).I(0).Run();  // fists: the right mouse button is ours
        std::string blue, red;
        for (auto& [pid, t] : teams) (t ? red : blue) += (t ? red : blue).empty() ? Name(pid) : ", " + Name(pid);
        ui::TheChat().AddSystem(practice ? TR("Тренировка. Все команды: /basquete comandos. Выйти: /basquete sair.",
                                              "Practice. All the commands: /basquete comandos. Leave with /basquete sair.")
                                         : Format(TR("Баскетбол до %d (%s): синие (%s) против красных (%s). Команды: /basquete comandos.",
                                                     "Basketball to %d (%s): blue (%s) against red (%s). Commands: /basquete comandos."),
                                                  target, RulesName(rules), blue.c_str(), red.c_str()));
        Show(practice ? TR("Тренировка", "Practice") : TR("Баскетбол!", "Basketball!"), 0xFFFFC864);
    }
    LOGI("Basketball: game %08X on, judge %u, to %d, rules %u, %zu players%s", id, judge, target, rules, teams.size(),
         practice ? " (practice)" : "");
}

void SendShot(const V3& from, bool made, bool dunk, int points, uint8_t blocker, bool oop) {
    Writer w(proto::G_BASKET);
    w.U8(OP_SHOT), w.U32(g_m.id), w.U16((uint16_t)(g_m.shotNo + 1)), w.U8(Me());
    WriteV3(w, from);
    w.U8((uint8_t)((made ? 1 : 0) | (dunk ? 2 : 0) | (blocker != kNobody ? 4 : 0) | (oop ? 8 : 0)));
    w.U32(Random32()), w.U8((uint8_t)points), w.U16((uint16_t)std::min(g_me.style, 65535)), w.U8(blocker);
    g_me.style = 0;
    Deliver(w);
}

void ApplyState(uint16_t seq, uint16_t shots, uint8_t s0, uint8_t s1, uint8_t holder, uint8_t bits, uint8_t ev, uint8_t who,
                uint8_t value, uint16_t st0, uint16_t st1, uint8_t clock) {
    bool stale = shots < g_m.shotNo;  // sent before the judge had our latest shot: its holder is old
    if (!IsJudge()) g_m.seq = seq, g_m.shotNo = std::max(g_m.shotNo, shots);  // and its turnovers count too
    g_m.score[0] = s0, g_m.score[1] = s1;
    g_m.clear[0] = bits & 1, g_m.clear[1] = (bits & 2) != 0;
    g_m.over = (bits & 4) != 0;
    g_m.style[0] = st0, g_m.style[1] = st1;
    g_m.clock = clock == 255 ? -1 : clock;
    g_m.clockMs = NowMs();
    if (!stale) {
        if (holder != kNobody) g_m.flying = false;
        g_m.holder = holder;
    }
    if (who == Me() && g_m.playing) {
        if ((ev == EV_SCORE || ev == EV_BREAKER) && !(value & kValDunk)) g_me.react = A_REACT_SCORE, g_me.reactUntil = NowMs() + 1500;
        if (ev == EV_MISS) g_me.react = A_REACT_MISS, g_me.reactUntil = NowMs() + 1200;
    }
    if (!g_m.playing && !Watching()) return;
    int points = value & 0x0F;
    switch (ev) {
    case EV_SCORE:
        if (value & kValOop) Show(Format(TR("Аллей-уп! %s +%d", "Alley-oop! %s +%d"), Name(who).c_str(), points), 0xFFFFC864);
        else if (value & kValDunk) Show(Format(TR("Данк! %s +%d", "Dunk! %s +%d"), Name(who).c_str(), points), 0xFFFFC864);
        else Show(Format(TR("+%d: %s!", "+%d: %s!"), points, Name(who).c_str()), 0xFFFFC864);
        break;
    case EV_BREAKER: {
        int t = TeamOf(who);
        Show(Format(TR("GAMEBREAKER! %s +%d, сопернику -1", "GAMEBREAKER! %s +%d, the others -1"), Name(who).c_str(), points),
             t >= 0 ? kTeamColour[t] : 0xFFFFC864, 3500);
        break;
    }
    case EV_NOCLEAR:
        Show(TR("Не засчитано: сначала выведите мяч за дугу", "No points: take the ball outside the arc first"), 0xFFFF8C3C);
        break;
    case EV_OUT:
        Show(TR("Аут", "Out of bounds"), 0xFFFF8C3C);
        break;
    case EV_CLOCK:
        Show(TR("Время атаки вышло", "Shot clock violation"), 0xFFFF8C3C);
        break;
    case EV_REBOUND:
        Show(Format(TR("Подбор: %s", "Rebound: %s"), Name(who).c_str()), 0xFFFFFFFF, 1500);
        break;
    case EV_STEAL:
        Show(Format(value ? TR("Перехват: %s!", "Intercepted: %s!") : TR("Отбор: %s!", "Stolen: %s!"), Name(who).c_str()),
             0xFFFFFFFF, 1800);
        break;
    case EV_BLOCK:
        Show(Format(TR("Блок-шот: %s!", "Blocked by %s!"), Name(who).c_str()), 0xFFFFFFFF, 1800);
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
        uint8_t rules = r.U8();
        CPlayerPed* me = FindPlayerPed(0);
        if (!r.Ok() || !me || g_m.on || !core::IsPlayerReady() || me->InVehicle() || me->AreaCode() != 0) return;
        if (Flat(ToV3(me->GetPosition()), c.rim) > kInviteReach) return;
        g_invited = Invite{};
        g_invited.id = id, g_invited.from = from, g_invited.court = c, g_invited.target = target, g_invited.rules = rules;
        g_invited.until = NowMs() + kAnswerMs;
        ui::TheChat().AddSystem(Format(TR("%s зовёт играть в баскетбол до %d (%s): Y - да, N - нет.",
                                          "%s asks you to play basketball to %d (%s): Y yes, N no."),
                                       Net().PeerName(from).c_str(), target, RulesName(rules)));
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
        int target = r.U8();
        uint8_t rules = r.U8();
        int n = r.U8();
        std::map<uint8_t, uint8_t> teams;
        for (int i = 0; i < n && i < 8; ++i) {
            uint8_t pid = r.U8(), t = r.U8();
            teams[pid] = t & 1;
        }
        uint8_t holder = r.U8();
        if (!r.Ok()) return;
        Begin(id, from, c, target, rules, false, teams, holder);
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
        uint16_t st0 = r.U16(), st1 = r.U16();
        uint8_t clock = r.U8();
        if (r.Ok() && from == g_m.judge) ApplyState(seq, shots, s0, s1, holder, bits, ev, who, value, st0, st1, clock);
        return;
    }
    case OP_SHOT: {
        uint16_t no = r.U16();
        uint8_t shooter = r.U8();
        Shot s;
        s.from = ReadV3(r);
        uint8_t bits = r.U8();
        s.seed = r.U32();
        int points = r.U8();
        int style = r.U16();
        uint8_t blocker = r.U8();
        if (!r.Ok() || shooter != from) return;
        if (IsJudge() && !g_m.over && !JudgeAccepts(shooter)) {
            // not its ball (stolen, out, its alley-oop taken): the others hear who has it, after this
            LOGW("Basketball: shot of %u, whose ball it is not (it is %u's)", shooter, g_m.holder);
            g_m.shotNo = std::max(g_m.shotNo, no);
            SendState();
            return;
        }
        s.made = bits & 1, s.dunk = (bits & 2) != 0;
        bool blocked = (bits & 4) != 0;
        g_m.shotNo = std::max(g_m.shotNo, no);
        g_m.flying = true;
        g_m.path = blocked ? BallPath::Knock(g_m.court, s.from, s.seed) : BallPath(g_m.court, s);
        g_m.shotMs = NowMs();
        g_m.shooter = shooter;
        g_m.made = s.made, g_m.dunk = s.dunk, g_m.blocked = blocked;
        g_m.pass = g_m.oop = false;
        g_m.holder = kNobody;
        JudgeShot(shooter, points, style, blocked, blocker, (bits & 8) != 0);
        return;
    }
    case OP_PASS: {
        uint16_t no = r.U16();
        uint8_t passer = r.U8(), kind = r.U8(), to = r.U8();
        bool oop = r.U8() != 0;
        V3 a = ReadV3(r), via = ReadV3(r), dest = ReadV3(r);
        float s1 = r.F32(), s2 = r.F32();
        uint32_t seed = r.U32();
        int style = r.U16();
        if (!r.Ok() || passer != from || !(s1 > 0.0f && s1 < 3.0f) || !(s2 >= 0.0f && s2 < 3.0f)) return;
        if (IsJudge() && !g_m.over && !JudgeAccepts(passer)) {
            LOGW("Basketball: pass of %u, whose ball it is not (it is %u's)", passer, g_m.holder);
            g_m.shotNo = std::max(g_m.shotNo, no);
            SendState();
            return;
        }
        bool bank = kind == P_BOARD || kind == P_HEAD;
        g_m.shotNo = std::max(g_m.shotNo, no);
        g_m.flying = true;
        g_m.path = bank ? BallPath::Bank(a, via, dest, s1, s2, g_m.court.floorZ, true, seed)
                        : BallPath::Toss(a, dest, s1, g_m.court.floorZ, true, seed);
        g_m.shotMs = NowMs();
        g_m.shooter = passer;
        g_m.made = g_m.dunk = g_m.blocked = false;
        g_m.pass = true, g_m.oop = oop, g_m.passTo = to, g_m.carry = style;
        g_m.holder = kNobody;
        if (IsJudge()) JudgePass(oop ? to : kNobody);
        // an alley-oop to us: up to the rim to meet it
        if (oop && to == Me() && passer != Me() && g_m.playing && g_me.move == Move::None && !g_def.down && OopOk(CjPos())) {
            CPlayerPed* cj = FindPlayerPed(0);
            if (cj && !cj->InVehicle()) {
                g_def = Def{};
                StartDunk(T_OOP, NowMs() + (uint64_t)((s1 + s2) * 1000.0f));
                g_me.style = TeamOf(passer) == TeamOf(Me()) ? style : 0;
                std::fill(std::begin(g_me.count), std::end(g_me.count), 0);
            }
        }
        return;
    }
    case OP_CLAIM: {
        uint16_t shot = r.U16();
        if (r.Ok()) JudgeClaim(from, shot);
        return;
    }
    case OP_TRICK: {
        uint8_t kind = r.U8();
        int pts = r.U16();
        int mult = r.U8();
        if (!r.Ok() || from == Me() || kind >= T_COUNT || !Watching()) return;
        int t = TeamOf(from);
        Feed(mult > 1 ? Format("%s: %s +%d  x%d", Name(from).c_str(), TrickName(kind), pts, mult)
                      : Format("%s: %s +%d", Name(from).c_str(), TrickName(kind), pts),
             t >= 0 ? kTeamColour[t] : 0xFFFFFFFF);
        return;
    }
    case OP_ANKLE: {
        uint8_t victim = r.U8();
        bool left = r.U8() != 0;
        if (!r.Ok()) return;
        if (victim == Me() && g_m.playing) Fall(left);
        if (Watching())
            Show(Format(TR("%s сломал лодыжки %s!", "%s broke %s's ankles!"), Name(from).c_str(), Name(victim).c_str()), 0xFFFFC864);
        return;
    }
    case OP_STEAL: {
        uint8_t target = r.U8();
        if (r.Ok() && target == Me() && g_m.playing) ResolveSteal(from);
        return;
    }
    case OP_STOLE: {
        uint16_t no = r.U16();
        uint8_t thief = r.U8();
        bool ok = r.U8() != 0;
        if (!r.Ok()) return;
        if (ok) {
            if (IsJudge()) JudgeSteal(thief, from, no);
            g_m.shotNo = std::max(g_m.shotNo, no);
            if (g_m.holder == from) g_m.holder = thief, g_m.flying = false;  // the judge will say so too
        }
        if (thief == Me()) Popup(ok ? TR("Отбор!", "Steal!") : TR("Мимо", "Missed the steal"), ok ? 0xFF50E66E : 0xFFDCDCE6);
        return;
    }
    case OP_BLOCK:
        if (from != Me()) {
            g_jumps.push_back({from, NowMs()});
            if (g_jumps.size() > 8) g_jumps.erase(g_jumps.begin());
        }
        return;
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
    w.U8((uint8_t)g_inviting.target), w.U8(g_inviting.rules), w.U8((uint8_t)teams.size());
    for (auto& [pid, t] : teams) w.U8(pid), w.U8(t);
    w.U8(holder);
    Net().Send(proto::CH_RELIABLE, w);
    Begin(g_inviting.id, Me(), g_inviting.court, g_inviting.target, g_inviting.rules, false, teams, holder);
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
    std::vector<std::string> a;
    for (std::string s : args) {
        for (char& ch : s) ch = (char)tolower((unsigned char)ch);
        a.push_back(s);
    }
    std::string first = a.empty() ? "" : a[0];
    if (first == "comandos" || first == "ajuda" || first == "help" || first == "commands") {
        g_helpUntil = NowMs() < g_helpUntil ? 0 : NowMs() + kHelpMs;
        return true;
    }
    if (first == "sair" || first == "leave" || first == "exit") {
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
    if (first == "treino" || first == "practice" || !Net().Online()) {
        std::map<uint8_t, uint8_t> teams{{Me(), 0}};
        Begin(Random32(), Me(), c, 0, 0, true, teams, Me());
        return true;
    }
    // /basquete [nba] [points]
    uint8_t rules = 0;
    size_t i = 0;
    if (first == "nba") rules = RULE_NBA, i = 1;
    int target = i < a.size() ? atoi(a[i].c_str()) : 0;
    target = target > 0 ? std::clamp(target, 5, 31) : 21;
    Invite inv;
    inv.id = Random32() | 1u;
    inv.from = Me(), inv.court = c, inv.target = target, inv.rules = rules, inv.until = NowMs() + kAnswerMs;
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
    w.U8((uint8_t)target), w.U8(rules);
    Net().Send(proto::CH_RELIABLE, w);
    ui::TheChat().AddSystem(Format(TR("Приглашение в баскетбол до %d (%s) отправлено игрокам рядом.",
                                      "Basketball to %d (%s): the players near the hoop are asked."),
                                   target, RulesName(rules)));
    LOGI("Basketball: invitation %08X to %zu players, to %d, rules %u", inv.id, inv.asked.size(), target, rules);
    return true;
}

// ---------------------------------------------------------------- each frame
// a loose or passed ball within our reach: ask the judge for it. A pass of ours: once it came back; one to us: a wider
// catch; a missed shot over the rim: in reach of a block's jump (a tip)
void TryClaim(CPlayerPed* cj) {
    if (!g_m.playing || g_m.holder != kNobody || !cj || g_def.down || g_me.move == Move::Dunk) return;
    uint64_t now = NowMs();
    float t = (now - g_m.shotMs) / 1000.0f;
    V3 b = g_m.path.At(t), p = ToV3(cj->GetPosition());
    float flat = Flat(b, p), h = g_m.path.Height(t);
    bool can = false, tip = false, jumping = now < g_def.tipUntil;
    if (g_m.pass) {
        if (t >= (g_m.shooter == Me() ? g_m.path.RimTime() : 0.1f)) {
            if (g_m.passTo == Me() && !g_m.oop) can = flat < kCatch && h < kCatchHeight;
            else can = flat < kGrab && h < kGrabHeight;
            if (!can && g_m.oop && jumping && flat < 1.6f && h < 3.4f) can = true;  // up at the alley-oop
        }
    } else if (t >= g_m.path.RimTime() && !(g_m.made && !g_m.practice)) {
        can = flat < kGrab && h < kGrabHeight;
        if (!can && jumping && !g_m.made && !g_m.blocked && t < g_m.path.RimTime() + 1.2f && flat < 1.6f && h < 3.4f) can = tip = true;
    }
    if (!can || (g_me.claimedShot == g_m.shotNo && now - g_me.claimMs <= 600)) return;
    g_me.claimedShot = g_m.shotNo;
    g_me.claimMs = now;
    g_me.claimTip = tip;
    if (IsJudge()) return JudgeClaim(Me(), g_m.shotNo);
    Writer w(proto::G_BASKET);
    w.U8(OP_CLAIM), w.U32(g_m.id), w.U16(g_m.shotNo);
    SendRaw(w);
}

// the ball lay still on the floor (the frame before it became someone's): picked up, not caught
bool g_wasResting = false;

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
    uint64_t now = NowMs();
    while (!g_jumps.empty() && now - g_jumps.front().ms > 1500) g_jumps.erase(g_jumps.begin());
    uint8_t me = Me();
    CPlayerPed* cj = FindPlayerPed(0);
    // our player with the ball, or not any more
    bool mine = g_m.playing && g_m.holder == me;
    if (mine && g_me.move == Move::None) Become(g_wasResting);
    if (!mine && g_me.move != Move::None && g_me.move != Move::Shoot && g_me.move != Move::Dunk && g_me.move != Move::Pass) LetGo(false);
    PadSync(g_m.playing, g_m.playing && (mine || g_me.move != Move::None));
    if (g_me.move != Move::None) UpdateMine();
    else if (g_m.playing) UpdateDefense();
    // the ball
    if (!watch) return BallDelete();
    if (g_m.holder != kNobody && !g_m.flying) {
        BallHeld(PedOf(g_m.holder));
        g_wasResting = false;
    } else if (g_m.flying || g_m.shotNo) {
        BallAt(g_m.path.At((now - g_m.shotMs) / 1000.0f));
        g_wasResting = now - g_m.shotMs > (uint64_t)(g_m.path.RestTime() * 1000.0f);
        TryClaim(cj);
    }
}

// ---------------------------------------------------------------- the screen
// /basquete comandos: what to type and which keys, in a dark box in the middle
void RenderHelp() {
    struct Row {
        const char* key;
        const char* what;
    };
    struct Part {
        const char* title;
        std::vector<Row> rows;
    };
    const Part parts[] = {
        {TR("В чате", "In the chat"),
         {{"/basquete", TR("позвать игроков у кольца: уличные правила, до 21", "ask the players near the hoop: street rules, to 21")},
          {"/basquete 11", TR("до 11 очков (любое число от 5 до 31)", "to 11 points (any number from 5 to 31)")},
          {"/basquete nba", TR("правила НБА: 2 и 3 очка (и /basquete nba 15)", "NBA rules: 2 and 3 points (and /basquete nba 15)")},
          {"/basquete treino", TR("тренировка в одиночку", "practise alone")},
          {"/basquete sair", TR("выйти из игры", "leave the game")},
          {"/basquete comandos", TR("показать или скрыть этот список", "show or hide this list")},
          {"Y / N", TR("принять или отклонить приглашение", "say yes or no to an invitation")}}},
        {TR("С мячом", "With the ball"),
         {{"W A S D", TR("вести мяч", "dribble around")},
          {TR("Пробел", "Space"), TR("бег (тратит турбо)", "run (uses turbo)")},
          {TR("ЛКМ", "Left mouse"), TR("бросок: держите и отпустите у отметки (или нажмите дважды)", "shoot: hold, let go at the mark (or tap twice)")},
          {TR("ЛКМ на бегу у кольца", "Left mouse running at the rim"), TR("данк до 3,5 м; с турбо - до 5,5 м", "dunk from up to 3.5 m; on turbo up to 5.5 m")},
          {"Shift", TR("кроссовер; на бегу с Пробелом - разворот; стоя - шоу", "crossover; running on Space: spin; standing: showboat")},
          {"C", TR("ложный бросок (или отмена броска в прыжке)", "pump fake (or cancel a jump shot in the air)")},
          {TR("ПКМ", "Right mouse"), TR("пас партнёру; без партнёра - от щита себе", "pass to your teammate; with none, off the board to you")},
          {TR("ПКМ + Пробел", "Right mouse + Space"), TR("аллей-уп партнёру у кольца, или от головы соперника", "alley-oop to a mate at the rim, or off a defender's head")}}},
        {TR("Без мяча", "Without the ball"),
         {{TR("ЛКМ", "Left mouse"), TR("отбор рядом с игроком с мячом (лучше во время финта)", "steal next to the holder (best during his trick)")},
          {"Shift", TR("блок-шот: прыжок на бросок; у кольца - добивание", "block: jump at the shot; at the rim: tip it in")},
          {TR("ПКМ (держать)", "Right mouse (hold)"), TR("защитная стойка, A / D - шаг в сторону", "defensive stance, A / D to slide")},
          {TR("Мяч в воздухе", "Ball in the air"), TR("подбор или перехват: подойдите к нему", "rebound or interception: walk into it")}}},
    };
    const char* const rules[] = {
        TR("Уличные: 1 очко из-под дуги, 2 из-за дуги; НБА: 2 и 3. Победа с разницей в 2.",
           "Street: 1 point inside the arc, 2 outside; NBA: 2 and 3. Win by 2."),
        TR("Финты и пасы копят стиль, он идёт в шкалу с попаданием.", "Tricks and passes build style, banked when the basket drops."),
        TR("Полная шкала: следующее попадание - GAMEBREAKER (+1 вам, -1 сопернику).",
           "A full meter: your next basket is a GAMEBREAKER (+1 for you, -1 for them)."),
        TR("24 секунды на атаку. После попадания или подбора в защите - мяч за дугу.",
           "24 seconds to shoot. After a basket or a defensive rebound: outside the arc first."),
        TR("Мяч дальше 12 м от кольца - аут, мяч у соперника.", "The ball over 12 m from the rim is out: the other team's."),
    };
    auto& g = ui::Gfx();
    float s = g.Scale(), lh = g.LineHeight(ui::FONT_SMALL), th = g.LineHeight(ui::FONT_MENU);
    float keyW = 0.0f, whatW = 0.0f;
    size_t rows = 0;
    for (const Part& p : parts)
        for (const Row& r : p.rows) {
            keyW = std::max(keyW, g.TextWidth(ui::FONT_SMALL, r.key));
            whatW = std::max(whatW, g.TextWidth(ui::FONT_SMALL, r.what));
            ++rows;
        }
    for (const char* r : rules) whatW = std::max(whatW, g.TextWidth(ui::FONT_SMALL, r) - keyW - 24 * s);
    size_t sections = std::size(parts) + 1;
    float pad = 22 * s, w = keyW + 24 * s + whatW + 2 * pad;
    float h = pad * 2 + th + 8 * s + (float)(rows + std::size(rules)) * lh + (float)sections * (lh + 6 * s);
    float x = (g.Width() - w) * 0.5f, y = std::max((g.Height() - h) * 0.5f, 4 * s);
    g.Rect(x, y, w, h, ui::Argb(210, 0, 0, 0));
    g.RectOutline(x, y, w, h, ui::Argb(120, 255, 200, 100), 1.5f * s);
    float cy = y + pad;
    g.Text(ui::FONT_MENU, g.Width() * 0.5f, cy, TR("БАСКЕТБОЛ - КОМАНДЫ", "BASKETBALL - COMMANDS"), 0xFFFFC864,
           ui::TEXT_CENTER | ui::TEXT_SHADOW);
    cy += th + 8 * s;
    auto section = [&](const char* title) {
        g.Text(ui::FONT_SMALL, x + pad, cy, title, 0xFF8CB4FF, ui::TEXT_SHADOW);
        cy += lh + 6 * s;
    };
    for (const Part& p : parts) {
        section(p.title);
        for (const Row& r : p.rows) {
            g.Text(ui::FONT_SMALL, x + pad, cy, r.key, 0xFFFFFFFF, ui::TEXT_SHADOW);
            g.Text(ui::FONT_SMALL, x + pad + keyW + 24 * s, cy, r.what, 0xFFC8C8D2, ui::TEXT_SHADOW);
            cy += lh;
        }
    }
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
            else if (g_m.flying && g_m.pass && (now - g_m.shotMs) / 1000.0f < g_m.path.RimTime()) sub += TR("  ·  пас", "  ·  pass");
            else sub += g_m.flying && (now - g_m.shotMs) / 1000.0f < g_m.path.RimTime() ? TR("  ·  бросок", "  ·  shot in the air")
                                                                                       : TR("  ·  мяч свободен", "  ·  loose ball");
            if (g_m.clock >= 0) {
                int left = std::max(0, g_m.clock - (int)((now - g_m.clockMs) / 1000));
                sub += Format(TR("  ·  атака %d", "  ·  shot clock %d"), left);
            }
        }
        float lw = g.TextWidth(ui::FONT_MENU, line), sw = sub.empty() ? 0.0f : g.TextWidth(ui::FONT_SMALL, sub);
        float w = std::max(std::max(lw, sw) + 40 * s, 360 * s), h = g.LineHeight(ui::FONT_MENU) + (sub.empty() ? 0 : g.LineHeight(ui::FONT_SMALL)) + 18 * s;
        float x = (g.Width() - w) * 0.5f, y = 8 * s;
        g.Rect(x, y, w, h, ui::Argb(170, 8, 10, 18));
        if (!g_m.practice) {
            g.Rect(x, y, 6 * s, h, kTeamColour[0]);
            g.Rect(x + w - 6 * s, y, 6 * s, h, kTeamColour[1]);
        }
        g.Text(ui::FONT_MENU, g.Width() * 0.5f, y + 8 * s, line, 0xFFFFFFFF, ui::TEXT_CENTER | ui::TEXT_SHADOW);
        if (!sub.empty())
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, y + 10 * s + g.LineHeight(ui::FONT_MENU), sub, 0xFFC8C8D2, ui::TEXT_CENTER | ui::TEXT_SHADOW);
        // the Gamebreaker meters: blue from the middle to the left, red to the right (practice: one, the whole width)
        float by = y + h + 2 * s, bh = 5 * s;
        auto meter = [&](float bx, float bw, int v, uint32_t colour, bool rightToLeft) {
            float fill = bw * std::min(1.0f, v / (float)kBreaker);
            bool full = v >= kBreaker;
            g.Rect(bx, by, bw, bh, ui::Argb(150, 0, 0, 0));
            uint32_t c = full && (now / 250) % 2 ? 0xFFFFFFFF : colour;
            g.Rect(rightToLeft ? bx + bw - fill : bx, by, fill, bh, c);
        };
        if (g_m.practice) meter(x, w, g_m.style[0], 0xFFFFC864, false);
        else meter(x, w * 0.5f - 1 * s, g_m.style[0], kTeamColour[0], true), meter(x + w * 0.5f + 1 * s, w * 0.5f - 1 * s, g_m.style[1], kTeamColour[1], false);
        float yy = by + bh + 4 * s;
        for (int t = 0; t < (g_m.practice ? 1 : 2); ++t)
            if (g_m.style[t] >= kBreaker) {
                g.Text(ui::FONT_SMALL, g.Width() * 0.5f, yy,
                       g_m.practice ? TR("GAMEBREAKER готов: следующее попадание", "GAMEBREAKER ready: your next basket")
                                    : Format(TR("GAMEBREAKER у %s: следующее попадание", "GAMEBREAKER ready for %s: the next basket"),
                                             t ? TR("красных", "Red") : TR("синих", "Blue")),
                       g_m.practice ? 0xFFFFC864 : kTeamColour[t], ui::TEXT_CENTER | ui::TEXT_OUTLINE);
                yy += g.LineHeight(ui::FONT_SMALL);
            }
        // what our team must do
        int mt = TeamOf(Me());
        if (g_m.playing && mt >= 0 && g_m.holder == Me() && g_m.clear[mt] && !g_m.practice) {
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, yy, TR("Выведите мяч за дугу", "Take the ball outside the arc"), 0xFFFFB450,
                   ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            yy += g.LineHeight(ui::FONT_SMALL);
        }
        // the others' tricks
        for (const FeedLine& f : g_feed)
            if (now < f.until) {
                g.Text(ui::FONT_SMALL, g.Width() * 0.5f, yy, f.text, f.colour, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
                yy += g.LineHeight(ui::FONT_SMALL);
            }
        // the keys, the turbo and the possession's style
        if (g_m.playing && g_me.move != Move::Shoot && g_me.move != Move::Dunk) {
            bool ball = g_m.holder == Me();
            g.Text(ui::FONT_SMALL, g.Width() * 0.5f, g.Height() - 40 * s,
                   ball ? TR("ЛКМ бросок  ·  Пробел турбо  ·  Shift финт  ·  C ложный бросок  ·  ПКМ пас  ·  /basquete comandos",
                             "Left mouse shoot  ·  Space turbo  ·  Shift trick  ·  C fake  ·  Right mouse pass  ·  /basquete comandos")
                        : TR("ЛКМ отбор  ·  Shift блок-шот  ·  ПКМ стойка  ·  /basquete comandos",
                             "Left mouse steal  ·  Shift block  ·  Right mouse stance  ·  /basquete comandos"),
                   0xFFDCDCE6, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
            if (ball || g_me.turbo < 0.999f) {
                float tw = 180 * s, tx = (g.Width() - tw) * 0.5f, ty = g.Height() - 52 * s;
                g.Rect(tx - 2 * s, ty - 2 * s, tw + 4 * s, 7 * s + 4 * s, ui::Argb(150, 0, 0, 0));
                g.Rect(tx, ty, tw * g_me.turbo, 7 * s, g_me.tired ? 0xFFFF6464 : 0xFFFFB43C);
                g.Text(ui::FONT_SMALL, tx - 8 * s, ty - 6 * s, TR("Турбо", "Turbo"), 0xFFDCDCE6, ui::TEXT_RIGHT | ui::TEXT_OUTLINE);
            }
            if (ball && g_me.style > 0)
                g.Text(ui::FONT_SMALL, g.Width() * 0.5f, g.Height() - 74 * s,
                       g_me.mult > 1 && now - g_me.lastTrickMs < kComboMs ? Format(TR("Стиль %d  ·  комбо x%d", "Style %d  ·  combo x%d"), g_me.style, g_me.mult)
                                                                           : Format(TR("Стиль %d", "Style %d"), g_me.style),
                       0xFFFFC864, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
        }
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
               g_me.held ? TR("Отпустите ЛКМ у отметки (C - отмена)", "Let go of the left mouse button at the mark (C cancels)")
                         : TR("Нажмите ЛКМ у отметки (C - отмена)", "Press the left mouse button at the mark (C cancels)"),
               0xFFDCDCE6, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    }
    if (g_me.timing >= 0 && now - g_me.timingMs < 1400) {
        static const uint32_t kc[3] = {0xFFFF6464, 0xFFFFD250, 0xFF50E66E};
        const char* t = g_me.timing == 2 ? TR("Идеально!", "Perfect!") : g_me.timing == 1 ? TR("Хорошо", "Good") : TR("Плохо", "Bad");
        g.Text(ui::FONT_NORMAL, g.Width() * 0.5f, g.Height() * 0.72f - 34 * s, t, kc[g_me.timing], ui::TEXT_CENTER | ui::TEXT_OUTLINE);
    }
    // our own trick
    if (now < g_me.popupUntil && !g_me.popup.empty())
        g.Text(ui::FONT_NORMAL, g.Width() * 0.5f, g.Height() * 0.64f, g_me.popup, g_me.popupColour, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
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
        // the world goes, and the ball with it: no commands now
        if (g_m.on) NoBasketball() = 0;
        PadSync(false, false);
        g_ball = {};
        g_me = Mine{};
        g_def = Def{};
        g_m = Match{};
        g_j = Judge{};
        g_jumps.clear();
        g_feed.clear();
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
