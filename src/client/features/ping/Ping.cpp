// Quick marks ("pings"): the key (keys/ping in coop.ini, Z) marks what the aim points at (the crosshair's place on the
// screen, where it shows when aiming; the middle in a first-person view and in a vehicle): a place, a car or a person
// (one near the line counts too). Every player sees it for 20 s in the colour of who made it (its mark on the radar,
// chosen in F9): on the screen with that player's name in that colour and the distance (at the screen's edge with an
// arrow when out of view), and on the radar and the map. A car or person marked is followed: the game that made the mark
// sends where it is (the others may not have that car or person, as with the street's own people); a player is found
// in each game. One mark per player: a new one takes the place of the last; the key twice quickly takes ours away.
#include "features/ping/Ping.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "common/api/protocol/Protocol.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::ping {

namespace {
using namespace game;

enum Kind : uint8_t { KIND_PLACE = 0, KIND_VEHICLE = 1, KIND_PED = 2, KIND_PLAYER = 3 };
enum Op : uint8_t { OP_MARK = 1, OP_CLEAR = 2, OP_MOVE = 3 };

constexpr float kRange = 400.0f;          // how far the aim's line goes
constexpr uint64_t kLifeMs = 20000;       // a mark lasts
constexpr uint64_t kFadeMs = 2000;        // ...fading out at its end
constexpr uint64_t kTwiceMs = 400;        // the key twice within: ours away
constexpr uint64_t kMoveEveryMs = 200;    // a followed car or person: where it is, this often
constexpr uintptr_t kAudioEngine = 0xB6BC90;
constexpr int kSoundMark = 11;            // AE_FRONTEND_PICKUP_INFO
constexpr int kSoundNothing = 4;          // AE_FRONTEND_ERROR

struct Mark {
    uint8_t kind = KIND_PLACE;
    CVector pos;             // the place (a followed one's: where it is, or was last)
    uint8_t player = 0;      // KIND_PLAYER: whose
    int8_t area = 0;         // the interior it was made in
    uint64_t atMs = 0;
    int handle = -1;         // ours: the car or person followed (the script's handle)
    uint64_t sentMs = 0;     // ours: its place sent last then, and that place
    CVector sentPos;
};

std::map<uint8_t, Mark> g_marks;  // by the player who made it (ours under our id)
uint64_t g_lastKeyMs = 0;

uint8_t Me() { return Net().Online() ? Net().MyId() : 0; }

void Sound(int event) {
    CallMethod<0x506EA0>((void*)kAudioEngine, event, 0.0f, 1.0f);  // CAudioEngine::ReportFrontendAudioEvent
}

void Send(uint8_t op, const Mark& m) {
    if (!Net().Online()) return;
    Writer w(proto::G_PING);
    w.U8(op);
    w.U8(m.kind);
    w.Vec({m.pos.x, m.pos.y, m.pos.z});
    w.U16(0);  // (a net id: unused, the place is sent)
    w.U8(m.player);
    w.I8(m.area);
    Net().Send(op == OP_MOVE ? proto::CH_STATE : proto::CH_RELIABLE, w);
}

// the car or person a mark follows here, if it still exists
CEntity* Followed(const Mark& m, uint8_t owner) {
    if (m.kind == KIND_PLAYER) {
        CPed* p = m.player == Me() ? (CPed*)FindPlayerPed(0) : nullptr;
        if (m.player != Me())
            if (RemotePlayer* rp = Players::Instance().Get(m.player)) p = rp->ped;
        if (!p || !IsPedValid(p)) return nullptr;
        return p->InVehicle() && p->Vehicle() ? (CEntity*)p->Vehicle() : (CEntity*)p;
    }
    if (owner != Me() || m.handle < 0) return nullptr;
    if (m.kind == KIND_VEHICLE) return (CEntity*)GetVehicle(m.handle);
    if (m.kind == KIND_PED) {
        CPed* p = GetPed(m.handle);
        return p && p->InVehicle() && p->Vehicle() ? (CEntity*)p->Vehicle() : (CEntity*)p;
    }
    return nullptr;
}

// where the mark is drawn: a bit above what it marks
CVector Where(Mark& m, uint8_t owner) {
    if (CEntity* e = Followed(m, owner)) m.pos = e->GetPosition();
    float above = m.kind == KIND_VEHICLE ? 1.6f : m.kind == KIND_PLACE ? 0.3f : 1.2f;
    return m.pos + CVector{0, 0, above};
}

uint32_t ColourOf(uint8_t player) {  // RGBA, as its mark on the radar
    if (player == Me()) return Players::MarkRgba(Cfg().markColour);
    RemotePlayer* rp = Players::Instance().Get(player);
    return Players::MarkRgba(rp ? rp->markColour : std::string());
}

std::string NameOf(uint8_t player) { return player == Me() ? Cfg().nickname : Net().PeerName(player); }

// the aim's line: from the camera through the crosshair's place on the screen (CCamera::Find3rdPersonCamTargetVector's
// sum); the middle of the screen in a first-person view (sniper, rocket launcher, camera...) and in a vehicle
void AimLine(CPlayerPed* me, CVector& from, CVector& dir) {
    from = cam::Source();
    CVector front = cam::Front().Normalized(), up = cam::Up().Normalized();
    dir = front;
    // eCamMode: the sniper's, rocket launchers', first person, M16's, the helicopter's cannon, the camera's
    static const uint16_t kMiddle[] = {7, 8, 16, 34, 39, 40, 41, 42, 45, 46, 51, 52};
    uint16_t mode = cam::Mode();
    bool middle = (me->InVehicle() && mode != 55) ||  // in a vehicle, but for a drive-by's aim (MODE_AIMWEAPON_FROMCAR)
                  std::find(std::begin(kMiddle), std::end(kMiddle), mode) != std::end(kMiddle);
    if (!middle) {
        float t = std::tan(cam::Fov() * 0.5f * 3.14159265f / 180.0f), aspect = Ref<float>(0xC3EFA4);  // CDraw::ms_fAspectRatio
        dir += up * (t * (0.5f - cam::CrosshairMultY()) * 2.0f / (aspect > 0.1f ? aspect : 1.6f));
        dir += front.Cross(up) * (t * (cam::CrosshairMultX() - 0.5f) * 2.0f);
    }
    dir = dir.Normalized();
}

// what the aim points at, as a mark of ours; false: nothing there
bool Aim(Mark& out) {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me) return false;
    CVector from, dir;
    AimLine(me, from, dir);
    CVector to = from + dir * kRange;
    CEntity* mine = me->InVehicle() && me->Vehicle() ? (CEntity*)me->Vehicle() : (CEntity*)me;
    CEntity*& ignore = Ref<CEntity*>(0xB7CD68);  // CWorld::pIgnoreEntity: not our own player or car
    CEntity* saved = ignore;
    ignore = mine;
    uint8_t cp[0x2C] = {};  // CColPoint: m_vecPoint first
    CEntity* hit = nullptr;
    bool any = Call<0x56BA00, bool>(&from, &to, (void*)cp, &hit, true, true, true, true, true, true, false, false);  // CWorld::ProcessLineOfSight
    ignore = saved;
    float far = any ? (*(CVector*)cp - from).Length() : kRange;
    // a person near the line before what it hit (thin to hit): the nearest to the camera
    CPed* near = nullptr;
    float nearT = far + 0.5f;
    CPoolHeader* pool = PedPool();
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* p = (CPed*)(pool->objects + (size_t)i * kPedPoolElemSize);
        if ((CEntity*)p == (CEntity*)me || p->InVehicle() || p->AreaCode() != me->AreaCode()) continue;
        CVector c = p->GetPosition() - from;
        float t = c.Dot(dir);
        if (t < 1.0f || t > nearT) continue;
        if ((c - dir * t).Length() < 0.9f + t * 0.01f) near = p, nearT = t;
    }
    if (!any && !near) return false;
    out = Mark{};
    out.pos = *(CVector*)cp;
    out.area = me->AreaCode();
    CEntity* e = near ? (CEntity*)near : hit;
    if (e && e->Type() == ENTITY_TYPE_VEHICLE) {
        out.kind = KIND_VEHICLE, out.handle = GetVehicleRef((CVehicle*)e), out.pos = e->GetPosition();
    } else if (e && e->Type() == ENTITY_TYPE_PED) {
        out.pos = e->GetPosition();
        if (RemotePlayer* rp = Players::Instance().FindByPed((CPed*)e)) out.kind = KIND_PLAYER, out.player = rp->id;
        else out.kind = KIND_PED, out.handle = GetPedRef((CPed*)e);
    }
    return true;
}

