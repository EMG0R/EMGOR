// test_hostclient.cpp -- max/pmor_host.h (the Max object's protocol engine) against the in-process simulated PMOR.
// Same simulated device glue as tools/pmorctl.cpp sim.  Run: build_host/test_hostclient
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <functional>
#include <set>
#include "../max/pmor_host.h"
#include "proto.h"
#include "proto_util.h"
#include "params.h"
#include "param.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"

static int g_fail = 0, g_pass = 0;
#define EXPECT(c, ...) do { if (c) g_pass++; else { g_fail++; printf("  FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

namespace sim {
static uint32_t now = 1000, epoch = 1; static uint8_t slot = 3;
static std::string toHost;
static std::vector<int16_t> pool[64][16];
static size_t tx(const uint8_t* d, size_t n) { toHost.append((const char*)d, n); return n; }
static size_t txSpace() { return 1u << 20; }
static bool idle() { return true; }
static uint32_t nowMs() { return now; }
static uint8_t patchSlot() { return slot; }
static uint32_t patchEpoch() { return epoch; }
static void changed() {}
static bool follow() { return false; }
static int policy = 0, busTicks = 0; static const char* roleField = ""; static bool joinClock = true; static std::vector<int> joinsSeen;
static int joinBegin(uint16_t spp) { joinsSeen.push_back(spp); return joinClock ? 0 : -1; }
static int joinPoll(int32_t* late) { *late = 4145; return 1; } static std::set<std::string> cmdsSent;
static bool pooled = false; static int storeState = 2, storePolls = 0, storeRetries = 0, poolErrors = 0;
static std::vector<std::pair<int, int>> assigned;
static void status(char* b, size_t n) {
    if (storeState == 1 && ++storePolls >= 2) storeState = 2;          // a retried storage job settles after two polls
    snprintf(b, n, "cm=0/%d rx=%d/0/0/0 clk=follow %srunning=0 bpm=%d store=%d/%d/0/0/0/0/%d pool=%d", policy, busTicks, roleField, (int)(clk.bpm + 0.5f), storeState, storeState == 1 ? 5 : 0, storeState == 3 ? 2 : 0, pooled ? 3 : 0);
}
static int scan(uint8_t) { return 1; }
static uint16_t poolIdOf(uint16_t id) { return id >= 1024 ? (uint16_t)(id - 1024) : id; }
static uint32_t plen(uint8_t p, uint8_t s) { return p == slot ? smp.len[s] : (uint32_t)pool[p][s].size(); }
static uint32_t pver(uint8_t p, uint8_t s) { return 0x1000u + p * 16 + s; }
static int32_t pread(uint16_t id0, uint32_t off, uint8_t* buf, uint32_t mx) {
    uint16_t id = poolIdOf(id0); uint8_t p = id / 16, s = id % 16; uint32_t total = plen(p, s) * 2; if (off >= total) return 0; uint32_t n = total - off; if (n > mx) n = mx;
    for (uint32_t i = 0; i < n; i += 2) { int16_t v = p == slot ? (int16_t)(smp_buf[s][(off + i) / 2] * 32767.f) : pool[p][s][(off + i) / 2]; buf[i] = (uint8_t)v; buf[i + 1] = (uint8_t)((uint16_t)v >> 8); }
    return (int32_t)n;
}
static int assign(uint8_t slot, uint16_t id) { assigned.push_back({slot, id}); return 0; }
static int del(uint8_t s) { samplerDelete(s); return 0; }
static int clr() { return 0; }

// new-layout hooks: global pool ids 1024+ (alias of patch*16+slot), PRESETS/<engine>.TXT through file_read 'R', STORE_RETRY
static int mancount() { if (!pooled) return 0; int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) n++; return n; }
static bool manat(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* ver, char* name) {
    int n = 0; for (int p = 0; p < 64; p++) for (int q = 0; q < 16; q++) if (plen((uint8_t)p, (uint8_t)q)) { if (n++ == i) {
        *id = (uint16_t)(1024 + p * 16 + q); *len = plen((uint8_t)p, (uint8_t)q); *rate = 48000; *ch = 1; *ver = 0xC0DE0000u + (uint32_t)(p * 16 + q); snprintf(name, 16, "S%u", (unsigned)*id); return true; } }
    return false;
}
static uint32_t slen(uint16_t id) { return plen((uint8_t)(poolIdOf(id) / 16), (uint8_t)(poolIdOf(id) % 16)); }
static uint16_t alias(uint16_t old) { return pooled ? (uint16_t)(1024 + old) : 0; }
static int storeRetry() { storeRetries++; storeState = 1; storePolls = 0; return 0; }
static std::string hexOf(const void* p, size_t n) { static const char* H = "0123456789ABCDEF"; std::string o; for (size_t i = 0; i < n; i++) { uint8_t b = ((const uint8_t*)p)[i]; o += H[b >> 4]; o += H[b & 15]; } return o; }
static std::string presetText(int e) {
    std::string t = "PMORPRESET 2\nengine " + std::to_string(e) + "\n";
    if (e >= 4) { SynPreset f; synFactoryPreset((uint8_t)(e - 4), 1, f); SynPreset u = f; u.cutoff = 1234.f; u.resonance = 0.25f;
        t += "f 1 PLUCK " + hexOf(&f, sizeof f) + "\nu 5 MYSOUND " + hexOf(&u, sizeof u) + "\n"; }
    else { uint8_t du[20] = {0}; du[0] = 2; float k[4] = {0.1f, 0.2f, 0.3f, 0.4f}; memcpy(du + 4, k, sizeof k);
        t += "f 3 - 00\nu 0 MYDRUM " + hexOf(du, sizeof du) + "\n"; }
    return t + "end 0\n";
}
static int32_t fread_(char k, uint16_t slotn, uint32_t off, uint8_t* buf, uint32_t mx, uint32_t* total) {
    if (k != 'R' || slotn >= 12 || poolErrors) return -1;
    std::string t = presetText(slotn); *total = (uint32_t)t.size(); if (off >= t.size()) return 0;
    uint32_t n = (uint32_t)t.size() - off; if (n > mx) n = mx; memcpy(buf, t.data() + off, n); return (int32_t)n;
}
static void aud(char, uint8_t, uint8_t) {}
static void clkmode(uint8_t) {}
static void legacy(const char*) {}
static void mode(ProtoMode) {}
static const ProtoOps ops = { tx, txSpace, idle, nowMs, "sim", patchSlot, patchEpoch, changed, follow, status, scan, plen, pver, pread, assign, del, clr, aud, clkmode, legacy, mode, fread_, nullptr, mancount, manat, slen, alias, storeRetry, nullptr, joinBegin, joinPoll, nullptr };
static void boot() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
    pmorSchemaInit();
    for (int i = 0; i < 4000; i++) smp_buf[3][i] = 0.5f * sinf(i * 0.05f); smp.len[3] = 4000;
    for (int p : {0, 7}) for (int s : {0, 5}) { pool[p][s].resize(2500 + p * 10 + s); for (size_t i = 0; i < pool[p][s].size(); i++) pool[p][s][i] = (int16_t)(1000 * (int)((i * 7 + p + s) % 31) - 15000); }
    protoInit(&ops); protoConnected(true);
}
}

