#include "features/perf-hud/PerfHud.h"

#include <cstdint>
#include <string>

#include "common/lib/ini/Ini.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/renderer/Renderer.h"

namespace coop::perfhud {

namespace {

constexpr uint64_t kEveryMs = 500;  // the numbers change twice a second, readable

uint64_t g_startMs = 0;
uint32_t g_frames = 0;
int g_fps = 0;

class PerfHud : public Module {
public:
    const char* Name() const override { return "PerfHud"; }
    void OnRender() override {
        uint64_t now = NowMs();
        ++g_frames;
        if (!g_startMs || now - g_startMs > 5000) g_startMs = now, g_frames = 0;  // after a pause or a load
        else if (now - g_startMs >= kEveryMs) {
            g_fps = (int)(g_frames * 1000 / (now - g_startMs));
            g_startMs = now, g_frames = 0;
        }
        if (!Cfg().fpsCounter || Cfg().cleanScreen) return;
        auto& g = ui::Gfx();
        float s = g.Scale();
        std::string text = Format("FPS: %d", g_fps);
        if (Net().Online()) text += Format("    %u ms", Net().Rtt());
        float pad = 10 * s, w = g.TextWidth(ui::FONT_SMALL, text) + 2 * pad, h = g.LineHeight(ui::FONT_SMALL) + pad;
        float x = g.Width() - w - 8 * s, y = 6 * s;
        g.Rect(x, y, w, h, ui::Argb(150, 8, 12, 18));
        g.Text(ui::FONT_SMALL, x + pad, y + pad * 0.5f, text, 0xFFFFFFFF, ui::TEXT_OUTLINE);
    }
    static PerfHud& Instance() {
        static PerfHud m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&PerfHud::Instance()); }
} g_reg;

}  // namespace

void Toggle() {
    auto& cfg = Cfg();
    cfg.fpsCounter = !cfg.fpsCounter;
    Ini ini;
    ini.Load(cfg.iniPath);
    ini.SetInt("game", "fps_counter", cfg.fpsCounter ? 1 : 0);
    ini.Save(cfg.iniPath);
    LOGI("FPS counter %s", cfg.fpsCounter ? "on" : "off");
}

}  // namespace coop::perfhud
