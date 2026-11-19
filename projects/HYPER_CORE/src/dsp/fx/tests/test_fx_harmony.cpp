// test_fx_harmony.cpp -- HARMONY audit: every interval (old up-table and new down-octave table) through the real fx_harmony.h on a trumpet-like input
// (8 harmonics, 5.5 Hz vibrato +-25 cents, slow dynamics, -45 dBFS RMS, noise floor -75 dB) with the real PitchTrack feeding f0/conf like fx_glue_chain.h.
// Per voice: wet level (output minus the dry) in dB re dry, pitch error in cents (mean log ratio wet/dry autocorrelation pitch over windows), latency.
// Usage: ./test_harmony [rmsDb=-45]. Exit code 1 on any failure of the new-table checks.
#include "../fx_harmony.h"
#include "../fx_pitchtrack.h"
#include "fx_testutil.h"
using namespace hx; using namespace hxt;

static const int OLD[16] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 17, 18, 20, 21 };
static int NEWT[16]; // = OLD - 12

static int gMel = 0;
struct Horn { float hz, rms; double ph = 0; uint32_t s = 99; long i = 0;
    float tick() {
        float t = (float)i++ / SR; double inst = hz * exp2(0.25 / 12.0 * sin(6.2831853 * 5.5 * t)); float gate = 1.f;
        if (gMel) { static const float nt[8] = { 0.f, 4.f, 7.f, 2.f, 9.f, -3.f, 5.f, 12.f }; int n = (int)(t / 0.3f); float tn = t - n * 0.3f; inst *= exp2(nt[n & 7] / 12.0 + (tn < 0.03f ? -0.4f * (1.f - tn / 0.03f) / 12.0 : 0.0)); gate = tn < 0.01f ? tn / 0.01f : (tn > 0.27f ? fmaxf(0.f, (0.3f - tn) / 0.03f) : 1.f); }
        ph += inst / SR; if (ph > 1.0) ph -= 1.0; float y = 0;
        for (int h = 1; h <= 8; h++) y += sinf(6.2831853f * (float)((ph * h) - floor(ph * h))) / powf((float)h, 0.8f);
        float env = 0.85f + 0.15f * sinf(6.2831853f * 0.7f * t);
        return gate * rms * 1.9f * y * env * 0.5f + rms * 0.02f * (rnd01(s) * 2 - 1);   // 1.9*0.5 ~ rms normalisation (checked below by measurement)
    } };

static double rmsOf(const float* x, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += (double)x[i] * x[i]; return sqrt(s / (double)n); }
static float acFreq(const float* x, size_t len, float lo, float hi) {
    int tmin = (int)(SR / hi), tmax = (int)(SR / lo); double mx = -2; std::vector<double> r(tmax + 2, 0.0);
    for (int t = tmin; t <= tmax; t++) { double s = 0, e1 = 0, e2 = 0; for (size_t i = 0; i + t < len; i++) { s += (double)x[i] * x[i + t]; e1 += (double)x[i] * x[i]; e2 += (double)x[i + t] * x[i + t]; } r[t] = s / sqrt(e1 * e2 + 1e-20); if (r[t] > mx) mx = r[t]; }
    int best = -1; for (int t = tmin + 1; t < tmax; t++) if (r[t] >= 0.9 * mx && r[t] >= r[t - 1] && r[t] >= r[t + 1]) { best = t; break; }
    if (best < 0) return 0.f; double a = r[best - 1], b = r[best], c = r[best + 1], den = a - 2 * b + c, off = den != 0 ? 0.5 * (a - c) / den : 0; return (float)(SR / (best + off));
}

struct Res { double blank = 0, dipDb = 0, trkFrac = 0, lvDb = -200, cents = 0, dryRms = 0, wetRms = 0; bool pitchOk = false; };

