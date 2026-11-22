// HYPERTRUMPET V2 CPU benchmark firmware (Daisy Seed 3). Runs every FX alone in the REAL audio callback (48 kHz, block 48),
// then the default 4-slot patch (with and without the mapper), then the worst 4, and streams newline JSON over USB CDC.
// Input = internal synthetic trumpet tone (no mic). Output stays silent-ish; audio out is the real engine output.
// Protocol (seed -> host, newline JSON):
//   {"t":"hello",...}   ~1 Hz: fw, clk, sr, blk, budget_cyc, state "running"|"done", case index / count, current label
//   {"t":"r","i":N,...} one record per finished case; replayed round-robin after the run, so a late-connecting host gets them all
// host -> seed: run (restart the whole run) | tone 0 | tone 1 (silence / synthetic tone, applies to the next run) | dfu
#include <stdlib.h>
#include <math.h>
#include "daisy_seed.h"
#include "util/CpuLoadMeter.h"
#include "hyper_hw.h"
#include "hx_seed.h"
#include "hx_fault.h"
#include "fx_mapping.h"
#include "tone.h"

#define FW_VERSION "hx-bench-2"
#ifndef BENCH_BOOST
#define BENCH_BOOST 1
#endif
using namespace daisy;

DaisySeed hw;
static const int   BLK = 48;
static const float SR  = 48000.f;
static hx::FxEngine g_eng;
static hx::HxMapper g_map;
static BenchTone    g_tone;
static CpuLoadMeter g_meter;
static HxOut        g_out;
static uint32_t     g_budget = 480000;     // cycles per block = BLK / SR * clk

// ---------------------------------------------------------------- measurement accumulators (callback writes, main reads)
static volatile uint32_t m_n = 0, m_peak = 0, m_over = 0; static volatile uint64_t m_sum = 0;
static volatile bool m_clear = false;
static volatile uint32_t g_underrun = 0;
// ---- overload guard (audio ISR): 100-block window average of the WHOLE callback > 95 % of the block budget -> trip. The callback then mutes
// and skips the engine until the main loop acknowledges, so a runaway case can never starve the main loop / USB.
static volatile bool g_trip = false; static volatile uint32_t g_tripAvg = 0, g_tripPeak = 0, g_trips = 0;
static uint32_t w_sum = 0, w_n = 0, w_peak = 0;

static void audioCb(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    uint32_t c0 = hxCyc();
    if (g_trip) { for (size_t i = 0; i < size; i++) { out[0][i] = 0.f; out[1][i] = 0.f; } w_sum = w_n = w_peak = 0; return; }
    g_meter.OnBlockStart();
    static float mono[BLK];
    if (g_tone.silent) memset(mono, 0, sizeof mono); else g_tone.process(mono, (int)size);
    (void)in;
    uint32_t t0 = hxCyc();
    g_eng.process(mono, out[0], out[1], (int)size);
    uint32_t cyc = hxCyc() - t0;
    if (m_clear) { m_n = 0; m_sum = 0; m_peak = 0; m_over = 0; m_clear = false; }
    m_n = m_n + 1; m_sum = m_sum + cyc; if (cyc > m_peak) m_peak = cyc; if (cyc > g_budget) m_over = m_over + 1;
    g_meter.OnBlockEnd();
    uint32_t tot = hxCyc() - c0; w_sum += tot; if (tot > w_peak) w_peak = tot;
    if (++w_n >= 100) { if (w_sum / 100u > g_budget / 100u * 95u) { g_tripAvg = w_sum / 100u; g_tripPeak = w_peak; g_trips = g_trips + 1; g_trip = true; } w_sum = w_n = w_peak = 0; }
}

// ---------------------------------------------------------------- commands (ISR context, flags only)
static volatile bool g_run = false, g_dfu = false; static volatile int g_toneCmd = -1;
static void onLine(const char* l) {
    if (strstr(l, "dfu")) g_dfu = true;
    else if (!strncmp(l, "run", 3)) g_run = true;
    else if (!strncmp(l, "tone", 4)) g_toneCmd = (l[4] == ' ' && l[5] == '0') ? 0 : 1;
}
extern "C" bool hyper_dfu_requested(void) { return g_dfu; }

