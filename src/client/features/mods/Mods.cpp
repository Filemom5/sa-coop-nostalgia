// The mods menu's data (Mods.h): the game's modloader folder and its modloader.ini. "/mods" lists them in the chat with
// their numbers, "/mods <number>" turns one on or off; the F2 window has the same list with buttons (PlayersMenu).
#include "features/mods/Mods.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "common/lib/log/Log.h"
#include "common/lib/util/Util.h"
#include "shared/config/Config.h"
#include "shared/lib/core/Core.h"
#include "shared/ui/chat/Chat.h"

namespace coop::mods {

namespace {
const char* const kIni = "modloader\\modloader.ini";
const char* const kSection = "[Profiles.Default.IgnoreMods]";
bool g_changed = false;

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::vector<std::string> ReadLines() {
    std::vector<std::string> out;
    std::ifstream f(kIni);
    for (std::string l; std::getline(f, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        out.push_back(l);
    }
    return out;
}

// the names in the section (its lines up to the next section, but comments)
std::vector<std::string> Ignored(const std::vector<std::string>& lines) {
    std::vector<std::string> out;
    bool in = false;
    for (const std::string& raw : lines) {
        std::string l = Trim(raw);
        if (!l.empty() && l[0] == '[') {
            in = l == kSection;
            continue;
        }
        if (in && !l.empty() && l[0] != ';' && l[0] != '#') out.push_back(Lower(l));
    }
    return out;
}

bool Locked(const std::string& name) { return name.size() && name[0] == '_'; }

struct Reg : Module {
    const char* Name() const override { return "Mods"; }
    bool OnCommand(const std::string& cmd, const std::vector<std::string>& args) override {
        if (cmd != "mods") return false;
        auto list = List();
        if (list.empty()) {
            ui::TheChat().AddError(TR("Папки modloader нет.", "No modloader folder."));
            return true;
        }
        if (!args.empty()) {
            int n = std::atoi(args[0].c_str()) - 1;
            std::string error;
            if (n < 0 || n >= (int)list.size()) ui::TheChat().AddError(TR("Нет такого номера (/mods).", "No such number (/mods)."));
            else if (!Toggle(list[n].name, error)) ui::TheChat().AddError(error);
            else
                ui::TheChat().AddSystem(Format(TR("%s: %s после перезапуска игры.", "%s: %s when the game starts again."),
                                               list[n].name.c_str(), list[n].on ? TR("выключен", "off") : TR("включён", "on")));
            return true;
        }
        ui::TheChat().AddSystem(TR("Моды (/mods <номер> - вкл/выкл, после перезапуска; также в F2):",
                                   "Mods (/mods <number> turns one on or off, after a restart; F2 has them too):"));
        for (size_t i = 0; i < list.size(); ++i)
            ui::TheChat().AddSystem(Format("  %zu. %s - %s%s", i + 1, list[i].name.c_str(), list[i].on ? TR("вкл", "on") : TR("выкл", "off"),
                                           list[i].locked ? TR(" (нужен игре)", " (the game needs it)") : ""));
        return true;
    }
} g_reg;
struct Add {
    Add() { core::RegisterModule(&g_reg); }
} g_add;
}  // namespace

std::vector<Mod> List() {
    std::vector<Mod> out;
    auto ignored = Ignored(ReadLines());
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("modloader\\*", &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        std::string n = fd.cFileName;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || n.empty() || n[0] == '.') continue;
        Mod m;
        m.name = n;
        m.locked = Locked(n);
        m.on = std::find(ignored.begin(), ignored.end(), Lower(n)) == ignored.end();
        out.push_back(m);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const Mod& a, const Mod& b) { return Lower(a.name) < Lower(b.name); });
    return out;
}

bool Toggle(const std::string& name, std::string& error) {
    if (Locked(name)) {
        error = TR("Этот мод нужен игре: он не выключается.", "The game needs this one: it stays on.");
        return false;
    }
    auto lines = ReadLines();
    if (lines.empty()) lines = {"[Folder.Config]", "Profile = Default"};
    std::string key = Lower(name);
    bool in = false, found = false, hasSection = false;
    std::vector<std::string> out;
    for (const std::string& raw : lines) {
        std::string l = Trim(raw);
        if (!l.empty() && l[0] == '[') in = l == kSection, hasSection |= in;
        else if (in && Lower(l) == key) {  // off: on again
            found = true;
            continue;
        }
        out.push_back(raw);
    }
    if (!found) {  // on: off, its name in the section
        if (!hasSection) out.push_back(kSection);
        auto it = std::find_if(out.begin(), out.end(), [](const std::string& l) { return Trim(l) == kSection; });
        out.insert(it + 1, name);
    }
    std::ofstream f(kIni, std::ios::trunc | std::ios::binary);
    if (!f) {
        error = TR("modloader.ini не записывается.", "modloader.ini cannot be written.");
        return false;
    }
    for (const std::string& l : out) f << l << "\r\n";
    g_changed = true;
    LOGI("Mods: %s %s (from the next start)", name.c_str(), found ? "on" : "off");
    return true;
}

bool Changed() { return g_changed; }

}  // namespace coop::mods
