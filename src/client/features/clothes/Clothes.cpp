// Our two jackets, the group's windbreakers, on the game's own (SubUrban's "coach" model): the Grn Windbreaker becomes
// the lilac "Matunas" one, the Blk Windbreaker the black and purple "Nostalgia" one. Everyone with the mod sees them on
// whoever wears them (the clothes go between the games already, as texture names).
//
//  * the pictures: as a TXD loads (CTxdStore::LoadTxd, FinishLoadTxd: a clothes texture from player.img too), ours
//    paints over the textures "coach" and "coachsemi" when they are the game's own (JacketPaint.cpp);
//  * the wardrobe: both are in every game's wardrobe (CShopping's "bought" flags, set as a game starts: the game keeps
//    them in its save), so no shop is needed;
//  * their names: COACHA, COACHB ("Corta-vento Matunas", "Corta-vento Nostalgia"), by CText::Get.
#include <cstdint>

#include "common/lib/log/Log.h"
#include "features/clothes/JacketPaint.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop::clothes {

namespace {
using namespace game;

constexpr const char* kTextures[] = {"coach", "coachsemi"};
constexpr uintptr_t kTxdPool = 0xC8800C;          // CTxdStore::ms_pTxdPool
constexpr uintptr_t kShopKeys = 0xA97D90;         // CShopping::ms_keys
constexpr uintptr_t kShopBought = 0xA972A0;       // CShopping::ms_bHasBought
constexpr uintptr_t kShopItems = 0xA9A310;        // CShopping::ms_numBuyableItems
constexpr int kRasterLockReadWrite = 3;           // rwRASTERLOCKWRITE | rwRASTERLOCKREAD

void PaintTxd(int index) {
    auto* pool = Ref<CPoolHeader*>(kTxdPool);
    if (!pool || index < 0 || index >= pool->size || pool->byteMap[index] < 0) return;
    void* dict = Ref<void*>((uintptr_t)pool->objects + (uintptr_t)index * 0xC);  // TxdDef::m_pRwDictionary
    if (!dict) return;
    for (const char* name : kTextures) {
        void* tex = Call<0x7F39F0, void*>(dict, name);  // RwTexDictionaryFindNamedTexture
        void* raster = tex ? Ref<void*>((uintptr_t)tex) : nullptr;  // RwTexture::raster
        if (!raster) continue;
        int width = Field<int32_t>(raster, 0x0C), height = Field<int32_t>(raster, 0x10), depth = Field<int32_t>(raster, 0x14);
        if (width != 256 || height != 256 || depth != 32) continue;
        auto* pixels = Call<0x7FB2D0, uint8_t*>(raster, (uint8_t)0, kRasterLockReadWrite);  // RwRasterLock
        if (!pixels) continue;
        bool painted = Paint(name, pixels, width, height, Field<int32_t>(raster, 0x18));  // RwRaster::stride
        Call<0x7FAEC0, void*>(raster);  // RwRasterUnlock
        if (painted) LOGI("Clothes: our %s painted over the game's", name);
    }
}

using LoadTxdFn = bool(__cdecl*)(int index, void* stream);
LoadTxdFn g_origLoadTxd = nullptr, g_origFinishLoadTxd = nullptr;
bool __cdecl HookLoadTxd(int index, void* stream) {
    bool ok = g_origLoadTxd(index, stream);
    if (ok) PaintTxd(index);
    return ok;
}
bool __cdecl HookFinishLoadTxd(int index, void* stream) {
    bool ok = g_origFinishLoadTxd(index, stream);
    if (ok) PaintTxd(index);
    return ok;
}

using TextGetFn = const char*(__fastcall*)(void* self, void* edx, const char* key);
TextGetFn g_origTextGet = nullptr;
const char* __fastcall HookTextGet(void* self, void* edx, const char* key) {
    if (const char* ours = NameFor(key)) return ours;
    return g_origTextGet(self, edx, key);
}

// both jackets in the wardrobe: as bought
bool Own() {
    uint32_t n = Ref<uint32_t>(kShopItems);
    if (n == 0 || n > 560) return false;
    int found = 0;
    for (const char* name : kTextures) {
        uint32_t key = Call<0x53CF30, uint32_t>(name);  // CKeyGen::GetUppercaseKey, as CShopping::GetKey for clothes
        for (uint32_t i = 0; i < n; ++i)
            if (Ref<uint32_t>(kShopKeys + i * 4) == key) {
                Ref<uint8_t>(kShopBought + i) = 1;
                ++found;
            }
    }
    return found == 2;
}

class Clothes : public Module {
public:
    const char* Name() const override { return "Clothes"; }
    void OnInstallHooks() override {
        mem::HookFn(0x731DD0, &HookLoadTxd, &g_origLoadTxd, "CTxdStore::LoadTxd");
        mem::HookFn(0x731E40, &HookFinishLoadTxd, &g_origFinishLoadTxd, "CTxdStore::FinishLoadTxd");
        mem::HookFn(0x6A0050, &HookTextGet, &g_origTextGet, "CText::Get");
    }
    void OnGameStarted() override {
        if (!Own()) LOGW("Clothes: the windbreakers are not in this game's shop list (shopping.dat changed?)");
    }
    static Clothes& Instance() {
        static Clothes m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Clothes::Instance()); }
} g_reg;
}  // namespace

}  // namespace coop::clothes