// ---------------------------------------------------------------- cases
struct Pre { const char* n; float v; };
struct StressDef { const char* id; Pre p[7]; };
static const StressDef kStress[] = {
    { "harm",   { {"MIX",1}, {"INTVL",11}, {"VOICES",1}, {nullptr,0} } },
    { "looper", { {"TRANSP",7}, {nullptr,0} } },                                  // + 4 layers playing, 10 s loops (see setupLooper)
    { "reverb", { {"MIX",1}, {"DECAY",0.95f}, {"SIZE",2}, {"MODDEPTH",1}, {"WARP",1}, {nullptr,0} } },
    { "grain",  { {"MIX",1}, {"DENS",2000}, {"GSIZE2",500}, {"GSIZE1",200}, {nullptr,0} } },
    { "delay",  { {"MIX",1}, {"FB",0.9f}, {"STOCH",1}, {nullptr,0} } },
    { "flange", { {"MIX",1}, {"RATE",5}, {"DEPTH",10}, {nullptr,0} } },
    { "chorus", { {"MIX",1}, {"DEPTH",1}, {"FB",0.5f}, {nullptr,0} } },
    { "mood",   { {"MIX",1}, {"MOOD",1}, {"AUTO",1}, {"RATE",30}, {nullptr,0} } },
    { "freeze", { {"MIX",1}, {"FREEZE",1}, {"FRAME",2}, {"SMEAR",1}, {"PHASE",1}, {nullptr,0} } },
    { "blur",   { {"MIX",1}, {"BLUR",1}, {"FRAME",2}, {"FSMEAR",1}, {"PHASE",1}, {nullptr,0} } },
    { "drone",  { {"MIX",1}, {"PARTIALS",16}, {"DETUNE",50}, {nullptr,0} } },
    { "roar",   { {"MIX",1}, {"DRIVE",1}, {"TYPE",3}, {"FOLD",1}, {nullptr,0} } },
    { "ring",   { {"MIX",1}, {"SHAPE",2}, {"DRIVE",12}, {"TRACK",1}, {nullptr,0} } },
    { "wah",    { {"MIX",1}, {"MODE",2}, {"Q",12}, {"ENVAMT",1}, {"AUTOQ",1}, {nullptr,0} } },
};
static const Pre* stressFor(const char* id) { for (auto& s : kStress) if (!strcmp(s.id, id)) return s.p; return nullptr; }

struct Rec {
    char label[28]; uint8_t nfx; char fxid[4][8]; uint8_t var;   // var 0 = defaults, 1 = stress
    uint32_t n, over, avgc, peakc; uint8_t trip; float m_avg, m_max, slotPct[4]; uint8_t ok; char note[40]; float mapUs;
};
static const int kMaxRec = 48;
static Rec g_rec[kMaxRec]; static int g_nrec = 0;
static const char* g_state = "boot"; static const char* g_curLabel = "";
static int g_ncase = 0;
static volatile int g_icase = 0;
static uint32_t g_tStart = 0;

