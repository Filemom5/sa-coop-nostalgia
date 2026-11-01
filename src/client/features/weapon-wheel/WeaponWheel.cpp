// The weapon wheel, as in GTA V: held down, the key (keys/weapon_wheel in coop.ini, Q) opens a ring of the player's
// weapon slots round the middle of the screen; the mouse points at one and letting go of the key takes its weapon
// (SET_CURRENT_CHAR_WEAPON, as a script would: the game switches as for its own keys, and the others see it as ever).
// The game's world goes on meanwhile (time is everyone's in co-op: no slow motion).
//
// SA keeps one weapon per slot, so the ring has a sector per slot (the fists at the bottom, the handguns at the top), dim
// when the slot is empty or out of ammo. In the middle, the weapon pointed at, with its ammo (in the clip / in all).
// It is drawn with the game's own 2D: its polygons (CSprite2d::Draw2DPolygon), the weapons' HUD icons and its fonts.
#include "features/weapon-wheel/WeaponWheel.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::wheel {

namespace {
using namespace game;
using namespace game::script;

constexpr uintptr_t kMouseNew = 0xB73418;  // CPad::NewMouseControllerState: +0x0C, +0x10 the mouse's move (x, y)
constexpr int kSlots = 13;
// the sectors clockwise from the top: handguns, SMGs, assault rifles, shotguns, rifles, heavy, the fists (bottom),
// melee, thrown, detonator, special (spray, extinguisher, camera), gifts, goggles and parachute
constexpr int kOrder[kSlots] = {2, 4, 5, 3, 6, 7, 0, 1, 8, 12, 9, 10, 11};
constexpr float kPointerDeadZone = 0.35f;  // the pointer this near the middle picks nothing new
constexpr float kMouseToPointer = 0.012f;  // a mouse's count moves the pointer this much of the ring's radius

const char* const kNames[47] = {
    "Fist", "Brass Knuckles", "Golf Club", "Nightstick", "Knife", "Baseball Bat", "Shovel", "Pool Cue", "Katana",
    "Chainsaw", "Purple Dildo", "Dildo", "Vibrator", "Silver Vibrator", "Flowers", "Cane", "Grenade", "Tear Gas",
    "Molotov", "", "", "", "Pistol", "Silenced Pistol", "Desert Eagle", "Shotgun", "Sawn-off Shotgun",
    "Combat Shotgun", "Micro SMG", "SMG", "AK-47", "M4", "Tec-9", "Country Rifle", "Sniper Rifle", "Rocket Launcher",
    "Heat-Seeking RPG", "Flamethrower", "Minigun", "Satchel Charges", "Detonator", "Spray Can", "Fire Extinguisher",
    "Camera", "Night Vision", "Thermal Goggles", "Parachute"};

bool g_open = false;
float g_px = 0, g_py = 0;  // the pointer, the ring's radius being 1
int g_pick = -1;           // the slot pointed at

struct CRGBA {
    uint8_t r, g, b, a;
};

CPlayerPed* Me() {
    CPlayerPed* me = FindPlayerPed(0);
    return me && !me->InVehicle() ? me : nullptr;
}

// the weapon in a slot, if it can be used (the fists always)
int WeaponIn(CPlayerPed* me, int slot) {
    const CWeapon& w = me->Weapons()[slot];
    if (slot == 0) return w.type == 1 ? 1 : 0;
    if (w.type <= 0 || w.type > 46) return -1;
    bool needsAmmo = w.type >= 16 && w.type != 40 && w.type != 44 && w.type != 45 && w.type != 46;
    return needsAmmo && w.ammoTotal <= 0 ? -1 : w.type;
}

void Close(bool take) {
    if (!g_open) return;
    g_open = false;
    CPlayerPed* me = Me();
    if (!take || !me || g_pick < 0) return;
    int weapon = WeaponIn(me, g_pick);
    if (weapon < 0 || g_pick == me->ActiveWeaponSlot()) return;
    Cmd(0x01B9).I(GetPedRef(me)).I(weapon).Run();  // SET_CURRENT_CHAR_WEAPON
    LOGD("Weapon wheel: %s", kNames[weapon]);
}

// ---------------------------------------------------------------- drawing (the game's own 2D)
void Polygon(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, CRGBA c) {
    Call<0x7285B0>(x1, y1, x2, y2, x3, y3, x4, y4, &c);  // CSprite2d::Draw2DPolygon: a fan of its corners 3, 4, 2, 1
}

// a piece of the ring between two angles (radians, clockwise from the right) and two radii
void Arc(float cx, float cy, float r0, float r1, float a0, float a1, CRGBA c) {
    int steps = std::max(2, (int)std::ceil((a1 - a0) / 0.08f));
    for (int i = 0; i < steps; ++i) {
        float t0 = a0 + (a1 - a0) * i / steps, t1 = a0 + (a1 - a0) * (i + 1) / steps;
        float c0 = std::cos(t0), s0 = std::sin(t0), c1 = std::cos(t1), s1 = std::sin(t1);
        Polygon(cx + r1 * c0, cy + r1 * s0, cx + r1 * c1, cy + r1 * s1, cx + r0 * c0, cy + r0 * s0, cx + r0 * c1,
                cy + r0 * s1, c);
    }
}

// a weapon's HUD icon (the texture "<model>ICON" of its model's TXD; the fist's own sprite), tinted
void Icon(int weapon, float x, float y, float size, CRGBA tint) {
    struct {
        void* texture;
    } sprite{};  // CSprite2d
    void* info = weapon > 0 ? Call<0x743C60, void*>(weapon, (uint8_t)1) : nullptr;  // CWeaponInfo::GetWeaponInfo
    int model = info ? Field<int32_t>(info, 0x0C) : -1;                             // m_nModelId1
    if (model <= 0) {
        sprite.texture = Ref<void*>(0xBAB1FC);  // CHud::Sprites[SPRITE_FIST]
    } else {
        void* mi = Ref<void*>(addr::ModelInfoPtrs + (uintptr_t)model * 4);
        auto* txds = Ref<CPoolHeader*>(0xC8800C);  // CTxdStore::ms_pTxdPool
        int slot = mi ? Field<int16_t>(mi, 0x0A) : -1;  // m_nTxdIndex
        if (!txds || slot < 0 || slot >= txds->size || txds->byteMap[slot] < 0) return;
        void* dict = Ref<void*>((uintptr_t)txds->objects + (uintptr_t)slot * 0xC);  // TxdDef::m_pRwDictionary
        uint32_t key = Call<0x53CF70, uint32_t>(Field<uint32_t>(mi, 0x04), "ICON");  // CKeyGen::AppendStringToKey
        if (dict) sprite.texture = Call<0x734E50, void*>(dict, key);               // RwTexDictionaryFindHashNamedTexture
    }
    if (!sprite.texture) return;
    float rect[4] = {x - size / 2, y + size / 2, x + size / 2, y - size / 2};  // CRect: left, bottom, right, top
    CallMethod<0x728350>(&sprite, rect, &tint);  // CSprite2d::Draw(const CRect&, const CRGBA&)
}

void Draw() {
    CPlayerPed* me = Me();
    if (!me) return;
    auto& g = ui::Gfx();
    float s = g.Scale(), cx = g.Width() / 2, cy = g.Height() / 2;
    float r0 = 168 * s, r1 = 300 * s, mid = (r0 + r1) / 2;
    const float pi = 3.14159265f, step = 2 * pi / kSlots, gap = 0.012f;
    g.End();  // the game draws with its own state
    for (int k = 0; k < kSlots; ++k) {
        int slot = kOrder[k];
        float a0 = -pi / 2 - step / 2 + k * step, a1 = a0 + step;
        int weapon = WeaponIn(me, slot);
        bool picked = slot == g_pick;
        CRGBA fill = picked ? CRGBA{235, 235, 238, 225} : weapon >= 0 ? CRGBA{18, 18, 22, 190} : CRGBA{18, 18, 22, 110};
        Arc(cx, cy, r0, r1, a0 + gap, a1 - gap, fill);
        if (picked) Arc(cx, cy, r1, r1 + 9 * s, a0 + gap, a1 - gap, CRGBA{255, 95, 176, 255});  // the pink edge
        if (slot == me->ActiveWeaponSlot() && !picked) Arc(cx, cy, r1 - 5 * s, r1, a0 + gap, a1 - gap, CRGBA{255, 255, 255, 120});
        if (weapon >= 0) {
            float a = (a0 + a1) / 2;
            CRGBA tint = picked ? CRGBA{25, 25, 30, 255} : CRGBA{255, 255, 255, 255};
            Icon(weapon, cx + mid * std::cos(a), cy + mid * std::sin(a), 74 * s, tint);
        }
    }
    g.Begin();
    // the weapon pointed at, and its ammo
    int weapon = g_pick >= 0 ? WeaponIn(me, g_pick) : -1;
    if (weapon >= 0) {
        g.GameText(ui::GAME_FONT_MENU, cx, cy - 34 * s, kNames[weapon], 34 * s, 0xFFFFFFFF, ui::TEXT_CENTER);
        const CWeapon& w = me->Weapons()[g_pick];
        if (weapon >= 16 && weapon != 40 && weapon != 44 && weapon != 45 && weapon != 46)
            g.GameText(ui::GAME_FONT_MENU, cx, cy + 6 * s, Format("%d / %d", w.ammoInClip, w.ammoTotal), 28 * s,
                       0xFFACCBF1, ui::TEXT_CENTER);
    } else {
        g.GameText(ui::GAME_FONT_MENU, cx, cy - 16 * s, "-", 34 * s, 0xFF9A9AA6, ui::TEXT_CENTER);
    }
}

class WeaponWheel : public Module {
public:
    const char* Name() const override { return "WeaponWheel"; }
    void OnGameShutdown() override { g_open = false; }

