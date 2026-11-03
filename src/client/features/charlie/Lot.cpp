// /lote: what is around the player, for placing a building of ours in the map (docs/casa-charlie.md). Charlie Harper's
// beach house goes on a lot of the beach in place of the houses there: standing at the spot, /lote writes coop\lote.txt
// with the spot, the ground and the sea around it, and the map's buildings, dummies and objects near it (model, place,
// heading, collision box, LOD), which the tools that make the house read.
#include <cmath>
#include <cstdio>
#include <string>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"

namespace coop {

namespace {
using namespace game;

constexpr float kReach = 70.0f;  // the lot and what stands around it
constexpr float kPi = 3.14159265f;

void Entities(std::string& out, uintptr_t poolAddr, size_t elem, const char* tag, const CVector& at) {
    auto* pool = Ref<CPoolHeader*>(poolAddr);
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* e = (CEntity*)(pool->objects + (size_t)i * elem);
        CVector p = e->GetPosition();
        if (std::hypot(p.x - at.x, p.y - at.y) > kReach) continue;
        int model = e->ModelIndex();
        void* mi = IsValidModel(model) ? Ref<void*>(addr::ModelInfoPtrs + (uintptr_t)model * 4) : nullptr;
        const float* col = mi ? Ref<const float*>((uintptr_t)mi + 0x14) : nullptr;  // CBaseModelInfo::m_pColModel: its box first
        auto* lod = Ref<CEntity*>((uintptr_t)e + 0x30);                              // CEntity::m_pLod
        out += Format("%s %d %.3f %.3f %.3f %.2f %u %d", tag, model, p.x, p.y, p.z, e->GetHeading() * 180.0f / kPi,
                      e->Flags(), lod ? (int)lod->ModelIndex() : -1);
        if (col) out += Format(" %.2f %.2f %.2f %.2f %.2f %.2f", col[0], col[1], col[2], col[3], col[4], col[5]);
        out += "\n";
    }
}

void Scan() {
    CPlayerPed* me = FindPlayerPed(0);
    if (!core::IsPlayerReady() || !me) return;
    if (me->AreaCode() != 0) return ui::TheChat().AddError(TR("Только на улице.", "Outside only."));
    CVector at = me->GetPosition();
    std::string out = Format("P %.3f %.3f %.3f %.2f\n", at.x, at.y, at.z, me->CurrentRotation() * 180.0f / kPi);
    // the ground and the sea, every 4 m
    for (int dy = -60; dy <= 60; dy += 4)
        for (int dx = -60; dx <= 60; dx += 4) {
            float x = at.x + dx, y = at.y + dy, level = 0, big = 0, small = 0;
            bool found = false;
            float z = Call<0x5696C0, float>(x, y, at.z + 60.0f, &found, (CEntity**)nullptr);  // CWorld::FindGroundZFor3DCoord
            bool water = Call<0x6E8580, bool>(x, y, at.z, &level, &big, &small);            // CWaterLevel::GetWaterLevelNoWaves
            out += Format("G %d %d %.2f %d %.2f\n", dx, dy, found ? z : -999.0f, water ? 1 : 0, water ? level : -999.0f);
        }
    Entities(out, 0xB74498, 0x38, "B", at);              // CPools::ms_pBuildingPool
    Entities(out, 0xB744A0, 0x38, "D", at);              // CPools::ms_pDummyPool
    Entities(out, addr::ObjectPool, kObjectPoolElemSize, "O", at);  // CPools::ms_pObjectPool
    std::wstring path = Cfg().coopDir + L"lote.txt";
    if (!WriteFileBytes(path, out.data(), out.size())) return ui::TheChat().AddError(TR("Не удалось записать lote.txt.", "Could not write lote.txt."));
    ui::TheChat().AddSystem(TR("Участок записан в coop\\lote.txt.", "The lot is written to coop\\lote.txt."));
    LOGI("Lot: %.1f,%.1f,%.1f written to lote.txt (%zu bytes)", at.x, at.y, at.z, out.size());
}

class Lot : public Module {
public:
    const char* Name() const override { return "Lot"; }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>&) override {
        if (cmd != "lote" && cmd != "lot") return false;
        Scan();
        return true;
    }
    static Lot& Instance() {
        static Lot m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Lot::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop
