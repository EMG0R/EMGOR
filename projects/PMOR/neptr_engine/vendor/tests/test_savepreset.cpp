// test_savepreset.cpp -- host side of SAVE_PRESET (pmor_host.h savePreset) against fakepmor.h: OK (EDIT for the call, STUDIO back, engine file re-pulled),
// overwrite of an existing user slot (the confirm itself is UI: test_browser.js), factory-slot refusal (never sent), ERR unknown (older firmware, remembered),
// ERR nosave, name sanitising (A-Z 0-9 space - , <= 9 chars), range checks.
#include <stdio.h>
#include "fakepmor.h"

static int pass = 0, fail = 0;
#define CHECK(c, ...) do { if (c) pass++; else { fail++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int countCmd(const FakePmor& d, const std::string& pfx) { int n = 0; for (auto& c : d.cmds) if (c.rfind(pfx, 0) == 0) n++; return n; }
struct Res { int e, n; bool ok; std::string code; };

int main() {
    FakePmor dev; pmorhost::Client C; FakeLink L(dev, C);
    std::vector<Res> res; int pres = 0;
    C.onPresetSaved = [&](int e, int n, bool ok, const std::string& c) { res.push_back({e, n, ok, c}); };
    C.onPresets = [&]() { pres++; };
    dev.userPre[5][6] = "OLD"; dev.userPre[1][2] = "SNR";
    C.connect("STUDIO"); L.until([&] { return C.state() == pmorhost::Client::READY; }, 8000); L.run(100);
    C.pullPresets(); L.until([&] { return pres == 1; }, 5000);
    auto find = [&](int e, int n) -> const pmorhost::Client::Preset* { for (auto& p : C.presets) if (p.engine == e && p.n == n && p.kind == 'u') return &p; return nullptr; };
    CHECK(find(5, 6) && find(5, 6)->name == "OLD" && find(1, 2), "initial user presets pulled through PULL_FILE R");

    // name sanitising (pure)
    CHECK(pmorhost::Client::sanitizePresetName("  lead-1 x!") == "LEAD-1 X", "sanitise: upper-case, drop '!', trim (%s)", pmorhost::Client::sanitizePresetName("  lead-1 x!").c_str());
    CHECK(pmorhost::Client::sanitizePresetName("abcdefghijkl") == "ABCDEFGHI", "sanitise: max 9 chars");
    CHECK(pmorhost::Client::sanitizePresetName("a_b;c\"d\n") == "ABCD", "sanitise: separators and control chars dropped");
    CHECK(pmorhost::Client::sanitizePresetName("abcdefgh  ij") == "ABCDEFGH", "sanitise: trailing space after the cut is trimmed (%s)", pmorhost::Client::sanitizePresetName("abcdefgh  ij").c_str());
    CHECK(pmorhost::Client::sanitizePresetName("___").empty(), "sanitise: nothing left -> empty");

    // OK: synth user slot, name sanitised on the wire, EDIT for the call and STUDIO after, engine file re-pulled, presets refreshed
    pres = 0; res.clear();
    C.savePreset(4, 7, "my lead!!");
    L.until([&] { return !res.empty() && C.hello["mode"] == "STUDIO"; }, 4000); L.run(100);
    CHECK(res.size() == 1 && res[0].ok && res[0].e == 4 && res[0].n == 7, "OK reported (%zu results)", res.size());
    CHECK(dev.saves.size() == 1 && dev.saves[0] == "SAVE_PRESET 4 7 MY LEAD", "wire command is sanitised (%s)", dev.saves.empty() ? "none" : dev.saves[0].c_str());
    CHECK(dev.mode == "STUDIO" && C.hello["mode"] == "STUDIO", "back in STUDIO (device %s)", dev.mode.c_str());
    CHECK(pres == 1 && find(4, 7) && find(4, 7)->name == "MY_LEAD", "engine file re-pulled: new user preset visible (%d refreshes)", pres);
    CHECK(find(5, 6) && find(1, 2), "other engines' presets untouched");

    // overwrite of an existing user slot goes through (the UI asked first)
    res.clear(); C.savePreset(5, 6, "new"); L.until([&] { return !res.empty(); }, 4000); L.run(100);
    CHECK(res.size() == 1 && res[0].ok && find(5, 6) && find(5, 6)->name == "NEW", "overwrite of an existing user slot: replaced (%s)", find(5, 6) ? find(5, 6)->name.c_str() : "none");
    // drum user slot 0..31
    res.clear(); C.savePreset(2, 31, "hat1"); L.until([&] { return !res.empty(); }, 4000); L.run(100);
    CHECK(res.size() == 1 && res[0].ok && find(2, 31) && find(2, 31)->name == "HAT1", "drum slot 31 saved");

    // factory slots are refused on the host and NEVER sent
    size_t before = dev.cmds.size(); res.clear();
    for (int n = 0; n < 5; n++) C.savePreset(4 + n, n, "x");
    L.run(100);
    CHECK(res.size() == 5 && res[0].code == "factory" && res[4].code == "factory" && !res[0].ok, "synth slots 0..4 -> factory");
    CHECK(countCmd(dev, "SAVE_PRESET") == 3 && dev.cmds.size() == before, "no SAVE_PRESET reached the device for factory slots");
    res.clear(); C.savePreset(4, 32, "x"); C.savePreset(0, -1, "x"); C.savePreset(12, 5, "x"); C.savePreset(3, 5, ""); C.savePreset(3, 5, "!!!");
    CHECK(res.size() == 5 && res[0].code == "range" && res[1].code == "range" && res[2].code == "range" && res[3].code == "name" && res[4].code == "name", "range / empty-name refused locally");
    CHECK(countCmd(dev, "SAVE_PRESET") == 3, "still only the 3 valid saves reached the device");

    // ERR nosave
    dev.nosave = true; res.clear(); C.savePreset(4, 9, "z"); L.until([&] { return !res.empty() && C.hello["mode"] == "STUDIO"; }, 4000); L.run(100);
    CHECK(res.size() == 1 && !res[0].ok && res[0].code == "nosave" && dev.mode == "STUDIO", "ERR nosave -> nosave, mode restored (%s)", res.empty() ? "none" : res[0].code.c_str());
    CHECK(!C.saveUnsupported(), "nosave is not 'unsupported'");
    dev.nosave = false;

    // older firmware: ERR unknown -> unsupported, remembered (no second request)
    dev.hasSave = false; res.clear(); int cmds = countCmd(dev, "SAVE_PRESET");
    C.savePreset(4, 10, "old"); L.until([&] { return !res.empty() && C.hello["mode"] == "STUDIO"; }, 4000); L.run(100);
    CHECK(res.size() == 1 && !res[0].ok && res[0].code == "unsupported" && C.saveUnsupported() && dev.mode == "STUDIO", "ERR unknown -> unsupported (%s)", res.empty() ? "none" : res[0].code.c_str());
    int after1 = (int)dev.cmds.size(); res.clear(); C.savePreset(4, 11, "old"); L.run(100);
    CHECK(res.size() == 1 && res[0].code == "unsupported" && (int)dev.cmds.size() == after1, "second call refused locally, nothing sent");
    (void)cmds;

    // saving from EDIT stays in EDIT
    FakePmor d2; pmorhost::Client C2; FakeLink L2(d2, C2); std::vector<Res> r2; C2.onPresetSaved = [&](int e, int n, bool ok, const std::string& c) { r2.push_back({e, n, ok, c}); };
    C2.connect("EDIT"); L2.until([&] { return C2.state() == pmorhost::Client::READY; }, 8000); L2.run(100);
    C2.savePreset(6, 5, "e"); L2.until([&] { return !r2.empty(); }, 4000); L2.run(100);
    CHECK(r2.size() == 1 && r2[0].ok && d2.mode == "EDIT" && countCmd(d2, "HELLO") == 1, "from EDIT: no mode switch (%d HELLO)", countCmd(d2, "HELLO"));

    printf("test_savepreset: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
