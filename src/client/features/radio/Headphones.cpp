// Headphones, as in GTA 6: a station of the radio heard on foot.
//
// A tap on the key (keys/headphones in coop.ini, H) puts them on or takes them off; while it is held, the mouse wheel
// turns their dial (up the next station, down the last one) and the weapons stay as they are. The game's own radio plays
// them, started as a car's would (CAudioEngine::StartRadio: the station goes on where it was). In a vehicle with a radio
// that radio plays as always; out of it the headphones play again, on their station. For the other players they are one
// more radio of that station (Radio.cpp: everyone on a station hears the song of whoever has listened to it longest).
// The game stops a radio that plays out of a vehicle (CAERadioTrackManager::CheckForPause, every frame it plays) unless
// the ambience's radio plays (an interior's music): with the headphones on, that check finds one playing.
// An MP3 player shows at the right of the screen (as a phone does in GTA V) while they are put on, the dial turns or the
// key is held, with the station's name, then fades.
#include "features/radio/Headphones.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/config/Config.h"
#include "shared/game/Game.h"
#include "shared/lib/core/Core.h"
#include "shared/lib/memory/Memory.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::headphones {

namespace {
using namespace game;

constexpr uintptr_t kAudioEngine = 0xB6BC90;  // CAudioEngine AudioEngine
constexpr uintptr_t kRadio = 0x8CB6F8;        // CAERadioTrackManager AERadioTrackManager
constexpr uintptr_t kUserTracks = 0xB6B970;   // CAEUserRadioTrackManager: +6 u16 m_nUserTracksCount
constexpr uintptr_t kMouseNew = 0xB73418;     // CPad::NewMouseControllerState: +3 wheel up, +4 wheel down
constexpr int8_t kFirstStation = 1, kLastMusicStation = 11, kUserTracksStation = 12;

constexpr uint64_t kTapMs = 600;         // held longer than this, the key was no tap
constexpr uint64_t kTapWaitMs = 400;     // a tap waits this long: another letter meanwhile was a cheat being typed
constexpr uint64_t kRestartGapMs = 2000;  // the radio stopped by something else (a cutscene): start it again after this
constexpr uint64_t kShowMs = 5000;       // the MP3 player stays on the screen this long after a change
constexpr uint64_t kFadeMs = 400;

bool g_on = false;          // the player put them on
bool g_playing = false;     // the radio plays for them (we started it and no vehicle's radio took it since)
int8_t g_station = kFirstStation;
bool g_held = false;        // the key is down (it went down in the game, not while typing)
bool g_turned = false;      // the wheel turned the dial while it was held
uint64_t g_heldMs = 0;
uint64_t g_tapMs = 0;       // a tap waiting to count
uint64_t g_startMs = 0;
uint64_t g_showUntilMs = 0;

uint8_t* Manager() { return (uint8_t*)kRadio; }
bool VehicleRadio();

// CAERadioTrackManager::CheckForPause -> CAudioEngine::IsAmbienceRadioActive: true, a radio may play out of a vehicle
using AmbienceRadioFn = bool(__fastcall*)(void*, void*);
AmbienceRadioFn g_origAmbienceRadio = nullptr;
bool __fastcall HookAmbienceRadio(void* engine, void* edx) {
    if (g_on && g_playing && !VehicleRadio()) return true;  // the headphones
    return g_origAmbienceRadio(engine, edx);
}
bool RadioOn() { return CallMethod<0x4E8350, bool>(Manager()); }      // CAERadioTrackManager::IsRadioOn
bool VehicleRadio() { return Call<0x4E9800, bool>(); }                // CAERadioTrackManager::IsVehicleRadioActive
bool CutsceneRunning() { return Ref<uint8_t>(0xB5F851) != 0; }        // CCutsceneMgr::ms_running
bool UserTracks() { return Ref<uint16_t>(kUserTracks + 6) > 0; }

std::string StationName(int8_t station) {
    const char* name = CallMethod<0x4E9E10, const char*>(Manager(), station);  // CAERadioTrackManager::GetRadioStationName
    return name ? std::string(name) : Format("%d", station);
}

void Show() { g_showUntilMs = NowMs() + kShowMs; }

void Start() {
    CallMethod<0x507DC0>((void*)kAudioEngine, g_station, (int8_t)0);  // CAudioEngine::StartRadio(station, no bass boost)
    g_playing = true;
    g_startMs = NowMs();
}

void Stop() {
    if (g_playing && !VehicleRadio()) CallMethod<0x4E9820>(Manager(), (void*)nullptr, (uint8_t)0);  // StopRadio
    g_playing = false;
}

void Toggle() {
    g_on = !g_on;
    if (g_on) {
        LOGI("Headphones: on, %s", StationName(g_station).c_str());
    } else {
        LOGI("Headphones: off");
        Stop();
    }
    Show();
}

// a step of the dial: the music stations, and the user tracks when there are any
void Turn(int delta) {
    int8_t last = UserTracks() ? kUserTracksStation : kLastMusicStation;
    int count = last - kFirstStation + 1;
    g_station = (int8_t)(kFirstStation + ((g_station - kFirstStation + delta) % count + count) % count);
    g_on = true;
    if (g_playing && !VehicleRadio()) Start();
    Show();
}

// The MP3 player: a small dark device with a lit screen, boxy as the game's own menus.
void DrawPlayer(float alpha) {
    auto& g = ui::Gfx();
    float s = g.Scale();
    auto c = [alpha](uint32_t argb) { return ui::WithAlpha(argb, alpha); };  // faded as a whole
    float w = 236 * s, h = 266 * s;
    float x = g.Width() - w - 44 * s, y = g.Height() * 0.50f;
    // the body
    g.Rect(x + 6 * s, y + 8 * s, w, h, c(0x70000000));  // shadow
    g.Rect(x, y, w, h, c(0xFF1C1B22));
    g.Rect(x, y, 4 * s, h, c(0xFF2C2A35));               // a light edge
    g.RectOutline(x, y, w, h, c(0xFF4A4658), 2 * s);
    g.Text(ui::FONT_MENU_SMALL, x + w / 2, y + 12 * s, "NOSTALGIA MP3", c(0xFF8E88A6), ui::TEXT_CENTER);
    // the screen
    float sx = x + 16 * s, sy = y + 40 * s, sw = w - 32 * s, sh = 156 * s;
    g.Rect(sx - 3 * s, sy - 3 * s, sw + 6 * s, sh + 6 * s, c(0xFF050507));
    g.Rect(sx, sy, sw, sh, c(0xFF0E2A44));
    g.Rect(sx, sy, sw, 22 * s, c(0xFF143A5C));
    std::string status = g_on ? (Playing() ? TR("ИГРАЕТ", "PLAYING") : TR("ЖДЁТ", "WAITING")) : TR("ВЫКЛ.", "OFF");
    g.Text(ui::FONT_SMALL, sx + 8 * s, sy + 3 * s, status, c(0xFF9FD8FF), 0);
    int8_t last = UserTracks() ? kUserTracksStation : kLastMusicStation;
    g.Text(ui::FONT_SMALL, sx + sw - 8 * s, sy + 3 * s, Format("%02d/%02d", g_station, last), c(0xFF9FD8FF), ui::TEXT_RIGHT);
    // the station's name: smaller when it does not fit, then on two lines (split between words)
    std::string name = g_on ? StationName(g_station) : std::string(TR("НАУШНИКИ", "HEADPHONES"));
    float room = sw - 16 * s;
    if (g.TextWidth(ui::FONT_MENU, name) <= room) {
        g.Text(ui::FONT_MENU, sx + sw / 2, sy + 40 * s, name, c(0xFFFFFFFF), ui::TEXT_CENTER | ui::TEXT_SHADOW);
    } else if (g.TextWidth(ui::FONT_MENU_SMALL, name) <= room || name.find(' ') == std::string::npos) {
        g.Text(ui::FONT_MENU_SMALL, sx + sw / 2, sy + 42 * s, name, c(0xFFFFFFFF), ui::TEXT_CENTER | ui::TEXT_SHADOW);
    } else {
        // as many words on the first line as fit, the rest on the second
        size_t cut = name.find(' ');
        for (size_t next = name.find(' ', cut + 1); next != std::string::npos; next = name.find(' ', next + 1)) {
            if (g.TextWidth(ui::FONT_MENU_SMALL, name.substr(0, next)) > room) break;
            cut = next;
        }
        float lh = g.LineHeight(ui::FONT_MENU_SMALL);
        g.Text(ui::FONT_MENU_SMALL, sx + sw / 2, sy + 32 * s, name.substr(0, cut), c(0xFFFFFFFF), ui::TEXT_CENTER | ui::TEXT_SHADOW);
        g.Text(ui::FONT_MENU_SMALL, sx + sw / 2, sy + 32 * s + lh, name.substr(cut + 1), c(0xFFFFFFFF),
               ui::TEXT_CENTER | ui::TEXT_SHADOW);
    }
    // an equalizer that moves while it plays
    const int bars = 14;
    float bw = (sw - 24 * s) / bars, by = sy + sh - 12 * s, maxH = 54 * s;
    double t = (double)NowMs() / 1000.0;
    for (int i = 0; i < bars; ++i) {
        float level = Playing() ? (float)(0.25 + 0.75 * std::fabs(std::sin(t * (2.1 + 0.37 * i) + i * 1.7))) : 0.06f;
        float bh = std::max(2 * s, maxH * level);
        g.Rect(sx + 12 * s + i * bw, by - bh, bw - 3 * s, bh, c(i % 2 ? 0xFF3FC8FF : 0xFFFF71CE));
    }
    // the buttons under the screen
    float bty = sy + sh + 20 * s, btw = 52 * s, bth = 34 * s, gap = (sw - 3 * btw) / 2;
    const char* labels[3] = {"<<", g_on ? "||" : ">", ">>"};
    for (int i = 0; i < 3; ++i) {
        float bx = sx + i * (btw + gap);
        g.Rect(bx, bty, btw, bth, c(0xFF2B2935));
        g.RectOutline(bx, bty, btw, bth, c(0xFF4A4658), 1 * s);
        g.Text(ui::FONT_NORMAL, bx + btw / 2, bty + 5 * s, labels[i], c(0xFFD6D2E6), ui::TEXT_CENTER);
    }
    // what the keys do
    g.Text(ui::FONT_SMALL, x + w / 2, y + h + 10 * s, TR("H: надеть / снять   H + колесо: станция", "H: on / off   H + wheel: station"),
           c(0xFFFFFFFF), ui::TEXT_CENTER | ui::TEXT_OUTLINE);
}

class Headphones : public Module {
public:
    const char* Name() const override { return "Headphones"; }
    void OnInstallHooks() override {
        if (!mem::RedirectCallFn(0x4EA5C3, &HookAmbienceRadio, &g_origAmbienceRadio))
            LOGW("Headphones: CAERadioTrackManager::CheckForPause is not as expected: the radio will stop out of a car");
    }
    void OnGameShutdown() override { g_playing = false; }

