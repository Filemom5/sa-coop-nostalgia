// Each player's own houses and cars, as on the group's old role-play server (docs/casas.md):
//  * /casa marcar at any door of the map: the house is there. The first one is free; another one, while the player has
//    one, is bought at a price of its neighbourhood (the game's own population zone type: poor, average, rich...,
//    and the city), as such a house cost in 1992;
//  * its door (the blue house of the game's bought properties) takes whoever walks into it to one of the game's own house
//    interiors (the burglary houses, the safe houses, a few more): each house of the group gets one no other house has,
//    so nobody meets anybody in there. The arrow inside takes back out to the door;
//  * the owner's game puts its save disk in front of the door (the game's save menu: the host saves its world, a
//    partner its profile, as at any save point), a blip on the radar, and its player there when it joins the session
//    (the main house); /home goes there;
//  * /casa carro in a car, parked: that car (model, colours, mod shop parts, paintjob) stands there whenever its owner
//    comes near and it is not out on the road. The first one is free, another one costs what the game says the model is
//    worth (its handling's value). Only the owner's game makes it: the others see its copy (Vehicles);
//  * the houses are kept in the player's own user files (coop_homes.bin) and sent to the others (G_HOME), who show
//    their doors and let their players in.
#include "features/home/Home.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "common/api/protocol/Protocol.h"
#include "common/lib/buffer/Buffer.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "entities/profile/Profile.h"
#include "entities/session/Flow.h"
#include "entities/vehicle/Vehicles.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/game/Script.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::home {

namespace {
using namespace game;
using game::script::Cmd;

enum : uint8_t { OP_HOUSES = 1 };
constexpr int kHousesMax = 8, kCarsMax = 10;
constexpr float kDoorReach = 1.1f, kDiskReach = 1.0f, kArm = 2.2f;  // walked into; and out of it again before the next time
constexpr float kIconsNear = 70.0f;      // the doors' icons are made this near
constexpr float kCarNear = 120.0f;       // a car comes to its place when its owner is this near
constexpr float kCarGoneFar = 300.0f;    // and goes back when it stands empty this far from everyone (150 for the others)
constexpr int kModelHouse = 1272, kModelDisk = 1277, kModelArrow = 1318;  // the bought property, the save disk, the arrow
constexpr uint64_t kAskMs = 20000, kPanelMs = 60000;
constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------- the game's house interiors
// where a player stands in each (as the burglary houses and the safe houses put it there), and its area
enum : uint8_t { I_HOUSE, I_SAFE, I_RYDER, I_SWEET, I_COLONEL, I_TRAILER, I_MADDOGG, I_JOHNSON, I_BURNING };
struct Interior {
    uint8_t area;
    float x, y, z;
    uint8_t kind, n;
};
const Interior kInteriors[] = {
    {1, 224.6351f, 1289.012f, 1082.141f, I_HOUSE, 1},    {2, 225.756f, 1240.000f, 1082.149f, I_HOUSE, 2},
    {2, 447.470f, 1398.348f, 1084.305f, I_HOUSE, 3},     {2, 491.740f, 1400.541f, 1080.265f, I_HOUSE, 4},
    {3, 234.733f, 1190.391f, 1080.258f, I_HOUSE, 5},     {4, -262.91f, 1454.966f, 1084.367f, I_HOUSE, 6},
    {4, 221.4296f, 1142.423f, 1082.609f, I_HOUSE, 7},    {4, 261.1168f, 1286.519f, 1080.258f, I_HOUSE, 8},
    {5, 22.79996f, 1404.642f, 1084.43f, I_HOUSE, 9},     {5, 228.9003f, 1114.477f, 1080.992f, I_HOUSE, 10},
    {5, 140.5631f, 1369.051f, 1083.864f, I_HOUSE, 11},   {9, 85.32596f, 1323.585f, 1083.859f, I_HOUSE, 12},
    {9, 260.3189f, 1239.663f, 1084.258f, I_HOUSE, 13},   {10, 21.241f, 1342.153f, 1084.375f, I_HOUSE, 14},
    {6, 234.319f, 1066.455f, 1084.208f, I_HOUSE, 15},    {6, -69.049f, 1354.056f, 1080.211f, I_HOUSE, 16},
    {15, -285.711f, 1470.697f, 1084.375f, I_HOUSE, 17},  {15, 327.808f, 1479.74f, 1084.438f, I_HOUSE, 18},
    {15, 375.572f, 1417.439f, 1081.328f, I_HOUSE, 19},   {15, 384.644f, 1471.479f, 1080.195f, I_HOUSE, 20},
    {15, 295.467f, 1474.697f, 1080.258f, I_HOUSE, 21},   {8, -42.490f, 1407.644f, 1084.43f, I_HOUSE, 22},
    {10, 2262.83f, -1137.71f, 1050.63f, I_SAFE, 1},      {8, 2365.2383f, -1134.2969f, 1050.875f, I_SAFE, 2},
    {6, 2333.033f, -1073.96f, 1049.023f, I_SAFE, 3},     {1, 2216.54f, -1076.29f, 1050.484f, I_SAFE, 4},
    {6, 2194.291f, -1204.015f, 1049.023f, I_SAFE, 5},    {6, 2308.871f, -1210.717f, 1049.023f, I_SAFE, 6},
    {12, 2324.3848f, -1148.4805f, 1050.7101f, I_SAFE, 7}, {2, 2237.59f, -1080.8699f, 1049.0234f, I_SAFE, 8},
    {9, 2253.174f, -1139.01f, 1050.633f, I_SAFE, 9},     {10, 2264.5231f, -1210.5229f, 1049.0234f, I_SAFE, 10},
    {11, 2282.9766f, -1140.2861f, 1050.8984f, I_SAFE, 11}, {5, 2351.154f, -1180.577f, 1027.977f, I_BURNING, 0},
    {2, 2464.211f, -1697.952f, 1013.508f, I_RYDER, 0},   {1, 2525.042f, -1679.115f, 1015.499f, I_SWEET, 0},
    {8, 2807.899f, -1172.921f, 1025.57f, I_COLONEL, 0},  {2, 0.344f, -0.514f, 999.4284f, I_TRAILER, 0},
    {5, 1262.1451f, -785.3298f, 1091.9063f, I_MADDOGG, 0}, {3, 2496.05f, -1693.926f, 1014.742f, I_JOHNSON, 0},
};
constexpr int kInteriorCount = (int)(sizeof kInteriors / sizeof kInteriors[0]);

std::string InteriorName(int i) {
    if (i < 0 || i >= kInteriorCount) return "?";
    const Interior& in = kInteriors[i];
    switch (in.kind) {
    case I_HOUSE: return Format(TR("Дом %d", "House %d"), in.n);
    case I_SAFE: return Format(TR("Убежище %d", "Safe house %d"), in.n);
    case I_RYDER: return TR("Дом Райдера", "Ryder's house");
    case I_SWEET: return TR("Дом Свита", "Sweet's house");
    case I_COLONEL: return TR("Дом полковника", "The Colonel's house");
    case I_TRAILER: return TR("Трейлер", "Trailer");
    case I_MADDOGG: return TR("Особняк Мэдд Догга", "Madd Dogg's mansion");
    case I_JOHNSON: return TR("Дом Джонсонов", "The Johnson house");
    default: return TR("Дом из миссии", "The mission's house");
    }
}

// ---------------------------------------------------------------- houses and cars
struct V3 {
    float x = 0, y = 0, z = 0;
};
struct House {
    V3 door;               // where the player stood marking it, facing the door
    float heading = 0.0f;  // degrees
    V3 icon;               // its icon: at the door itself, as the game's entrances stand (hasIcon: found)
    bool hasIcon = false;
    uint8_t interior = 0;
    int32_t paid = 0;
    std::string zone;      // its neighbourhood as the game names it
    uint8_t city = 0;      // 0 the country, 1 Los Santos, 2 San Fierro, 3 Las Venturas
};
struct Car {
    int16_t model = 0;
    V3 at;
    float heading = 0.0f;
    uint8_t colors[4] = {};
    uint16_t upgrades[15] = {};
    int8_t paintjob = -1;
    int32_t paid = 0;
};
struct Own {
    std::vector<House> houses;
    int main = 0;  // the one /home and the session's start go to
    std::vector<Car> cars;
    bool loaded = false;
} g_own;

// what is out in this game: icons, blips, cars
struct Icons {
    int door = 0, disk = 0, blip = 0;
    bool doorArmed = false, diskArmed = false;  // left since the last time (made armed when made away from us)
};
std::vector<Icons> g_ownIcons;
std::map<uint8_t, std::vector<House>> g_others;
std::map<uint8_t, std::vector<Icons>> g_otherIcons;
struct CarOut {
    CVehicle* v = nullptr;
    int handle = 0;
    bool away = false;      // lost (wrecked, taken): back only once its owner was far from its place
    uint64_t lookUntil = 0;  // its parts still coming on
};
std::vector<CarOut> g_carsOut;

// inside a house of the group now
struct Inside {
    bool on = false;
    House house;
    int exitIcon = 0;
    bool armed = false;
    V3 exitAt;             // the way out: the interior's own door (hasExit: found), else where we came in
    bool hasExit = false;
} g_in;
// The game's own doors of that interior lead to its real street door, another house of the map (02.11: out of the
// player's house through the yellow marker, in another part of the city): shut while we are in there.
constexpr uintptr_t kEnexPool = 0x96A7D8;  // CEntryExitManager::mp_poolEntryExits, CEntryExit 0x3C bytes, +0x30 m_nFlags
std::vector<int> g_shut;                   // the pool slots we shut (ENABLE_ACCESS taken off)
void ShutDoors() {
    auto* pool = Ref<CPoolHeader*>(kEnexPool);
    for (int i = 0; pool && i < pool->size; ++i) {
        uintptr_t e = (uintptr_t)pool->objects + (uintptr_t)i * 0x3C;
        uint16_t& f = Ref<uint16_t>(e + 0x30);
        if (pool->byteMap[i] < 0 || !IsPrivateDoor(e) || !(f & 0x4000)) continue;
        f &= (uint16_t)~0x4000;
        if (std::find(g_shut.begin(), g_shut.end(), i) == g_shut.end()) g_shut.push_back(i), LOGD("Homes: the game's door %.8s shut", (const char*)e);
    }
}
void OpenDoors() {
    auto* pool = Ref<CPoolHeader*>(kEnexPool);
    for (int i : g_shut)
        if (pool && i < pool->size && pool->byteMap[i] >= 0) Ref<uint16_t>((uintptr_t)pool->objects + (uintptr_t)i * 0x3C + 0x30) |= 0x4000;
    g_shut.clear();
    SetPrivateInterior(0, CVector());
}
// a trip through a door, to a house or home: the fade, then the move
struct Trip {
    int stage = 0;
    uint64_t ms = 0;
    int area = 0;
    V3 to;
    float heading = 0.0f;
    bool intoHouse = false;
    House house;
} g_trip;
// a question for Y or N
enum : int { ASK_NONE, ASK_HOUSE, ASK_CAR, ASK_DROP_HOUSE, ASK_DROP_CAR };
struct Ask {
    int kind = ASK_NONE;
    int price = 0, index = -1;
    House house;
    Car car;
    std::string text;
    uint64_t until = 0;
} g_ask;
uint64_t g_panelUntil = 0;
bool g_spawnPending = false;  // a partner just joined: to its main house once in the host's world
uint64_t g_readyMs = 0;

V3 ToV3(const CVector& p) { return {p.x, p.y, p.z}; }
CVector ToCV(const V3& p) { return CVector(p.x, p.y, p.z); }
float Flat(const V3& a, const V3& b) { return std::hypot(a.x - b.x, a.y - b.y); }
float Dist(const V3& a, const V3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
float Wrap(float deg) { return std::fmod(std::fmod(deg, 360.0f) + 360.0f, 360.0f); }
V3 Ahead(float heading) { return {-std::sin(heading * kPi / 180.0f), std::cos(heading * kPi / 180.0f), 0.0f}; }
V3 Right(float heading) { return {std::cos(heading * kPi / 180.0f), std::sin(heading * kPi / 180.0f), 0.0f}; }
V3 Add(const V3& a, const V3& d, float k) { return {a.x + d.x * k, a.y + d.y * k, a.z + d.z * k}; }
int CjRef() { return GetPedRef(FindPlayerPed(0)); }
V3 CjPos() { return ToV3(FindPlayerPed(0)->GetPosition()); }
int32_t& Money() { return PlayerInfos()[0].money; }

// the house's icon: at its door (by the wall in front of where it was marked), else where it was marked
V3 DoorSpot(const House& h) { return h.hasIcon ? h.icon : h.door; }
// the door's street side, where one comes out; the disk at its right
V3 OutSpot(const House& h) { return Add(DoorSpot(h), Ahead(h.heading), -1.4f); }
V3 DiskSpot(const House& h) { return Add(Add(DoorSpot(h), Right(h.heading), 1.6f), Ahead(h.heading), -0.9f); }

// The wall ahead of where the house was marked (the player faced its door): the icon 0.7 m in front of it, as the game's
// own yellow markers stand at a building's door, not where the player stood (03.11). Nothing within 5 m: half a metre on.
void FindIcon(House& h) {
    CPlayerPed* me = FindPlayerPed(0);
    V3 f = Ahead(h.heading);
    CVector from(h.door.x, h.door.y, h.door.z + 0.3f), to(h.door.x + f.x * 5.0f, h.door.y + f.y * 5.0f, h.door.z + 0.3f);
    CEntity*& ignore = Ref<CEntity*>(0xB7CD68);  // CWorld::pIgnoreEntity: not our own player
    CEntity* saved = ignore;
    ignore = me;
    uint8_t cp[0x2C] = {};  // CColPoint: m_vecPoint first
    CEntity* hit = nullptr;
    bool any = Call<0x56BA00, bool>(&from, &to, (void*)cp, &hit, true, false, false, true, false, true, false, false);  // CWorld::ProcessLineOfSight
    ignore = saved;
    float d = any ? std::hypot(((CVector*)cp)->x - from.x, ((CVector*)cp)->y - from.y) : 1.3f;
    d = std::clamp(d - 0.7f, 0.0f, 4.3f);
    h.icon = Add(h.door, f, d);
    h.hasIcon = true;
    LOGI("Homes: the door of the house in %s %.1f m ahead of where it was marked%s", h.zone.c_str(), d, any ? "" : " (no wall)");
}

std::string Dollars(int v) {
    std::string s = std::to_string(std::max(v, 0)), out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i && (s.size() - i) % 3 == 0) out += Cfg().english ? ',' : ' ';
        out += s[i];
    }
    return "$" + out;
}

