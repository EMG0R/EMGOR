// test_parity.cpp -- the SAME performance rendered two ways:
//   A  firmware path: pmor_core as hw/ runs it (sequencer + generative lanes + sensors), its MIDI produced exactly like hw/studio.cpp (events -> notes, 7-bit sensor CCs,
//      fxMidiPoll for FX engage / amount / per-bank params) plus the ch6 perform-state CCs of FIRMWARE_TODO.md F1 (tilt, octave, env macro, throws), emulated here.
//   B  Live path: a fresh core in g_render_notes_only mode fed ONLY that MIDI + the schema parameters, through max/pmor_hostctl.h (the code pmor.core~ runs).
// The MIDI of block k is applied before B renders block k (an offline oracle: it compares the sound the stream describes, not the 1-block Live latency).
// Residual = 20 log10(rms(A-B)/rms(A)). Every scene also runs with the ch6 CCs withheld: scenes that depend on them must then be clearly worse (the test is sensitive).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <functional>
#include "config.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "fx_midi.h"
#include "output.h"
#include "metro.h"
#include "params.h"
#include "render.h"
#include "events.h"
#include "../max/pmor_hostctl.h"

static int passes = 0, fails = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Msg { uint8_t st, d1, d2; };
struct Run { std::vector<float> L, R; std::vector<std::vector<Msg>> msgs; };

static void boot() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit(); pmorSchemaInit();
    memset(smp_buf, 0, sizeof(smp_buf));
    pmorReseed(0); PmorEvent ev; while (pmorEventPop(&ev)) {}   // random streams are file statics: every run starts from the original seeds, as a fresh process would
}
static void fillSample(int slot, uint32_t len) {
    uint32_t s = 0xBADC0DEu + (uint32_t)slot;
    for (uint32_t i = 0; i < len; i++) {
        float env = expf(-(float)i / (0.25f * SR)); s = s * 1664525u + 1013904223u;
        smp_buf[slot][i] = env * (0.6f * sinf(6.2831853f * 220.f * (float)i / SR) + 0.2f * ((float)(s >> 8) * (1.f / 16777216.f) * 2.f - 1.f));
    }
    smp.len[slot] = len;
}
static void setSeq(int e, int mode, float density, float prob, float poly, float octAmt, float octProb) {
    LaneSet& l = seq.lane[e]; l.mode = (uint8_t)mode; l.mode_f = (float)mode; l.density = density; l.probability = prob; l.polyphony = poly; l.oct_amount = octAmt; l.oct_prob = octProb;
}

