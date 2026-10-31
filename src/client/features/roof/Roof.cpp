// Holding on to a car's roof, as on some MTA servers: standing on a car (its roof, bonnet, flatbed), the hold key
// (roof_lock, X) keeps the player there while the car drives, turns and jumps. The player keeps its own camera, aims and
// shoots, and does not walk; the key again or a jump lets go.
//
// Here the player is put back on its spot of the car after every frame's physics, moving as fast as that spot of the
// car. The other games put its copy on their copy of the car with ATTACH_CHAR_TO_CAR (G_PED_ATTACH, as the host's
// mission's people on cars, Missions.cpp), so that it rides along there too.
#include "features/roof/Roof.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "common/api/protocol/Protocol.h"
#include "common/lib/buffer/Buffer.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/vehicle/Vehicles.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"

namespace coop::roof {

namespace {
using namespace game;

constexpr float kPi = 3.14159265f;
constexpr float kTurretRange = 360.0f;  // the copies' ATTACH_CHAR_TO_CAR angle limit: all round
constexpr float kMaxStandHeight = 1.6f; // a standing ped's centre is about a metre above what it stands on
constexpr float kMinStandHeight = 0.3f;
constexpr float kLostDist = 6.0f;       // further from its spot than this, the player went elsewhere (a teleport)

struct Hold {
    bool on = false;
    uint16_t car = 0;           // net id
    CVehicle* veh = nullptr;
    CVector offset;             // the player's centre in the car's frame
    float carHeading = 0.0f;    // at the last frame: the player turns as the car turns
    bool madeProof = false;     // we made the player collision-proof (the car it rides must not run it over)
};
Hold g_hold;

std::string KeyName(int vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, (char)vk);
    return Format("#%d", vk);
}

float WrapAngle(float a) {
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

CVector ToLocal(CVehicle* v, const CVector& p) {
    const CMatrix* m = v->Matrix();
    CVector d = p - m->pos;
    return {d.Dot(m->right), d.Dot(m->forward), d.Dot(m->up)};
}

CVector ToWorld(CVehicle* v, const CVector& o) {
    const CMatrix* m = v->Matrix();
    return m->pos + m->right * o.x + m->forward * o.y + m->up * o.z;
}

bool Dead(CPlayerPed* p) { return p->Health() <= 0.0f || p->PedState() == 54 || p->PedState() == 55; }  // DIE, DEAD

// what the player stands on, if a car. CWorld::FindGroundZFor3DCoord sees only buildings: the same line down
// (CWorld::ProcessVerticalLine) with the vehicles in.
CVehicle* CarUnder(CPlayerPed* me) {
    CVector p = me->GetPosition();
    alignas(16) uint8_t colPoint[0x40] = {};  // CColPoint (0x2C): the point first
    CEntity* hit = nullptr;
    bool found = Call<0x5674E0, bool>(&p, p.z - 3.0f, (void*)colPoint, &hit, true, true, false, false, false, false,
                                      (void*)nullptr);  // buildings, vehicles; no peds, objects, dummies, see-through
    if (!found || !hit || !hit->IsVehicle()) return nullptr;
    float h = p.z - Field<float>(colPoint, 8);
    if (h < kMinStandHeight || h > kMaxStandHeight) return nullptr;
    return (CVehicle*)hit;
}

// the other games put (or take) the copy of our player on their copy of the car (Missions.cpp, HandlePersonOnCar)
void SendCopy(bool on, int weapon) {
    Writer w(proto::G_PED_ATTACH);
    w.U8(on ? 1 : 0);
    w.U8(3);  // a player, by its id
    w.U16(Net().MyId());
    w.U16(on ? g_hold.car : 0);
    w.Vec({on ? g_hold.offset.x : 0.0f, on ? g_hold.offset.y : 0.0f, on ? g_hold.offset.z : 0.0f});
    w.I32(0);  // facing the car's front
    w.F32(on ? kTurretRange : 0.0f);
    w.I32(on ? weapon : 0);
    Net().Send(proto::CH_RELIABLE, w);
}

void LetGo(const char* why) {
    if (!g_hold.on) return;
    CPlayerPed* me = FindPlayerPed(0);
    if (me && g_hold.madeProof) me->PhysicalFlags() &= ~CPhysical::PF_COLLISION_PROOF;
    g_hold.on = false;
    if (Net().Online()) SendCopy(false, 0);
    LOGI("Roof: let go of car %04X (%s)", g_hold.car, why);
    g_hold = Hold{};
}

bool TakeHold(CPlayerPed* me) {
    CVehicle* v = CarUnder(me);
    if (!v) {
        ui::TheChat().AddSystem(TR("Встаньте на крышу машины, чтобы держаться за неё.", "Stand on a car's roof to hold on to it."));
        return false;  // the key may be the game's too
    }
    NetVehicle* nv = Net().Online() ? Vehicles::Instance().EnsureNetworked(v) : nullptr;
    g_hold = Hold{};
    g_hold.on = true;
    g_hold.car = nv ? nv->netId : 0;
    g_hold.veh = v;
    g_hold.offset = ToLocal(v, me->GetPosition());
    g_hold.carHeading = v->GetHeading();
    if (!(me->PhysicalFlags() & CPhysical::PF_COLLISION_PROOF)) {
        me->PhysicalFlags() |= CPhysical::PF_COLLISION_PROOF;
        g_hold.madeProof = true;
    }
    if (nv) SendCopy(true, me->ActiveWeapon().type);
    ui::TheChat().AddSystem(Format(TR("Вы держитесь за машину: %s или прыжок — отпустить.",
                                      "Holding on to the car: %s or a jump lets go."),
                                   KeyName(Cfg().roofKey).c_str()));
    LOGI("Roof: holding on to car %04X at (%.2f, %.2f, %.2f)", g_hold.car, g_hold.offset.x, g_hold.offset.y, g_hold.offset.z);
    return true;
}

class Roof : public Module {
public:
    const char* Name() const override { return "Roof"; }
    void OnSessionEnd() override { LetGo("session end"); }
    void OnGameShutdown() override { g_hold = Hold{}; }  // the world goes, and the car with it

    // after the frame's physics: the player back on its spot of the car, moving as that spot moves
    void OnPostProcess() override {
        if (!g_hold.on) return;
        CPlayerPed* me = core::IsPlayerReady() ? FindPlayerPed(0) : nullptr;
        CVehicle* v = g_hold.veh;
        if (g_hold.car) {
            NetVehicle* nv = Vehicles::Instance().Find(g_hold.car);
            if (!nv || !Vehicles::Alive(*nv) || nv->veh != v) v = nullptr;
        } else if (v && !IsVehicleValid(v)) {
            v = nullptr;
        }
        if (!me || !v) return LetGo("the car or the player is gone");
        if (Dead(me) || me->InVehicle()) return LetGo("dead or in a vehicle");
        if (me->AttachedTo()) return LetGo("attached by something else");
        CVector spot = ToWorld(v, g_hold.offset);
        if ((spot - me->GetPosition()).Length() > kLostDist) return LetGo("moved away from its spot");
        SetEntityPosition(me, spot);
        me->MoveSpeed() = v->MoveSpeed() + v->TurnSpeed().Cross(spot - v->GetPosition());
        me->PedFlags() |= 1u;  // bIsStanding: no fall while the car bumps
        float heading = v->GetHeading();
        float turn = WrapAngle(heading - g_hold.carHeading);
        g_hold.carHeading = heading;
        me->CurrentRotation() = WrapAngle(me->CurrentRotation() + turn);
        me->AimingRotation() = WrapAngle(me->AimingRotation() + turn);
    }

    static Roof& Instance() {
        static Roof m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Roof::Instance()); }
} g_reg;
}  // namespace

bool Key() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me || !core::IsPlayerReady()) return false;
    if (g_hold.on) {
        LetGo("its key");
        ui::TheChat().AddSystem(TR("Вы отпустили машину.", "You let go of the car."));
        return true;
    }
    if (me->InVehicle() || me->AttachedTo() || Dead(me)) return false;
    return TakeHold(me);
}

void AfterPadUpdate() {
    if (!g_hold.on) return;
    CPad* pad = GetPad(0);
    if (pad->NewState.ButtonSquare && !pad->OldState.ButtonSquare) {  // a jump lets go (and jumps)
        LetGo("a jump");
        return;
    }
    pad->NewState.LeftStickX = pad->NewState.LeftStickY = 0;  // no walking off the spot
}

}  // namespace coop::roof