// a key of the game's text (an English game's: what our fonts show), else the key itself
std::string GameText(const char* key) {
    char k[9] = {};
    memcpy(k, key, 8);
    if (!k[0]) return "";
    const char* t = CallMethod<0x6A0050, const char*>((void*)0xC1B340, k);  // CText::Get (TheText)
    std::string out;
    for (const char* c = t; c && *c && out.size() < 40; ++c)
        if (*c >= 32 && *c < 127 && *c != '~') out += *c;
    return out.empty() ? std::string(k) : out;
}
std::string ZoneName(const V3& p) {
    CVector v = ToCV(p);
    const char* zone = Call<0x572360, const char*>(&v, (uint8_t)0);  // CTheZones::FindSmallestZoneForPosition: a CZone
    return zone ? GameText(zone + 8) : std::string();                 // CZone::m_szTextKey
}
int City(const V3& p) {
    CVector v = ToCV(p);
    return std::clamp(Call<0x572300, int>(&v), 0, 3);  // CTheZones::GetLevelFromPosition
}
const char* CityName(int city) {
    switch (city) {
    case 1: return "Los Santos";
    case 2: return "San Fierro";
    case 3: return "Las Venturas";
    default: return "San Andreas";
    }
}
std::string CarName(int model) {
    void* mi = IsValidModel(model) ? Ref<void*>(addr::ModelInfoPtrs + (uintptr_t)model * 4) : nullptr;
    return mi ? GameText((const char*)mi + 0x32) : std::to_string(model);  // CVehicleModelInfo::m_szGameName
}