    void OnPostProcess() override {
        if (!g_open) return;
        if (!Me() || !core::IsPlayerReady() || MenuActive()) {  // into a car, dead, the menu: closed, nothing taken
            Close(false);
            return;
        }
        if (!(GetAsyncKeyState(Cfg().weaponWheelKey) & 0x8000)) Close(true);  // let go: the weapon pointed at
    }

    void OnRender() override {
        if (g_open && !MenuActive()) Draw();
    }

    static WeaponWheel& Instance() {
        static WeaponWheel m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&WeaponWheel::Instance()); }
} g_reg;
}  // namespace

bool KeyDown() {
    CPlayerPed* me = Me();
    if (!me || g_open) return false;
    g_open = true;
    g_px = g_py = 0;
    g_pick = me->ActiveWeaponSlot();
    return true;
}

void AfterPadUpdate() {
    if (!g_open) return;
    uint8_t* mouse = (uint8_t*)kMouseNew;
    float& mx = *(float*)(mouse + 0x0C);
    float& my = *(float*)(mouse + 0x10);
    g_px += mx * kMouseToPointer;
    g_py += my * kMouseToPointer;
    mx = my = 0;  // the camera stays
    float len = std::sqrt(g_px * g_px + g_py * g_py);
    if (len > 1) g_px /= len, g_py /= len, len = 1;
    if (len >= kPointerDeadZone) {
        const float pi = 3.14159265f, step = 2 * pi / kSlots;
        float a = std::atan2(g_py, g_px) + pi / 2 + step / 2;  // from the top sector's start, clockwise
        a = std::fmod(a + 4 * pi, 2 * pi);
        g_pick = kOrder[std::min(kSlots - 1, (int)(a / step))];
    }
    CPad* pad = GetPad(0);
    pad->NewState.LeftShoulder2 = pad->NewState.RightShoulder2 = 0;  // the mouse wheel's (and Q's) change of weapon
    pad->NewState.ButtonCircle = 0;                                   // no shooting
    mouse[3] = mouse[4] = 0;
}

}  // namespace coop::wheel
