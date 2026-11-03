// Charlie Harper's beach house (Two and a Half Men), 1.6 times life size as the game's people and doors are, on
// Santa Maria Beach in place of the block of houses santahouse04_law2, and over part of the block east of it,
// santahouse02_law2, which becomes sand (docs/casa-charlie.md). Part of the map: one walks in and out through its
// open doorways, the deck toward the sea. Everyone with the mod sees it; another mod's blocks (not the game's own)
// are left as they are.
//
//  * the models (near: santahouse04_law2; far: LODtahouse04_law2, only the outside; the east block's, sand) and the
//    collisions are ours, from CharlieData.h (tools/charlie, from the SketchUp model), loaded in place of the game's by
//    shared/game/ModelSwap;
//  * their textures (stucco, clay roof tiles, terracotta floor, the ceiling's planks, Spanish tiles on the stairs...)
//    are ours too: made from CharlieData.h's (DXT1) and put in the blocks' texture dictionaries (bev_law2, near;
//    welod_law2, far, smaller) as these load (shared/game/TxdLoad), before the models that use them;
//  * the palms, bushes and the lifebuoy of the map that would stand in it are hidden (and walked through): again
//    whenever that part of the map loads anew;
//  * no rain under its roofs: CCullZones::CamNoRain and PlayerNoRain answer yes there.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/lib/log/Log.h"
#include "features/charlie/CharlieData.h"
#include "shared/game/Game.h"
#include "shared/game/ModelSwap.h"
#include "shared/game/TxdLoad.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop::charlie {

