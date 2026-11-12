// host_clock_test.cpp -- MEASURES the clock-agent changes on the Mac, no board, no audio.
//   1. Tap phase: ISR-stamped tap (new) vs processing-time tap (old behaviour) under main-loop stalls.
//   2. Loop tap-in: the REAL seqRecordHit() vs the previous algorithm, with press-to-processing delay.
//   3. Tap tempo through the real clockTap(): live from tap 2, canonical (wire-quantised), bounce-proof.
// Build/run: bash tools/host_clock_test.sh
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "daisy_seed.h"
#include "config.h"
#include "pmor_core/clock.h"
#include "pmor_core/sequencer.h"
#include "pmor_core/shared_state.h"
#include "demiurge-transport.h"

uint32_t daisy::System::s_now_ms = 0;
SharedState g;

// ---- stubs for symbols sequencer.cpp links but this test never exercises
#include "pmor_core/drums.h"
DrumGroup drumPadGroup(uint8_t) { return (DrumGroup)0; }

static int g_fail = 0, g_pass = 0;
#define CHECK(c, ...) do { if (c) g_pass++; else { g_fail++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t g_sample = 0;                       // simulated ISR sample counter (block start)
static uint32_t g_fire_sample[4096]; static uint32_t g_fire_gstep[4096]; static int g_nfire = 0;
static void run_blocks(int n) {
    for (int i = 0; i < n; i++) {
        daisy::System::s_now_ms = (uint32_t)((double)g_sample * 1000.0 / SR);
        if (clockTick(g_sample)) { if (g_nfire < 4096) { g_fire_sample[g_nfire] = g_sample; g_fire_gstep[g_nfire] = clk.gstep; g_nfire++; } }
        g_sample += BLOCK;
    }
}
static void run_until(uint32_t sample) { while ((int32_t)(sample - g_sample) > 0) run_blocks(1); }
static uint32_t ms(double m) { return (uint32_t)(m * SR / 1000.0); }
static uint32_t lcg = 1; static double urand() { lcg = lcg * 1664525u + 1013904223u; return ((lcg >> 8) + 0.5) / 16777216.0; }

// ---- the PREVIOUS seqRecordHit target computation (kept only as the measured baseline)
static uint32_t old_target(uint32_t now_sample) {
    int32_t whole_diff   = (int32_t)(clk.next_step_whole - now_sample);
    float   dist_to_next = (float)whole_diff + (float)clk.next_step_frac / 4294967296.f;
    bool  nearer_next   = (clk.step_samples > 0.f) && (dist_to_next <= clk.step_samples * 0.5f) && (dist_to_next >= 0.f);
    return nearer_next ? (clk.gstep + 1) : clk.gstep;
}

