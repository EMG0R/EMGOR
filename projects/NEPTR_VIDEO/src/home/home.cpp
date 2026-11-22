#include "home/home.h"

#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace nv {

const std::vector<HomeItem>& homeItems() {
    // SEPTO RENDERER is listed even though nothing is behind it. An instrument
    // that hides its unfinished half reads as finished, and then the day it
    // arrives it is a surprise instead of the thing you were waiting for.
    static const std::vector<HomeItem> items = {
        {"AUDIO",          "SYNTH + FX + LOOPER",    true},
        {"VIDEO",          "FRACTAL + FEEDBACK",     true},
        {"GAMES",          "ARCADE CONSOLE",         true},
        {"NORNS",          "MONOME SCRIPTS + GRID",  true},
        {"SEPTO RENDERER", "NOT BUILT YET",          false},
    };
    return items;
}

const char* homeEntryName(HomeEntry e) {
    int i = static_cast<int>(e);
    if (i < 0 || i >= static_cast<int>(homeItems().size())) return "?";
    return homeItems()[static_cast<size_t>(i)].name.c_str();
}

void HomeMenu::setSelected(int i) {
    int n = static_cast<int>(HomeEntry::kCount);
    if (i < 0 || i >= n) return;
    if (i != selected_) message_.clear();
    selected_ = i;
}

void HomeMenu::move(int delta) {
    int n = static_cast<int>(HomeEntry::kCount);
    selected_ = ((selected_ + delta) % n + n) % n;
    message_.clear();
}

HomeAction HomeMenu::activate() const {
    const auto& items = homeItems();
    size_t i = static_cast<size_t>(selected_);
    if (i >= items.size()) return HomeAction::None;
    if (!items[i].available) return HomeAction::Unavailable;

    switch (static_cast<HomeEntry>(selected_)) {
        case HomeEntry::Audio: return HomeAction::LaunchAudio;
        case HomeEntry::Video: return HomeAction::EnterVideo;
        case HomeEntry::Games: return HomeAction::LaunchGames;
        case HomeEntry::Norns: return HomeAction::LaunchNorns;
        default:               return HomeAction::Unavailable;
    }
}

std::string confValue(const std::string& text, const std::string& key) {
    std::istringstream in(text);
    std::string line;
    std::string found;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        auto trim = [](std::string& s) {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) { s.clear(); return; }
            s = s.substr(a, b - a + 1);
        };
        trim(k);
        trim(v);
        // Last assignment wins, matching how the rest of the stack reads these
        // files: an appended key at the bottom is how they are edited by hand.
        if (k == key) found = v;
    }
    return found;
}

namespace {

// One mode's resolution, so the order (env > home.conf > shipped script) is
// written once and cannot drift between modes.
std::string resolveOne(const std::string& homeConfText, const std::string& key,
                       const HomeModeSource& src) {
    std::string v = confValue(homeConfText, key);
    if (v.empty()) v = src.builtin;
    if (src.env && *src.env) v = src.env;
    return v;
}

}  // namespace

HomeCommands resolveHomeCommands(const std::string& homeConfText,
                                 const HomeModeSources& src) {
    HomeCommands c;
    c.audio = resolveOne(homeConfText, "audio", src.audio);
    c.games = resolveOne(homeConfText, "games", src.games);
    c.norns = resolveOne(homeConfText, "norns", src.norns);
    return c;
}

namespace {

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// The mode scripts ship next to the engine. Looked up rather than assumed, so
// an entry whose script is missing says so instead of failing at exec time
// with the display already handed away.
std::string findScript(const std::string& name) {
    const char* home = std::getenv("HOME");
    std::vector<std::string> candidates = {"deploy/" + name};
    if (home) {
        candidates.push_back(std::string(home) + "/NEPTR_VIDEO/deploy/" + name);
    }
    for (const auto& p : candidates) {
        if (access(p.c_str(), X_OK) == 0) return "/bin/sh " + p;
    }
    return {};
}

}  // namespace

HomeCommands loadHomeCommands() {
    const char* home = std::getenv("HOME");

    // home.conf sits with the engine, not with the audio config: it describes
    // what THIS program launches, and putting it in live.conf would mean
    // writing to a file owned by demiurge-set.
    std::string local = readFile("home.conf");
    if (local.empty() && home) {
        local = readFile(std::string(home) + "/NEPTR_VIDEO/home.conf");
    }

    HomeModeSources src;
    src.audio = {findScript("mode-audio.sh"), std::getenv("NV_HOME_AUDIO")};
    src.games = {findScript("mode-games.sh"), std::getenv("NV_HOME_GAMES")};
    src.norns = {findScript("mode-norns.sh"), std::getenv("NV_HOME_NORNS")};
    return resolveHomeCommands(local, src);
}

}  // namespace nv
