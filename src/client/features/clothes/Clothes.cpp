// The group's jackets on the game's own: SubUrban's "coach" model (the Grn Windbreaker becomes the lilac "Matunas"
// one, the Blk Windbreaker the black and purple "Nostalgia" one) and Victim's "hoodjack" (the Beige Hooded Jacket becomes
// the "Sunset" one). Everyone with the mod sees them on whoever wears them (the clothes go between the games already, as
// texture names).
//
//  * the pictures: as a TXD loads (shared/game/TxdLoad: a clothes texture from player.img too), ours
//    paints over the textures "coach", "coachsemi" and "hoodjackbeige" when they are the game's own (JacketPaint.cpp);
//  * in HD: CClothesBuilder::ConstructTextures makes a dressed ped's "torso" texture from the jacket's, 256 x 256 like
//    it (its skin, the jacket over it). With one of ours on, that texture gets a raster of 1024 x 1024 with mipmaps,
//    our design whole on it: the logo is sharp;
//  * the wardrobe: all are in every game's wardrobe (CShopping's "bought" flags, set as a game starts: the game keeps
//    them in its save), so no shop is needed;
//  * their names: COACHA, COACHB, HODJCK1 ("Corta-vento Matunas", "Corta-vento Nostalgia", "Jaqueta Sunset
//    Nostalgia"), by CText::Get.
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/lib/log/Log.h"
#include "features/clothes/JacketPaint.h"
#include "shared/game/Game.h"
#include "shared/game/TxdLoad.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"