// ---------------------------------------------------------------- prices
// a house with its plot in 1992, the game's year, by the population zone type the game gives where it stands
// (popcycle.dat: business, desert, entertainment, countryside, rich, average, poor, gangland, beach, shopping, park,
// industry, busy entertainment, busy shopping, posh shopping, secluded rich, airport, golf club, factories, runway)
constexpr int kZonePrice[20] = {350000, 70000,  380000, 130000, 1500000, 240000, 140000, 110000, 600000, 300000,
                                400000, 120000, 420000, 320000, 900000,  3000000, 150000, 750000, 100000, 150000};
const char* ZoneKind(int type) {
    switch (type) {
    case 1: return TR("пустыня", "desert");
    case 3: return TR("деревня", "countryside");
    case 4: return TR("богатый район", "rich neighbourhood");
    case 5: return TR("средний район", "average neighbourhood");
    case 6: return TR("бедный район", "poor neighbourhood");
    case 7: return TR("гетто", "the hood");
    case 8: return TR("пляж", "beach");
    case 10: case 17: return TR("у парка", "by a park");
    case 11: case 18: return TR("промзона", "industrial");
    case 14: return TR("элитный район", "posh district");
    case 15: return TR("закрытый богатый район", "secluded rich hills");
    case 16: case 19: return TR("у аэропорта", "by the airport");
    default: return TR("центр", "downtown");
    }
}
int ZoneType() {
    int t = 0;
    Cmd(0x08D3).Out(&t).Run();  // GET_CURRENT_POPULATION_ZONE_TYPE: CPopCycle's, where we stand
    return std::clamp(t, 0, 19);
}
int HousePrice(int type, int city) {
    float f = city == 2 ? 1.2f : city == 3 ? 0.6f : 1.0f;  // San Fierro dear, Las Venturas cheap, as then
    return (int)(kZonePrice[type] * f / 5000.0f + 0.5f) * 5000;
}
// what the game says the model is worth (tHandlingData::m_nMonetaryValue)
int CarValue(CVehicle* v) {
    uintptr_t handling = Ref<uintptr_t>((uintptr_t)v + 0x384);  // CVehicle::m_pHandlingData
    int value = handling ? Ref<int32_t>(handling + 0xD8) : 0;
    if (value < 500 || value > 1000000) value = 25000;
    return (value + 250) / 500 * 500;
}

// ---------------------------------------------------------------- kept on disk, sent to the others
std::wstring FilePath() {
    std::wstring dir = core::UserFilesDir();
    return dir.empty() ? std::wstring() : dir + L"coop_homes.bin";
}
void WriteHouse(Writer& w, const House& h) {
    w.F32(h.door.x), w.F32(h.door.y), w.F32(h.door.z), w.F32(h.heading), w.U8(h.interior);
    std::string z = h.zone.substr(0, 40);
    w.U8((uint8_t)z.size());
    w.Bytes(z.data(), z.size());
    w.U8(h.city);
    w.U8(h.hasIcon ? 1 : 0), w.F32(h.icon.x), w.F32(h.icon.y), w.F32(h.icon.z);
}
// version 1 of the file had no icon
House ReadHouse(Reader& r, int version = 2) {
    House h;
    h.door.x = r.F32(), h.door.y = r.F32(), h.door.z = r.F32(), h.heading = r.F32(), h.interior = r.U8();
    int n = r.U8();
    for (int i = 0; i < n && r.Ok(); ++i) h.zone += (char)r.U8();
    h.city = r.U8();
    if (version >= 2) h.hasIcon = r.U8() != 0, h.icon.x = r.F32(), h.icon.y = r.F32(), h.icon.z = r.F32();
    if (h.interior >= kInteriorCount) h.interior = 0;
    return h;
}

void SaveOwn() {
    std::wstring path = FilePath();
    if (path.empty()) return;
    Writer w;
    w.U32(0x32454D48);  // "HME2"
    w.U8((uint8_t)g_own.houses.size()), w.U8((uint8_t)g_own.main);
    for (const House& h : g_own.houses) WriteHouse(w, h), w.I32(h.paid);
    w.U8((uint8_t)g_own.cars.size());
    for (const Car& c : g_own.cars) {
        w.I16(c.model), w.F32(c.at.x), w.F32(c.at.y), w.F32(c.at.z), w.F32(c.heading);
        w.Bytes(c.colors, 4);
        for (uint16_t u : c.upgrades) w.U16(u);
        w.I8(c.paintjob), w.I32(c.paid);
    }
    if (!WriteFileBytes(path, w.Data(), w.Size())) LOGW("Homes: could not write %s", WideToUtf8(path).c_str());
}

void LoadOwn() {
    g_own = Own{};
    g_own.loaded = true;
    std::vector<uint8_t> b;
    std::wstring path = FilePath();
    if (path.empty() || !ReadFileBytes(path, b) || b.size() < 6) return;
    Reader r(b.data(), b.size());
    uint32_t magic = r.U32();
    int version = magic == 0x32454D48 ? 2 : magic == 0x31454D48 ? 1 : 0;
    if (!version) return LOGW("Homes: %s is not ours", WideToUtf8(path).c_str());
    int n = r.U8();
    g_own.main = r.U8();
    for (int i = 0; i < n && r.Ok(); ++i) {
        House h = ReadHouse(r, version);
        h.paid = r.I32();
        if (r.Ok()) g_own.houses.push_back(h);
    }
    int m = r.U8();
    for (int i = 0; i < m && r.Ok(); ++i) {
        Car c;
        c.model = r.I16(), c.at.x = r.F32(), c.at.y = r.F32(), c.at.z = r.F32(), c.heading = r.F32();
        r.Raw(c.colors, 4);
        for (uint16_t& u : c.upgrades) u = r.U16();
        c.paintjob = r.I8(), c.paid = r.I32();
        if (r.Ok() && IsVehicleModel(c.model)) g_own.cars.push_back(c);
    }
    if (g_own.main >= (int)g_own.houses.size()) g_own.main = 0;
    LOGI("Homes: %zu houses, %zu cars", g_own.houses.size(), g_own.cars.size());
}

void SendOwn() {
    if (!Net().Online()) return;
    Writer w(proto::G_HOME);
    w.U8(OP_HOUSES), w.U8((uint8_t)g_own.houses.size());
    for (const House& h : g_own.houses) WriteHouse(w, h);
    Net().Send(proto::CH_RELIABLE, w);
}

// an interior no house of the group has (its first after `after`), else the next one
bool Taken(int interior, int skipOwn) {
    for (int i = 0; i < (int)g_own.houses.size(); ++i)
        if (i != skipOwn && g_own.houses[i].interior == interior) return true;
    for (auto& [id, list] : g_others)
        for (const House& h : list)
            if (h.interior == interior) return true;
    return false;
}
int FreeInterior(int after, int skipOwn) {
    for (int k = 1; k <= kInteriorCount; ++k) {
        int i = (after + k + kInteriorCount) % kInteriorCount;
        if (!Taken(i, skipOwn)) return i;
    }
    return (after + 1 + kInteriorCount) % kInteriorCount;
}

// ---------------------------------------------------------------- icons
bool ModelReady(int model) {
    if (IsModelLoaded(model)) return true;
    RequestModel(model);
    return false;
}
int MakeIcon(int model, const V3& at, int area) {
    if (!ModelReady(model) || !HasRoom(ObjectPool(), kKeepObjects, "object")) return 0;
    int h = 0;
    Cmd(0x029B).I(model).F(at.x).F(at.y).F(at.z).Out(&h).Run();  // CREATE_OBJECT_NO_OFFSET
    if (!h) return 0;
    Cmd(0x0382).I(h).I(0).Run();                    // SET_OBJECT_COLLISION
    if (area) Cmd(0x0566).I(h).I(area).Run();      // SET_OBJECT_AREA_VISIBLE
    return h;
}
void DropIcon(int& h) {
    if (h && GetObject(h)) Cmd(0x0108).I(h).Run();  // DELETE_OBJECT
    h = 0;
}
void Spin(int h) {
    if (h && GetObject(h)) Cmd(0x0177).I(h).F(Wrap(NowMs() * 0.09f)).Run();  // SET_OBJECT_HEADING: a quarter turn a second
}
void DropIcons(Icons& ic, bool blip) {
    DropIcon(ic.door), DropIcon(ic.disk);
    if (blip && ic.blip) Cmd(0x0164).I(ic.blip).Run();  // REMOVE_BLIP
    if (blip) ic.blip = 0;
}
void DropAllIcons() {
    for (Icons& ic : g_ownIcons) DropIcons(ic, true);
    g_ownIcons.clear();
    for (auto& [id, list] : g_otherIcons)
        for (Icons& ic : list) DropIcons(ic, false);
    g_otherIcons.clear();
    DropIcon(g_in.exitIcon);
}