// ---------------------------------------------------------------- drawing (the game's own 2D, as the weapon wheel)
void Polygon(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, CRGBA c) {
    Call<0x7285B0>(x1, y1, x2, y2, x3, y3, x4, y4, &c);  // CSprite2d::Draw2DPolygon: a fan of its corners 3, 4, 2, 1
}
void Triangle(float x1, float y1, float x2, float y2, float x3, float y3, CRGBA c) {
    Polygon(x1, y1, x2, y2, x3, y3, x3, y3, c);
}
void Ring(float cx, float cy, float r0, float r1, CRGBA c) {
    const int steps = 28;
    for (int i = 0; i < steps; ++i) {
        float t0 = 6.2831853f * i / steps, t1 = 6.2831853f * (i + 1) / steps;
        float c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
        Polygon(cx + r1 * c0, cy + r1 * s0, cx + r1 * c1, cy + r1 * s1, cx + r0 * c0, cy + r0 * s0, cx + r0 * c1, cy + r0 * s1, c);
    }
}
CRGBA Rgba(uint32_t rgba, float alpha) {
    return CRGBA{(uint8_t)(rgba >> 24), (uint8_t)(rgba >> 16), (uint8_t)(rgba >> 8), (uint8_t)(255 * alpha)};
}

// the mark's sign: a place a diamond, a car a ring, a person a triangle pointing down at it
void Sign(uint8_t kind, float x, float y, float s, CRGBA colour, float alpha) {
    CRGBA dark{12, 12, 16, (uint8_t)(220 * alpha)}, white{255, 255, 255, (uint8_t)(255 * alpha)};
    if (kind == KIND_VEHICLE) {
        Ring(x, y, 0, 15 * s, dark);
        Ring(x, y, 7 * s, 12 * s, colour);
        Ring(x, y, 0, 3.5f * s, white);
    } else if (kind == KIND_PED || kind == KIND_PLAYER) {
        Triangle(x - 15 * s, y - 11 * s, x + 15 * s, y - 11 * s, x, y + 15 * s, dark);
        Triangle(x - 10 * s, y - 7.5f * s, x + 10 * s, y - 7.5f * s, x, y + 9.5f * s, colour);
    } else {
        Polygon(x, y - 16 * s, x + 16 * s, y, x - 16 * s, y, x, y + 16 * s, dark);
        Polygon(x, y - 11 * s, x + 11 * s, y, x - 11 * s, y, x, y + 11 * s, colour);
        Polygon(x, y - 3.5f * s, x + 3.5f * s, y, x - 3.5f * s, y, x, y + 3.5f * s, white);
    }
}

