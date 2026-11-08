// Charlie Harper's beach house (Two and a Half Men), 1.6 times life size as the game's people and doors are, on
// Santa Maria Beach in place of the block of houses santahouse04_law2 and of the two houses of the block east of it
// (santahouse02_law2) west of the beach safe house, which stays (docs/casa-charlie.md). Up on its lot as a Malibu
// house is: a wooden bulkhead toward the sea with stairs to the sand, pilings under the house's part over the beach,
// a wall and a gate toward the road. Part of the map: one walks in and out through its open doorways, the deck toward
// the sea. Everyone with the mod sees it; another mod's blocks (not the game's own) are left as they are.
//
//  * the models (near: santahouse04_law2; far: LODtahouse04_law2, only the outside) and the collision are ours, from
//    CharlieData.h (tools/charlie, from the SketchUp model), loaded in place of the game's by shared/game/ModelSwap;
//    the east block's (and its LOD's) are the game's own, cut as they load (Cut.cpp);
//  * their textures (stucco, clay roof tiles, terracotta floor, the ceiling's planks, Spanish tiles on the stairs...)
//    are ours too: made from CharlieData.h's (DXT1) and put in the blocks' texture dictionaries (bev_law2, near;
//    welod_law2, far, smaller) as these load (shared/game/TxdLoad), before the models that use them;
//  * the furniture: the game's own objects (sofas, beds, the kitchen, the bathrooms, the deck's table and lounge
//    chairs...) where the show's floor plan has them, made in each game as its player comes near (within 160 m) and
//    taken away again past 220 m; not kept by the game's saves. The grand piano is ours, in the house's model;
//  * the palms, bushes and the lifebuoy of the map that would stand in the house are hidden (and walked through): again
//    whenever that part of the map loads anew;
//  * the furniture can come from coop\casa_charlie.ipl instead (the house finished in a map editor): /editorcasa writes
//    it as it stands into modloader\CasaCharlie_Editor (tools/charlie/editor.py: the house's files for the editor);
//    with that folder on, its .ipl puts the furniture, not the mod;
//  * no rain under its roofs: CCullZones::CamNoRain and PlayerNoRain answer yes there.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "features/charlie/CharlieData.h"
#include "features/charlie/Cut.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/game/ModelSwap.h"
#include "shared/game/Script.h"
#include "shared/game/TxdLoad.h"
#include "shared/game/WalkIn.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/chat/Chat.h"