// ---------------------------------------------------------------- moving the player
void StartTrip(int area, const V3& to, float heading, bool intoHouse, const House& h) {
    if (g_trip.stage) return;
    g_trip = Trip{};
    g_trip.stage = 1, g_trip.ms = NowMs(), g_trip.area = area, g_trip.to = to, g_trip.heading = heading;
    g_trip.intoHouse = intoHouse, g_trip.house = h;
    Cmd(0x016A).I(400).I(0).Run();  // DO_FADE out
}
void GoOut(const House& h) { StartTrip(0, OutSpot(h), Wrap(h.heading + 180.0f), false, h); }

void UpdateTrip() {
    if (g_trip.stage != 1 || NowMs() - g_trip.ms < 450) return;
    CPlayerPed* me = FindPlayerPed(0);
    int h = CjRef();
    const V3& p = g_trip.to;
    if (me->InVehicle()) Cmd(0x0362).I(h).F(p.x).F(p.y).F(p.z - 1.0f).Run();  // WARP_CHAR_FROM_CAR_TO_COORD
    Cmd(0x04BB).I(g_trip.area).Run();                       // SET_AREA_VISIBLE
    Cmd(0x0860).I(h).I(g_trip.area).Run();                  // SET_CHAR_AREA_VISIBLE
    Cmd(0x03CB).F(p.x).F(p.y).F(p.z).Run();                 // LOAD_SCENE: its ground first
    Cmd(0x00A1).I(h).F(p.x).F(p.y).F(p.z - 1.0f).Run();     // SET_CHAR_COORDINATES
    Cmd(0x0173).I(h).F(g_trip.heading).Run();               // SET_CHAR_HEADING
    LightArea(g_trip.area, ToCV(p));
    Cmd(0x0373).Run();                                      // SET_CAMERA_BEHIND_PLAYER
    DropIcon(g_in.exitIcon);
    g_in = Inside{};
    OpenDoors();
    if (g_trip.intoHouse) {
        g_in.on = true;
        g_in.house = g_trip.house;
        SetPrivateInterior(g_trip.area, ToCV(p));
        ShutDoors();
        LOGI("Homes: into %s (area %d)", InteriorName(g_trip.house.interior).c_str(), g_trip.area);
    }
    // the door and the disk we stand at wait till we stepped away
    for (Icons& ic : g_ownIcons) ic.doorArmed = ic.diskArmed = false;
    for (auto& [id, list] : g_otherIcons)
        for (Icons& ic : list) ic.doorArmed = false;
    Cmd(0x016A).I(500).I(1).Run();  // DO_FADE in
    g_trip.stage = 0;
}

bool FreeToGo(bool tellWhy) {
    CPlayerPed* me = FindPlayerPed(0);
    const char* why = nullptr;
    if (core::IsOnMission()) why = TR("Не во время миссии.", "Not during a mission.");
    else if (me->Health() <= 0.0f || PlayerInfos()[0].playerState != 0) why = TR("Сейчас нельзя.", "Not now.");
    if (why && tellWhy) ui::TheChat().AddError(why);
    return !why;
}

// ---------------------------------------------------------------- cars
bool InPool(CVehicle* v) {
    CPoolHeader* pool = VehiclePool();
    for (int i = 0; pool && v && i < pool->size; ++i)
        if (pool->byteMap[i] >= 0 && (CVehicle*)(pool->objects + (size_t)i * kVehiclePoolElemSize) == v) return true;
    return false;
}
bool SpotClear(const V3& at, CVehicle* except) {
    CPoolHeader* pool = VehiclePool();
    for (int i = 0; pool && i < pool->size; ++i) {
        if (pool->byteMap[i] < 0) continue;
        auto* v = (CVehicle*)(pool->objects + (size_t)i * kVehiclePoolElemSize);
        if (v != except && Dist(ToV3(v->GetPosition()), at) < 3.0f) return false;
    }
    return true;
}
// the mod shop's parts and the paintjob on the car, as Vehicles does for a copy: true when all are on
bool PutLook(int handle, CVehicle* v, const Car& c) {
    bool done = true;
    for (int slot = 0; slot < 15; ++slot) {
        int16_t want = (int16_t)c.upgrades[slot], have = (int16_t)v->Upgrades()[slot];
        if (want == have || want < 0 || !IsValidModel(want) || !Call<0x49B010, bool>(want, v)) continue;  // IsValidModForVehicle
        Cmd(0x06E9).I(want).Run();                      // REQUEST_VEHICLE_MOD
        if (!Cmd(0x06EA).I(want).Run()) {               // HAS_VEHICLE_MOD_LOADED
            done = false;
            continue;
        }
        int dummy = 0;
        Cmd(0x06E7).I(handle).I(want).Out(&dummy).Run();  // ADD_VEHICLE_MOD
    }
    int pj = -1;
    Cmd(0x0988).I(handle).Out(&pj).Run();  // GET_CURRENT_VEHICLE_PAINTJOB
    if (c.paintjob >= 0 && c.paintjob < 4 && pj != c.paintjob) Cmd(0x06ED).I(handle).I(c.paintjob).Run();  // GIVE_VEHICLE_PAINTJOB
    memcpy(v->Colors(), c.colors, 4);
    return done;
}
void TakeLook(int handle, CVehicle* v, Car& c) {
    memcpy(c.colors, v->Colors(), 4);
    for (int slot = 0; slot < 15; ++slot) c.upgrades[slot] = v->Upgrades()[slot];
    int pj = -1;
    Cmd(0x0988).I(handle).Out(&pj).Run();
    c.paintjob = (int8_t)(pj >= 0 && pj < 4 ? pj : -1);
}

// another player this near (by its last state)
bool OthersNear(const V3& at, float reach) {
    for (auto& [id, rp] : Players::Instance().All())
        if (rp.hasState && Dist(ToV3(rp.state.pos), at) < reach) return true;
    return false;
}

void UpdateCars() {
    static uint64_t lastMs = 0;
    if (NowMs() - lastMs < 400) return;
    lastMs = NowMs();
    g_carsOut.resize(g_own.cars.size());
    CPlayerPed* me = FindPlayerPed(0);
    V3 mp = g_in.on ? g_in.house.door : CjPos();  // in a house (its interior far off in the map): at its door
    for (size_t i = 0; i < g_own.cars.size(); ++i) {
        Car& c = g_own.cars[i];
        CarOut& o = g_carsOut[i];
        if (o.v) {
            bool alive = InPool(o.v) && o.v->ModelIndex() == c.model && o.v->Status() != 5 && o.v->Health() >= 250.0f;
            if (!alive) {
                if (InPool(o.v) && GetVehicle(o.handle) == o.v) Cmd(0x01C3).I(o.handle).Run();  // MARK_CAR_AS_NO_LONGER_NEEDED
                o = CarOut{};
                o.away = true;
                continue;
            }
            NetVehicle* nv = Vehicles::Instance().Find(o.v);
            bool mirror = nv && Vehicles::Instance().IsMirror(*nv);
            bool empty = !o.v->Driver() && o.v->NumPassengers() == 0;
            V3 at = ToV3(o.v->GetPosition());
            if (o.lookUntil && NowMs() < o.lookUntil && !mirror) {
                if (PutLook(o.handle, o.v, c)) o.lookUntil = 0;
            }
            // out on the road with someone else (a friend drove it off): no longer ours to keep
            if (mirror) {
                o = CarOut{};
                o.away = true;
                continue;
            }
            // left empty far from everyone: back home, made again at its place when its owner comes
            if (empty && Dist(mp, at) > kCarGoneFar && !OthersNear(at, 150.0f)) {
                if (GetVehicle(o.handle) == o.v) Cmd(0x00A6).I(o.handle).Run();  // DELETE_CAR
                LOGI("Homes: the %s went back to its place", CarName(c.model).c_str());
                o = CarOut{};
            }
            continue;
        }
        float d = Dist(mp, c.at);
        if (o.away) {
            if (d > 200.0f) o.away = false;
            continue;
        }
        if (d > kCarNear || d < 4.0f || me->AreaCode() != 0 || !SpotClear(c.at, nullptr)) continue;
        if (!ModelReady(c.model) || !HasRoom(VehiclePool(), kKeepVehicles, "vehicle")) continue;
        int h = -1;
        Cmd(0x00A5).I(c.model).F(c.at.x).F(c.at.y).F(c.at.z).Out(&h).Run();  // CREATE_CAR
        CVehicle* v = GetVehicle(h);
        if (!v) continue;
        Cmd(0x0175).I(h).F(c.heading).Run();  // SET_CAR_HEADING
        o.v = v, o.handle = h, o.lookUntil = NowMs() + 10000;
        PutLook(h, v, c);
        LOGI("Homes: the %s at its place", CarName(c.model).c_str());
    }
}