namespace {
using namespace game;

constexpr uint32_t kHouseModel = 6285, kLodModel = 6433, kEastModel = 6284, kEastLodModel = 6434;  // LAw2.ide
constexpr const char* kHouseName = "santahouse04_law2";
constexpr const char* kLodName = "LODtahouse04_law2";
constexpr const char* kEastName = "santahouse02_law2";
constexpr const char* kEastLodName = "LODtahouse02_law2";
constexpr float kX = 265.969f, kY = -1757.578f, kZ = 8.906f;  // the house's block (heading 0)
constexpr float kDrawDistance = 170.0f;  // the house near (the game's 80 for the block: the house is bigger)
constexpr int kFarTexture = 64;          // the textures' size far away

bool g_ours = false;  // the block's collision is ours: the house is there

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a, y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

uint32_t Key(const char* name) { return Call<0x53CF30, uint32_t>(name); }  // CKeyGen::GetUppercaseKey

void* ModelInfo(uint32_t model) {
    return IsValidModel((int)model) ? Ref<void*>(addr::ModelInfoPtrs + (uintptr_t)model * 4) : nullptr;
}

// the model is the game's one of that name (CBaseModelInfo::m_nKey), not another mod's
bool IsGames(uint32_t model, const char* name) {
    void* mi = ModelInfo(model);
    return mi && Ref<uint32_t>((uintptr_t)mi + 4) == Key(name);
}

std::vector<uint8_t> Copy(const unsigned char* data, size_t size) { return std::vector<uint8_t>(data, data + size); }

std::vector<uint8_t> Model(uint32_t modelId, const uint8_t*, size_t) {
    if (modelId == kHouseModel && IsGames(modelId, kHouseName)) return Copy(data::kHouseDff, sizeof data::kHouseDff);
    if (modelId == kLodModel && IsGames(modelId, kLodName)) return Copy(data::kLodDff, sizeof data::kLodDff);
    if (modelId == kEastModel && IsGames(modelId, kEastName)) return Copy(data::kEastDff, sizeof data::kEastDff);
    if (modelId == kEastLodModel && IsGames(modelId, kEastLodName)) return Copy(data::kEastLodDff, sizeof data::kEastLodDff);
    return {};
}

const uint8_t* Collision(const char* name, const uint8_t*, uint32_t, uint32_t* ourSize) {
    if (!name) return nullptr;
    if (SameName(name, kHouseName) && IsGames(kHouseModel, kHouseName)) {
        if (!g_ours) LOGI("Charlie: the house's collision in place of %s", kHouseName);
        g_ours = true;
        *ourSize = sizeof data::kHouseCol;
        return data::kHouseCol;
    }
    if (SameName(name, kEastName) && IsGames(kEastModel, kEastName)) {
        *ourSize = sizeof data::kEastCol;
        return data::kEastCol;
    }
    return nullptr;
}

// ---------------------------------------------------------------- the textures
// DXT1 -> B, G, R, X (the game's 888 rasters)
std::vector<uint8_t> Decode(const unsigned char* dxt, int size) {
    std::vector<uint8_t> px(size_t(size) * size * 4, 255);
    auto rgb = [](uint16_t c, uint8_t* o) {  // o: b, g, r
        int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
        o[2] = (uint8_t)((r << 3) | (r >> 2));
        o[1] = (uint8_t)((g << 2) | (g >> 4));
        o[0] = (uint8_t)((b << 3) | (b >> 2));
    };
    const unsigned char* p = dxt;
    for (int by = 0; by < size / 4; ++by)
        for (int bx = 0; bx < size / 4; ++bx, p += 8) {
            uint16_t c0 = (uint16_t)(p[0] | (p[1] << 8)), c1 = (uint16_t)(p[2] | (p[3] << 8));
            uint32_t bits = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
            uint8_t pal[4][3];
            rgb(c0, pal[0]);
            rgb(c1, pal[1]);
            for (int k = 0; k < 3; ++k) {
                pal[2][k] = (uint8_t)((2 * pal[0][k] + pal[1][k]) / 3);
                pal[3][k] = (uint8_t)((pal[0][k] + 2 * pal[1][k]) / 3);
            }
            for (int i = 0; i < 16; ++i) {
                const uint8_t* c = pal[(bits >> (2 * i)) & 3];
                uint8_t* o = &px[((size_t(by) * 4 + i / 4) * size + bx * 4 + i % 4) * 4];
                o[0] = c[0], o[1] = c[1], o[2] = c[2];
            }
        }
    return px;
}

std::vector<uint8_t> Half(const std::vector<uint8_t>& px, int size) {
    int h = size / 2;
    std::vector<uint8_t> out(size_t(h) * h * 4, 255);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < h; ++x)
            for (int k = 0; k < 3; ++k) {
                int s = px[((size_t(2 * y) * size + 2 * x) * 4) + k] + px[((size_t(2 * y) * size + 2 * x + 1) * 4) + k] +
                        px[((size_t(2 * y + 1) * size + 2 * x) * 4) + k] + px[((size_t(2 * y + 1) * size + 2 * x + 1) * 4) + k];
                out[(size_t(y) * h + x) * 4 + k] = (uint8_t)((s + 2) / 4);
            }
    return out;
}