struct Shown {
    uint8_t player;
    float x, y, alpha;
    std::string label;
};

void Draw() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me || g_marks.empty()) return;
    auto& g = ui::Gfx();
    float s = g.Scale(), w = g.Width(), h = g.Height(), margin = 46 * s;
    CVector eye = cam::Source(), front = cam::Front().Normalized();
    CVector right = front.Cross(CVector{0, 0, 1}).Normalized(), up = right.Cross(front);
    uint64_t now = NowMs();
    std::vector<Shown> labels;
    g.End();  // the game draws with its own state
    for (auto& [id, m] : g_marks) {
        uint64_t age = now - m.atMs;
        if (age >= kLifeMs || m.area != me->AreaCode()) continue;
        float alpha = age > kLifeMs - kFadeMs ? (float)(kLifeMs - age) / kFadeMs : 1.0f;
        CVector p = Where(m, id);
        CRGBA colour = Rgba(ColourOf(id), alpha);
        float x, y, depth;
        bool inView = WorldToScreen(p, x, y, depth) && x > margin && x < w - margin && y > margin && y < h - margin;
        if (!inView) {  // at the edge, towards it
            CVector v = p - eye;
            float dx = v.Dot(right), dy = -v.Dot(up);
            if (std::fabs(dx) + std::fabs(dy) < 1e-3f) dy = 1;
            float k = std::min((w / 2 - margin) / std::max(std::fabs(dx), 1e-4f), (h / 2 - margin) / std::max(std::fabs(dy), 1e-4f));
            x = w / 2 + dx * k, y = h / 2 + dy * k;
            float len = std::sqrt(dx * dx + dy * dy), ux = dx / len, uy = dy / len;
            float ax = x + ux * 24 * s, ay = y + uy * 24 * s;  // the arrow, out from the sign
            Triangle(ax + ux * 9 * s, ay + uy * 9 * s, ax - uy * 7 * s, ay + ux * 7 * s, ax + uy * 7 * s, ay - ux * 7 * s, colour);
        }
        if (age < 700) {  // a ring going out as it comes
            float t = age / 700.0f;
            Ring(x, y, (16 + 30 * t) * s, (19 + 30 * t) * s, Rgba(ColourOf(id), (1 - t) * alpha));
        }
        Sign(m.kind, x, y, s, colour, alpha);
        float metres = (p - me->GetPosition()).Length();
        uint32_t c = ColourOf(id);  // the name in its player's colour, the distance in white
        labels.push_back({id, x, y + 20 * s, alpha,
                          Format("{%06X}%s{FFFFFF}  %.0f m", c >> 8, NameOf(id).c_str(), metres)});
    }
    g.Begin();
    for (auto& l : labels)
        g.Text(ui::FONT_SMALL, l.x, l.y, l.label, ui::WithAlpha(0xFFFFFFFF, l.alpha), ui::TEXT_CENTER | ui::TEXT_OUTLINE);
}

