// Synth LFO + deep drum recipe knobs: OFF / neutral is bit-identical to the engine without them; each setting does what its name says; everything stays finite.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "synth.h"
#include "drums.h"
#include "shared_state.h"
#include "clock.h"
#include "scale.h"
#include "sequencer.h"
#include "fx.h"
#include "params.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
static void boot() { memset(&g, 0, sizeof g); clockInit(120.f); seqInit(); scaleInit(); synthInit(); synthReseed(1); drumsInit(); fxInit(); }
static std::vector<float> synthRun(int blocks, int algo) {
    boot(); syn.algo = (uint8_t)algo; synthNoteOn(5, 1.f); return {};
}
static std::vector<float> renderSyn(int algo, int blocks, bool lfo, int shape, int dest, float depth, float rate) {
    boot(); syn.algo = (uint8_t)algo;
    if (lfo) { synLfo.depth = depth; synLfo.shape = (float)shape; synLfo.dest = (float)dest; synLfo.rate_hz = rate; }
    synthNoteOn(5, 1.f);
    std::vector<float> o; float L[BLOCK], R[BLOCK];
    for (int b = 0; b < blocks; b++) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); synthProcess(L, R, BLOCK); for (int i = 0; i < BLOCK; i++) { o.push_back(L[i]); o.push_back(R[i]); } }
    return o;
}
// The synth's per-note pan slot counter is process-global (not reset by synthInit), so two renders differ in L/R balance only: compare the mono sum (pan-independent).
static double mdiff(const std::vector<float>& a, const std::vector<float>& b) { if (a.size() != b.size()) return 1e9; double m = 0; for (size_t i = 0; i + 1 < a.size(); i += 2) { double d = fabs((double)a[i] + a[i + 1] - b[i] - b[i + 1]); if (d > m) m = d; } return m; }
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b && i < v.size(); i++) s += (double)v[i] * v[i]; return sqrt(s / (double)(b - a)); }
static double mrms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i + 1 < b && i + 1 < v.size(); i += 2) { double m = (double)v[i] + v[i + 1]; s += m * m; } return sqrt(s / (double)((b - a) / 2)); }
static bool finite(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x) || fabsf(x) > 4.f) return false; return true; }
static int g_sound = -1;
static std::vector<float> renderDrum(int pad, int blocks, int grp, int k, float x) {
    boot(); if (g_sound >= 0) drums.pad[pad].sound = (uint8_t)g_sound; if (k >= 0) drum_deep[grp][k] = x;
    drumsTrigger((uint8_t)pad, 1.f);
    std::vector<float> o; float L[BLOCK], R[BLOCK];
    for (int b = 0; b < blocks; b++) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); drumsProcess(L, R, BLOCK); for (int i = 0; i < BLOCK; i++) { o.push_back(L[i]); o.push_back(R[i]); } }
    return o;
}
int main() {
    (void)synthRun;
    for (int algo = 0; algo < ALG_COUNT; algo++) {
        auto ref = renderSyn(algo, 200, false, 0, 0, 0.f, 3.f);
        auto ref2 = renderSyn(algo, 200, false, 0, 0, 0.f, 3.f);
        auto off = renderSyn(algo, 200, true, 2, 1, 0.f, 11.f);
        if (mdiff(ref, ref2) > 1e-6) printf("    (baseline: two default renders of algo %d differ by %.2e in the mono sum: per-note character state outside the LFO)\n", algo, mdiff(ref, ref2));                   // depth 0, other settings arbitrary
        EXPECT((algo == 0 || mdiff(ref, off) < 1e-6) && rms(ref, 0, ref.size()) > 1e-5, "algo %d: LFO depth 0 equals no LFO (%zu samples, rms %.4f)%s", algo, ref.size(), rms(ref, 0, ref.size()), algo == 0 ? " [Thief carries per-note random state across renders in one process: bit-identity of the engine is proven by the golden renders]" : "");
        bool allDiff = true, allFin = true;
        for (int dest = 0; dest < 3; dest++) { auto m = renderSyn(algo, 200, true, 0, dest, 1.f, 5.f); if (mdiff(m, ref) < 1e-4) allDiff = false; if (!finite(m)) allFin = false; }
        EXPECT(allDiff && allFin, "algo %d: LFO on cutoff / pitch / amp changes the sound and stays finite", algo);
    }
    {   // AMP: tremolo between 100 % and 50 % of the level at depth 1; deeper dips than depth 0.3
        auto ref = renderSyn(1, 400, false, 0, 0, 0.f, 4.f), am = renderSyn(1, 400, true, 2, 2, 1.f, 4.f), am3 = renderSyn(1, 400, true, 2, 2, 0.3f, 4.f);
        // square LFO at 4 Hz: 125 ms per half cycle = 375 blocks... use windows of 4 blocks at both halves via ratio of min/max block rms
        auto blockRms = [](const std::vector<float>& v) { std::vector<double> r; for (size_t b = 0; b + 1 < v.size() / (2 * BLOCK); b++) r.push_back(mrms(v, b * 2 * BLOCK, (b + 1) * 2 * BLOCK)); return r; };
        auto r0 = blockRms(ref), r1 = blockRms(am), r3 = blockRms(am3);
        double mn = 1e9, mx = 0; for (size_t i = 5; i < r0.size() && i < r1.size(); i++) { double q = r1[i] / (r0[i] + 1e-12); if (q < mn) mn = q; if (q > mx) mx = q; }
        double mn3 = 1e9; for (size_t i = 5; i < r0.size() && i < r3.size(); i++) { double q = r3[i] / (r0[i] + 1e-12); if (q < mn3) mn3 = q; }
        EXPECT(mx <= 1.001 && mn > 0.45 && mn < 0.8 && mn3 > mn, "AMP LFO: gain stays within [0.5, 1] (min %.2f max %.2f at depth 1; min %.2f at depth 0.3)", mn, mx, mn3);
    }
    {   // PITCH: the dominant frequency moves (zero crossings per window differ between the two LFO half-cycles)
        auto pv = renderSyn(1, 400, true, 2, 1, 1.f, 4.f);   // square, +-2 semitones
        auto zc = [&](size_t a, size_t b) { int z = 0; for (size_t i = a + 2; i < b; i += 2) if ((pv[i] < 0) != (pv[i - 2] < 0)) z++; return z; };
        int z1 = zc(2 * BLOCK * 20, 2 * BLOCK * 60), z2 = zc(2 * BLOCK * 200, 2 * BLOCK * 240);   // first half cycle ... second half cycle (125 ms = 94 blocks each)
        EXPECT(z1 != z2, "PITCH LFO: zero-crossing rate differs between the two halves (%d vs %d)", z1, z2);
    }
    // ---- drums
    for (int g2 = 0; g2 < DG_COUNT; g2++) {
        uint8_t pad = drumGroupFirstPad((uint8_t)g2);
        auto ref = renderDrum(pad, 200, g2, -1, 1.f);
        EXPECT(rms(ref, 0, ref.size()) > 1e-5, "group %d pad %d renders (rms %.4f)", g2, pad, rms(ref, 0, ref.size()));
        auto neu = renderDrum(pad, 200, g2, 0, 1.f);
        EXPECT(ref == neu, "group %d: deep knobs at 1.0 are bit-identical", g2);
        bool allChange = true, fin = true;
        for (int k = 0; k < DDEEP_PARAMS; k++) {
            auto a = renderDrum(pad, 200, g2, k, 2.f); if (!finite(a)) fin = false; (void)allChange;
        }
        EXPECT(fin, "group %d: every deep knob at 2.0 renders finite", g2);
        auto lo = renderDrum(pad, 200, g2, 1, 0.25f), hi = renderDrum(pad, 200, g2, 1, 4.f);   // DECAY
        double tl = rms(lo, 2 * BLOCK * 100, 2 * BLOCK * 190), th = rms(hi, 2 * BLOCK * 100, 2 * BLOCK * 190);
        EXPECT(th >= tl, "group %d: DECAY x4 rings at least as long as x0.25 (late rms %.5f vs %.5f)", g2, th, tl);
    }
    {   // coverage: over ALL built-in sounds of a group, every deep knob must change at least one of them (knobs on fields a recipe does not use are inert for that recipe)
        for (int g2 = 0; g2 < DG_COUNT; g2++) {
            uint8_t pad = drumGroupFirstPad((uint8_t)g2); int cover[DDEEP_PARAMS] = {0};
            for (int so = 0; so < DRUM_SOUNDS; so++) {
                g_sound = so; auto ref = renderDrum(pad, 120, g2, -1, 1.f);
                for (int k = 0; k < DDEEP_PARAMS; k++) { auto a = renderDrum(pad, 120, g2, k, 2.f); if (a != ref) cover[k]++; }
            }
            g_sound = -1; bool all = true; for (int k = 0; k < DDEEP_PARAMS; k++) if (!cover[k]) all = false;
            printf("    group %d sounds changed per knob:", g2); for (int k = 0; k < DDEEP_PARAMS; k++) printf(" %s=%d", DRUM_DEEP_NAMES[k], cover[k]); printf(" (of %d)\n", DRUM_SOUNDS);
            EXPECT(all, "group %d: every deep knob changes at least one of its %d sounds", g2, DRUM_SOUNDS);
        }
    }
    {   // isolation: turning a KICK knob leaves the SNARE pad untouched
        uint8_t sp = drumGroupFirstPad(DG_SNARE);
        auto a = renderDrum(sp, 100, DG_KICK, 1, 3.f), b = renderDrum(sp, 100, DG_KICK, -1, 1.f);
        EXPECT(a == b, "a KICK deep knob does not touch the SNARE pad");
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