// one of ours into dict, at most maxSize, with its mipmaps; false: it could not be made
bool AddTexture(void* dict, const data::Texture& t, int maxSize) {
    if (Call<0x7F39F0, void*>(dict, t.name)) return true;  // RwTexDictionaryFindNamedTexture: there already
    std::vector<uint8_t> px = Decode(t.dxt1, t.size);
    int size = t.size;
    while (size > maxSize && size > 4) px = Half(px, size), size /= 2;
    // rwRASTERTYPETEXTURE | rwRASTERFORMAT888 | rwRASTERFORMATMIPMAP
    void* raster = Call<0x7FB230, void*>(size, size, 32, 0x04 | 0x0600 | 0x8000);  // RwRasterCreate
    if (!raster) return false;
    int levels = 0;
    for (int s = size; s >= 1; s /= 2, ++levels) {
        auto* dst = Call<0x7FB2D0, uint8_t*>(raster, (uint8_t)levels, 5);  // RwRasterLock: write, no fetch
        if (!dst) break;
        int stride = Field<int32_t>(raster, 0x18);
        bool fits = Field<int32_t>(raster, 0x0C) == s && Field<int32_t>(raster, 0x10) == s && stride >= s * 4;
        if (fits)
            for (int y = 0; y < s; ++y) memcpy(dst + size_t(y) * stride, &px[size_t(y) * s * 4], size_t(s) * 4);
        Call<0x7FAEC0, void*>(raster);  // RwRasterUnlock
        if (!fits) {
            if (levels == 0) {
                Call<0x7FB020, int>(raster);  // RwRasterDestroy
                return false;
            }
            break;
        }
        if (s > 1) px = Half(px, s);
    }
    void* tex = Call<0x7F37C0, void*>(raster);  // RwTextureCreate
    if (!tex) {
        Call<0x7FB020, int>(raster);
        return false;
    }
    Call<0x7F38A0, void*>(tex, t.name);  // RwTextureSetName
    // RwTexture::filterAddressing: linear between the mipmaps, repeated both ways
    Ref<uint32_t>((uintptr_t)tex + 0x50) = 6u | (1u << 8) | (1u << 12);
    Call<0x7F3980, void*>(dict, tex);  // RwTexDictionaryAddTexture
    return true;
}

void TxdLoaded(int index, void*dict) {
    static int near = -2, far = -2;
    if (near == -2) {
        near = Call<0x731850, int>("bev_law2");  // CTxdStore::FindTxdSlot
        far = Call<0x731850, int>("welod_law2");
    }
    if (index != near && index != far) return;
    int n = 0;
    for (const data::Texture& t : data::kTextures) n += AddTexture(dict, t, index == near ? 1024 : kFarTexture);
    LOGI("Charlie: %d of our textures in %s", n, index == near ? "bev_law2" : "welod_law2");
}

// ---------------------------------------------------------------- no rain under the roofs
bool Covered(const CVector& p) {
    if (!g_ours) return false;
    float x = p.x - kX, y = p.y - kY, z = p.z - kZ;
    if (p.z < data::kFloorZ - 1.0f || z > data::kRoofTop) return false;
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

// ---------------------------------------------------------------- what of the map stood there
void Hide(uintptr_t poolAddr, size_t elem) {
    auto* pool = Ref<CPoolHeader*>(poolAddr);
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* e = (CEntity*)(pool->objects + (size_t)i * elem);
        for (const data::Hidden& h : data::kHidden) {
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
        game::AddTxdLoaded(&TxdLoaded);
        mem::HookFn(0x72DDB0, &HookCamNoRain, &g_origCamNoRain, "CCullZones::CamNoRain");
        mem::HookFn(0x72DDC0, &HookPlayerNoRain, &g_origPlayerNoRain, "CCullZones::PlayerNoRain");
    }
    void OnGameStarted() override {
        LOGI("Charlie: house %s (models: %s, %s, %s, %s)", g_ours ? "in place" : "not in place (not the game's block)",
             IsGames(kHouseModel, kHouseName) ? "game's" : "other", IsGames(kLodModel, kLodName) ? "game's" : "other",
             IsGames(kEastModel, kEastName) ? "game's" : "other", IsGames(kEastLodModel, kEastLodName) ? "game's" : "other");
        if (void* mi = g_ours ? ModelInfo(kHouseModel) : nullptr) Ref<float>((uintptr_t)mi + 0x18) = kDrawDistance;
        m_next = 0;
    }
    void OnPostProcess() override {
        if (!g_ours || !core::IsPlayerReady() || TimeMs() < m_next) return;
        m_next = TimeMs() + 1000;
        CPlayerPed* me = FindPlayerPed(0);
        if (!me) return;
        CVector p = me->GetPosition();
        if (std::hypot(p.x - kX, p.y - kY) > 400.0f) return;
        Hide(0xB74498, 0x38);                        // CPools::ms_pBuildingPool
        Hide(addr::ObjectPool, kObjectPoolElemSize);  // the lifebuoy is an object (from a dummy)
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