// run ONE voice (slot 0) at `semi`; returns wet level re dry and the pitch error. nvox>1 adds the extra voices (for the mix-balance test)
static int gTrk = 1;
static Res measure(int semi, float f0, float rmsDb, bool useRealTracker = true, float voiceLvl = -1.f) {
    useRealTracker = useRealTracker && gTrk;
    Host h; h.make(fx_create_harmony); FxHarmony* hm = static_cast<FxHarmony*>(h.fx);
    hm->setParam(FxHarmony::P_MIX, 1.f); if (voiceLvl >= 0) hm->setParam(FxHarmony::P_VOICE, voiceLvl); hm->snap();
    Horn hornA{f0, powf(10.f, rmsDb / 20.f)}; PitchTrack pt; pt.init(SR);
    const double secs = 2.5; const long total = (long)(secs * SR); std::vector<float> dry, out; float L[48], R[48], d0[48];
    hm->setSlot(0, semi);
    long nTrk = 0, nB = 0;
    for (long b = 0; b < total / 48; b++) {
        for (int i = 0; i < 48; i++) L[i] = R[i] = hornA.tick(); memcpy(d0, L, sizeof d0);
        FxCtx c = defCtx(0.f); if (useRealTracker) { pt.process(L, 48); c.f0 = pt.f0; c.conf = pt.conf; if (b > 1000) { nB++; if (pt.f0 > 70 && pt.conf > 0.4f) nTrk++; } } c.sr = SR;
        hm->smoothBlock(); hm->process(c, L, R, 48, false);
        for (int i = 0; i < 48; i++) { dry.push_back(d0[i]); out.push_back(L[i] - d0[i]); }
    }
    Res r; size_t a = (size_t)(1.0 * SR), e = dry.size();
    r.dryRms = rmsOf(&dry[a], e - a); r.wetRms = rmsOf(&out[a], e - a); r.lvDb = 20 * log10(r.wetRms / r.dryRms + 1e-12);
    r.trkFrac = nB ? (double)nTrk / nB : 0;
    { double mn = 1e9; const size_t w = 480; for (size_t p = a; p + w < e; p += w / 2) mn = fmin(mn, rmsOf(&out[p], w)); r.dipDb = 20 * log10(mn / r.wetRms + 1e-12); }
    { size_t w = 960, nb = 0, nt = 0; for (size_t p = a; p + w + 960 < e; p += w) { double dr = rmsOf(&dry[p], w), wr = rmsOf(&out[p + 0], w); if (dr > 0.5 * r.dryRms) { nt++; if (wr < 0.1 * dr) nb++; } } r.blank = nt ? (double)nb / nt : 0; }
    // pitch: mean log-ratio of wet vs dry autocorrelation over 8 windows of 80 ms (wet delayed by ~12 ms: windows are long vs that, vibrato averages out)
    double acc = 0; int cnt = 0; const size_t W = (size_t)(0.08 * SR);
    for (size_t p = a; p + W + 2400 < e; p += (size_t)(0.1 * SR)) {
        float fd = acFreq(&dry[p], W, 70.f, 1700.f), fw = acFreq(&out[p], W, 40.f, 3000.f);
        if (fd > 0 && fw > 0) { acc += log2(fw / fd) * 1200.0 - semi * 100.0; cnt++; }
    }
    r.pitchOk = cnt >= 6; r.cents = cnt ? acc / cnt : 999.0; return r;
}