static void sendHello() {
    g_out.begin();
    g_out.s("{\"t\":\"hello\",\"fw\":\""); g_out.s(FW_VERSION); g_out.s("\",\"clk\":"); g_out.u(g_hxClkMHz * 1000000u);
    g_out.s(",\"sr\":48000,\"blk\":"); g_out.u(BLK); g_out.s(",\"budget_cyc\":"); g_out.u(g_budget);
    g_out.s(",\"src\":\""); g_out.s(g_tone.silent ? "silence" : "tone"); g_out.s("\",\"loop_s\":"); g_out.u(HX_LOOP_SECONDS);
    g_out.s(",\"state\":\""); g_out.s(g_state); g_out.s("\",\"case\":"); g_out.u(g_icase); g_out.s(",\"ncase\":"); g_out.u(g_ncase);
    g_out.s(",\"cur\":\""); g_out.s(g_curLabel); g_out.s("\",\"nrec\":"); g_out.u(g_nrec); g_out.s(",\"ms\":"); g_out.u(System::GetNow() - g_tStart);
    g_out.s(",\"underrun\":"); g_out.u(g_underrun); g_out.s(",\"trips\":"); g_out.u(g_trips); hxFaultJson(g_out); g_out.s("}"); g_out.end(hw);
}
static void sendRec(int i) {
    const Rec& r = g_rec[i]; float bud = (float)g_budget;
    g_out.begin();
    g_out.s("{\"t\":\"r\",\"i\":"); g_out.u(i); g_out.s(",\"label\":"); g_out.str(r.label);
    g_out.s(",\"fx\":["); for (int k = 0; k < r.nfx; k++) { if (k) g_out.c(','); g_out.str(r.fxid[k]); }
    g_out.s("],\"var\":"); g_out.u(r.var); g_out.s(",\"trip\":"); g_out.u(r.trip);
    g_out.s(",\"avg\":"); g_out.f(100.f * r.avgc / bud); g_out.s(",\"peak\":"); g_out.f(100.f * r.peakc / bud);
    g_out.s(",\"avgc\":"); g_out.u(r.avgc); g_out.s(",\"peakc\":"); g_out.u(r.peakc);
    g_out.s(",\"m_avg\":"); g_out.f(100.f * r.m_avg); g_out.s(",\"m_max\":"); g_out.f(100.f * r.m_max);
    g_out.s(",\"n\":"); g_out.u(r.n); g_out.s(",\"over\":"); g_out.u(r.over);
    g_out.s(",\"slot\":["); for (int k = 0; k < 4; k++) { if (k) g_out.c(','); g_out.f(r.slotPct[k]); }
    g_out.s("],\"map_us\":"); g_out.f(r.mapUs); g_out.s(",\"ok\":"); g_out.u(r.ok); g_out.s(",\"note\":"); g_out.str(r.note); g_out.s("}");
    g_out.end(hw, 25);
}

// ---------------------------------------------------------------- service: USB chatter + mapper tick; call from every wait
static bool g_mapOn = false; static uint32_t g_mapNext = 0, g_mapCyc = 0, g_mapN = 0, g_mapMax = 0;
static float g_mapT = 0.f;
static void mapperTick() {
    uint32_t now = System::GetNow();
    if ((int32_t)(now - g_mapNext) < 0) return;
    g_mapNext = now + 5;
    g_mapT += 0.005f;
    hx::HxControls c;   // synthetic hands: valves cycle, faders/pots/lidar slow triangles, fsr pulses, joystick circles
    float t = g_mapT;
    for (int k = 0; k < 3; k++) c.valve[k] = (fmodf(t * (0.7f + 0.3f * k), 1.f) > 0.5f) ? 1.f : 0.f;
    for (int k = 0; k < 3; k++) c.fader[k] = 0.5f + 0.5f * sinf(t * (0.3f + 0.1f * k));
    c.pot[0] = 0.5f + 0.5f * sinf(t * 0.25f); c.pot[1] = 0.5f + 0.5f * sinf(t * 0.4f + 1.f);
    c.fsr = 0.5f + 0.5f * sinf(t * 1.1f); c.joyX = 0.8f * sinf(t * 0.8f); c.joyY = 0.8f * cosf(t * 0.8f);
    c.tiltX = 0.5f * sinf(t * 0.5f); c.tiltY = 0.5f * cosf(t * 0.6f); c.lidar = 0.5f + 0.5f * sinf(t * 0.9f);
    uint32_t a = hxCyc(); g_map.update(c, 0.005f); uint32_t d = hxCyc() - a;
    g_mapCyc += d; g_mapN++; if (d > g_mapMax) g_mapMax = d;
}
static uint32_t g_nextHello = 0, g_nextRep = 0; static int g_rep = 0;
static void service() {
    g_hxFaultCtx = (uint32_t)g_icase;
    if (g_dfu) { g_out.begin(); g_out.s("{\"t\":\"dfu\"}"); g_out.end(hw); System::Delay(100); dfuCheck(); }
    if (g_toneCmd >= 0) { g_tone.silent = (g_toneCmd == 0); g_toneCmd = -1; }
    if (g_mapOn) mapperTick();
    uint32_t now = System::GetNow();
    hw.SetLed(((now / 500) & 1) != 0);
    if ((int32_t)(now - g_nextHello) >= 0) { g_nextHello = now + 1000; sendHello(); return; }
    if (g_nrec > 0 && (int32_t)(now - g_nextRep) >= 0) { g_nextRep = now + 40; if (g_rep >= g_nrec) g_rep = 0; sendRec(g_rep++); }
}
static void waitMs(uint32_t ms) { uint32_t t0 = System::GetNow(); while (!g_trip && System::GetNow() - t0 < ms) { service(); System::Delay(1); } }