// on the radar and the map: a blinking square in the player's colour (a triangle when it is above or below us)
void DrawOnRadar() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me) return;
    uint64_t now = NowMs();
    for (auto& [id, m] : g_marks) {
        uint64_t age = now - m.atMs;
        if (age >= kLifeMs || m.area != me->AreaCode() || (age < 3000 && (age / 250) % 2)) continue;
        CVector p = Where(m, id);
        CVector2D world{p.x, p.y}, radar, screen;
        Call<0x583530>(&radar, &world);   // CRadar::TransformRealWorldPointToRadarSpace
        Call<0x5832F0, float>(&radar);    // CRadar::LimitRadarPoint: onto the radar's edge
        Call<0x583480>(&screen, &radar);  // CRadar::TransformRadarPointToScreenSpace
        float dz = p.z - me->GetPosition().z;
        uint32_t height = dz > 4.0f ? 0 : dz < -6.0f ? 1 : 2;  // above: up; below: down; else a square
        uint32_t c = ColourOf(id);
        Call<0x584070>(screen.x, screen.y, 3u, c >> 24, (c >> 16) & 0xFF, (c >> 8) & 0xFF, 255u, height);  // CRadar::ShowRadarTraceWithHeight
    }
}

class Ping : public Module {
public:
    const char* Name() const override { return "Ping"; }
    void OnInstallHooks() override {
        Players::s_onRadarBlips = &DrawOnRadar;
        Net().SetHandler(proto::G_PING, [](uint8_t from, Reader& r) {
            uint8_t op = r.U8();
            Mark m;
            m.kind = r.U8();
            Vec3 p = r.Vec();
            m.pos = {p.x, p.y, p.z};
            r.U16();
            m.player = r.U8();
            m.area = r.I8();
            if (!r.Ok()) return;
            if (op == OP_CLEAR) {
                g_marks.erase(from);
                return;
            }
            if (op == OP_MOVE) {  // a followed car or person moved in its game
                if (auto it = g_marks.find(from); it != g_marks.end()) it->second.pos = m.pos;
                return;
            }
            m.atMs = NowMs();
            g_marks[from] = m;
            CPlayerPed* me = FindPlayerPed(0);
            if (me && me->AreaCode() == m.area) Sound(kSoundMark);
        });
    }
    // ours follows its car or person: the others are told where it is
    void OnPostProcess() override {
        auto it = g_marks.find(Me());
        if (it == g_marks.end()) return;
        Mark& m = it->second;
        uint64_t now = NowMs();
        if (now - m.atMs >= kLifeMs) return;
        CEntity* e = m.kind == KIND_VEHICLE || m.kind == KIND_PED ? Followed(m, Me()) : nullptr;
        if (!e || now - m.sentMs < kMoveEveryMs) return;
        m.pos = e->GetPosition();
        if ((m.pos - m.sentPos).Length() < 0.25f) return;
        m.sentMs = now, m.sentPos = m.pos;
        Send(OP_MOVE, m);
    }
    void OnSessionStart() override { g_marks.clear(); }
    void OnSessionEnd() override { g_marks.clear(); }
    void OnPeerLeave(uint8_t id) override { g_marks.erase(id); }
    void OnGameShutdown() override { g_marks.clear(); }
    void OnRender() override {
        if (!MenuActive() && !Cfg().cleanScreen && !cam::WideScreenOn()) Draw();  // not over the story's scenes
    }
    static Ping& Instance() {
        static Ping m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Ping::Instance()); }
} g_reg;
}  // namespace

void KeyDown() {
    uint64_t now = NowMs();
    auto mine = g_marks.find(Me());
    if (now - g_lastKeyMs < kTwiceMs && mine != g_marks.end()) {  // twice: ours away
        Send(OP_CLEAR, mine->second);
        g_marks.erase(mine);
        g_lastKeyMs = 0;
        return;
    }
    g_lastKeyMs = now;
    Mark m;
    if (!Aim(m)) {
        Sound(kSoundNothing);
        return;
    }
    m.atMs = now;
    g_marks[Me()] = m;
    Send(OP_MARK, m);
    Sound(kSoundMark);
}

}  // namespace coop::ping
