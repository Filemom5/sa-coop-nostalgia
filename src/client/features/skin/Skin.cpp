// /skin: each player picks a skin of its own in place of CJ (any of the game's people: a preset by name, or a model's
// number), and /skin cj goes back to CJ in his clothes. The skin is the game's SET_PLAYER_MODEL; the other games see it
// as they see a skin mod's (features/progress sends our model with the look). Kept: put back when the game changed it
// (a saved game loaded, a death). Not in a car nor in a cutscene (the model is changed on foot).
//
// With a skin on, CClothes::RebuildPlayer (the changing room, a mission's clothes) would build CJ's clothes on another
// model: it waits for CJ instead (hooked).
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/chat/Chat.h"

namespace coop {

namespace {
using namespace game;
using game::script::Cmd;

struct Preset {
    const char* name;
    int model;
};
const Preset kPresets[] = {
    {"cj", 0},          {"sweet", 270},     {"smoke", 269},   {"ryder", 271},    {"tenpenny", 265},
    {"pulaski", 266},   {"hernandez", 267}, {"grove", 105},   {"ballas", 102},   {"vagos", 108},
    {"aztecas", 114},   {"triade", 117},    {"mafia", 113},   {"policia", 280},  {"swat", 285},
    {"fbi", 286},       {"exercito", 287},  {"motoqueiro", 247}, {"medico", 274}, {"bombeiro", 277},
};

int g_skin = 0;          // the one picked (0: CJ)
uint64_t g_nextCheck = 0;

using RebuildFn = void(__cdecl*)(CPlayerPed*, bool);
RebuildFn g_origRebuild = nullptr;
void __cdecl HookRebuild(CPlayerPed* ped, bool keep) {
    if (ped && ped->ModelIndex() != 0) return;  // a skin: no CJ's clothes on it
    g_origRebuild(ped, keep);
}

bool CanChange(CPlayerPed* me) { return me && !me->InVehicle() && !Ref<uint8_t>(0xB5F851); }  // not in a cutscene

bool Apply(int model) {
    CPlayerPed* me = FindPlayerPed(0);
    if (!CanChange(me)) return false;
    if (me->ModelIndex() == model) return true;
    if (model != 0 && !LoadModelNow(model)) return false;
    Cmd(0x09C7).I(0).I(model).Run();  // SET_PLAYER_MODEL
    if (model == 0) g_origRebuild(me, false);  // CJ in his clothes again
    return true;
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

class Skin : public Module {
public:
    const char* Name() const override { return "Skin"; }
    void OnInstallHooks() override { mem::HookFn(0x5A82C0, &HookRebuild, &g_origRebuild, "CClothes::RebuildPlayer"); }

    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "skin") return false;
        auto& chat = ui::TheChat();
        if (args.empty()) {
            std::string list;
            for (const Preset& p : kPresets) list += (list.empty() ? "" : ", ") + std::string(p.name);
            chat.AddSystem(Format(TR("/skin <имя или номер модели>: %s. /skin cj - снова CJ.",
                                     "/skin <name or model number>: %s. /skin cj - CJ again."), list.c_str()));
            return true;
        }
        std::string a = Lower(args[0]);
        int model = -1;
        for (const Preset& p : kPresets)
            if (a == p.name) model = p.model;
        if (model < 0 && !a.empty() && std::isdigit((unsigned char)a[0])) model = std::atoi(a.c_str());
        if (model < 0 || (model != 0 && !IsPedModel(model))) {
            chat.AddError(TR("Нет такого скина. /skin - список.", "No such skin. /skin - the list."));
            return true;
        }
        if (!core::IsPlayerReady() || !CanChange(FindPlayerPed(0))) {
            chat.AddError(TR("Только пешком и не в кат-сцене.", "On foot only, and not in a cutscene."));
            return true;
        }
        if (!Apply(model)) {
            chat.AddError(TR("Не удалось загрузить этот скин.", "Could not load this skin."));
            return true;
        }
        g_skin = model;
        LOGI("Skin: model %d", model);
        chat.AddSystem(model ? Format(TR("Скин: %d.", "Skin: %d."), model) : std::string(TR("Снова CJ.", "CJ again.")));
        return true;
    }

    void OnPostProcess() override {
        if (!g_skin || !core::IsPlayerReady() || TimeMs() < g_nextCheck) return;
        g_nextCheck = TimeMs() + 2000;
        CPlayerPed* me = FindPlayerPed(0);
        if (me && me->ModelIndex() != g_skin && CanChange(me) && Apply(g_skin)) LOGI("Skin: model %d put back", g_skin);
    }

    static Skin& Instance() {
        static Skin m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Skin::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop
