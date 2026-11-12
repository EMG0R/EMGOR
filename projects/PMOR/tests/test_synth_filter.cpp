// test_synth_filter.cpp -- the synth VCF (Chamberlin SVF, pmor_core/synth.cpp) is stable for EVERY
// (coefficient, damping) pair the engine can produce, with the state clamp OFF (the clamp only hides
// instability). Also an end-to-end sweep: every algorithm x every note x cutoff/reso extremes renders
// finite and bounded.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "synth_private.h"
#include "shared_state.h"
#include "clock.h"
#include "scale.h"
#include "sequencer.h"
#include "engine_util.h"

static int fails = 0;
#define CHECK(c, ...) do { if(!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)

int main() {
    // 1. raw recurrence, no clamps: impulse then 20000 samples of zero input must decay (spectral radius < 1)
    int worst_i = 0; double worst = 0;
    for (int ri = 0; ri <= 100; ri++) {
        float reso = ri / 100.f, q = 2.f * (1.02f - reso);
        float fmax = svfMaxF(q);
        for (int fi = 0; fi <= 100; fi++) {
            float f = 0.001f + (fmax - 0.001f) * fi / 100.f;   // everything the engine may now use
            double lp = 0, bp = 0, x = 1.0;
            for (int n = 0; n < 40000; n++) {
                lp += f * bp; double hp = x - lp - q * bp; bp += f * hp; x = 0;
                if (!(fabs(lp) < 1e6)) break;
            }
            double m = fabs(lp) + fabs(bp);
            if (m > worst) { worst = m; worst_i = ri; }
            CHECK(m < 1e-3, "unstable: reso=%.2f q=%.3f f=%.3f |state|=%g", reso, q, f, m);
        }
    }
    printf("raw SVF: worst residual after 40000 samples = %g (reso %d%%)\n", worst, worst_i);
    // sanity: the OLD cap (flat 0.9) really was unstable at low resonance
    { double lp = 0, bp = 0, x = 1; float f = 0.9f, q = 2.f * (1.02f - 0.05f);
      for (int n = 0; n < 4000; n++) { lp += f * bp; double hp = x - lp - q * bp; bp += f * hp; x = 0; if (fabs(lp) > 1e9) break; }
      CHECK(fabs(lp) + fabs(bp) > 1.0, "old cap f=0.9 q=%.2f should be unstable (test premise)", q); }
    // 2. engine: all algos, pads 0..15 (low to high notes), cutoff/reso extremes -> finite + bounded
    memset(&g, 0, sizeof(g));
    clockInit(120.f); seqInit(); scaleInit(); synthInit();
    static float L[BLOCK], R[BLOCK];
    int bad = 0; float peak = 0;
    const float cuts[] = { 40.f, 3000.f, 12000.f }, resos[] = { 0.f, 0.5f, 1.f };
    for (int a = 0; a < ALG_COUNT; a++) for (int note = 0; note < 16; note += 3)
      for (float c : cuts) for (float rs : resos) {
        synthInit(); syn.algo = (uint8_t)a; syn.preset[a].cutoff = c; syn.preset[a].resonance = rs;
        synthNoteOn((uint8_t)note, 1.f);
        for (int b = 0; b < 40; b++) {
            memset(L, 0, sizeof L); memset(R, 0, sizeof R); synthProcess(L, R, BLOCK);
            for (int i = 0; i < BLOCK; i++) { if (!isfinite(L[i]) || !isfinite(R[i])) bad++; peak = fmaxf(peak, fmaxf(fabsf(L[i]), fabsf(R[i]))); }
        }
    }
    CHECK(bad == 0 && peak < 4.f, "engine sweep: %d non-finite, peak %.3f", bad, peak);
    printf("engine sweep peak %.3f\n", peak);
    printf(fails ? "test_synth_filter: %d FAILURES\n" : "test_synth_filter: PASS (%d failures)\n", fails);
    return fails ? 1 : 0;
}