// ---- on/off clicks, 3-voice mix, API semantics, latency, CPU
static void apiAndClicks(int& fails) {
    // (1) click test: steady 3-voice stack, momentary voiceOn / voiceOff of a 4th: max sample step of the WET path around the event vs the steady wet max step
    Host h; h.make(fx_create_harmony); FxHarmony* hm = static_cast<FxHarmony*>(h.fx); hm->setParam(FxHarmony::P_MIX, 1.f); hm->snap();
    Horn hornA{349.f, powf(10.f, -45.f / 20.f)}; PitchTrack pt; pt.init(SR); float L[48], R[48], d0[48]; std::vector<float> wet;
    auto blk = [&]() { for (int i = 0; i < 48; i++) L[i] = R[i] = hornA.tick(); memcpy(d0, L, sizeof d0); pt.process(L, 48); FxCtx c = defCtx(0.f); c.f0 = pt.f0; c.conf = pt.conf; hm->smoothBlock(); hm->process(c, L, R, 48, false); for (int i = 0; i < 48; i++) wet.push_back(L[i] - d0[i]); };
    hm->voiceOn(-10); for (int b = 0; b < 200; b++) blk();
    size_t t0 = wet.size(); for (int b = 0; b < 400; b++) blk(); double stepSteady = 0; for (size_t i = t0 + 1; i < wet.size(); i++) stepSteady = fmax(stepSteady, fabs(wet[i] - wet[i - 1]));
    size_t tOn = wet.size(); hm->voiceOn(-5); for (int b = 0; b < 100; b++) blk();     // momentary press ...
    size_t tOff = wet.size(); hm->voiceOff(-5); for (int b = 0; b < 100; b++) blk();   // ... release
    double stepOn = 0, stepOff = 0, aOn = 0, aSt = 0; for (size_t i = tOn + 1; i < tOn + 4800; i++) stepOn = fmax(stepOn, fabs(wet[i] - wet[i - 1])); for (size_t i = tOff + 1; i < tOff + 4800; i++) stepOff = fmax(stepOff, fabs(wet[i] - wet[i - 1]));
    for (size_t i = t0; i < tOn; i++) aSt = fmax(aSt, fabs(wet[i])); for (size_t i = tOn; i < tOn + 4800; i++) aOn = fmax(aOn, fabs(wet[i]));
    printf("\nclicks: max wet step steady(1 voice) %.2e, voiceOn(-5) %.2e, voiceOff %.2e (peak 1 voice %.2e, 2 voices %.2e)\n", stepSteady, stepOn, stepOff, aSt, aOn);
    if (stepOn > 2.5 * stepSteady || stepOff > 2.5 * stepSteady) { printf("  FAIL click\n"); fails++; }   // 2 voices may legitimately add up to 2x the 1-voice step
    // (2) API semantics: 3 voices max, steal oldest, voiceOff of unknown is a no-op, duplicates don't take a second slot, momentary = on then off leaves nothing
    FxHarmony* q = hm; q->allOff(); for (int b = 0; b < 100; b++) blk();
    q->voiceOn(-10); q->voiceOn(-9); q->voiceOn(-9); q->voiceOn(-8); q->voiceOff(7);
    bool a1 = q->voiceIsOn(-10) && q->voiceIsOn(-9) && q->voiceIsOn(-8); q->voiceOn(-7); bool a2 = !q->voiceIsOn(-10) && q->voiceIsOn(-9) && q->voiceIsOn(-8) && q->voiceIsOn(-7);
    q->voiceOff(-9); q->voiceOff(-8); q->voiceOff(-7); bool a3 = !q->voiceIsOn(-9) && !q->voiceIsOn(-8) && !q->voiceIsOn(-7);
    printf("api: 3-slot %s, steal-oldest %s, release %s\n", a1 ? "ok" : "BAD", a2 ? "ok" : "BAD", a3 ? "ok" : "BAD"); if (!(a1 && a2 && a3)) fails++;
    // after everything is released the wet must decay to exact silence
    for (int b = 0; b < 100; b++) blk(); double tail = 0; for (size_t i = wet.size() - 2400; i < wet.size(); i++) tail = fmax(tail, fabs(wet[i])); printf("api: wet peak 50 ms after all voices released: %.2e\n", tail); if (tail > 1e-6) fails++;
}
static void latencyAndCpu(int& fails) {
    printf("\nlatency (tone onset -> wet envelope reaches 50%% of steady, 349 Hz):\n");
    double worst = 0; for (int semi : {-10, -5, -2, +1, +5, +9}) {
        Host h; h.make(fx_create_harmony); FxHarmony* hm = static_cast<FxHarmony*>(h.fx); hm->setParam(FxHarmony::P_MIX, 1.f); hm->snap(); hm->voiceOn(semi);
        Horn hornA{349.f, powf(10.f, -45.f / 20.f)}; PitchTrack pt; pt.init(SR); float L[48], R[48], d0[48]; std::vector<float> wet; const int tOn = 1000 * 48 / 48 * 48;
        for (int b = 0; b < 700; b++) { for (int i = 0; i < 48; i++) { float x = hornA.tick(); if ((long)b * 48 + i < 24000) x *= 0.f; L[i] = R[i] = x; } memcpy(d0, L, sizeof d0); pt.process(L, 48); FxCtx c = defCtx(0.f); c.f0 = pt.f0; c.conf = pt.conf; hm->smoothBlock(); hm->process(c, L, R, 48, false); for (int i = 0; i < 48; i++) wet.push_back(L[i] - d0[i]); }
        (void)tOn; size_t on = 24000; double ss = 0; for (size_t i = on + 6000; i < on + 12000; i++) ss += (double)wet[i] * wet[i]; ss = sqrt(ss / 6000.0);
        size_t t50 = 0; for (size_t i = on; i + 96 < wet.size(); i++) { double e = 0; for (int j = 0; j < 96; j++) e += (double)wet[i + j] * wet[i + j]; if (sqrt(e / 96.0) > 0.5 * ss) { t50 = i - on + 48; break; } }
        printf("  %+3d st: %.1f ms\n", semi, t50 / 48.0); worst = fmax(worst, t50 / 48.0); }
    printf("  worst %.1f ms (includes the 18 ms voice fade-in? no: voice is already faded in at tone onset; this is path delay + window ramp)\n", worst);
    (void)fails;
    printf("\nCPU (host ns/sample, 3 voices + tracker fed ctx; device %% ~ ns x 0.48):\n");
    struct Cs { const char* n; int a, b, c; bool trk; } cs[] = { {"-10,-9,-8", -10, -9, -8, true}, {"-5,-1,+5", -5, -1, 5, true}, {"+6,+8,+9 (worst, fastest wraps)", 6, 8, 9, true}, {"-10,+9,+2 no tracker (wide search)", -10, 9, 2, false}, {"old table +10,+14,+21", 10, 14, 21, true} };
    for (auto& cc : cs) {
        Host h; h.make(fx_create_harmony); FxHarmony* hm = static_cast<FxHarmony*>(h.fx); hm->setParam(FxHarmony::P_MIX, 1.f); hm->snap(); hm->setSlot(0, cc.a); hm->setSlot(1, cc.b); hm->setSlot(2, cc.c);
        FxCtx c = defCtx(cc.trk ? 349.f : 0.f); std::vector<float> L(48), R(48), in(48000); long nb = 4 * 48000 / 48; Horn hornA{349.f, 0.01f}; double best = 1e18; for (auto& v : in) v = hornA.tick();
        for (int rep = 0; rep < 3; rep++) { auto t0 = std::chrono::steady_clock::now(); float sink = 0; for (long b = 0; b < nb; b++) { const float* ip = &in[(size_t)(b % 1000) * 48]; for (int i = 0; i < 48; i++) L[i] = R[i] = ip[i]; hm->smoothBlock(); hm->process(c, L.data(), R.data(), 48, false); sink += L[0]; }
            double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (double)(nb * 48); best = fmin(best, ns); (void)sink; }
        printf("  %-36s %.1f ns/sample -> ~%.1f %% device\n", cc.n, best, best * 0.48); }
}