static bool syn_algo_is(int a) { float v = -1; pmorParamGet(pmorParamId(PD_SYNTH, 256), &v); return (int)v == a; }
static pmorhost::Client C;
static int hostSetParamFrames = 0, synced = 0, paramEvents = 0; static float lastVol = -1; static std::vector<std::string> infos;
static void run(int ms) {            // 5 ms steps: client -> device, device -> client
    for (int t = 0; t < ms; t += 5) { sim::now += 5; protoPoll(); std::string o; o.swap(sim::toHost); if (!o.empty()) C.feed(o.data(), o.size()); C.tick(5); }
}
static bool until(std::function<bool()> pred, int maxMs) { for (int t = 0; t < maxMs; t += 5) { if (pred()) return true; run(5); } return pred(); }

int main() {
    sim::boot();
    C.send = [](const std::string& s) {
        if (s.find(" SET_PARAM ") != std::string::npos) hostSetParamFrames++;
        { size_t a = s.find(' '), b = s.find_first_of(" *", a + 1); if (a != std::string::npos && b != std::string::npos) sim::cmdsSent.insert(s.substr(a + 1, b - a - 1)); }
        protoRx((const uint8_t*)s.data(), s.size()); };
    const uint16_t VOL = pmorParamId(PD_SYNTH, 257);
    C.onParam = [VOL](uint16_t id, float v) { paramEvents++; if (id == VOL) lastVol = v; };
    C.onInfo = [](const std::string& s) { infos.push_back(s); };
    C.onSynced = []() { synced++; };

    printf("[connect + full resync]\n");
    C.connect("STUDIO");
    EXPECT(until([] { return C.state() == pmorhost::Client::READY; }, 8000), "READY (state %s)", C.stateName());
    EXPECT(C.hello["mode"] == "STUDIO" && C.hello["proto"] == "1", "hello fields");
    EXPECT(C.schemaCount == pmorSchemaCount() && C.schemaHash == C.hello["schema"], "schema %zu entries hash %s vs hello %s", C.schemaCount, C.schemaHash.c_str(), C.hello["schema"].c_str());
    EXPECT(C.params.size() == pmorSchemaCount(), "params mirrored %zu", C.params.size());
    EXPECT(C.seq.size() == 3, "sequences %zu", C.seq.size());
    EXPECT(!C.manifest.empty(), "manifest %zu", C.manifest.size());
    EXPECT(synced == 1, "onSynced once (%d)", synced);
    EXPECT(protoMode() == PM_STUDIO, "device in STUDIO");

    printf("[PMOR is the source of truth: device edit -> host, no echo]\n");
    int before = hostSetParamFrames; lastVol = -1; syn.volume = 0.8f;
    EXPECT(until([] { return fabsf(lastVol - 0.8f) < 1e-3f; }, 1000), "push reached the host (lastVol %.3f)", lastVol);
    run(200);
    EXPECT(hostSetParamFrames == before, "host did not answer a push with SET_PARAM");

    printf("[host edit -> device]\n");
    lastVol = -1; C.setParam(VOL, 0.5f);
    EXPECT(until([] { return fabsf(lastVol - 0.5f) < 1e-3f; }, 1000), "ack value %.3f", lastVol);
    EXPECT(fabsf(syn.volume - 0.5f) < 1e-3f, "device holds %.3f", syn.volume);
    size_t ni = infos.size(); C.setParam(VOL, 99.f); run(300);
    EXPECT(infos.size() > ni && infos.back().find("refused") != std::string::npos, "range error reported: %s", infos.empty() ? "none" : infos.back().c_str());

    printf("[sample cache: STUDIO pulls the current patch, EDIT pulls all]\n");
    {
        int done = 0, bad = 0, prog = 0; uint32_t lastTotal = 0; std::vector<int> ids;
        C.haveSample = [](const pmorhost::Client::Sample&) { return false; };
        C.onSampleProgress = [&](const pmorhost::Client::Sample&, uint32_t, uint32_t t, int) { prog++; lastTotal = t; };
        C.onSampleDone = [&](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>& d, bool ok) { done++; if (!ok) bad++; ids.push_back(sm.id); (void)d; };
        C.pullSamples(false);
        EXPECT(until([] { return C.pullsQueued() == 0; }, 20000), "current-patch pulls finish");
        int slot = atoi(C.hello["patch"].c_str());
        bool onlyCurrent = !ids.empty(); for (int id : ids) if (id / 16 != slot) onlyCurrent = false;
        EXPECT(done >= 1 && bad == 0 && onlyCurrent, "STUDIO pulled %d sample(s) of patch %d, %d bad", done, slot, bad);
        EXPECT(prog >= 1 && lastTotal > 0, "progress callbacks (%d, total %u bytes)", prog, lastTotal);
        // cached samples are skipped
        int d2 = 0; C.haveSample = [](const pmorhost::Client::Sample&) { return true; }; C.onSampleDone = [&](const pmorhost::Client::Sample&, const std::vector<uint8_t>&, bool) { d2++; };
        C.pullSamples(false); run(200);
        EXPECT(d2 == 0 && C.pullsQueued() == 0, "cached samples are not pulled again");
        // EDIT pulls everything in the manifest
        done = 0; bad = 0; ids.clear();
        C.haveSample = [](const pmorhost::Client::Sample&) { return false; };
        C.onSampleDone = [&](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>&, bool ok) { done++; if (!ok) bad++; ids.push_back(sm.id); (void)sm; };
        C.pullSamples(true);
        EXPECT(until([] { return C.pullsQueued() == 0; }, 60000), "all-sample pulls finish");
        size_t nonEmpty = 0; for (auto& l : C.manifest) { unsigned id, len; if (sscanf(l.c_str(), "M %u %u", &id, &len) == 2 && len > 0) nonEmpty++; }
        EXPECT((size_t)done == nonEmpty && bad == 0, "EDIT pulled %d of %zu non-empty samples, %d bad", done, nonEmpty, bad);
        C.onSampleDone = nullptr; C.onSampleProgress = nullptr;
    }

    printf("[new layout: pool ids >= 1024, PATCHINFO ids=, pullid, preset files via PULL_FILE R, apply, STORE_RETRY]\n");
    {
        sim::pooled = true; int pools = 0; C.onPool = [&]() { pools++; };
        C.refreshPool();
        EXPECT(until([&] { return pools == 1; }, 2000), "pool refreshed");
        auto pl = C.pool(); bool allHigh = !pl.empty(); for (auto& x : pl) if (x.id < 1024 || x.name != "S" + std::to_string(x.id) || x.crc < 0xC0DE0000u) allHigh = false;
        EXPECT(allHigh && pl.size() == 5, "pool lists %zu global ids (all >= 1024, name S<id>, ver token kept)", pl.size());
        EXPECT(C.manifest.size() == pl.size(), "manifest replaced, not appended (%zu)", C.manifest.size());
        // current-patch pull follows ids= from PATCHINFO (the id/16 == patch rule no longer holds)
        int done = 0, bad = 0; std::vector<int> ids; C.haveSample = [](const pmorhost::Client::Sample&) { return false; };
        C.onSampleDone = [&](const pmorhost::Client::Sample& sm, const std::vector<uint8_t>&, bool ok) { done++; if (!ok) bad++; ids.push_back(sm.id); };
        C.pullSamples(false);
        EXPECT(until([&] { return done >= 1 && C.pullsQueued() == 0; }, 20000) && done == 1 && bad == 0 && ids[0] == 1024 + 3 * 16 + 3, "current patch pulled by ids= (%d sample, id %d, %d bad)", done, ids.empty() ? -1 : ids[0], bad);
        done = 0; bad = 0; ids.clear(); C.pullId(1024 + 7 * 16 + 5); EXPECT(until([&] { return done >= 1 && C.pullsQueued() == 0; }, 20000) && done == 1 && bad == 0, "pullid pool sample (%d)", done);
        size_t ni = infos.size(); C.pullId(9999); EXPECT(infos.size() > ni, "pullid of an unknown id is reported");
        C.onSampleDone = nullptr;
        // presets
        int pres = 0; C.onPresets = [&]() { pres++; };
        C.pullPresets();
        EXPECT(until([&] { return pres == 1; }, 5000), "preset files pulled");
        EXPECT(C.presets.size() == 24, "12 engines x (factory + user) = %zu entries", C.presets.size());
        int syn = 0, drm = 0, user = 0; for (auto& x : C.presets) { (x.engine >= 4 ? syn : drm)++; if (x.kind == 'u') user++; }
        EXPECT(syn == 16 && drm == 8 && user == 12, "synth %d drum %d user %d", syn, drm, user);
        bool unnamed = false; for (auto& x : C.presets) if (x.engine == 1 && x.kind == 'f' && x.n == 3 && x.name == "-") unnamed = true;
        EXPECT(unnamed, "unnamed factory entry keeps '-' (the UI falls back to the schema sound names)");
        bool named = false; for (auto& x : C.presets) if (x.engine == 5 && x.kind == 'u' && x.n == 5 && x.name == "MYSOUND" && x.blob.size() == 80) named = true;
        EXPECT(named, "user synth preset parsed (name + 80 B SynPreset)");
        // apply a user synth preset (algo 1 = engine 5): every field lands on the device through SET_PARAM
        int before = hostSetParamFrames; C.applyPreset(5, 5); run(600);
        float cut = 0, res = 0; pmorParamGet(pmorParamId(PD_SYNTH, 1 * 32 + 8), &cut); pmorParamGet(pmorParamId(PD_SYNTH, 1 * 32 + 9), &res);
        EXPECT(fabsf(cut - 1234.f) < 1.f && fabsf(res - 0.25f) < 1e-3f && syn_algo_is(1) && hostSetParamFrames - before == 21, "user synth preset applied: cutoff %.1f reso %.3f, %d SET_PARAMs", cut, res, hostSetParamFrames - before);
        // factory drum entry = SOUND index on the pad; user drum entry = base sound + 4 engine knobs
        C.applyPreset(0, 3, 6); run(200); float snd = -1; pmorParamGet(pmorParamId(PD_DRUMS, 6 * 8 + 5), &snd);
        EXPECT(snd == 3.f, "factory drum preset sets pad 6 SOUND (%g)", snd);
        C.applyPreset(1, 0, 9); run(300); float b = -1, k0 = -1, k3 = -1; pmorParamGet(pmorParamId(PD_DRUMS, 9 * 8 + 5), &b); pmorParamGet(pmorParamId(PD_DRUMS, 272 + 1 * 8 + 0), &k0); pmorParamGet(pmorParamId(PD_DRUMS, 272 + 1 * 8 + 3), &k3);
        EXPECT(b == 2.f && fabsf(k0 - 0.1f) < 1e-3f && fabsf(k3 - 0.4f) < 1e-3f, "user drum preset: base %g knobs %.2f..%.2f", b, k0, k3);
        ni = infos.size(); EXPECT(!C.applyPreset(2, 3) && infos.size() > ni, "drum preset without a pad is refused");
        // a card without preset files is not an error for the client
        sim::poolErrors = 1; pres = 0; C.pullPresets(); EXPECT(until([&] { return pres == 1; }, 5000) && C.presets.empty(), "missing preset files -> empty list, callback still fires");
        sim::poolErrors = 0; C.pullPresets(); until([&] { return pres == 2; }, 5000);
        // storage: error state is reported with the retry hint, STORE_RETRY re-runs it, pool + presets reload when it settles
        std::vector<int> states; C.onStore = [&](int st, int, int, int) { states.push_back(st); };
        sim::storeState = 3; ni = infos.size(); until([&] { return !states.empty(); }, 3000);
        bool hint = false; for (size_t i = ni; i < infos.size(); i++) if (infos[i].find("storeretry") != std::string::npos) hint = true;
        EXPECT(!states.empty() && states.back() == 3 && hint, "storage errors reported with a retry hint (state %d)", states.empty() ? -1 : states.back());
        pools = 0; pres = 0; int rt = sim::storeRetries; C.storeRetry();
        EXPECT(until([&] { return pools >= 1 && pres >= 1; }, 20000), "after STORE_RETRY: pool reloaded %d, presets reloaded %d", pools, pres);
        bool sawRun = false; for (int st : states) if (st == 1) sawRun = true;
        EXPECT(sim::storeRetries == rt + 1 && sawRun && states.back() == 2, "retry ran once, saw 'migrating' then 'done' (last %d)", states.back());
        C.onStore = nullptr; C.onPool = nullptr; C.onPresets = nullptr;
        // a GET_* issued while a PULL_FILE reply is running answers `ERR busy`: the client retries it instead of reporting a failure
        int s1 = synced; size_t ni2 = infos.size(); C.pullPresets(); C.getPatch();
        EXPECT(until([&] { return synced == s1 + 1; }, 8000), "GET_PATCH during a preset pull completes (busy retried)");
        bool failed = false; for (size_t i = ni2; i < infos.size(); i++) if (infos[i].find("GET_PATCH failed") != std::string::npos) failed = true;
        EXPECT(!failed, "no 'GET_PATCH failed' reported for a busy retry"); run(1500);
    }

    printf("[PATCH_CHANGED -> pull whole patch]\n");
    int s0 = synced; sim::epoch++; run(500);
    EXPECT(synced == s0 + 1, "patch pulled after PATCH_CHANGED (synced %d -> %d)", s0, synced);

    printf("[liveness: no traffic for 4 s]\n");
    run(4000);
    EXPECT(protoMode() == PM_STUDIO && C.state() == pmorhost::Client::READY, "still STUDIO after 4 s idle (auto PING)");

    printf("[unplug / replug]\n");
    protoConnected(false); C.portClosed(); protoConnected(true); sim::toHost.clear();
    EXPECT(protoMode() == PM_STANDALONE, "standalone after port close");
    synced = 0; C.connect("EDIT");
    EXPECT(until([] { return C.state() == pmorhost::Client::READY; }, 8000), "resync after replug");
    EXPECT(protoMode() == PM_EDIT && synced == 1, "EDIT mode, one resync");

    printf("[ASSIGN_SAMPLE (EDIT only)]\n");
    {
        std::vector<std::string> res; C.onAssigned = [&](int sl, bool ok, const std::string& c) { res.push_back(std::to_string(sl) + (ok ? " ok" : " err " + c)); };
        sim::assigned.clear(); C.assign(4, 1024); run(100);
        EXPECT(sim::assigned.size() == 1 && sim::assigned[0].first == 4 && sim::assigned[0].second == 1024, "ASSIGN_SAMPLE reached the device (slot 4, id 1024)");
        protoSampleDone(4, 0); run(200);
        EXPECT(!res.empty() && res.back() == "4 ok", "SAMPLE_READY -> onAssigned ok (%s)", res.empty() ? "none" : res.back().c_str());
        protoSampleDone(5, 3); sim::assigned.clear(); C.assign(5, 1025); run(100); protoSampleDone(5, 3); run(200);
        EXPECT(!res.empty() && res.back().rfind("5 err", 0) == 0, "SAMPLE_ERR -> onAssigned err (%s)", res.empty() ? "none" : res.back().c_str());
        C.assign(99, 1024); EXPECT(res.back() == "99 err arg", "slot range checked on the host (%s)", res.back().c_str());
        C.onAssigned = nullptr;
    }
    printf("[SAVE_PRESET against the real firmware protocol (no SAVE_PRESET yet): ERR unknown -> unsupported]\n");
    {
        std::vector<std::string> res; C.onPresetSaved = [&](int e, int n, bool ok, const std::string& c) { res.push_back(std::to_string(e) + "/" + std::to_string(n) + (ok ? " ok" : " " + c)); };
        C.savePreset(4, 6, "x"); run(300);
        EXPECT(res.size() == 1 && res[0] == "4/6 unsupported" && C.saveUnsupported(), "old firmware: save not supported (%s)", res.empty() ? "none" : res[0].c_str());
        C.onPresetSaved = nullptr;
    }
    printf("[ASSIGN from STUDIO switches to EDIT for the assign and back; policy/bus status from cm= and rx=]\n");
    {
        C.bye(); run(200); protoConnected(false); protoConnected(true); sim::toHost.clear();
        C.connect("STUDIO"); until([] { return C.state() == pmorhost::Client::READY; }, 8000);
        std::vector<std::string> res; C.onAssigned = [&](int sl, bool ok, const std::string& c) { res.push_back(std::to_string(sl) + (ok ? " ok" : " err " + c)); };
        std::vector<std::map<std::string, std::string>> sts; C.onStatus = [&](const std::map<std::string, std::string>& m) { sts.push_back(m); };
        sim::assigned.clear(); C.assign(2, 1025); run(200);
        EXPECT(protoMode() == PM_EDIT && sim::assigned.size() == 1, "assign from STUDIO: device went to EDIT and got ASSIGN_SAMPLE (mode %s)", protoModeName(protoMode()));
        protoSampleDone(2, 0); run(300);
        EXPECT(!res.empty() && res.back() == "2 ok" && protoMode() == PM_STUDIO && C.hello["mode"] == "STUDIO", "SAMPLE_READY -> ok, back in STUDIO (%s, %s)", res.empty() ? "none" : res.back().c_str(), protoModeName(protoMode()));
        sim::busTicks = 0; sts.clear(); C.pollStatus(); run(200); C.pollStatus(); run(200);
        EXPECT(!sts.empty() && sts.back()["policy"] == "AUTO" && sts.back()["bus"] == "0", "policy AUTO read from cm=, no bus clock (%s/%s)", sts.empty() ? "-" : sts.back()["policy"].c_str(), sts.empty() ? "-" : sts.back()["bus"].c_str());
        sim::busTicks += 24; sts.clear(); C.pollStatus(); run(200);
        EXPECT(!sts.empty() && sts.back()["bus"] == "1" && sts.back()["clk"] == "follow", "bus clock arriving detected from rx= although clk= says follow");
        EXPECT(sts.back()["role"] == "follow", "role inferred: AUTO + bus clock arriving = follow (%s)", sts.back()["role"].c_str());
        sim::busTicks += 24; sim::policy = 3; sts.clear(); C.pollStatus(); run(200); EXPECT(!sts.empty() && sts.back()["policy"] == "MASTER" && sts.back()["role"] == "master", "policy MASTER -> role master");
        sim::policy = 0; sim::busTicks += 24; sim::roleField = "role=master "; sts.clear(); C.pollStatus(); run(200);
        EXPECT(!sts.empty() && sts.back()["role"] == "master", "firmware role= wins over inference (%s)", sts.back()["role"].c_str());
        sim::busTicks += 24; sim::roleField = "role=follow "; sts.clear(); C.pollStatus(); run(200); EXPECT(!sts.empty() && sts.back()["role"] == "follow", "role=follow from firmware");
        sim::roleField = "role=weird "; sim::busTicks += 24; sts.clear(); C.pollStatus(); run(200); EXPECT(!sts.empty() && (sts.back()["role"] == "follow" || sts.back()["role"] == "master"), "unknown role value falls back to inference");
        sim::roleField = ""; sim::policy = 0;
        C.onAssigned = nullptr; C.onStatus = nullptr;
    }
    {
        pmorhost::Client S; std::string out; S.send = [&](const std::string& x) { out += x; };
        S.assign(1, 1024); EXPECT(out.empty(), "assign while disconnected is ignored without traffic");
    }

    printf("[link lost -> STANDALONE, auto reconnect + full resync FROM PMOR; mode switch; patch switch pulls; JOIN]\n");
    {
        C.bye(); run(200); protoConnected(false); protoConnected(true); sim::toHost.clear(); C.onSynced = [&]() { synced++; };
        synced = 0; C.connect("STUDIO"); until([] { return C.state() == pmorhost::Client::READY; }, 8000);
        // 1. mode switch: HELLO again, no resync
        std::vector<std::string> modes; C.onMode = [&](const std::string& m) { modes.push_back(m); };
        int sy0 = synced; C.switchMode("EDIT"); run(300);
        EXPECT(protoMode() == PM_EDIT && !modes.empty() && modes.back() == "EDIT" && synced == sy0, "switchMode EDIT: device in EDIT, no resync (mode %s, synced %d->%d)", protoModeName(protoMode()), sy0, synced);
        C.switchMode("STUDIO"); run(300); EXPECT(protoMode() == PM_STUDIO && C.hello["mode"] == "STUDIO", "switchMode back to STUDIO");
        // 2. patch switch on PMOR: whole patch pulled, manifest reloaded, owner told (then pulls samples)
        int sw = -1, pools = 0, patches = 0; C.onPatchSwitched = [&](int sl) { sw = sl; }; C.onPool = [&]() { pools++; }; C.onPatch = [&](int) { patches++; };
        sim::slot = 5; sim::epoch++; run(1500);
        EXPECT(sw == 5 && pools >= 1 && patches >= 1 && C.hello["patch"] == "5", "PATCH_CHANGED -> GET_PATCH + GET_MANIFEST + onPatchSwitched(5) (sw %d pools %d)", sw, pools);
        sim::slot = 3; C.onPatchSwitched = nullptr; C.onPool = nullptr; C.onPatch = nullptr; sim::epoch++; run(1200);
        // 3. JOIN (real proto path, sim hooks): OK JOIN spp16= tick_at=<late_us>; noclock; arg; busy during a bulk reply is avoided by joinPrep()
        std::vector<std::string> js; C.onJoin = [&](bool ok, int spp, const std::string& x) { js.push_back(std::string(ok ? "ok " : "err ") + std::to_string(spp) + " " + x); };
        C.join(32); run(200);
        EXPECT(js.size() == 1 && js[0] == "ok 32 4145" && sim::joinsSeen.size() == 1 && sim::joinsSeen[0] == 32, "JOIN 32 -> OK tick_at=4145 us (%s)", js.empty() ? "none" : js[0].c_str());
        bool probe = false; for (auto& x : infos) if (x.find("JOIN spp16=32 latency 4145 us") != std::string::npos) probe = true; EXPECT(probe, "latency is logged as the probe");
        sim::joinClock = false; C.join(48); run(200); EXPECT(js.size() == 2 && js[1] == "err 48 noclock", "JOIN without bus clock -> noclock (%s)", js.size() < 2 ? "none" : js[1].c_str());
        sim::joinClock = true; C.join(0x4000); EXPECT(js.size() == 3 && js[2].rfind("err", 0) == 0, "JOIN out of range refused on the host");
        {   // bulk transfer running: a plain JOIN hits the device's busy job; joinPrep() holds the chunk requests so JOIN goes through
            std::vector<std::string> jb; C.onJoin = [&](bool ok, int spp, const std::string& x) { jb.push_back(std::string(ok ? "ok " : "err ") + std::to_string(spp) + " " + x); };
            C.haveSample = [](const pmorhost::Client::Sample&) { return false; }; C.onSampleDone = [](const pmorhost::Client::Sample&, const std::vector<uint8_t>&, bool) {};
            C.pullSamples(true); run(60);
            C.joinPrep(); run(10); C.join(64); run(400);
            EXPECT(!jb.empty() && jb[0] == "ok 64 4145", "JOIN during an all-sample pull succeeds after joinPrep (%s)", jb.empty() ? "none" : jb[0].c_str());
            C.cancelPulls(); C.haveSample = nullptr; C.onSampleDone = nullptr; run(500);
        }
        C.onJoin = nullptr;
        // 4. cable yank while READY: nothing answers -> link lost -> disconnected (STANDALONE) -> retries; replug -> resync from PMOR
        std::vector<std::string> inf; C.onInfo = [&](const std::string& x) { inf.push_back(x); infos.push_back(x); };
        bool lost = false; synced = 0;
        int lostAt = -1;
        for (int t = 0; t < 9000 && !lost; t += 5) { sim::now += 5; protoPoll(); sim::toHost.clear(); C.tick(5); for (auto& x : inf) if (x.find("link lost") != std::string::npos) lost = true; if (lost) lostAt = t; }
        EXPECT(lost && lostAt <= 2600, "unplugged: client reports link lost within 2.6 s (%d ms, state %s)", lostAt, C.stateName());
        EXPECT(C.state() != pmorhost::Client::READY && C.wantsConnection(), "device shows STANDALONE (state %s), still wants the connection", C.stateName());
        protoConnected(false); protoConnected(true); sim::toHost.clear();             // replug: the port is back, DTR high
        EXPECT(until([] { return C.state() == pmorhost::Client::READY; }, 3000), "replug: reconnects by itself within 3 s (state %s)", C.stateName());
        EXPECT(synced >= 1 && C.params.size() == pmorSchemaCount() && protoMode() == PM_STUDIO, "full resync from PMOR after replug (synced %d, params %zu)", synced, C.params.size());
        // bye clears the want: no more reconnects
        C.bye(); run(300); EXPECT(!C.wantsConnection(), "after bye the client does not reconnect");
        C.connect("STUDIO"); until([] { return C.state() == pmorhost::Client::READY; }, 8000);
    }
    printf("[cable yank during sync: no device answers]\n");
    {
        pmorhost::Client Y; std::vector<std::string> yi; Y.onInfo = [&](const std::string& x) { yi.push_back(x); }; std::string out; Y.send = [&](const std::string& x) { out += x; };
        Y.autoReconnect = false; Y.connect("STUDIO"); for (int t = 0; t < 7000; t += 100) Y.tick(100);
        bool timeoutSaid = false; for (auto& x : yi) if (x.find("timeout") != std::string::npos || x.find("HELLO refused") != std::string::npos) timeoutSaid = true;
        EXPECT(Y.state() == pmorhost::Client::DISCONNECTED && timeoutSaid, "dead link while connecting: state %s, reported %d", Y.stateName(), (int)timeoutSaid);
    }
    printf("[bye]\n");
    C.bye(); run(200);
    EXPECT(protoMode() == PM_STANDALONE && C.state() == pmorhost::Client::DISCONNECTED, "BYE");
    {   // "patch changes only on PMOR": the host client has no way to write, replace, clear or delete a patch, nor to touch storage settings
        const char* never[] = {"CLEAR_PATCH", "DELETE_SAMPLE", "SAVE_SCRATCH", "CLOCKMODE", "NOSAVE"}; bool bad = false; std::string seen;
        for (auto& c : sim::cmdsSent) { seen += c + " "; for (auto n : never) if (c == n) bad = true; }
        EXPECT(!bad, "commands the client ever sent: %s(no patch write/clear/delete)", seen.c_str());
    }
    printf("\ntest_hostclient: %d passed, %d failed (bad frames %d, naks %d)\n", g_pass, g_fail, C.badFrames, C.nakCount);
    return g_fail ? 1 : 0;
}
