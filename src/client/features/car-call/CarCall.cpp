// /carro: a car beside our player, quietly (asked for on 02.11: in the middle of a mission the host drives off in its
// car, and the rule of one extra car (Players::PartnerGetsOne) gives the others none when the host's has seats, or none
// came at all). Like the host's when the host drives a land vehicle (the mission's kind), else a Sultan (four seats,
// fast). Nothing goes to the other players: no chat line, no message of ours (the car itself is synced as any other).
// Only our own chat says when it could not be done.
#include <cstdint>
#include <string>
#include <vector>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"

namespace coop::carcall {

namespace {
using namespace game;
using game::script::Cmd;

constexpr int kSultan = 560;
constexpr uint64_t kTryMs = 6000;  // the model loads in the background meanwhile

int g_model = -1;       // the car being called, -1 none
uint64_t g_until = 0;

// the host's vehicle's model while it drives a land one, else 0
int HostsModel() {
    if (!Net().Online() || Net().IsHost()) return 0;
    const RemotePlayer* host = Players::Instance().Get(Net().HostId());
    CPed* ped = host && host->ped && IsPedValid(host->ped) ? host->ped : nullptr;
    CVehicle* v = ped ? ped->VehicleIfIn() : nullptr;
    if (!v) return 0;
    int type = v->VehicleSubClass();
    bool land = type == VEHICLE_TYPE_AUTOMOBILE || type == VEHICLE_TYPE_MTRUCK || type == VEHICLE_TYPE_QUAD || v->IsBike();
    return land ? v->ModelIndex() : 0;
}

void Try() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!me || me->InVehicle()) return void(g_model = -1);
    int car = Players::Instance().VehicleBeside(g_model);
    if (car >= 0) {
        Cmd(0x01C3).I(car).Run();  // MARK_CAR_AS_NO_LONGER_NEEDED: an ordinary car from now on
        LOGI("Car call: model %d beside us", g_model);
        g_model = -1;
        return;
    }
    if (car == -2 || NowMs() > g_until) {
        ui::TheChat().AddError(car == -2 ? TR("Здесь нет места для машины.", "No room for a car here.")
                                         : TR("Машина не появилась, попробуйте ещё раз.", "The car did not come, try again."));
        LOGI("Car call: model %d failed (%s)", g_model, car == -2 ? "no room" : "timed out");
        g_model = -1;
    }
}

class CarCall : public Module {
public:
    const char* Name() const override { return "CarCall"; }
    void OnPostProcess() override {
        if (g_model >= 0 && core::IsPlayerReady()) Try();
    }
    void OnGameShutdown() override { g_model = -1; }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "carro" && cmd != "car") return false;
        CPlayerPed* me = FindPlayerPed(0);
        if (!core::IsPlayerReady() || !me) return true;
        if (me->InVehicle()) {
            ui::TheChat().AddError(TR("Вы уже в машине.", "You are in a vehicle already."));
            return true;
        }
        int model = HostsModel();
        g_model = model ? model : kSultan;
        g_until = NowMs() + kTryMs;
        Try();
        return true;
    }
    static CarCall& Instance() {
        static CarCall m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&CarCall::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop::carcall