// ---------------------------------------------------------------- case plumbing
static void clearSlots() { for (int s = 0; s < HX_NSLOTS; s++) { g_eng.setBypass(s, false); g_eng.setEffect(s, -1); } }
static void applyPreset(int slot, const Pre* p) {
    hx::Fx* f = g_eng.fx(slot); if (!f || !p) return;
    for (; p->n; p++) { int i = f->findParam(p->n); if (i >= 0) f->setParam(i, p->v); }
    f->snap();
}
static bool setupLooper(int slot, int loopMs, bool stress) {
    hx::FxLooper* L = static_cast<hx::FxLooper*>(g_eng.fx(slot)); if (!L || !L->ok) return false;
    L->action(hx::A_MAIN, 0); waitMs(loopMs); L->action(hx::A_MAIN, 0);          // record layer 1 for loopMs, then play
    for (int k = 1; k < 4; k++) { waitMs(150); L->action(hx::A_MAIN, 0); waitMs(loopMs + 300); }   // 3 overdubs, each punches out after one loop
    if (stress) applyPreset(slot, stressFor("looper"));
    return L->layers() == 4 && L->state() == hx::FxLooper::S_PLAY;
}

// run one measurement window on whatever is currently loaded in the engine
static void measure(const char* label, uint8_t var, int nfx, const char* const* ids, bool slotsOn, bool note_ok, const char* note) {
    g_curLabel = label;
    waitMs(1000);                                   // warm-up: caches, SDRAM, tracker, smoothing
    g_meter.Reset(); m_clear = true; g_mapCyc = g_mapN = g_mapMax = 0;
    if (!g_trip) waitMs(3000);                      // window = ~3000 blocks (cut short by the overload guard)
    Rec& r = g_rec[g_nrec];
    memset(&r, 0, sizeof r);
    strncpy(r.label, label, sizeof r.label - 1); r.nfx = (uint8_t)nfx; r.var = var;
    for (int k = 0; k < nfx && k < 4; k++) strncpy(r.fxid[k], ids[k], 7);
    uint32_t n = m_n; r.n = n; r.over = m_over; r.peakc = m_peak; r.avgc = n ? (uint32_t)(m_sum / n) : 0;
    r.m_avg = g_meter.GetAvgCpuLoad(); r.m_max = g_meter.GetMaxCpuLoad();
    float blockNs = 1e9f * BLK / SR;
    if (slotsOn) for (int k = 0; k < HX_NSLOTS; k++) r.slotPct[k] = 100.f * g_eng.slotNs(k) / blockNs;
    r.ok = note_ok; strncpy(r.note, note, sizeof r.note - 1);
    r.mapUs = g_mapN ? (float)g_mapCyc / g_mapN / (float)g_hxClkMHz : 0.f;
    if (g_trip) {                                   // OVER: >95 % avg over 100 consecutive blocks. Record the tripping window, mute stays until the slots are cleared.
        r.trip = 1; r.ok = 0; r.avgc = g_tripAvg; r.peakc = g_tripPeak; r.over = 100; r.n = 0;
        strncpy(r.note, "OVER: >95% avg, 100 blocks, aborted", sizeof r.note - 1);
        clearSlots(); g_mapOn = false; g_trip = false;
    }
    int idx = g_nrec; if (g_nrec < kMaxRec - 1) g_nrec++;
    g_icase++;
    sendRec(idx);                                   // emit immediately; the round-robin replay keeps repeating it
}