int main(int argc, char** argv) {
    float rmsDb = argc > 1 ? (float)atof(argv[1]) : -45.f; if (argc > 2) gTrk = atoi(argv[2]); if (argc > 3) gMel = atoi(argv[3]); for (int i = 0; i < 16; i++) NEWT[i] = OLD[i] - 12;
    int fails = 0;
    { Host h; h.make(fx_create_harmony); printf("arena bytes used: %zu\n", h.arena.used); }
    const float f0s[] = { 233.f, 349.f, 523.f };
    for (int tbl = 0; tbl < 2; tbl++) {
        const int* T = tbl ? NEWT : OLD; printf("\n== %s table, trumpet RMS %.0f dBFS ==\n   st   |", tbl ? "NEW (down)" : "OLD (up)", rmsDb);
        for (float f : f0s) printf("  %3.0f Hz: lvl dB   cents |", f); printf("\n");
        double lo = 1e9, hi = -1e9; double worst = 0;
        for (int k = 0; k < 16; k++) {
            printf("  %+3d   |", T[k]);
            for (float f : f0s) { Res r = measure(T[k], f, rmsDb); printf(" %5.1f %+5.1f%s dip%4.0f t%.0f%% b%.0f%% |", r.lvDb, r.pitchOk ? r.cents : 999.0, r.pitchOk ? " " : "?", r.dipDb, 100 * r.trkFrac, 100 * r.blank);
                if (f == 349.f) { lo = fmin(lo, r.lvDb); hi = fmax(hi, r.lvDb); }
                if (tbl && (r.lvDb < -30 || !r.pitchOk || fabs(r.cents) > 5.0)) { fails++; }
                if (tbl && r.pitchOk) worst = fmax(worst, fabs(r.cents)); }
            printf("\n"); }
        printf("   level spread at 349 Hz: %.1f dB (want <= 3 dB); worst |cents| %.1f\n", hi - lo, worst);
        if (tbl && hi - lo > 3.0) fails++;
    }
    printf("\nlevel vs input level (new table, 349 Hz, +/- steps):\n");
    for (float db : {-50.f, -45.f, -40.f}) { double lo = 1e9, hi = -1e9; for (int k = 0; k < 16; k++) { Res r = measure(NEWT[k], 349.f, db); lo = fmin(lo, r.lvDb); hi = fmax(hi, r.lvDb); } printf("  in %.0f dBFS: wet level re dry min %.1f max %.1f\n", db, lo, hi); if (hi - lo > 3.0) fails++; }
    apiAndClicks(fails); latencyAndCpu(fails);
    printf("\nHARMONY %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