namespace coop::clothes {

namespace {
using namespace game;

constexpr uintptr_t kShopKeys = 0xA97D90;         // CShopping::ms_keys
constexpr uintptr_t kShopBought = 0xA972A0;       // CShopping::ms_bHasBought
constexpr uintptr_t kShopItems = 0xA9A310;        // CShopping::ms_numBuyableItems
constexpr int kRasterLockReadWrite = 3;           // rwRASTERLOCKWRITE | rwRASTERLOCKREAD
constexpr int kRasterLockRead = 2, kRasterLockWriteNoFetch = 5;  // rwRASTERLOCKREAD; rwRASTERLOCKWRITE | NOFETCH
// rwRASTERTYPETEXTURE | rwRASTERFORMAT888 | rwRASTERFORMATMIPMAP: the dressed ped's texture's own (888), with mipmaps
constexpr int kHdRasterFlags = 0x04 | 0x0600 | 0x8000;
constexpr uint8_t kFilterLinear = 2, kFilterLinearMipLinear = 6;  // rwFILTERLINEAR, rwFILTERLINEARMIPLINEAR

uint8_t* Lock(void* raster, int level, int mode) { return Call<0x7FB2D0, uint8_t*>(raster, (uint8_t)level, mode); }  // RwRasterLock
void Unlock(void* raster) { Call<0x7FAEC0, void*>(raster); }  // RwRasterUnlock

void PaintTxd(int, void* dict) {
    for (int j = 0; j < Count(); ++j) {
        const char* name = Texture(j);
        void* tex = Call<0x7F39F0, void*>(dict, name);  // RwTexDictionaryFindNamedTexture
        void* raster = tex ? Ref<void*>((uintptr_t)tex) : nullptr;  // RwTexture::raster
        if (!raster) continue;
        int width = Field<int32_t>(raster, 0x0C), height = Field<int32_t>(raster, 0x10), depth = Field<int32_t>(raster, 0x14);
        if (width != 256 || height != 256 || depth != 32) continue;
        auto* pixels = Lock(raster, 0, kRasterLockReadWrite);
        if (!pixels) continue;
        bool painted = Paint(name, pixels, width, height, Field<int32_t>(raster, 0x18));  // RwRaster::stride
        Unlock(raster);
        if (painted) LOGI("Clothes: our %s painted over the game's", name);
    }
}

// A dressed ped's "torso" texture made from one of ours (256 x 256): a new raster of 1024 x 1024 for it, PaintHd's, and
// its mipmaps (a far ped's logo does not flicker). Anything off (not the size it should be, a level not there): it
// stays as it was, or goes without mipmaps.
void MakeHd(void* tex, int jacket) {
    void* old = Ref<void*>((uintptr_t)tex);  // RwTexture::raster
    if (!old || Field<int32_t>(old, 0x0C) != kGameSize || Field<int32_t>(old, 0x10) != kGameSize || Field<int32_t>(old, 0x14) != 32)
        return;
    std::vector<uint8_t> hd(size_t(kHdSize) * kHdSize * 4);
    const uint8_t* base = Lock(old, 0, kRasterLockRead);
    if (!base) return;
    bool ok = PaintHd(jacket, base, Field<int32_t>(old, 0x18), hd.data());
    Unlock(old);
    if (!ok) return;
    void* raster = Call<0x7FB230, void*>(kHdSize, kHdSize, 32, kHdRasterFlags);  // RwRasterCreate
    if (!raster) return;
    int levels = 0;
    std::vector<uint8_t> next;
    for (int size = kHdSize; size >= 1; size /= 2, ++levels) {
        uint8_t* px = Lock(raster, levels, kRasterLockWriteNoFetch);
        if (!px) break;
        // a mipmap's lock gives that level's size and stride
        int stride = Field<int32_t>(raster, 0x18);
        bool fits = Field<int32_t>(raster, 0x0C) == size && Field<int32_t>(raster, 0x10) == size && stride >= size * 4;
        if (fits)
            for (int y = 0; y < size; ++y) memcpy(px + size_t(y) * stride, &hd[size_t(y) * size * 4], size_t(size) * 4);
        Unlock(raster);
        if (!fits) {
            if (levels == 0) {
                Call<0x7FB020, int>(raster);  // RwRasterDestroy
                LOGW("Clothes: the HD jacket's raster is not %dx%d", kHdSize, kHdSize);
                return;
            }
            break;
        }
        if (size > 1) {
            next.resize(size_t(size / 2) * (size / 2) * 4);
            HalfSize(hd.data(), size, next.data());
            hd.swap(next);
        }
    }
    Ref<void*>((uintptr_t)tex) = raster;
    Call<0x7FB020, int>(old);  // RwRasterDestroy: CopyTexture's, only this texture had it
    // RwTexture::filterAddressing: with all its mipmaps, smoothed between them; else the plain bilinear CopyTexture gave
    uint32_t& fa = Ref<uint32_t>((uintptr_t)tex + 0x50);
    fa = (fa & ~0xFFu) | (levels == 11 ? kFilterLinearMipLinear : kFilterLinear);
    static bool s_logged = false;
    if (!s_logged) LOGI("Clothes: %s in HD (%dx%d, %d levels)", Texture(jacket), kHdSize, kHdSize, levels), s_logged = true;
}

using ConstructTexturesFn = void(__cdecl*)(void* dict, uint32_t* keys, float normal, float fat, float muscle);
ConstructTexturesFn g_origConstructTextures = nullptr;
// keys: CPedClothesDesc::m_anTextureKeys, the torso's first (CKeyGen::GetUppercaseKey of its texture's name)
void __cdecl HookConstructTextures(void* dict, uint32_t* keys, float normal, float fat, float muscle) {
    g_origConstructTextures(dict, keys, normal, fat, muscle);
    if (!dict || !keys) return;
    for (int j = 0; j < Count(); ++j)
        if (keys[0] == Call<0x53CF30, uint32_t>(Texture(j))) {  // CKeyGen::GetUppercaseKey
            if (void* tex = Call<0x7F39F0, void*>(dict, "torso")) MakeHd(tex, j);  // RwTexDictionaryFindNamedTexture
            break;
        }
}

using TextGetFn = const char*(__fastcall*)(void* self, void* edx, const char* key);
TextGetFn g_origTextGet = nullptr;
const char* __fastcall HookTextGet(void* self, void* edx, const char* key) {
    if (const char* ours = NameFor(key)) return ours;
    return g_origTextGet(self, edx, key);
}

// all our jackets in the wardrobe: as bought
bool Own() {
    uint32_t n = Ref<uint32_t>(kShopItems);
    if (n == 0 || n > 560) return false;
    int found = 0;
    for (int j = 0; j < Count(); ++j) {
        uint32_t key = Call<0x53CF30, uint32_t>(Texture(j));  // CKeyGen::GetUppercaseKey, as CShopping::GetKey for clothes
        for (uint32_t i = 0; i < n; ++i)
            if (Ref<uint32_t>(kShopKeys + i * 4) == key) {
                Ref<uint8_t>(kShopBought + i) = 1;
                ++found;
            }
    }
    return found == Count();
}

class Clothes : public Module {
public:
    const char* Name() const override { return "Clothes"; }
    void OnInstallHooks() override {
        game::AddTxdLoaded(&PaintTxd);
        mem::HookFn(0x6A0050, &HookTextGet, &g_origTextGet, "CText::Get");
        mem::HookFn(0x5A6040, &HookConstructTextures, &g_origConstructTextures, "CClothesBuilder::ConstructTextures");
    }
    void OnGameStarted() override {
        if (!Own()) LOGW("Clothes: our jackets are not all in this game's shop list (shopping.dat changed?)");
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