static void runAll() {
    g_nrec = 0; g_icase = 0; g_rep = 0; g_state = "running"; g_mapOn = false; g_tStart = System::GetNow();
    const int nT = hx::fxTypeCount();
    g_ncase = 1 + nT * 2 + 3;
    clearSlots();
    { const char* none[1] = {""}; measure("none (tone+tracker only)", 0, 0, none, false, true, "baseline"); }
    for (int t = 0; t < nT; t++) {
        const char* id = hx::fxType(t)->id;
        for (int var = 0; var < 2; var++) {
            clearSlots();
            char lab[28]; snprintf(lab, sizeof lab, "%s %s", id, var ? "stress" : "def");
            g_curLabel = lab; bool ok = g_eng.setEffect(0, t); bool good = ok; char note[40] = "";
            if (!ok) snprintf(note, sizeof note, "setEffect failed");
            if (var && !strcmp(id, "looper") == false) applyPreset(0, stressFor(id));
            if (!strcmp(id, "looper")) {
                good = setupLooper(0, var ? 10000 : 2000, var != 0);
                snprintf(note, sizeof note, "%s 4 layers x %d s playing%s", good ? "ok" : "NOT 4 layers", var ? 10 : 2, var ? ", +7st" : "");
            } else if (ok) snprintf(note, sizeof note, var ? "stress preset" : "defaults");
            const char* ids[1] = { id };
            measure(lab, (uint8_t)var, 1, ids, false, good, note);
        }
    }
    // worst 4 (by max avg of the two variants, one entry per effect, excluding looper unless it ranks)
    int best[4] = {-1,-1,-1,-1}; float bv[4] = {-1,-1,-1,-1};
    for (int t = 0; t < nT; t++) {
        float v = 0; for (int i = 0; i < g_nrec; i++) if (g_rec[i].nfx == 1 && !strcmp(g_rec[i].fxid[0], hx::fxType(t)->id)) if ((float)g_rec[i].avgc > v) v = (float)g_rec[i].avgc;
        for (int k = 0; k < 4; k++) if (v > bv[k]) { for (int m = 3; m > k; m--) { bv[m] = bv[m-1]; best[m] = best[m-1]; } bv[k] = v; best[k] = t; break; }
    }
    // default 4-slot patch = bank A "CORE": harm, grain, reverb, delay
    { clearSlots(); static const char* d4[4] = { "harm", "grain", "reverb", "delay" };
      for (int s = 0; s < 4; s++) g_eng.setEffect(s, hx::fxTypeById(d4[s]));
      g_curLabel = "default 4-slot"; measure("default 4-slot", 0, 4, d4, true, true, "harm grain reverb delay, defaults"); }
    // same, driven by the mapper (loadDefaultPatch via init) with synthetic controls, update every 5 ms in the main loop
    { clearSlots(); g_map.init(&g_eng); g_mapOn = true; g_mapNext = System::GetNow(); static const char* d4[4] = { "harm", "grain", "reverb", "delay" };
      measure("default 4-slot + mapper", 0, 4, d4, true, true, "mapper.update 5 ms; map_us = avg update time"); g_mapOn = false; }
    { clearSlots(); static char ids[4][8]; static const char* p[4]; bool good = true;
      for (int s = 0; s < 4; s++) { int t = best[s]; strncpy(ids[s], hx::fxType(t)->id, 7); p[s] = ids[s]; g_eng.setEffect(s, t); applyPreset(s, stressFor(ids[s])); if (!strcmp(ids[s], "looper")) good = good && setupLooper(s, 1500, false); }
      measure("worst 4 (stress)", 1, 4, p, true, good, "top-4 single-effect avg, stress presets"); }
    clearSlots(); g_state = "done"; g_curLabel = "";
}

int main(void) {
    hw.Init(BENCH_BOOST != 0);                    // 480 MHz when boosted (libDaisy default is 400)
    dfuBootGesture();
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);   // USB first (PMOR rule)
    g_hxLineFn = onLine;
    hw.usb_handle.SetReceiveCallback(hxRxCb, UsbHandle::FS_INTERNAL);
    System::Delay(500);
    hxDwtInit();
    hxFaultBoot();                                // report a fault from the previous run (hello "fault")
    g_budget = (uint32_t)((uint64_t)BLK * g_hxClkMHz * 1000000ull / 48000ull);
    hx::g_nowNs = hxNowNs;
    hxSdramInit(g_eng, SR);
    g_tone.init(SR);
    g_meter.Init(SR, BLK);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(BLK);
    hw.StartAudio(audioCb);
    for (;;) {
        runAll();
        g_run = false;
        while (!g_run) { service(); System::Delay(1); }
    }
}
