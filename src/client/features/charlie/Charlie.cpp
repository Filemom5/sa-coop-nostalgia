// Charlie Harper's beach house (Two and a Half Men) at 1:1, on Santa Maria Beach, in place of the block of houses
// santahouse04_law2 (docs/casa-charlie.md). Part of the map: one walks in and out through its open doorways, the deck
// toward the sea. Everyone with the mod sees it; another mod's block (not the game's own) is left as it is.
//
//  * the model (near: santahouse04_law2; far: LODtahouse04_law2, only the outside) and the collision are ours, from
//    CharlieData.h (tools/charlie, from the SketchUp model), loaded in place of the game's by shared/game/ModelSwap;
//  * a palm and a bush of the map that would stand in it are hidden (and walked through): again whenever that part
//    of the map loads anew;
//  * no rain under its roofs: CCullZones::CamNoRain and PlayerNoRain answer yes there.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/lib/log/Log.h"
#include "features/charlie/CharlieData.h"
#include "shared/game/Game.h"
#include "shared/game/ModelSwap.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop::charlie {

namespace {
using namespace game;

constexpr uint32_t kHouseModel = 6285, kLodModel = 6433;  // LAw2.ide
constexpr const char* kHouseName = "santahouse04_law2";
constexpr const char* kLodName = "LODtahouse04_law2";
constexpr float kX = 265.969f, kY = -1757.578f, kZ = 8.906f;  // the block (heading 0)
constexpr float kFloor = 3.6f;

struct Hidden {
    int model;
    float x, y;
};
constexpr Hidden kHidden[] = {
    {647, 255.969f, -1765.961f},  // a bush, in the living room
    {620, 256.359f, -1745.742f},  // a palm leaning over the front
};

bool g_ours = false;  // the block's collision is ours: the house is there

// CKeyGen::GetUppercaseKey: the key a model's name gives (CBaseModelInfo::m_nKey)
uint32_t Key(const char* s) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t key = 0xFFFFFFFFu;
    for (; *s; ++s) {
        char c = (*s >= 'a' && *s <= 'z') ? (char)(*s - 32) : *s;
        key = table[(key ^ (uint8_t)c) & 0xFF] ^ (key >> 8);
    }
    return key;
}

// the model is the game's one of that name (not another mod's)
bool IsGames(uint32_t model, const char* name) {
    if (!IsValidModel((int)model)) return false;
    void* mi = Ref<void*>(addr::ModelInfoPtrs + (uintptr_t)model * 4);
    if (!mi) return false;
    uint32_t key = Ref<uint32_t>((uintptr_t)mi + 4), ours = Key(name);
    return key == ours || key == ~ours;
}

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a, y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

std::vector<uint8_t> Model(uint32_t modelId, const uint8_t*, size_t) {
    if (modelId == kHouseModel && IsGames(modelId, kHouseName))
        return std::vector<uint8_t>(data::kHouseDff, data::kHouseDff + sizeof data::kHouseDff);
    if (modelId == kLodModel && IsGames(modelId, kLodName))
        return std::vector<uint8_t>(data::kLodDff, data::kLodDff + sizeof data::kLodDff);
    return {};
}

const uint8_t* Collision(const char* name, const uint8_t*, uint32_t, uint32_t* ourSize) {
    if (!name || !SameName(name, kHouseName) || !IsGames(kHouseModel, kHouseName)) return nullptr;
    if (!g_ours) LOGI("Charlie: the house's collision in place of %s", kHouseName);
    g_ours = true;
    *ourSize = sizeof data::kHouseCol;
    return data::kHouseCol;
}

// under one of the house's roofs
bool Covered(const CVector& p) {
    if (!g_ours) return false;
    float x = p.x - kX, y = p.y - kY, z = p.z - kZ;
    if (z < kFloor - kZ - 1.0f || z > data::kRoofTop) return false;
    int cx = (int)std::floor((x - data::kRoofX) / data::kRoofCell), cy = (int)std::floor((y - data::kRoofY) / data::kRoofCell);
    if (cx < 0 || cy < 0 || cx >= data::kRoofW || cy >= data::kRoofH) return false;
    int bit = cy * data::kRoofW + cx;
    return (data::kRoofBits[bit >> 3] >> (bit & 7)) & 1;
}

using NoRainFn = bool(__cdecl*)();
NoRainFn g_origCamNoRain = nullptr, g_origPlayerNoRain = nullptr;

bool __cdecl HookCamNoRain() { return g_origCamNoRain() || Covered(cam::Source()); }

bool __cdecl HookPlayerNoRain() {
    if (g_origPlayerNoRain()) return true;
    CPlayerPed* me = FindPlayerPed(0);
    return me && Covered(me->GetPosition());
}

void Hide() {
    auto* pool = Ref<CPoolHeader*>(0xB74498);  // CPools::ms_pBuildingPool
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* e = (CEntity*)(pool->objects + (size_t)i * 0x38);
        for (const Hidden& h : kHidden) {
            if (e->ModelIndex() != h.model) continue;
            CVector p = e->GetPosition();
            if (std::fabs(p.x - h.x) > 1.0f || std::fabs(p.y - h.y) > 1.0f) continue;
            if (e->Flags() & 0x81) LOGI("Charlie: model %d at %.1f,%.1f hidden", h.model, p.x, p.y);
            e->Flags() &= ~0x81u;  // bIsVisible, bUsesCollision
        }
    }
}

class Charlie : public Module {
public:
    const char* Name() const override { return "Charlie"; }
    void OnInstallHooks() override {
        game::AddModelSwap(&Model);
        game::AddCollisionSwap(&Collision);
        mem::HookFn(0x72DDB0, &HookCamNoRain, &g_origCamNoRain, "CCullZones::CamNoRain");
        mem::HookFn(0x72DDC0, &HookPlayerNoRain, &g_origPlayerNoRain, "CCullZones::PlayerNoRain");
    }
    void OnGameStarted() override {
        LOGI("Charlie: house %s (models: %s, %s)", g_ours ? "in place" : "not in place (not the game's block)",
             IsGames(kHouseModel, kHouseName) ? "game's" : "other", IsGames(kLodModel, kLodName) ? "game's" : "other");
        m_next = 0;
    }
    void OnPostProcess() override {
        if (!g_ours || !core::IsPlayerReady() || TimeMs() < m_next) return;
        m_next = TimeMs() + 1000;
        CPlayerPed* me = FindPlayerPed(0);
        if (!me) return;
        CVector p = me->GetPosition();
        if (std::hypot(p.x - kX, p.y - kY) < 400.0f) Hide();
    }
    static Charlie& Instance() {
        static Charlie m;
        return m;
    }

private:
    uint32_t m_next = 0;
};

struct Reg {
    Reg() { core::RegisterModule(&Charlie::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop::charlie