// ---- A: the MIDI hw/studio.cpp would send for this block
static int oct14() { float o = fmaxf(-2.f, fminf(2.f, syn.perf_octave)); return (int)(8192.f + o * (8191.f / 2.f) + 0.5f); }
struct FwOut {
    size_t evCount = 0, evStart = 0;   // messages [0, evCount) of the last poll() are note/seed events
    uint8_t last[8] = {0}, last6[48] = {0}; bool have6[48] = {false}; int last14 = 8192; uint8_t lastBend = 0; uint16_t lastBendV = 8192; bool held[128] = {false}; bool seeded = false;
    static uint8_t q(float v) { int x = (int)(v * 127.f + 0.5f); return (uint8_t)(x < 0 ? 0 : x > 127 ? 127 : x); }
    void poll(uint32_t block, bool ch6, std::vector<Msg>& out) {
        PmorEvent e; evStart = out.size();
        while (pmorEventPop(&e)) {
            switch (e.kind) {
                case EV_DRUM_ON: out.push_back({0x90 | 0, (uint8_t)(36 + e.a), e.b}); break;
                case EV_SMP_ON:  out.push_back({0x90 | 2, (uint8_t)(36 + e.a), e.b}); break;
                case EV_SYN_ON:
                    if (e.c != lastBendV) { out.push_back({0xE0 | 1, (uint8_t)(e.c & 127), (uint8_t)(e.c >> 7)}); lastBendV = e.c; }
                    if (held[e.a & 127]) out.push_back({0x80 | 1, e.a, 0});
                    out.push_back({0x90 | 1, e.a, e.b}); held[e.a & 127] = true; break;
                case EV_SYN_OFF: if (held[e.a & 127]) { out.push_back({0x80 | 1, e.a, 0}); held[e.a & 127] = false; } break;
                case EV_SEED:
                    out.push_back({0xB3, 110, (uint8_t)((e.seed >> 21) & 127)}); out.push_back({0xB3, 111, (uint8_t)((e.seed >> 14) & 127)});
                    out.push_back({0xB3, 112, (uint8_t)((e.seed >> 7) & 127)}); out.push_back({0xB3, 113, (uint8_t)(e.seed & 127)}); break;
            }
        }
        evCount = out.size() - evStart;
        static std::vector<Msg>* sink; sink = &out;
        fxMidiPoll(block * BLOCK * 1000u / (uint32_t)SR, [](uint8_t ch, uint8_t cc, uint8_t v) { sink->push_back({(uint8_t)(0xB0 | ch), cc, v}); });
        auto qb = [](float v) { return q(0.5f + 0.5f * v); };
        uint8_t cur[7] = { q(g.fsr), q(g.lidar), qb(g.accel_x), qb(g.accel_y), qb(g.joy_x), qb(g.joy_y), q(outp.fader * 0.75f) };
        for (int i = 0; i < 7; i++) {
            int d = (int)cur[i] - (int)last[i];
            if (d >= 2 || d <= -2 || (d != 0 && (cur[i] == 0 || cur[i] == 127))) { last[i] = cur[i]; out.push_back({0xB3, (uint8_t)(1 + i), cur[i]}); }
        }
        if (!ch6) return;
        // FIRMWARE_TODO.md F1: ch6 CC2 tilt, CC3/CC35 octave (14 bit), CC4 env macro, CC8..11 throws, CC12 engine, sent on change (deadband 2, exact at the ends)
        struct { uint8_t cc, v; } c6[] = { {2, q(fx.tilt_t)}, {4, q(syn.perf_env_macro)}, {8, q(fx.perf_up)}, {9, q(fx.perf_right)}, {10, q(fx.perf_down)}, {11, q(fx.perf_left)},
                                          {12, (uint8_t)(fx.perf_engine < ENG_COUNT ? fx.perf_engine : 127)} };
        { int o = oct14(); if (o != last14) { last14 = o; out.push_back({0xB5, 3, (uint8_t)(o >> 7)}); out.push_back({0xB5, 35, (uint8_t)(o & 127)}); } }   // 14 bit: any change
        for (auto& c : c6) {
            int d = (int)c.v - (int)last6[c.cc]; bool first = !have6[c.cc];
            if (first ? c.v != (c.cc == 12 ? 127 : 0) && !(c.cc == 2 && c.v == 127) && !(c.cc == 4 && c.v == q(0.5f)) : (d >= 2 || d <= -2 || (d != 0 && (c.v == 0 || c.v == 127)))) {
                last6[c.cc] = c.v; have6[c.cc] = true; out.push_back({0xB5, c.cc, c.v});
            } else if (first) { last6[c.cc] = c.v; have6[c.cc] = true; }
        }
    }
};

struct Scene {
    const char* name;
    double seconds;
    std::function<void()> setup;                 // core state before the run (A); B receives it as schema params
    std::function<void(int block)> drive;        // A only: stands in for the user / nav
    double maxResidualDb;                        // metric (see below) with all MIDI must be this good
    int  needs;                                  // 0 none; 1 = withholding the F1 perform CCs (ch6 CC2/3/35/4/8..12) must clearly hurt; 2 = withholding every ch6 CC must
    int metric = 1;
    bool known = false;                          // KNOWN GAP (needs firmware, see FIRMWARE_TODO.md): reported, not asserted                              // 0 = sample-exact residual, 1 = envelope, 2 = envelope + synth note onsets (worst of the two)
};