    void OnPostProcess() override {
        uint64_t now = NowMs();
        if (g_held && !(GetAsyncKeyState(Cfg().headphonesKey) & 0x8000)) {  // let go
            g_held = false;
            if (!g_turned && now - g_heldMs < kTapMs) g_tapMs = now;
        }
        if (g_tapMs && now - g_tapMs >= kTapWaitMs) {
            g_tapMs = 0;
            Toggle();
        }
        if (!core::IsPlayerReady()) {
            g_playing = false;
            return;
        }
        if (VehicleRadio()) {  // the vehicle's radio plays: the headphones wait for the way out
            g_playing = false;
            return;
        }
        if (!g_on || CutsceneRunning() || MenuActive()) return;
        if (!RadioOn() && now - g_startMs >= kRestartGapMs) Start();  // put on, out of a car, after a cutscene
    }

    void OnRender() override {
        uint64_t now = NowMs();
        if (g_held) g_showUntilMs = std::max(g_showUntilMs, now + kFadeMs);  // the key held: the player stays up
        if (now >= g_showUntilMs || MenuActive() || !core::IsPlayerReady()) return;
        float alpha = std::min(1.0f, (float)(g_showUntilMs - now) / (float)kFadeMs);
        DrawPlayer(alpha);
    }

    static Headphones& Instance() {
        static Headphones m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Headphones::Instance()); }
} g_reg;
}  // namespace

bool Playing() { return g_on && g_playing && !VehicleRadio() && RadioOn(); }
bool On() { return g_on; }
bool Worn() { return g_on && !VehicleRadio() && core::IsPlayerReady(); }

void KeyDown() {
    if (VehicleRadio()) return;  // in a car the key is the horn
    g_held = true;
    g_turned = false;
    g_heldMs = NowMs();
    g_tapMs = 0;
}

void OtherKey() { g_tapMs = 0; }

void AfterPadUpdate() {
    if (!g_held || VehicleRadio() || !core::IsPlayerReady()) return;
    uint8_t* mouse = (uint8_t*)kMouseNew;
    int delta = mouse[3] ? 1 : mouse[4] ? -1 : 0;  // the wheel up, down
    if (!delta) return;
    mouse[3] = mouse[4] = 0;
    CPad* pad = GetPad(0);
    pad->NewState.LeftShoulder2 = pad->NewState.RightShoulder2 = 0;  // the wheel's change of weapon
    g_turned = true;
    Turn(delta);
}

}  // namespace coop::headphones