// ---------------------------------------------------------------- each frame
bool Ready() { return core::IsPlayerReady() && !flow::HostWorldLoading() && (!Net().Online() || flow::InSharedWorld()); }

void UpdateDoors() {
    CPlayerPed* me = FindPlayerPed(0);
    V3 p = CjPos();
    int area = me->AreaCode();
    bool onFoot = !me->InVehicle() && me->Health() > 0.0f;
    // ours: the blip always, the door and the disk near
    g_ownIcons.resize(g_own.houses.size());
    for (size_t i = 0; i < g_own.houses.size(); ++i) {
        const House& h = g_own.houses[i];
        Icons& ic = g_ownIcons[i];
        if (!ic.blip) {
            int b = 0;
            Cmd(0x0570).F(h.door.x).F(h.door.y).F(h.door.z).I(35).Out(&b).Run();  // ADD_SHORT_RANGE_SPRITE_BLIP_FOR_CONTACT_POINT: a save house
            ic.blip = b;
        }
        bool near = area == 0 && Flat(p, h.door) < kIconsNear;
        if (!near) {
            DropIcons(ic, false);
            continue;
        }
        if (!g_own.houses[i].hasIcon && Flat(p, h.door) < 25.0f) {
            FindIcon(g_own.houses[i]);
            DropIcons(ic, false);
            SaveOwn();
            SendOwn();
        }
        V3 at = DoorSpot(h);
        if (!ic.door) ic.door = MakeIcon(kModelHouse, {at.x, at.y, at.z - 0.35f}, 0), ic.doorArmed = Dist(p, at) > kArm;
        V3 disk = DiskSpot(h);
        if (!ic.disk) ic.disk = MakeIcon(kModelDisk, {disk.x, disk.y, disk.z - 0.35f}, 0), ic.diskArmed = Dist(p, disk) > kArm;
        Spin(ic.door), Spin(ic.disk);
        float dd = Dist(p, at), ds = Dist(p, disk);
        if (dd > kArm) ic.doorArmed = true;
        if (ds > kArm) ic.diskArmed = true;
        if (!onFoot || g_trip.stage) continue;
        if (ic.doorArmed && dd < kDoorReach) {
            ic.doorArmed = false;
            if (!FreeToGo(true)) continue;
            const Interior& in = kInteriors[h.interior];
            StartTrip(in.area, {in.x, in.y, in.z}, me->CurrentRotation() * 57.29578f, true, h);
        } else if (ic.diskArmed && ds < kDiskReach) {
            ic.diskArmed = false;
            if (FreeToGo(true)) {
                LOGI("Homes: the save menu at the house's disk");
                Cmd(0x03D8).Run();  // ACTIVATE_SAVE_MENU
            }
        }
    }
    // the others' doors: a visit
    for (auto& [id, list] : g_others) {
        auto& icons = g_otherIcons[id];
        icons.resize(list.size());
        for (size_t i = 0; i < list.size(); ++i) {
            const House& h = list[i];
            Icons& ic = icons[i];
            if (area != 0 || Flat(p, h.door) >= kIconsNear) {
                DropIcons(ic, false);
                continue;
            }
            V3 at = DoorSpot(h);
            if (!ic.door) ic.door = MakeIcon(kModelHouse, {at.x, at.y, at.z - 0.35f}, 0), ic.doorArmed = Dist(p, at) > kArm;
            Spin(ic.door);
            float dd = Dist(p, at);
            if (dd > kArm) ic.doorArmed = true;
            if (onFoot && !g_trip.stage && ic.doorArmed && dd < kDoorReach) {
                ic.doorArmed = false;
                if (!FreeToGo(true)) continue;
                const Interior& in = kInteriors[h.interior];
                StartTrip(in.area, {in.x, in.y, in.z}, me->CurrentRotation() * 57.29578f, true, h);
            }
        }
    }
    // inside: the arrow back out where we came in
    if (g_in.on) {
        const Interior& in = kInteriors[g_in.house.interior];
        V3 at{in.x, in.y, in.z};
        if (area != in.area || Dist(p, {in.x, in.y, in.z}) > 60.0f || me->Health() <= 0.0f) {
            // gone another way (died, a teleport): no longer in there
            DropIcon(g_in.exitIcon);
            g_in = Inside{};
            OpenDoors();
            return;
        }
        ShutDoors();  // the host's state of them may have opened one again
        // the interior's own door (shut by us): its marker's place, where the way out was in the game (03.11)
        if (!g_in.hasExit) {
            auto* pool = Ref<CPoolHeader*>(kEnexPool);
            float best = 40.0f;
            for (int i = 0; pool && i < pool->size; ++i) {
                uintptr_t e = (uintptr_t)pool->objects + (uintptr_t)i * 0x3C;
                if (pool->byteMap[i] < 0 || !IsPrivateDoor(e)) continue;
                const float* r = &Ref<float>(e + 0x08);  // m_recEntrance, then m_fEntranceZ
                V3 c{(r[0] + r[2]) * 0.5f, (r[1] + r[3]) * 0.5f, Ref<float>(e + 0x18) + 1.0f};
                if (float d = Flat(c, at); d < best) best = d, g_in.exitAt = c, g_in.hasExit = true;
            }
            if (g_in.hasExit) DropIcon(g_in.exitIcon), LOGI("Homes: the way out at the interior's door, %.1f m from where we came in", best);
        }
        if (g_in.hasExit) at = g_in.exitAt;
        if (!g_in.exitIcon) g_in.exitIcon = MakeIcon(kModelArrow, {at.x, at.y, at.z - 0.2f}, in.area);
        Spin(g_in.exitIcon);
        float d = std::fabs(p.z - at.z) < 2.0f ? Flat(p, at) : 99.0f;
        if (d > kArm) g_in.armed = true;
        if (onFoot && g_in.armed && d < kDoorReach && !g_trip.stage) {
            g_in.armed = false;
            GoOut(g_in.house);
        }
    }
}

void Tick() {
    if (!Ready()) {
        g_readyMs = 0;
        return;
    }
    if (!g_own.loaded) {
        LoadOwn();
        SendOwn();
    }
    if (!g_readyMs) g_readyMs = NowMs();
    if (g_ask.kind && NowMs() > g_ask.until) g_ask = Ask{};
    UpdateTrip();
    // a partner that just joined: at its main house, once the host's world put it next to the host
    if (g_spawnPending && NowMs() - g_readyMs > 1500 && !g_trip.stage) {
        g_spawnPending = false;
        if (!g_own.houses.empty()) {
            LOGI("Homes: the session starts at the main house");
            GoOut(g_own.houses[g_own.main]);
        }
    }
    UpdateDoors();
    UpdateCars();
}

// ---------------------------------------------------------------- the screen
void Label(const V3& at, const std::string& text, uint32_t colour) {
    float sx = 0, sy = 0, depth = 0;
    if (!WorldToScreen(CVector(at.x, at.y, at.z + 0.9f), sx, sy, depth) || depth <= 0.0f) return;
    ui::Gfx().Text(ui::FONT_SMALL, sx, sy, text, colour, ui::TEXT_CENTER | ui::TEXT_OUTLINE);
}