int main() {
    // ============ 1. tap phase under main-loop stall =====================================
    printf("1. TAP PHASE: grid anchor error after a tap whose UI event is processed L ms late (120 BPM, 16th = 125 ms)\n");
    printf("   (error = how far the next grid boundary lands from tap_instant + 1 step; ISR-stamped vs the old 'time the loop noticed it')\n");
    printf("   stall L   old(mean/max abs err)    new(mean/max abs err)\n");
    const double stalls[] = { 0, 5, 10, 20, 35 };
    for (double L : stalls) {
        double sum_old = 0, sum_new = 0, mx_old = 0, mx_new = 0; int trials = 300;
        for (int variant = 0; variant < 2; variant++) {
            for (int k = 0; k < trials; k++) {
                g_sample = 0; g_nfire = 0; clockInit(120.f); seqInit(); run_blocks(400);
                // tap lands at a random instant, in the first half of a step (the 'fired step was the click' branch is
                // exercised by the other half) -- cover the whole step
                uint32_t t0 = g_sample + ms(urand() * 125.0);
                run_until(t0 + ms(L));                         // the loop only gets to it L ms later
                uint32_t stamp = variant == 0 ? clockSampleNow() /* old: noticed now */ : t0 /* new: ISR stamp */;
                clockTap(stamp);
                g_nfire = 0;
                run_blocks((int)(SR * 0.4 / BLOCK));
                // the first boundary at/after t0 + 0.5 step is the "next beat after the click"
                double want = (double)t0 + 125.0 * SR / 1000.0;
                double best = 1e9;
                for (int f = 0; f < g_nfire; f++) { double e = fabs((double)g_fire_sample[f] - want); if (e < best) best = e; }
                double err_ms = best * 1000.0 / SR;
                if (variant == 0) { sum_old += err_ms; if (err_ms > mx_old) mx_old = err_ms; }
                else              { sum_new += err_ms; if (err_ms > mx_new) mx_new = err_ms; }
            }
        }
        printf("   %5.0f ms   %6.2f / %6.2f ms        %6.2f / %6.2f ms\n", L, sum_old / trials, mx_old, sum_new / trials, mx_new);
        CHECK(sum_new / trials <= sum_old / trials + 1.5, "ISR-stamped tap worse than the old behaviour at stall %.0f ms", L);
        if (L >= 10) CHECK(sum_new / trials < 0.6 * (sum_old / trials) + 0.01, "ISR stamp did not clearly beat the old path at %.0f ms (new %.2f old %.2f)", L, sum_new / trials, sum_old / trials);
        CHECK(mx_new < 4.0, "new tap anchor error exceeds 4 ms at stall %.0f ms (max %.2f)", L, mx_new);   // 1.33 ms block + slack
    }

    // ============ 2. loop tap-in quantisation ============================================
    printf("\n2. LOOP TAP-IN: wrong-step rate. Player hits within +-50 ms of a grid line; the hit is PROCESSED L ms later\n");
    printf("   (real seqRecordHit with the press timestamp vs the previous algorithm with the processing-time stamp)\n");
    printf("   delay L    old wrong      new wrong     (of 2000 hits)\n");
    const double delays[] = { 0, 10, 20, 30, 45 };
    for (double L : delays) {
        int wrong_old = 0, wrong_new = 0, n = 2000;
        for (int k = 0; k < n; k++) {
            g_sample = 0; g_nfire = 0; clockInit(120.f); seqInit();
            seq.lane[ENG_DRUMS].bars = 1; seq.lane[ENG_DRUMS].steps_per_bar = 16; clockSetLoopLen(seqLongestLoop());
            run_blocks(600);
            // pick the NEXT boundary that will fire; hit lands at boundary + delta
            int before = g_nfire;
            run_blocks(1);   // settle
            // look ahead: simulate to find the next fire without disturbing: just run until one fires
            while (g_nfire == before) run_blocks(1);
            uint32_t bt = g_fire_sample[g_nfire - 1];             // block-quantised; true boundary is within one block of this
            uint32_t bg = g_fire_gstep[g_nfire - 1];
            // run ahead to the NEXT boundary and aim the hit at it (so everything after is in the future of the sim)
            double step_s = (double)clk.step_q / 4294967296.0;
            double delta_ms = (urand() * 2.0 - 1.0) * 50.0;
            double true_boundary = (double)bt + step_s;           // approximate (<= 1 block off): fine for a +-50 ms test
            uint32_t press = (uint32_t)(true_boundary + delta_ms * SR / 1000.0);
            uint32_t expect = (bg + 1) % 16;
            run_until(press + ms(L));
            uint32_t o = old_target(clockSampleNow()) % 16;
            for (int i = 0; i < MAXSTEPS; i++) seq.lane[ENG_DRUMS].step[i] = 0;
            seq.lane[ENG_DRUMS].step[15] = 1u << 5;   // a non-empty lane = OVERDUB (an empty lane's first hit is the loop start, see test_firsthit)
            seqRecordHit(ENG_DRUMS, 0, press);
            int got = -1; for (int i = 0; i < 16; i++) if (seq.lane[ENG_DRUMS].step[i] & 1) got = i;
            if (o != expect) wrong_old++;
            if (got != (int)expect) wrong_new++;
        }
        printf("   %5.0f ms   %5d (%4.1f%%)   %5d (%4.1f%%)\n", L, wrong_old, 100.0 * wrong_old / n, wrong_new, 100.0 * wrong_new / n);
        CHECK(wrong_new <= wrong_old, "new quantiser worse than old at L=%.0f", L);
        CHECK(wrong_new <= n / 50, "new quantiser wrong on %d of %d hits at L=%.0f ms (should be only the block-quantisation edge)", wrong_new, n, L);
    }

    // ============ 3. tap tempo through the real clockTap =================================
    printf("\n3. TAP TEMPO (real clockTap, shared analyzer)\n");
    g_sample = 0; g_nfire = 0; clockInit(100.f); run_blocks(100);
    uint32_t t = g_sample;
    int r1 = clockTap(t);                                         CHECK(r1 == DT_TAP_ARMED, "first tap should arm (got %d)", r1);
    run_until(t + ms(500));
    int r2 = clockTap(t + ms(500));                               CHECK(r2 == DT_TAP_TEMPO, "second tap should give a tempo (got %d)", r2);
    printf("   2 taps 500 ms apart -> clk.bpm = %.4f (wire-canonical %.4f)\n", clk.bpm, dt_bpm_canon(120.0));
    CHECK(fabs(clk.bpm - dt_bpm_canon(120.0)) < 1e-3, "tap tempo not canonical/120: %.5f", clk.bpm);
    int r3 = clockTap(t + ms(500) + 40);                          CHECK(r3 == DT_TAP_IGNORED, "0.8 ms duplicate tap not ignored");
    CHECK(clockTapCount() == 2, "duplicate entered the window (n=%d)", clockTapCount());
    clockSetBpm(137.3f);
    CHECK(fabs(clk.bpm - dt_bpm_canon(137.3)) < 1e-4, "clockSetBpm not canonicalised: %.5f vs %.5f", clk.bpm, dt_bpm_canon(137.3));
    printf("   clockSetBpm(137.3) -> %.4f (what the wire carries)\n", clk.bpm);

    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