static Run runA(const Scene& sc, bool ch6, std::vector<std::pair<uint16_t, float>>& params) {
    boot(); g_render_notes_only = false; fxMidiReset();
    sc.setup();
    params.clear(); for (size_t i = 0; i < pmorSchemaCount(); i++) { float v; if (pmorParamGetAt(i, &v)) params.push_back({pmorSchemaAt(i)->id, v}); }
    Run r; FwOut fo; int nb = (int)(sc.seconds * SR / BLOCK); float in0[BLOCK] = {0}, L[BLOCK], R[BLOCK]; std::vector<Msg> carry;
    for (int b = 0; b < nb; b++) {
        if (b == 0) { std::vector<Msg> sink; fo.poll(0, ch6, sink); }   // arms fxMidiPoll (first call = baseline) and consumes boot events
        sc.drive(b);
        const bool stut = fxStutterActive();
        pmorRenderBlock(in0, L, R, BLOCK);
        r.L.insert(r.L.end(), L, L + BLOCK); r.R.insert(r.R.end(), R, R + BLOCK);
        r.msgs.emplace_back(); r.msgs.back() = carry; carry.clear(); fo.poll((uint32_t)b + 1, ch6, r.msgs.back());
        // stutter retriggers are fired by the FX timer AFTER the engines rendered this block, so the hit sounds from the next block on: the oracle moves those events there
        if (stut && fo.evCount) { std::vector<Msg>& m = r.msgs.back(); carry.assign(m.begin() + (long)fo.evStart, m.begin() + (long)(fo.evStart + fo.evCount)); m.erase(m.begin() + (long)fo.evStart, m.begin() + (long)(fo.evStart + fo.evCount)); }
    }
    return r;
}
// envelope residual: per-512-sample rms of both channels (insensitive to the sub-cent phase drift 14-bit pitch bend leaves on long notes)
static double envResidualDb(const Run& a, const Run& b) {
    const size_t W = 512; double num = 0, den = 0;
    for (size_t i = 0; i + W <= a.L.size(); i += W) {
        double ea = 0, eb = 0; for (size_t k = i; k < i + W; k++) { ea += (double)a.L[k] * a.L[k] + (double)a.R[k] * a.R[k]; eb += (double)b.L[k] * b.L[k] + (double)b.R[k] * b.R[k]; }
        ea = sqrt(ea / (2.0 * W)); eb = sqrt(eb / (2.0 * W)); num += (ea - eb) * (ea - eb); den += ea * ea;
    }
    return den < 1e-12 ? 999 : (num < 1e-18 ? -240 : 10.0 * log10(num / den));
}
// onset residual: the first 4096 samples (85 ms) of every synth note-on, sample-exact (pitch errors show at once, drift has no time to build)
static double onsetResidualDb(const Run& a, const Run& b) {
    double num = 0, den = 0; size_t n = 0;
    for (size_t k = 0; k < a.msgs.size(); k++) { bool on = false; for (auto& m : a.msgs[k]) if (m.st == 0x91 && m.d2 > 0) on = true; if (!on) continue;
        for (size_t i = k * BLOCK; i < k * BLOCK + 4096 && i < a.L.size(); i++) { double dl = (double)a.L[i] - b.L[i], dr = (double)a.R[i] - b.R[i]; num += dl * dl + dr * dr; den += (double)a.L[i] * a.L[i] + (double)a.R[i] * a.R[i]; n++; } }
    return (n == 0 || den < 1e-12) ? 999 : (num < 1e-18 ? -240 : 10.0 * log10(num / den));
}
void parityFillSamples() { fillSample(0, 48000); fillSample(1, 48000); }
static Run runB(const Scene& sc, const Run& a, const std::vector<std::pair<uint16_t, float>>& params) {
    boot(); g_render_notes_only = true; fxMidiReset(); pmorhc::State hs;
    for (auto& p : params) pmorhc::apply(hs, { pmorhc::HE_PARAM, p.first, p.second });          // what P pushes / the snapshot deliver
    Run r; int nb = (int)a.msgs.size(); float in0[BLOCK] = {0}, L[BLOCK], R[BLOCK]; uint32_t seedp[4] = {0, 0, 0, 0};
    parityFillSamples();                                                                          // a core `slot` message loads the same audio on the Mac
    for (int b = 0; b < nb; b++) {
        for (const Msg& m : a.msgs[b]) {                      // everything A's block b produced (notes, CCs) is applied before B renders block b
            pmorhc::Ev e;
            if (pmorhc::decode(m.st, m.d1, m.d2, e)) pmorhc::apply(hs, e);
            else if ((m.st & 0xF0) == 0xB0 && (m.st & 0x0F) == 3 && m.d1 >= 110 && m.d1 <= 113) { seedp[m.d1 - 110] = m.d2; if (m.d1 == 113) pmorReseed(((seedp[0] << 21) | (seedp[1] << 14) | (seedp[2] << 7) | seedp[3]) & 0x0FFFFFFFu); }
        }
        metro.level = 0.f; pmorRenderBlock(in0, L, R, BLOCK);
        r.L.insert(r.L.end(), L, L + BLOCK); r.R.insert(r.R.end(), R, R + BLOCK);
    }
    return r;
}
// pmor_core keeps file-static state (synth RNG / slot counter, FX statics) that no init resets, so every render runs in its own forked process,
// exactly like a fresh firmware boot and a fresh Max process.
template <class F> static Run inChild(F f, std::vector<std::pair<uint16_t, float>>& params) {
    char path[] = "/tmp/pmor_parity_XXXXXX"; int fd = mkstemp(path); close(fd);
    pid_t pid = fork();
    if (pid == 0) {
        Run r = f(params); FILE* o = fopen(path, "wb");
        auto w = [&](const void* p, size_t n) { fwrite(p, 1, n, o); };
        size_t n = r.L.size(); w(&n, sizeof n); w(r.L.data(), n * 4); w(r.R.data(), n * 4);
        size_t nm = r.msgs.size(); w(&nm, sizeof nm); for (auto& v : r.msgs) { size_t k = v.size(); w(&k, sizeof k); w(v.data(), k * sizeof(Msg)); }
        size_t np = params.size(); w(&np, sizeof np); w(params.data(), np * sizeof(params[0]));
        fclose(o); _exit(0);
    }
    int st = 0; waitpid(pid, &st, 0);
    Run r; FILE* in = fopen(path, "rb"); if (!in) { unlink(path); return r; }
    auto rd = [&](void* p, size_t n) { if (fread(p, 1, n, in) != n) memset(p, 0, n); };
    size_t n; rd(&n, sizeof n); r.L.resize(n); r.R.resize(n); rd(r.L.data(), n * 4); rd(r.R.data(), n * 4);
    size_t nm; rd(&nm, sizeof nm); r.msgs.resize(nm); for (auto& v : r.msgs) { size_t k; rd(&k, sizeof k); v.resize(k); rd(v.data(), k * sizeof(Msg)); }
    size_t np; rd(&np, sizeof np); params.resize(np); rd(params.data(), np * sizeof(params[0]));
    fclose(in); unlink(path); return r;
}
static double rms(const std::vector<float>& a, const std::vector<float>& b, bool diff) { double s = 0; for (size_t i = 0; i < a.size(); i++) { double d = diff ? (double)a[i] - b[i] : a[i]; s += d * d; } return sqrt(s / (double)a.size()); }
static double residualDb(const Run& a, const Run& b) {
    double ra = (rms(a.L, a.L, false) + rms(a.R, a.R, false)) * .5, rd = (rms(a.L, b.L, true) + rms(a.R, b.R, true)) * .5;
    if (ra < 1e-6) return 999; if (rd < 1e-12) return -240; return 20.0 * log10(rd / ra);
}