void RenderPanel() {
    auto& g = ui::Gfx();
    float s = g.Scale(), lh = g.LineHeight(ui::FONT_SMALL), th = g.LineHeight(ui::FONT_MENU);
    std::vector<std::pair<std::string, uint32_t>> lines;
    lines.push_back({TR("Ваши дома", "Your houses"), 0xFF8CB4FF});
    if (g_own.houses.empty()) lines.push_back({TR("  нет: /casa marcar у любой двери", "  none: /casa marcar at any door"), 0xFFC8C8D2});
    for (size_t i = 0; i < g_own.houses.size(); ++i) {
        const House& h = g_own.houses[i];
        std::string l = Format("  %zu. %s, %s  ·  %s", i + 1, h.zone.c_str(), CityName(h.city), InteriorName(h.interior).c_str());
        l += h.paid ? Format(TR("  ·  куплен за %s", "  ·  bought for %s"), Dollars(h.paid).c_str()) : std::string(TR("  ·  бесплатно", "  ·  free"));
        if ((int)i == g_own.main) l += TR("  ·  главный", "  ·  main");
        lines.push_back({l, 0xFFFFFFFF});
    }
    lines.push_back({TR("Ваши машины", "Your cars"), 0xFF8CB4FF});
    if (g_own.cars.empty()) lines.push_back({TR("  нет: /casa carro в машине", "  none: /casa carro in a car"), 0xFFC8C8D2});
    for (size_t i = 0; i < g_own.cars.size(); ++i) {
        const Car& c = g_own.cars[i];
        lines.push_back({Format("  %zu. %s  ·  %s", i + 1, CarName(c.model).c_str(),
                                c.paid ? Format(TR("куплена за %s", "bought for %s"), Dollars(c.paid).c_str()).c_str() : TR("бесплатно", "free")),
                         0xFFFFFFFF});
    }
    lines.push_back({TR("Команды", "Commands"), 0xFF8CB4FF});
    const char* const cmds[] = {
        TR("  /casa marcar  ·  дом у этой двери (первый бесплатно, следующие за деньги)",
           "  /casa marcar  ·  a house at this door (the first is free, more are bought)"),
        TR("  /casa remover  ·  убрать дом, у которого вы (половина цены назад)",
           "  /casa remover  ·  give up the house you are at (half its price back)"),
        TR("  /casa principal  ·  этот дом - главный (/home и начало игры)",
           "  /casa principal  ·  this house is the main one (/home and the session's start)"),
        TR("  /casa interior  ·  другой интерьер для этого дома", "  /casa interior  ·  another interior for this house"),
        TR("  /casa carro  ·  в машине: она будет стоять здесь (или обновить её)",
           "  /casa carro  ·  in a car: it will stand here (or update it)"),
        TR("  /casa carro remover  ·  в своей машине: убрать её", "  /casa carro remover  ·  in your car: give it up"),
        TR("  /home  ·  к двери главного дома", "  /home  ·  to the main house's door"),
    };
    for (const char* c : cmds) lines.push_back({c, 0xFFC8C8D2});
    float w = 0.0f;
    for (auto& [t, c] : lines) w = std::max(w, g.TextWidth(ui::FONT_SMALL, t));
    float pad = 22 * s;
    w += 2 * pad;
    float h = pad * 2 + th + 8 * s + lines.size() * lh;
    float x = (g.Width() - w) * 0.5f, y = std::max((g.Height() - h) * 0.5f, 4 * s);
    g.Rect(x, y, w, h, ui::Argb(210, 0, 0, 0));
    g.RectOutline(x, y, w, h, ui::Argb(120, 120, 170, 255), 1.5f * s);
    float cy = y + pad;
    g.Text(ui::FONT_MENU, g.Width() * 0.5f, cy, TR("ДОМА И МАШИНЫ", "HOUSES AND CARS"), 0xFF8CB4FF, ui::TEXT_CENTER | ui::TEXT_SHADOW);
    cy += th + 8 * s;
    for (auto& [t, c] : lines) {
        g.Text(ui::FONT_SMALL, x + pad, cy, t, c, ui::TEXT_SHADOW);
        cy += lh;
    }
}

void Render() {
    if (Ready() && !Cfg().cleanScreen) {
        V3 p = CjPos();
        if (FindPlayerPed(0)->AreaCode() == 0) {
            for (size_t i = 0; i < g_own.houses.size(); ++i) {
                const House& h = g_own.houses[i];
                if (Dist(p, DoorSpot(h)) < 25.0f)
                    Label(DoorSpot(h), (int)i == g_own.main ? TR("Ваш дом (главный)", "Your house (main)") : TR("Ваш дом", "Your house"), 0xFF8CB4FF);
                if (Dist(p, DiskSpot(h)) < 12.0f) Label(DiskSpot(h), TR("Сохранить игру", "Save the game"), 0xFFDCDCE6);
            }
            for (auto& [id, list] : g_others)
                for (const House& h : list)
                    if (Dist(p, DoorSpot(h)) < 25.0f) Label(DoorSpot(h), Format(TR("Дом: %s", "%s's house"), Net().PeerName(id).c_str()), 0xFFDCDCE6);
        } else if (g_in.on) {
            const Interior& in = kInteriors[g_in.house.interior];
            V3 at = g_in.hasExit ? g_in.exitAt : V3{in.x, in.y, in.z};
            if (Dist(p, at) < 15.0f) Label(at, TR("Выход", "Way out"), 0xFFDCDCE6);
        }
    }
    if (NowMs() < g_panelUntil) RenderPanel();
}

// ---------------------------------------------------------------- the commands
// the house of ours at its door (or the one we are in), else -1
int OwnHouseHere() {
    V3 p = CjPos();
    if (g_in.on)
        for (size_t i = 0; i < g_own.houses.size(); ++i)
            if (Dist(g_own.houses[i].door, g_in.house.door) < 0.5f) return (int)i;
    for (size_t i = 0; i < g_own.houses.size(); ++i)
        if (Flat(g_own.houses[i].door, p) < 10.0f && FindPlayerPed(0)->AreaCode() == 0) return (int)i;
    return -1;
}

void AddHouse(const House& h) {
    g_own.houses.push_back(h);
    if (g_own.houses.size() == 1) g_own.main = 0;
    for (Icons& ic : g_ownIcons) DropIcons(ic, true);
    g_ownIcons.clear();
    SaveOwn();
    SendOwn();
    if (!Net().IsHost() && Net().Online() && h.paid) SaveProfileNow();  // its money: a world reload puts the profile's back
    ui::TheChat().AddSystem(Format(TR("Дом в %s готов: %s. Синий дом у двери - вход, диск рядом - сохранение. Все команды: /casa.",
                                      "Your house in %s is ready: %s. The blue house at the door goes in, the disk beside it saves. All commands: /casa."),
                                   h.zone.c_str(), InteriorName(h.interior).c_str()));
    LOGI("Homes: a house in %s (%s), interior %d, paid $%d", h.zone.c_str(), CityName(h.city), h.interior, h.paid);
}

