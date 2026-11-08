// /host <nickname or number>: the host hands its role to another player of the session, and back, between missions.
// The host's game is the one where the story runs (its missions, their people, who drives, who flies the RC plane): the
// player chosen becomes that one. Only the host can, only on foot and out of missions and cutscenes (a running mission
// lives in the host's game and cannot move). Its game goes to the server first, as after a mission ("progress"): the
// partners load it at the next mission's start, so every game starts from the same world. The server makes the change
// (C_HOST_TRANSFER, after the upload on the same channel) and tells everyone (S_HOST_CHANGED). The old host plays on as a
// partner; its money and things are kept in its profile on the server from then on, as a partner's are.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "common/api/protocol/Protocol.h"
#include "common/lib/buffer/Buffer.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/profile/Profile.h"
#include "entities/session/Flow.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"

namespace coop {

namespace {
bool g_wasHost = false;

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

class HostSwap : public Module {
public:
    const char* Name() const override { return "HostSwap"; }

    void OnSessionStart() override { g_wasHost = Net().IsHost(); }

    void OnHostChanged(uint8_t id) override {
        bool now = id == Net().MyId();
        if (g_wasHost && !now) SaveProfileNow();  // a partner now: its money and things live in its profile
        if (now && !g_wasHost)
            ui::TheChat().AddSystem(TR("Теперь вы хост: следующие миссии идут в вашей игре.",
                                       "You are the host now: the next missions run in your game."));
        g_wasHost = now;
    }

    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "host") return false;
        auto& net = Net();
        if (!net.Online()) {
            ui::TheChat().AddError(TR("Вы не подключены.", "You are not connected."));
            return true;
        }
        if (args.empty()) {
            ui::TheChat().AddSystem(Format(TR("Хост: %s. /host <ник или номер> - передать роль (только хост, вне миссий).",
                                              "The host: %s. /host <nickname or number> hands the role over (the host only, out of missions)."),
                                           net.IsHost() ? Cfg().nickname.c_str() : net.PeerName(net.HostId()).c_str()));
            return true;
        }
        if (!net.IsHost()) {
            ui::TheChat().AddError(TR("Передать роль хоста может только хост.", "Only the host can hand the role over."));
            return true;
        }
        // the player: a nickname (any case) or the number of /players
        std::string want = Lower(args[0]);
        int to = -1;
        for (auto& [id, p] : net.Peers())
            if (Lower(p.name) == want) to = id;
        if (to < 0 && want.find_first_not_of("0123456789") == std::string::npos && net.Peers().count((uint8_t)std::atoi(want.c_str())))
            to = std::atoi(want.c_str());
        if (to < 0 || to == net.MyId()) {
            ui::TheChat().AddError(TR("Нет такого игрока (ник или номер из /players).", "No such player (a nickname, or a number from /players)."));
            return true;
        }
        game::CPlayerPed* me = game::FindPlayerPed(0);
        bool cutscene = game::Ref<uint8_t>(0xB5F851) != 0;  // CCutsceneMgr::ms_running
        if (core::IsOnMission() || cutscene) {
            ui::TheChat().AddError(TR("Не во время миссии или катсцены: миссия идёт в игре хоста.",
                                      "Not during a mission or a cutscene: the mission runs in the host's game."));
            return true;
        }
        if (!me || me->InVehicle()) {
            ui::TheChat().AddError(TR("Выйдите из машины: игра сохраняется только пешком.", "Get out of the vehicle: the game saves on foot only."));
            return true;
        }
        if (!flow::UploadWorldNow()) {
            ui::TheChat().AddError(TR("Игру сейчас нельзя сохранить. Попробуйте чуть позже.", "The game cannot be saved right now. Try again in a moment."));
            return true;
        }
        Writer w(proto::C_HOST_TRANSFER);
        w.U8((uint8_t)to);
        net.Send(proto::CH_BULK, w);  // after the world's upload, on its channel: the server takes it as the host's
        LOGI("Host role to player %d", to);
        return true;
    }

    static HostSwap& Instance() {
        static HostSwap m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&HostSwap::Instance()); }
} g_reg;
}  // namespace

}  // namespace coop
