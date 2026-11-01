// A line in coop.log every 10 s while playing together: how the game and the connection fared. Copies that stutter or
// jump come from states that arrive late or in bunches; this tells whose: our ping and packet loss, our frame rate and
// its longest frame, and for each other player the longest wait between two of its states (its game hitched, or the
// way lost them), with how many times a copy of a car had to be put on its owner's place with a jump.
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "entities/player/Players.h"
#include "entities/vehicle/Vehicles.h"
#include "shared/api/net-session/Session.h"
#include "shared/lib/core/Core.h"

namespace coop {

namespace {

constexpr uint64_t kEveryMs = 10000;

struct Seen {
    uint64_t lastRecvMs = 0;
    uint64_t longestGapMs = 0;
};

class NetStats : public Module {
public:
    const char* Name() const override { return "NetStats"; }
    void OnSessionEnd() override { Reset(NowMs()); }

    void OnPostProcess() override {
        uint64_t now = NowMs();
        if (!Net().Online() || Net().Peers().empty() || !core::IsPlayerReady()) {
            Reset(now);
            return;
        }
        if (m_lastFrameMs) m_longestFrameMs = std::max(m_longestFrameMs, now - m_lastFrameMs);
        m_lastFrameMs = now;
        ++m_frames;
        for (const auto& [id, rp] : Players::Instance().All()) {
            if (!rp.hasState) continue;
            Seen& s = m_seen[id];
            if (rp.stateRecvMs != s.lastRecvMs) {
                if (s.lastRecvMs) s.longestGapMs = std::max(s.longestGapMs, rp.stateRecvMs - s.lastRecvMs);
                s.lastRecvMs = rp.stateRecvMs;
            }
        }
        if (now - m_startMs < kEveryMs) return;
        std::string gaps;
        for (auto& [id, s] : m_seen) {
            if (!Net().Peers().count(id)) continue;
            if (!gaps.empty()) gaps += ", ";
            gaps += Format("%s %llu ms", Net().PeerName(id).c_str(), (unsigned long long)s.longestGapMs);
            s.longestGapMs = 0;
        }
        float fps = (float)m_frames * 1000.0f / (float)(now - m_startMs);
        LOGI("Net: ping %u ms, loss %.1f%%, fps %.0f (longest frame %llu ms), longest wait for a state: %s; car copies jumped %u times",
             Net().Rtt(), Net().Net().LossPercent(), fps, (unsigned long long)m_longestFrameMs, gaps.empty() ? "-" : gaps.c_str(),
             Vehicles::s_mirrorJumps - m_jumpsAtStart);
        m_startMs = now;
        m_frames = 0;
        m_longestFrameMs = 0;
        m_jumpsAtStart = Vehicles::s_mirrorJumps;
    }

    static NetStats& Instance() {
        static NetStats m;
        return m;
    }

private:
    void Reset(uint64_t now) {
        m_startMs = now;
        m_lastFrameMs = 0;
        m_frames = 0;
        m_longestFrameMs = 0;
        m_seen.clear();
        m_jumpsAtStart = Vehicles::s_mirrorJumps;
    }

    uint64_t m_startMs = 0, m_lastFrameMs = 0, m_longestFrameMs = 0;
    uint32_t m_frames = 0, m_jumpsAtStart = 0;
    std::map<uint8_t, Seen> m_seen;
};

struct Reg {
    Reg() { core::RegisterModule(&NetStats::Instance()); }
} g_reg;

}  // namespace

}  // namespace coop