void MarkHouse() {
    CPlayerPed* me = FindPlayerPed(0);
    if (me->InVehicle() || me->AreaCode() != 0) return ui::TheChat().AddError(TR("Встаньте у двери дома на улице.", "Stand at a house's door, outside."));
    if ((int)g_own.houses.size() >= kHousesMax) return ui::TheChat().AddError(Format(TR("Не больше %d домов.", "%d houses at most."), kHousesMax));
    V3 p = CjPos();
    for (const House& h : g_own.houses)
        if (Flat(h.door, p) < 6.0f) return ui::TheChat().AddError(TR("Здесь уже ваш дом.", "Your house is here already."));
    for (auto& [id, list] : g_others)
        for (const House& h : list)
            if (Flat(h.door, p) < 6.0f)
                return ui::TheChat().AddError(Format(TR("Здесь дом игрока %s.", "This is %s's house."), Net().PeerName(id).c_str()));
    House h;
    h.door = p;
    h.heading = Wrap(me->CurrentRotation() * 57.29578f);
    h.city = (uint8_t)City(p);
    h.zone = ZoneName(p);
    if (h.zone.empty()) h.zone = CityName(h.city);
    h.interior = (uint8_t)FreeInterior(-1, -1);
    FindIcon(h);
    if (g_own.houses.empty()) return AddHouse(h);
    int type = ZoneType();
    h.paid = HousePrice(type, h.city);
    g_ask = Ask{};
    g_ask.kind = ASK_HOUSE, g_ask.price = h.paid, g_ask.house = h, g_ask.until = NowMs() + kAskMs;
    g_ask.text = Format(TR("Ещё один дом: %s (%s), %s?", "Another house: %s (%s), %s?"), h.zone.c_str(), ZoneKind(type), Dollars(h.paid).c_str());
    ui::TheChat().AddSystem(Format(TR("У вас уже есть дом. Этот, в %s (%s), стоит %s: Y - купить, N - нет. Или сначала /casa remover у старого.",
                                      "You have a house already. This one, in %s (%s), costs %s: Y buys it, N not. Or /casa remover at the old one first."),
                                   h.zone.c_str(), ZoneKind(type), Dollars(h.paid).c_str()));
}

void SaveCar() {
    CPlayerPed* me = FindPlayerPed(0);
    CVehicle* v = me->InVehicle() ? me->Vehicle() : nullptr;
    if (!v || v->Driver() != me) return ui::TheChat().AddError(TR("Сядьте за руль машины и поставьте её, где ей стоять.", "Sit at the wheel and park it where it is to stand."));
    if (v->MoveSpeed().Length() > 0.05f) return ui::TheChat().AddError(TR("Сначала остановитесь.", "Stop first."));
    V3 p = ToV3(v->GetPosition());
    bool nearHouse = false;
    for (const House& h : g_own.houses) nearHouse = nearHouse || Flat(h.door, p) < 80.0f;
    if (!nearHouse) return ui::TheChat().AddError(TR("Машина ставится у вашего дома (до 80 м от двери).", "A car is kept at your house (up to 80 m from its door)."));
    int handle = GetVehicleRef(v);
    Car c;
    c.model = (int16_t)v->ModelIndex();
    c.at = p;
    CMatrix* m = v->Matrix();
    c.heading = Wrap(m ? std::atan2(-m->forward.x, m->forward.y) * 57.29578f : 0.0f);
    TakeLook(handle, v, c);
    // one of ours again: its place and its look now, nothing to pay
    for (size_t i = 0; i < g_own.cars.size(); ++i)
        if (i < g_carsOut.size() && g_carsOut[i].v == v) {
            c.paid = g_own.cars[i].paid;
            g_own.cars[i] = c;
            SaveOwn();
            ui::TheChat().AddSystem(Format(TR("%s: место, цвета и детали сохранены.", "%s: its place, colours and parts are kept."), CarName(c.model).c_str()));
            return;
        }
    if ((int)g_own.cars.size() >= kCarsMax) return ui::TheChat().AddError(Format(TR("Не больше %d машин.", "%d cars at most."), kCarsMax));
    for (const Car& o : g_own.cars)
        if (Flat(o.at, p) < 3.0f) return ui::TheChat().AddError(TR("Здесь уже стоит ваша машина: поставьте эту рядом.", "One of your cars stands here: park this one beside it."));
    auto keep = [&](const Car& car) {
        g_own.cars.push_back(car);
        g_carsOut.resize(g_own.cars.size());
        g_carsOut.back().v = v, g_carsOut.back().handle = handle;
        SaveOwn();
        if (!Net().IsHost() && Net().Online() && car.paid) SaveProfileNow();
        ui::TheChat().AddSystem(Format(TR("%s теперь ваша: она будет стоять здесь, когда вы рядом. /casa - все команды.",
                                          "The %s is yours: it will stand here whenever you are near. /casa: all the commands."),
                                       CarName(car.model).c_str()));
        LOGI("Homes: the %s kept (model %d), paid $%d", CarName(car.model).c_str(), car.model, car.paid);
    };
    if (g_own.cars.empty()) return keep(c);
    c.paid = CarValue(v);
    g_ask = Ask{};
    g_ask.kind = ASK_CAR, g_ask.price = c.paid, g_ask.car = c, g_ask.until = NowMs() + kAskMs;
    g_ask.index = handle;
    g_ask.text = Format(TR("Ещё одна машина: %s, %s?", "Another car: the %s, %s?"), CarName(c.model).c_str(), Dollars(c.paid).c_str());
    ui::TheChat().AddSystem(Format(TR("Первая машина уже есть. %s стоит %s: Y - купить, N - нет.",
                                      "You have your free car already. The %s costs %s: Y buys it, N not."),
                                   CarName(c.model).c_str(), Dollars(c.paid).c_str()));
}

void DropCar() {
    CPlayerPed* me = FindPlayerPed(0);
    V3 p = CjPos();
    int found = -1;
    for (size_t i = 0; i < g_own.cars.size() && i < g_carsOut.size(); ++i) {
        CVehicle* v = g_carsOut[i].v;
        if (v && ((me->InVehicle() && me->Vehicle() == v) || Dist(ToV3(v->GetPosition()), p) < 6.0f)) found = (int)i;
    }
    if (found < 0) return ui::TheChat().AddError(TR("Сядьте в свою машину (или встаньте рядом).", "Sit in your car (or stand by it)."));
    const Car& c = g_own.cars[found];
    g_ask = Ask{};
    g_ask.kind = ASK_DROP_CAR, g_ask.index = found, g_ask.price = c.paid / 2, g_ask.until = NowMs() + kAskMs;
    g_ask.text = Format(TR("Убрать %s? Назад: %s", "Give up the %s? Back: %s"), CarName(c.model).c_str(), Dollars(c.paid / 2).c_str());
    ui::TheChat().AddSystem(Format(TR("Убрать %s из ваших машин? Вы получите %s. Y - да, N - нет.", "Give up the %s? You get %s back. Y yes, N no."),
                                   CarName(c.model).c_str(), Dollars(c.paid / 2).c_str()));
}

bool Pay(int price) {
    if (Money() < price) {
        ui::TheChat().AddError(Format(TR("Не хватает денег: нужно %s.", "Not enough money: it takes %s."), Dollars(price).c_str()));
        return false;
    }
    Money() -= price;
    return true;
}

