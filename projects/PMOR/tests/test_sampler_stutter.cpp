// Stutter only chops: after it releases, playback continues exactly where it would have been without it (shadow playhead).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "sampler.h"
#include "shared_state.h"
#include "clock.h"
#include "fx.h"
#include "fx_host.h"
#include "synth.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
static void boot() { memset(&g, 0, sizeof g); clockInit(120.f); fxInit(); synthInit(); samplerInit(); fx_host_stut.rate = STUT_NONE; fx_host_stut.bzz = false;
    for (int i = 0; i < 90000; i++) smp_buf[0][i] = 0.4f * sinf(i * 0.021f) + 0.3f * sinf(i * 0.0037f); smp.len[0] = 90000; smp.polyphony = 4.f; }
// blocks [0,total): the stutter (if any) is engaged at block a, retriggers every `period` samples, released at block b
static std::vector<float> run(bool stutter, int a, int b, int total, int period = 1500) {
    boot(); samplerPlay(0, 1.f); std::vector<float> o; float in0[64] = {0}, L[64], R[64]; long next = -1;
    for (int blk = 0; blk < total; blk++) {
        if (stutter && blk == a) { fxHostSetRate(STUT_16); next = (long)blk * 64; }
        if (stutter && blk == b) { fxHostClearRate(STUT_16); next = -1; }
        if (next >= 0 && next < (long)(blk + 1) * 64) { samplerRetrig(1); next += period; }
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); samplerProcess(in0, L, R, 64);
        for (int i = 0; i < 64; i++) o.push_back(L[i] + R[i]);
    }
    return o;
}
int main() {
    const int A = 40, B = 100, T = 400;                 // stutter from block 40 to 100 (60 blocks = 3840 samples, ~3 chops)
    auto ref = run(false, 0, 0, T), st = run(true, A, B, T);
    double diffDuring = 0; for (size_t i = (size_t)A * 64 + 200; i < (size_t)B * 64; i++) diffDuring += fabs(ref[i] - st[i]);
    EXPECT(diffDuring > 1.0, "during the stutter the output differs from the plain playback (it chops): sum |diff| %.2f", diffDuring);
    size_t from = (size_t)(B * 64) + 480;               // 10 ms after the release: the crossfade has finished
    size_t bad = 0; double worst = 0; for (size_t i = from; i < ref.size(); i++) { double d = fabs(ref[i] - st[i]); if (d > worst) worst = d; if (d > 1e-6) bad++; }
    EXPECT(bad == 0, "after the release the samples are identical to the never-stuttered render (%zu differ, worst %.2e)", bad, worst);
    // a stutter that outlives the sample: nothing resumes (the sample would have ended)
    auto ref2 = run(false, 0, 0, 1500), st2 = run(true, 100, 1450, 1500);
    double tail = 0; for (size_t i = (size_t)1460 * 64; i < ref2.size(); i++) tail += fabs(st2[i]);
    EXPECT(tail < 1e-3, "stutter released after the sample's natural end: silence, no resurrected voice (%.5f)", tail);
    // BZZ / shadow with a second hit during the stutter: the new hit replaces everything on the slot
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