namespace coop::charlie {

namespace {
using namespace game;
using game::script::Cmd;

constexpr uint32_t kHouseModel = 6285, kLodModel = 6433, kEastModel = 6284, kEastLodModel = 6434;  // LAw2.ide
constexpr const char* kHouseName = "santahouse04_law2";
constexpr const char* kLodName = "LODtahouse04_law2";
constexpr const char* kEastName = "santahouse02_law2";
constexpr const char* kEastLodName = "LODtahouse02_law2";
constexpr float kX = 265.969f, kY = -1757.578f, kZ = 8.906f;  // the house's block (heading 0)
constexpr float kEastX = 307.265625f;                         // the east block (and its LOD), heading 0
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

std::vector<uint8_t> Model(uint32_t modelId, const uint8_t* dff, size_t size) {
    if (modelId == kHouseModel && IsGames(modelId, kHouseName)) return Copy(data::kHouseDff, sizeof data::kHouseDff);
    if (modelId == kLodModel && IsGames(modelId, kLodName)) return Copy(data::kLodDff, sizeof data::kLodDff);
    if ((modelId == kEastModel && IsGames(modelId, kEastName)) || (modelId == kEastLodModel && IsGames(modelId, kEastLodName))) {
        std::vector<uint8_t> cut = CutModel(dff, size, data::kEastCutX - kEastX);
        if (cut.empty()) LOGW("Charlie: model %u could not be cut (left as it is)", modelId);
        return cut;
    }
    return {};
}

const uint8_t* Collision(const char* name, const uint8_t* body, uint32_t size, uint32_t* ourSize) {
    if (!name) return nullptr;
    if (SameName(name, kHouseName) && IsGames(kHouseModel, kHouseName)) {
        if (!g_ours) LOGI("Charlie: the house's collision in place of %s", kHouseName);
        g_ours = true;
        *ourSize = sizeof data::kHouseCol;
        return data::kHouseCol;
    }
    if (SameName(name, kEastName) && IsGames(kEastModel, kEastName)) {
        static std::vector<uint8_t> cut;  // the game copies it (CColModel's own data)
        cut = CutCollision(body, size, data::kEastCutX - kEastX);
        if (cut.empty()) return nullptr;
        *ourSize = (uint32_t)cut.size();
        return cut.data();
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
    static int near = -1, far = -1;  // their slots, once the game has them (the first TXDs load before gta3.img's)
    if (near < 0) near = Call<0x731850, int>("bev_law2");  // CTxdStore::FindTxdSlot
    if (far < 0) far = Call<0x731850, int>("welod_law2");
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

// ---------------------------------------------------------------- the furniture
constexpr float kFurnishIn = 160.0f, kFurnishOut = 220.0f;  // from the house's middle
constexpr float kMidX = 273.4f, kMidY = -1766.8f;
constexpr int kFurnishPerFrame = 6;
constexpr const wchar_t* kEditorDir = L"modloader\\CasaCharlie_Editor\\";  // the house for a map editor (tools/charlie/editor.py)
constexpr const wchar_t* kIplName = L"casa_charlie.ipl";

// a piece: the furniture's (kFurniture: its bottom at z, a heading) or a placement of casa_charlie.ipl (its origin, a
// rotation as an IPL has it: the inverse of the object's)
struct Placed {
    int model;
    float x, y, z, heading;
    bool ipl;
    float qx, qy, qz, qw;
};
std::vector<Placed> g_list;
std::vector<int> g_piece;  // their objects' handles (0: not made)
bool g_furnished = false;
bool g_editor = false;  // the editor's folder is on: its .ipl puts the furniture (modloader), not we

std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

bool ReadText(const std::wstring& path, std::string& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// the editor's folder with its .ipl, not off in modloader.ini's [Profiles.Default.IgnoreMods]
bool EditorOn() {
    std::error_code ec;
    if (!std::filesystem::exists(Cfg().gameDir + kEditorDir + kIplName, ec)) return false;
    std::string ini, line;
    if (!ReadText(Cfg().gameDir + L"modloader\\modloader.ini", ini)) return true;
    std::istringstream in(ini);
    bool section = false;
    while (std::getline(in, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (!line.empty() && line[0] == '[') section = Lower(line) == "[profiles.default.ignoremods]";
        else if (section && Lower(line) == "casacharlie_editor") return false;
    }
    return true;
}

// the inst lines of an .ipl (model, name, interior, x, y, z, rx, ry, rz, rw, lod)
std::vector<Placed> ReadIpl(const std::string& text) {
    std::vector<Placed> out;
    std::istringstream in(text);
    std::string line;
    bool inst = false;
    while (std::getline(in, line)) {
        std::string l = Lower(line);
        l.erase(0, l.find_first_not_of(" \t"));
        if (l.rfind("inst", 0) == 0) inst = true;
        else if (l.rfind("end", 0) == 0) inst = false;
        else if (inst && !l.empty() && l[0] != '#') {
            for (char& c : l) if (c == ',') c = ' ';
            Placed p{};
            char name[64];
            int interior = 0;
            if (sscanf(l.c_str(), "%d %63s %d %f %f %f %f %f %f %f", &p.model, name, &interior, &p.x, &p.y, &p.z, &p.qx,
                       &p.qy, &p.qz, &p.qw) == 10 && interior == 0) {
                p.ipl = true;
                out.push_back(p);
            }
        }
    }
    return out;
}

// coop\casa_charlie.ipl (the house finished in the editor) or the furniture of CharlieData.h
void LoadList() {
    g_list.clear();
    std::string text;
    if (ReadText(Cfg().coopDir + kIplName, text)) g_list = ReadIpl(text);
    if (!g_list.empty()) LOGI("Charlie: %zu pieces from coop\\casa_charlie.ipl", g_list.size());
    else
        for (const data::Piece& p : data::kFurniture) g_list.push_back({p.model, p.x, p.y, p.z, p.heading, false, 0, 0, 0, 1});
    g_piece.assign(g_list.size(), 0);
    g_editor = EditorOn();
    if (g_editor) LOGI("Charlie: the editor's folder is on (modloader\\CasaCharlie_Editor): the furniture is its");
}

void Furnish() {
    if (g_editor) return;
    int made = 0;
    bool all = true;
    for (size_t i = 0; i < g_list.size() && made < kFurnishPerFrame; ++i) {
        if (g_piece[i] && GetObject(g_piece[i])) continue;
        g_piece[i] = 0;
        const Placed& p = g_list[i];
        if (p.ipl ? !IsValidModel(p.model) : !IsObjectModel(p.model)) continue;  // not in this game's files
        all = false;
        if (!IsModelLoaded(p.model)) {
            RequestModel(p.model);
            continue;
        }
        if (!HasRoom(ObjectPool(), kKeepObjects, "object")) return;
        int h = 0;
        Cmd(0x0107).I(p.model).F(p.x).F(p.y).F(p.z).Out(&h).Run();  // CREATE_OBJECT: its bottom at z
        CObject* o = h ? GetObject(h) : nullptr;
        if (!o) continue;
        // OBJECT_MISSION2: the game's saves keep only OBJECT_MISSION ones (a save would bring a copy back)
        Field<uint8_t>(o, 0x13C) = 6;
        if (p.ipl) {
            Cmd(0x01BC).I(h).F(p.x).F(p.y).F(p.z).Run();                // SET_OBJECT_COORDINATES: its origin there
            Cmd(0x07C5).I(h).F(-p.qx).F(-p.qy).F(-p.qz).F(p.qw).Run();  // SET_OBJECT_QUATERNION (the IPL's inverse)
        } else {
            Cmd(0x0177).I(h).F(p.heading).Run();  // SET_OBJECT_HEADING
        }
        Cmd(0x0550).I(h).I(1).Run();  // FREEZE_OBJECT_POSITION: a chair is not knocked over
        g_piece[i] = h;
        ++made;
    }
    if (all && !made && !g_furnished) {
        g_furnished = true;
        for (const Placed& p : g_list) MarkModelNotNeeded(p.model);
        LOGI("Charlie: the house furnished (%zu pieces)", g_list.size());
    }
}

void Unfurnish() {
    for (int& h : g_piece) {
        if (h && GetObject(h)) Cmd(0x0108).I(h).Run();  // DELETE_OBJECT
        h = 0;
    }
    g_furnished = false;
}

// /editorcasa: the furniture as it stands, as modloader\CasaCharlie_Editor\casa_charlie.ipl (for the editor)
void ExportIpl() {
    if (g_editor)
        return ui::TheChat().AddError(TR("Папка редактора уже включена: выключите её в /mods и перезапустите игру.",
                                         "The editor's folder is on already: turn it off in /mods and restart the game."));
    if (!g_furnished)
        return ui::TheChat().AddError(TR("Подойдите к дому Чарли (мебель должна стоять).",
                                         "Go to Charlie's house first (its furniture has to be there)."));
    std::string out = "# Charlie Harper's house: its furniture (/editorcasa). Edit it in the map editor; then it goes to "
                      "coop\\casa_charlie.ipl.\ninst\n";
    int n = 0;
    for (int h : g_piece) {
        CObject* o = h ? GetObject(h) : nullptr;
        if (!o) continue;
        CVector p = o->GetPosition();
        float half = o->GetHeading() * 0.5f;
        out += Format("%d, coop_obj, 0, %.4f, %.4f, %.4f, 0, 0, %.6f, %.6f, -1\n", (int)o->ModelIndex(), p.x, p.y, p.z,
                      -std::sin(half), std::cos(half));
        ++n;
    }
    out += "end\n";
    std::error_code ec;
    std::filesystem::create_directories(Cfg().gameDir + kEditorDir, ec);
    if (!WriteFileBytes(Cfg().gameDir + kEditorDir + kIplName, out.data(), out.size()))
        return ui::TheChat().AddError(TR("Не удалось записать файл.", "Could not write the file."));
    LOGI("Charlie: %d pieces written to modloader\\CasaCharlie_Editor\\casa_charlie.ipl", n);
    ui::TheChat().AddSystem(Format(TR("Мебель (%d) записана в modloader\\CasaCharlie_Editor\\casa_charlie.ipl. "
                                      "Перезапустите игру и откройте редактор.",
                                      "The furniture (%d) is written to modloader\\CasaCharlie_Editor\\casa_charlie.ipl. "
                                      "Restart the game and open the editor."), n));
}

// for the group's homes (/casa marcar here: the house itself, its save disk inside)
bool InPlace() { return g_ours; }
bool OnLot(float x, float y) { return x > kX + data::kLotX0 && x < kX + data::kLotX1 && y > kY + data::kLotY0 && y < kY + data::kLotY1; }

class Charlie : public Module {
public:
    const char* Name() const override { return "Charlie"; }
    void OnInstallHooks() override {
        game::AddModelSwap(&Model);
        game::AddCollisionSwap(&Collision);
        game::AddTxdLoaded(&TxdLoaded);
        game::walkin::House h;
        h.inPlace = &InPlace, h.onLot = &OnLot;
        h.doorX = data::kDoorX, h.doorY = data::kDoorY, h.doorZ = data::kDoorZ, h.heading = data::kDoorHeading;
        h.diskX = data::kDiskX, h.diskY = data::kDiskY, h.diskZ = data::kDiskZ;
        game::walkin::Set(h);
        mem::HookFn(0x72DDB0, &HookCamNoRain, &g_origCamNoRain, "CCullZones::CamNoRain");
        mem::HookFn(0x72DDC0, &HookPlayerNoRain, &g_origPlayerNoRain, "CCullZones::PlayerNoRain");
    }
    void OnGameStarted() override {
        LOGI("Charlie: house %s (models: %s, %s, %s, %s)", g_ours ? "in place" : "not in place (not the game's block)",
             IsGames(kHouseModel, kHouseName) ? "game's" : "other", IsGames(kLodModel, kLodName) ? "game's" : "other",
             IsGames(kEastModel, kEastName) ? "game's" : "other", IsGames(kEastLodModel, kEastLodName) ? "game's" : "other");
        for (uint32_t model : {kHouseModel, kEastModel})  // near as far as the house is seen (the east block: not its LOD)
            if (void* mi = g_ours ? ModelInfo(model) : nullptr) Ref<float>((uintptr_t)mi + 0x18) = kDrawDistance;
        m_next = 0;
        LoadList();  // a new game: the old one's objects are gone
        g_furnished = false;
    }
    void OnGameShutdown() override {
        for (int& h : g_piece) h = 0;
        g_furnished = false;
    }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>&) override {
        if (cmd != "editorcasa") return false;
        ExportIpl();
        return true;
    }
    void OnPostProcess() override {
        if (!g_ours || !core::IsPlayerReady()) return;
        CPlayerPed* me = FindPlayerPed(0);
        if (!me) return;
        CVector p = me->GetPosition();
        float d = std::hypot(p.x - kMidX, p.y - kMidY);
        if (d < kFurnishIn && me->AreaCode() == 0) {
            if (!g_furnished) Furnish();
        } else if (d > kFurnishOut) {
            if (g_furnished || (!g_piece.empty() && g_piece[0])) Unfurnish();
        }
        if (TimeMs() < m_next) return;
        m_next = TimeMs() + 1000;
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