void Answer(bool yes) {
    Ask a = g_ask;
    g_ask = Ask{};
    if (!yes) return ui::TheChat().AddSystem(TR("Отменено.", "Cancelled."));
    if (!Ready()) return;
    switch (a.kind) {
    case ASK_HOUSE:
        if ((int)g_own.houses.size() >= kHousesMax || !Pay(a.price)) return;
        a.house.interior = (uint8_t)FreeInterior(-1, -1);
        return AddHouse(a.house);
    case ASK_CAR: {
        CVehicle* v = GetVehicle(a.index);
        CPlayerPed* me = FindPlayerPed(0);
        if (!v || !me->InVehicle() || me->Vehicle() != v) return ui::TheChat().AddError(TR("Вы уже не в той машине.", "You are no longer in that car."));
        if (!Pay(a.price)) return;
        g_own.cars.push_back(a.car);
        g_carsOut.resize(g_own.cars.size());
        g_carsOut.back().v = v, g_carsOut.back().handle = a.index;
        SaveOwn();
        if (!Net().IsHost() && Net().Online()) SaveProfileNow();
        ui::TheChat().AddSystem(Format(TR("%s теперь ваша: она будет стоять здесь, когда вы рядом.", "The %s is yours: it will stand here whenever you are near."),
                                       CarName(a.car.model).c_str()));
        LOGI("Homes: the %s bought (model %d) for $%d", CarName(a.car.model).c_str(), a.car.model, a.price);
        return;
    }
    case ASK_DROP_HOUSE: {
        if (a.index < 0 || a.index >= (int)g_own.houses.size()) return;
        House h = g_own.houses[a.index];
        Money() = (int32_t)std::min<int64_t>((int64_t)Money() + a.price, 999999999);
        for (Icons& ic : g_ownIcons) DropIcons(ic, true);
        g_ownIcons.clear();
        g_own.houses.erase(g_own.houses.begin() + a.index);
        if (g_own.main >= (int)g_own.houses.size() || g_own.main == a.index) g_own.main = 0;
        else if (g_own.main > a.index) --g_own.main;
        SaveOwn();
        SendOwn();
        if (!Net().IsHost() && Net().Online() && a.price) SaveProfileNow();
        if (g_in.on && Dist(g_in.house.door, h.door) < 0.5f) GoOut(h);
        ui::TheChat().AddSystem(a.price ? Format(TR("Дом в %s больше не ваш: %s вернулись к вам.", "The house in %s is no longer yours: %s came back to you."),
                                                 h.zone.c_str(), Dollars(a.price).c_str())
                                        : Format(TR("Дом в %s больше не ваш. Новый: /casa marcar у другой двери.",
                                                    "The house in %s is no longer yours. A new one: /casa marcar at another door."),
                                                 h.zone.c_str()));
        LOGI("Homes: the house in %s given up ($%d back)", h.zone.c_str(), a.price);
        return;
    }
    case ASK_DROP_CAR: {
        if (a.index < 0 || a.index >= (int)g_own.cars.size()) return;
        Car c = g_own.cars[a.index];
        if (a.index < (int)g_carsOut.size()) {
            CarOut& o = g_carsOut[a.index];
            if (o.v && InPool(o.v) && GetVehicle(o.handle) == o.v) Cmd(0x01C3).I(o.handle).Run();  // an ordinary car from now on
            g_carsOut.erase(g_carsOut.begin() + a.index);
        }
        g_own.cars.erase(g_own.cars.begin() + a.index);
        Money() = (int32_t)std::min<int64_t>((int64_t)Money() + a.price, 999999999);
        SaveOwn();
        if (!Net().IsHost() && Net().Online() && a.price) SaveProfileNow();
        ui::TheChat().AddSystem(Format(TR("%s больше не ваша.", "The %s is no longer yours."), CarName(c.model).c_str()));
        return;
    }
    default:
        return;
    }
}

bool Command(const std::string& cmd, const std::vector<std::string>& args) {
    std::vector<std::string> a;
    for (std::string s : args) {
        for (char& ch : s) ch = (char)tolower((unsigned char)ch);
        a.push_back(s);
    }
    std::string first = a.empty() ? "" : a[0], second = a.size() > 1 ? a[1] : "";
    if (!Ready()) {
        ui::TheChat().AddError(TR("Сейчас нельзя.", "Not now."));
        return true;
    }
    if (!g_own.loaded) LoadOwn();
    if (cmd == "home" || (cmd == "casa" && first == "ir")) {
        if (g_own.houses.empty()) {
            ui::TheChat().AddError(TR("У вас нет дома: /casa marcar у любой двери.", "You have no house: /casa marcar at any door."));
            return true;
        }
        if (FreeToGo(true)) GoOut(g_own.houses[g_own.main]);
        return true;
    }
    if (first.empty() || first == "comandos" || first == "ajuda" || first == "help" || first == "painel") {
        g_panelUntil = NowMs() < g_panelUntil ? 0 : NowMs() + kPanelMs;
        return true;
    }
    if (first == "marcar" || first == "mark") {
        MarkHouse();
        return true;
    }
    if (first == "carro" || first == "car") {
        if (second == "remover" || second == "remove") DropCar();
        else SaveCar();
        return true;
    }
    int here = OwnHouseHere();
    if (first == "remover" || first == "remove" || first == "principal" || first == "main" || first == "interior") {
        if (here < 0) {
            ui::TheChat().AddError(TR("Подойдите к двери своего дома (или зайдите в него).", "Go to your house's door (or into it)."));
            return true;
        }
        House& h = g_own.houses[here];
        if (first == "principal" || first == "main") {
            g_own.main = here;
            SaveOwn();
            ui::TheChat().AddSystem(Format(TR("Главный дом: %s. /home и начало игры - сюда.", "Main house: %s. /home and the session's start come here."), h.zone.c_str()));
        } else if (first == "interior") {
            h.interior = (uint8_t)FreeInterior(h.interior, here);
            SaveOwn();
            SendOwn();
            ui::TheChat().AddSystem(Format(TR("Интерьер: %s.", "Interior: %s."), InteriorName(h.interior).c_str()));
            if (g_in.on && Dist(g_in.house.door, h.door) < 0.5f) {
                const Interior& in = kInteriors[h.interior];
                StartTrip(in.area, {in.x, in.y, in.z}, 0.0f, true, h);
            }
        } else {
            g_ask = Ask{};
            g_ask.kind = ASK_DROP_HOUSE, g_ask.index = here, g_ask.price = h.paid / 2, g_ask.until = NowMs() + kAskMs;
            g_ask.text = Format(TR("Отказаться от дома в %s? Назад: %s", "Give up the house in %s? Back: %s"), h.zone.c_str(), Dollars(h.paid / 2).c_str());
            ui::TheChat().AddSystem(Format(TR("Отказаться от дома в %s? Вы получите %s. Y - да, N - нет.", "Give up the house in %s? You get %s back. Y yes, N no."),
                                           h.zone.c_str(), Dollars(h.paid / 2).c_str()));
        }
        return true;
    }
    ui::TheChat().AddError(TR("Нет такой команды. Все команды: /casa.", "No such command. All of them: /casa."));
    return true;
}

class Homes : public Module {
public:
    const char* Name() const override { return "Homes"; }
    void OnInstallHooks() override {
        Net().SetHandler(proto::G_HOME, [](uint8_t from, Reader& r) {
            if (r.U8() != OP_HOUSES) return;
            int n = r.U8();
            std::vector<House> list;
            for (int i = 0; i < n && i < kHousesMax; ++i) {
                House h = ReadHouse(r);
                if (r.Ok()) list.push_back(h);
            }
            if (!r.Ok()) return;
            for (Icons& ic : g_otherIcons[from]) DropIcons(ic, false);
            g_otherIcons.erase(from);
            g_others[from] = list;
            // one interior, two houses (marked apart, out of the session): the newer player's house takes another
            bool moved = false;
            for (size_t i = 0; from < Net().MyId() && i < g_own.houses.size(); ++i)
                for (const House& h : list)
                    if (h.interior == g_own.houses[i].interior) g_own.houses[i].interior = (uint8_t)FreeInterior(h.interior, (int)i), moved = true;
            if (moved) SaveOwn(), SendOwn();
            LOGD("Homes: %zu houses of player %u", list.size(), from);
        });
    }
    void OnSessionStart() override {
        g_spawnPending = !Net().IsHost();
        if (g_own.loaded) SendOwn();
    }
    void OnSessionEnd() override {
        for (auto& [id, list] : g_otherIcons)
            for (Icons& ic : list) DropIcons(ic, false);
        g_otherIcons.clear();
        g_others.clear();
        g_spawnPending = false;
    }
    void OnPeerJoin(uint8_t) override {
        if (g_own.loaded) SendOwn();
    }
    void OnPeerLeave(uint8_t id) override {
        for (Icons& ic : g_otherIcons[id]) DropIcons(ic, false);
        g_otherIcons.erase(id);
        g_others.erase(id);
    }
    void OnGameShutdown() override {
        // the world goes with what we made in it: no commands now
        g_shut.clear();
        SetPrivateInterior(0, CVector());
        g_ownIcons.clear();
        g_otherIcons.clear();
        g_carsOut.clear();
        g_in = Inside{};
        g_trip = Trip{};
        g_ask = Ask{};
    }
    void OnPostProcess() override { Tick(); }
    void OnRender() override { Render(); }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "casa" && cmd != "home" && cmd != "house") return false;
        return Command(cmd == "house" ? "casa" : cmd, args);
    }
    static Homes& Instance() {
        static Homes m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Homes::Instance()); }
} g_reg;

}  // namespace

bool Key(bool yes) {
    if (!g_ask.kind || NowMs() > g_ask.until) return false;
    Answer(yes);
    return true;
}

std::string Text(std::string& hint) {
    if (!g_ask.kind || NowMs() > g_ask.until) return "";
    hint = TR("Y - да, N - нет", "Y: yes, N: no");
    return g_ask.text;
}

}  // namespace coop::home
