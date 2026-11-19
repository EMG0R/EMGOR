// Host CPU budget bench for the HYPER_CORE effects (no hardware). Build + run (from bringup/fw):
//   c++ -std=c++17 -O2 -fno-vectorize -fno-slp-vectorize -I../../../HYPER_CORE/src/dsp/fx tests/fx_budget.cpp -o /tmp/fx_budget && /tmp/fx_budget
// (-fno-vectorize: the Cortex-M7 has no NEON, gcc -O2 does not auto-vectorize either.)
// Per effect, default params as the firmware glue sets them, MIX 1, continuous input (noise + 440 Hz), 48 samples per block:
//   avg ns/sample over the whole run, worst single block (timer overhead removed), and the Seed estimate = host ns/sample * F / 20833 ns,
//   F = host-to-Seed factor derived from the one known pair: the reverb measured 29.2 % on the Seed (cpu4[1], fader 1 at 95 %).
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <chrono>
#include <string.h>
#include <vector>
#include <algorithm>
#include <string>
#include "fx_harmonizer.h"
#include "fx_looper.h"
#include "fx_reverb.h"
#include "fx_grain.h"
#include "fx_delay.h"
#include "fx_flanger.h"
#include "fx_chorus.h"
#include "fx_mood.h"
#include "fx_freeze.h"
#include "fx_blur.h"
#include "fx_drone.h"
#include "fx_roar.h"
#include "fx_ringmod.h"
#include "fx_wah.h"
#include "fx_autopan.h"
#include "fx_harmony.h"
using namespace hx;
typedef std::chrono::steady_clock Clk;
static double nowNs() { return std::chrono::duration<double, std::nano>(Clk::now().time_since_epoch()).count(); }
struct Res { std::string id; double avg, worst; };
static double g_F = 0.0;
static const double BLOCK_NS = 48.0 * 1e9 / 48000.0;
template <class FN> static Res run(const char* id, FN create, void (*setup)(Fx*) = nullptr, void (*start)(Fx*) = nullptr) {
    // 5 identical passes (same state, same input); per block the MINIMUM over the passes removes OS jitter, so the worst block is the algorithm's own worst block.
    static std::vector<uint8_t> mem(64u << 20); const int NB = 30000, PASSES = 5;
    std::vector<double> best(NB, 1e18); double bestTotal = 1e18;
    double ov = 1e9; for (int k = 0; k < 4000; k++) { double t0 = nowNs(); double t1 = nowNs(); if (t1 - t0 < ov) ov = t1 - t0; }
    for (int pass = 0; pass < PASSES; pass++) {
        Arena a; a.bind(mem.data(), mem.size()); memset(mem.data(), 0, 1 << 20);
        Fx* f = create(a, 48000.f); f->init(48000.f, 48.f); if (setup) setup(f); f->setParam(0, 1.f); f->Fx::snap();
        FxCtx c = {}; c.sr = 48000.f; c.f0 = 440.f; c.conf = 1.f; c.env = 0.5f;
        float L[48], R[48]; uint32_t rs = 12345; auto rnd = [&]() { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.f / 16777216.f) - 0.5f; };
        long blk = 0; auto fill = [&]() { for (int i = 0; i < 48; i++) { L[i] = 0.25f * sinf(6.2831853f * 440.f * (float)((blk * 48 + i) % 48000) / 48000.f) + 0.1f * rnd(); R[i] = L[i]; } blk++; };
        if (start) start(f);
        for (int b = 0; b < 6000; b++) { fill(); f->smoothBlock(); f->process(c, L, R, 48, false); }        // warm-up: caches, loops, freeze capture
        double total = 0;
        for (int b = 0; b < NB; b++) { fill(); double t0 = nowNs(); f->smoothBlock(); f->process(c, L, R, 48, false); double t1 = nowNs(); double d = t1 - t0 - ov; if (d < 0) d = 0; total += d; if (d < best[b]) best[b] = d; }
        if (total < bestTotal) bestTotal = total;
    }
    double worst = 0; for (int b = 0; b < NB; b++) if (best[b] > worst) worst = best[b];
    if (getenv("FX_TOP") && !strcmp(getenv("FX_TOP"), id)) { std::vector<int> ix(NB); for (int i = 0; i < NB; i++) ix[i] = i; std::sort(ix.begin(), ix.end(), [&](int a, int b) { return best[a] > best[b]; }); for (int k = 0; k < 12; k++) printf("  top%d: block %d  %.0f ns\n", k, ix[k], best[ix[k]]); double med = 0; { std::vector<double> c = best; std::sort(c.begin(), c.end()); med = c[NB / 2]; printf("  median block %.0f ns\n", med); } }
    return { id, bestTotal / NB / 48.0, worst };
}
static void rvSetupF(Fx* f) { FxReverb* r = static_cast<FxReverb*>(f); using R = FxReverb; r->setParam(R::P_DECAY, 0.94f); r->setParam(R::P_SIZE, 1.6f); r->setParam(R::P_DAMP, 5500.f); r->setParam(R::P_MODD, 0.55f); r->setParam(R::P_MODR, 0.4f); r->setParam(R::P_LOWCUT, 80.f); r->setParam(R::P_WIDTH, 1.f); r->setParam(R::P_PRE, 25.f); r->setParam(R::P_WARP, 0.f); }
static void loopStart(Fx* f) { f->action(A_MAIN, 1.f); }          // record the first layer during warm-up (the loop then plays)
int main(int argc, char** argv) {
    double target = 0.292;                                    // reverb on the Seed
    std::vector<Res> R;
    R.push_back(run("reverb", fx_create_reverb, rvSetupF));
    g_F = target * (BLOCK_NS / 48.0) / 74.1;                  // F is fixed from the ORIGINAL reverb (74.1 ns/sample on this host = 29.2 % on the Seed), so after-numbers are comparable
    if (getenv("FX_F")) g_F = atof(getenv("FX_F"));
    R.push_back(run("drone", fx_create_drone)); R.push_back(run("harm", fx_create_harm)); R.push_back(run("looper", fx_create_looper));
    R.push_back(run("freeze", fx_create_freeze)); R.push_back(run("blur", fx_create_blur)); R.push_back(run("flange", fx_create_flanger)); R.push_back(run("chorus", fx_create_chorus));
    R.push_back(run("mood", fx_create_mood)); R.push_back(run("grain", fx_create_grain)); R.push_back(run("delay", fx_create_delay)); R.push_back(run("roar", fx_create_roar));
    R.push_back(run("ring", fx_create_ringmod)); R.push_back(run("wah", fx_create_wah)); R.push_back(run("autopan", fx_create_autopan)); R.push_back(run("harmony", fx_create_harmony, nullptr, [](Fx* f) { auto* h = static_cast<FxHarmony*>(f); h->setSlot(0, 4); h->setSlot(1, 7); h->setSlot(2, 10); }));
    printf("host-to-Seed factor F = %.1f (fixed: original reverb = 74.1 ns/sample on the host = 29.2 %% on the Seed)\n", g_F);
    printf("%-8s %9s %10s %9s %10s\n", "effect", "ns/sample", "worst ns", "avg %", "worst %");
    for (auto& r : R) printf("%-8s %9.1f %10.0f %8.1f%% %9.1f%%\n", r.id.c_str(), r.avg, r.worst, 100.0 * r.avg * g_F / (BLOCK_NS / 48.0), 100.0 * r.worst * g_F / BLOCK_NS);
    return 0;
}
