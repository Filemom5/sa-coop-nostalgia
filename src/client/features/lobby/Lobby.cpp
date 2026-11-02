#include "features/lobby/Lobby.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#include "common/api/nat-traversal/NatTraversal.h"
#include "common/api/protocol/Protocol.h"
#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/api/net-session/Session.h"
#include "shared/config/Config.h"
#include "shared/lib/core/Core.h"

namespace coop::lobby {

namespace {

// The group's channel, by its name (coop.ini, [connect] group: "nostalgia" unless the friends chose their own, which
// keeps their codes, and so the host's address, from anyone who knows only this mod). Its messages:
// "OPEN <code> <players> <max> <name>", "CLOSED <code>".
std::string Topic() {
    uint64_t h = 1469598103934665603ull;  // FNV-1a
    for (char c : Cfg().lobbyGroup) h = (h ^ (uint8_t)tolower((unsigned char)c)) * 1099511628211ull;
    return Format("sacoop-lobby-%016llx", (unsigned long long)h);
}
// ntfy.sh takes some 250 messages a day from one address: the host says it again every 10 minutes (and when someone
// comes or goes, not more often than every 20 s); one not heard of for 11 minutes is gone (its game closed)
constexpr uint64_t kBeatMs = 10 * 60 * 1000, kChangeMs = 20 * 1000, kPollMs = 8000;
constexpr int64_t kFreshS = 11 * 60;

struct Shared {
    std::mutex mutex;
    Server want;                // what our game should say is open (code empty: nothing)
    uint64_t listUntil = 0;     // the list is wanted till then
    std::vector<Server> list;   // the last fetch
    bool started = false;
};
// never freed: the worker runs till the game's process ends
Shared& S() {
    static Shared* s = new Shared;
    return *s;
}

void Publish(const std::string& text) {
    nat::SignalPublish(nat::kDefaultSignalUrl, Topic(), text);
}

std::string Field(const std::string& line, const char* key) {
    size_t at = line.find(key);
    if (at == std::string::npos) return "";
    at += strlen(key);
    if (at < line.size() && line[at] == '"') {
        size_t e = line.find('"', at + 1);
        return e == std::string::npos ? "" : line.substr(at + 1, e - at - 1);
    }
    size_t e = line.find_first_of(",}", at);
    return line.substr(at, e == std::string::npos ? std::string::npos : e - at);
}

std::vector<Server> Fetch(const std::string& ours) {
    struct Seen {
        Server s;
        int64_t time = 0;
        bool open = false;
    };
    std::map<std::string, Seen> byCode;
    auto r = nat::HttpRequest("GET", std::string(nat::kDefaultSignalUrl) + "/" + Topic() + "/json?poll=1&since=15m", "", "", 6000);
    std::istringstream in(r.status == 200 ? r.body : "");
    for (std::string line; std::getline(in, line);) {
        if (line.find("\"event\":\"message\"") == std::string::npos) continue;
        int64_t time = _atoi64(Field(line, "\"time\":").c_str());
        std::istringstream msg(Field(line, "\"message\":"));
        std::string op, code;
        msg >> op >> code;
        if (code.empty()) continue;
        Seen& seen = byCode[code];
        seen.time = time;
        seen.open = op == "OPEN";
        if (seen.open) {
            seen.s.code = code;
            msg >> seen.s.players >> seen.s.max >> seen.s.name;
        }
    }
    std::vector<Server> out;
    int64_t now = (int64_t)::time(nullptr);
    for (auto& [code, seen] : byCode)
        if (seen.open && code != ours && !seen.s.name.empty() && now - seen.time < kFreshS) out.push_back(seen.s);
    std::sort(out.begin(), out.end(), [](const Server& a, const Server& b) { return a.name < b.name; });
    return out;
}

void Worker() {
    Server said;  // what we said last
    uint64_t saidMs = 0, pollMs = 0;
    for (;;) {
        Sleep(500);
        Server want;
        bool listWanted;
        {
            std::lock_guard<std::mutex> lock(S().mutex);
            want = S().want;
            listWanted = NowMs() < S().listUntil;
        }
        uint64_t now = NowMs();
        if (want.code.empty() && !said.code.empty()) {
            Publish("CLOSED " + said.code);
            LOGI("Lobby: our server %s is closed", said.code.c_str());
            said = Server{};
        } else if (!want.code.empty()) {
            bool changed = want.code != said.code || want.players != said.players || want.name != said.name;
            if ((want.code != said.code) || (changed && now - saidMs >= kChangeMs) || now - saidMs >= kBeatMs) {
                Publish(Format("OPEN %s %d %d %s", want.code.c_str(), want.players, want.max, want.name.c_str()));
                if (want.code != said.code) LOGI("Lobby: our server %s is open for the group", want.code.c_str());
                said = want;
                saidMs = now;
            }
        }
        if (listWanted && now - pollMs >= kPollMs) {
            pollMs = now;
            std::vector<Server> list = Fetch(said.code);
            std::lock_guard<std::mutex> lock(S().mutex);
            S().list = std::move(list);
        }
    }
}

void Start() {
    std::lock_guard<std::mutex> lock(S().mutex);
    if (S().started) return;
    S().started = true;
    std::thread(Worker).detach();
}

class Lobby : public Module {
public:
    const char* Name() const override { return "Lobby"; }
    // the host's game says its server is open while it runs (every player gets its code; the host says it)
    void OnPostProcess() override {
        Server want;
        if (Net().Online() && Net().IsHost() && !Net().InviteCode().empty() && !Cfg().lobbyGroup.empty()) {
            want.code = Net().InviteCode();
            want.name = Cfg().nickname;
            want.players = 1 + (int)Net().Peers().size();
            want.max = proto::kGamePlayers;
        }
        if (!want.code.empty()) Start();
        std::lock_guard<std::mutex> lock(S().mutex);
        S().want = want;
    }
    void OnSessionEnd() override {
        std::lock_guard<std::mutex> lock(S().mutex);
        S().want = Server{};
    }
    static Lobby& Instance() {
        static Lobby m;
        return m;
    }
};

struct Reg {
    Reg() { core::RegisterModule(&Lobby::Instance()); }
} g_reg;

}  // namespace

std::vector<Server> OpenServers() {
    if (Cfg().lobbyGroup.empty()) return {};
    Start();
    std::lock_guard<std::mutex> lock(S().mutex);
    S().listUntil = NowMs() + 2000;
    return S().list;
}

}  // namespace coop::lobby