static void scenes(std::vector<Scene>& S) {
    // 0. baselines: one thing at a time, so a regression names its cause
    S.push_back({"baseline: drum sequence only", 2.0, [=] { setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 4, 8, 12}) seq.lane[ENG_DRUMS].step[s] = 0x1; }, [](int) {}, -60.0, 0, 0});
    S.push_back({"baseline: synth played by hand (pad 5 then 9)", 2.0, [=] { syn.algo = 1; }, [](int b) { if (b == 10) synthNoteOn(5, 0.8f); if (b == 300) synthNoteOff(5); if (b == 320) synthNoteOn(9, 0.8f); if (b == 600) synthNoteOff(9); }, -30.0, 0, 2});

    // polyphony 1: the MIDI note stream cannot tell two voices on the same note apart (off-before-on retrigger, PROTOCOL section 7), so chords with duplicate notes are a protocol limit, not a host one (FIRMWARE_TODO F6)
    auto synBase = [] { syn.algo = 4; setSeq(ENG_SYNTH, 2, 0.6f, 0.9f, 1.f, 0.f, 0.f); g.pads_held = 0x11; };
    // 1. generative synth with octave offsets up to 4 octaves: the pad lookup of the old host clamped everything above 2 octaves + 2 degrees
    S.push_back({"synth generative, oct_amount 4 (octaves above the 16-pad range)", 4.0, [=] { synBase(); seq.lane[ENG_SYNTH].oct_amount = 4.f; seq.lane[ENG_SYNTH].oct_prob = 0.9f; }, [](int) {}, -25.0, 0, 2});
    // 2. stick-Y octave sweep on the synth PERF card (firmware syn.perf_octave, not in the note)
    S.push_back({"synth stick-Y octave sweep (perf_octave)", 4.0, [=] { synBase(); }, [](int b) { syn.perf_octave = 2.f * sinf(b * 0.004f); }, -25.0, 1, 2});
    // 3. accel-X envelope macro sweep
    S.push_back({"synth accel-X envelope macro, plucky half (0..0.5)", 4.0, [=] { synBase(); }, [](int b) { syn.perf_env_macro = 0.25f + 0.25f * sinf(b * 0.006f); }, -25.0, 1, 1});
    // KNOWN GAP (FIRMWARE_TODO F7): above 0.5 the swell attack of a SEQUENCED note is capped by the sequencer's budget (s_seqAtkCap, set only around synthFireStep); a note arriving over MIDI
    // cannot be told from a hand-played one, so it gets the uncapped 1.5 s swell and is far quieter than on PMOR.
    { Scene k = {"synth accel-X envelope macro, ambient (1.0)  [known gap F7]", 4.0, [=] { synBase(); }, [](int) { syn.perf_env_macro = 1.0f; }, 0.0, 0, 1}; k.known = true; S.push_back(k); }
    // 4. FX engage + per-bank AMOUNT / LIDAR / MIX params and the FSR macro
    S.push_back({"FX engage + per-bank AMOUNT/LIDAR/MIX + FSR", 4.0, [=] { synBase(); setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 4, 8, 12}) seq.lane[ENG_DRUMS].step[s] = 0x1; },
         [](int b) {
             if (b == 40) fxPress(16 + 3);     // bank 1 effect
             if (b == 200) { pmorParamSet(pmorParamId(PD_FX, 130 + 1), 0.8f); pmorParamSet(pmorParamId(PD_FX, 150 + 1), 0.5f); }
             if (b == 120) g.fsr = 64.f / 127.f;
             if (b == 300) fxRelease(16 + 3);
         }, -25.0, 2, 1});
    // 5. FX-page tilt filter (accel X on the FX page)
    S.push_back({"FX tilt filter (FX page, accel X)", 4.0, [=] { synBase(); }, [](int b) { g.on_fx_page = true; g.accel_x = -0.9f + 0.5f * (1.f + sinf(b * 0.01f)) * 0.8f; }, -25.0, 1, 1});
    // 6. stutter ST8 over a drum + synth lane: PMOR's retrigger notes arrive over MIDI; the host must not retrigger them again
    S.push_back({"stutter ST8 over drums (no host double trigger)", 4.0, [=] { setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 3, 4, 8, 10, 12}) seq.lane[ENG_DRUMS].step[s] = 0x3; },
         [](int b) { if (b == 100) fxPress(1); if (b == 330) fxRelease(1); }, -30.0, 0, 1});
    // 7. drum perform throw (joystick on the drums PERF card)
    S.push_back({"drum perform throws", 4.0, [=] { setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 2, 4, 6, 8, 10, 12, 14}) seq.lane[ENG_DRUMS].step[s] = 0x11; },
         [](int b) { fx.perf_engine = ENG_DRUMS; fx.perf_up = (b > 80 && b < 300) ? 0.8f : 0.f; fx.perf_right = (b > 150 && b < 350) ? 0.6f : 0.f; fx.perf_down = (b > 200 && b < 260) ? 0.5f : 0.f; }, -25.0, 1, 1});
    // 8. sampler START/END/ATTACK/RELEASE and a sampler lane
    S.push_back({"sampler START/END/ATTACK/RELEASE", 3.0, [=] { fillSample(0, 48000); fillSample(1, 48000); setSeq(ENG_SMP, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 4, 8, 12}) seq.lane[ENG_SMP].step[s] = 0x3;
         pmorParamSet(pmorParamId(PD_SAMPLER, 512 + 0), 0.1f); pmorParamSet(pmorParamId(PD_SAMPLER, 512 + 1), 0.6f); pmorParamSet(pmorParamId(PD_SAMPLER, 512 + 3), 0.05f); pmorParamSet(pmorParamId(PD_SAMPLER, 512 + 4), 0.3f); },
         [](int) {}, -45.0, 0, 0});
    // 9. new synth LFO + drum deep knobs
    S.push_back({"synth LFO + drum deep knobs", 3.0, [=] { synBase(); setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); for (int s : {0, 4, 8, 12}) seq.lane[ENG_DRUMS].step[s] = 0x1;
         pmorParamSet(pmorParamId(PD_SYNTH, 258), 6.f); pmorParamSet(pmorParamId(PD_SYNTH, 259), 0.7f); pmorParamSet(pmorParamId(PD_DRUMS, 320 + 0), 2.f); pmorParamSet(pmorParamId(PD_DRUMS, 320 + 7), 1.5f); },
         [](int) {}, -25.0, 0, 1});
    // 10. drum synth engine: per-pad super voices, other sounds picked and a few synth params customised (domains 7..10): hits on pads 0 / 5 / 10 / 15 over both paths
    S.push_back({"drum synth: pads 0/5/10/15, sounds 3/17/9/20, custom synth params (domains 7..10)", 3.0, [=] {
         setSeq(ENG_DRUMS, 1, 1.f, 1.f, 1.f, 0.f, 0.f); uint32_t m[4] = {0x1, 0x20, 0x400, 0x8000}; for (int s = 0; s < 16; s += 2) seq.lane[ENG_DRUMS].step[s] = (uint16_t)m[(s / 2) & 3];
         pmorParamSet(pmorParamId(PD_DRUMS, 0 * 8 + 5), 3.f); pmorParamSet(pmorParamId(PD_DRUMS, 5 * 8 + 5), 17.f); pmorParamSet(pmorParamId(PD_DRUMS, 10 * 8 + 5), 9.f); pmorParamSet(pmorParamId(PD_DRUMS, 15 * 8 + 5), 20.f);
         for (int pad : {0, 5, 10, 15}) {      // customise pad's synth: every schema field of its engine that belongs to the PITCH / OUT layers moves to 1.3x default (sparse per engine: taken from the schema)
             int n = 0;
             for (size_t i = 0; i < pmorSchemaCount(); i++) { const PmorParamDesc* sp = pmorSchemaAt(i); if ((sp->id >> 10) != (unsigned)(7 + pad / 4) || (((sp->id & 1023) >> 8) != (unsigned)(pad % 4))) continue;
                 if (strstr(sp->group, ".PITCH") == nullptr && strstr(sp->group, ".OUT") == nullptr) continue;
                 float v = sp->def * 1.3f; if (v > sp->max) v = sp->max; if (v < sp->min) v = sp->min; pmorParamSet(sp->id, v); n++; }
             CHECK(n > 4, "drum synth pad %d: schema lists only %d PITCH/OUT fields", pad, n);
         } },
         [](int) {}, -40.0, 0, 0});
}
static double metricOf(int metric, const Run& a, const Run& b) {
    if (metric == 0) return residualDb(a, b);
    double e = envResidualDb(a, b); return metric == 2 ? fmax(e, onsetResidualDb(a, b)) : e;
}
int main() {
    std::vector<Scene> S; scenes(S);
    for (auto& sc : S) {
        std::vector<std::pair<uint16_t, float>> params;
        Run a = inChild([&](std::vector<std::pair<uint16_t, float>>& p) { return runA(sc, true, p); }, params);
        Run b = inChild([&](std::vector<std::pair<uint16_t, float>>& p) { return runB(sc, a, p); }, params);
        double ra = (rms(a.L, a.L, false) + rms(a.R, a.R, false)) * .5, m1 = metricOf(sc.metric, a, b);
        printf("  %-72s A rms %.4f  %s %7.1f dB (limit %.0f)\n", sc.name, ra, sc.metric == 0 ? "exact" : sc.metric == 1 ? "envelope" : "onset/env", m1, sc.maxResidualDb);
        if (getenv("PARITY_DBG")) for (size_t k = 0; k < a.msgs.size(); k++) { for (auto& m : a.msgs[k]) if ((m.st & 0xF0) == 0x90 && (m.st & 0xF) == 1 || (m.st&0xF0)==0x80 && (m.st&0xF)==1) { double num = 0, den = 0; for (size_t i = k * BLOCK; i < k * BLOCK + 2048 && i < a.L.size(); i++) { double dl = (double)a.L[i] - b.L[i]; num += dl * dl; den += (double)a.L[i] * a.L[i]; }
            printf("      blk %zu %s note %d  resid %.1f dB\n", k, (m.st & 0xF0) == 0x90 ? "on " : "off", m.d1, den > 0 ? 10 * log10(num / den + 1e-12) : 0.0); } }
        if (getenv("PARITY_DBG")) { int c6 = 0, c4 = 0; for (auto& v : a.msgs) for (auto& m : v) { if (m.st == 0xB5) c6++; if (m.st == 0xB3) c4++; } printf("      ch6 CCs %d, ch4 CCs %d\n", c6, c4); }
        if (getenv("PARITY_FIRST")) { size_t f = 0; for (size_t i = 0; i < a.L.size(); i++) if (fabs((double)a.L[i] - b.L[i]) > 1e-6) { f = i; break; } printf("      first diff block %zu; msgs there:", f / BLOCK); for (size_t k = (f / BLOCK > 3 ? f / BLOCK - 3 : 0); k <= f / BLOCK + 1 && k < a.msgs.size(); k++) for (auto& m : a.msgs[k]) printf(" [b%zu %02X %d %d]", k, m.st, m.d1, m.d2); printf("\n"); for (size_t i = f; i < f + 300 && i < a.L.size(); i += 40) printf("        %zu A %.5f B %.5f\n", i, a.L[i], b.L[i]); }
        if (getenv("PARITY_ENV")) for (size_t i = 0; i + 8192 <= a.L.size(); i += 8192) { double ea = 0, eb = 0; for (size_t k = i; k < i + 8192; k++) { ea += (double)a.L[k] * a.L[k]; eb += (double)b.L[k] * b.L[k]; } printf("      t %.2f  A %.5f  B %.5f\n", i / 48000.0, sqrt(ea / 8192), sqrt(eb / 8192)); }
        CHECK(ra > 0.003, "%s: reference render is silent (rms %.6f)", sc.name, ra);
        if (sc.known) { printf("      KNOWN GAP (not asserted)\n"); continue; }
        CHECK(m1 <= sc.maxResidualDb, "%s: %.1f dB > %.0f dB", sc.name, m1, sc.maxResidualDb);
        if (sc.needs) {
            Run af = a;   // same reference, but the host never receives the withheld CCs
            for (auto& v : af.msgs) { std::vector<Msg> k; for (auto& m : v) { bool f1 = m.st == 0xB5 && (m.d1 == 2 || m.d1 == 3 || m.d1 == 35 || m.d1 == 4 || (m.d1 >= 8 && m.d1 <= 12)); if (!(sc.needs == 2 ? m.st == 0xB5 : f1)) k.push_back(m); } v = k; }
            Run b0 = inChild([&](std::vector<std::pair<uint16_t, float>>& p) { return runB(sc, af, p); }, params);
            double m0 = metricOf(sc.metric, a, b0);
            printf("      host without those CCs: %7.1f dB\n", m0);
            CHECK(m0 - m1 >= 10.0 || m0 > -12.0, "%s: withholding the CCs changed nothing (%.1f vs %.1f dB): the scene does not exercise them", sc.name, m0, m1);
        }
    }
    printf("test_parity: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
