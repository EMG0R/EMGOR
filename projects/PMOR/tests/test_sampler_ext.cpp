// Sampler per-slot START / END / LOOP / ATTACK / RELEASE: defaults identical to the old voice; the extras do what they say.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "sampler.h"
#include "shared_state.h"
#include "clock.h"
#include "fx.h"
#include "params.h"
#include "synth.h"
static int fails;
#define EXPECT(c, ...) do { if (c) { printf("  ok   "); printf(__VA_ARGS__); puts(""); } else { printf("  FAIL "); printf(__VA_ARGS__); puts(""); fails++; } } while (0)
static void boot() { memset(&g, 0, sizeof g); clockInit(120.f); fxInit(); synthInit(); samplerInit(); for (int i = 0; i < 20000; i++) smp_buf[0][i] = 0.5f * sinf(i * 0.03f) + 0.25f * (float)((i / 2000) % 2); smp.len[0] = 20000; smp.polyphony = 4.f; }
static std::vector<float> render(int blocks) { std::vector<float> o; float in0[64] = {0}, L[64], R[64]; for (int b = 0; b < blocks; b++) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); samplerProcess(in0, L, R, 64); for (int i = 0; i < 64; i++) o.push_back(L[i] + R[i]); } return o; }
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b && i < v.size(); i++) s += v[i] * v[i]; return sqrt(s / (double)(b - a)); }
int main() {
    boot(); samplerPlay(0, 1.f); auto ref = render(400);
    boot(); smp_ext[0].start = 0.f; smp_ext[0].end = 1.f; smp_ext[0].loop_f = 0.f; samplerPlay(0, 1.f); auto def = render(400);
    EXPECT(ref == def, "defaults: bit-identical to the old voice (%zu samples)", ref.size());
    boot(); smp_ext[0].start = 0.5f; samplerPlay(0, 1.f); auto st = render(400);
    boot(); smp.slot[0].reverse = true; samplerPlay(0, 1.f); (void) render(1);
    boot(); smp_ext[0].end = 0.25f; samplerPlay(0, 1.f); auto en = render(400);
    size_t endAt = 0; for (size_t i = en.size(); i-- > 0;) if (fabsf(en[i]) > 1e-4f) { endAt = i; break; }
    EXPECT(endAt > 4500 && endAt < 5600, "END 0.25 stops playback at ~5000 samples (last audible %zu)", endAt);
    size_t stAt = 0; for (size_t i = st.size(); i-- > 0;) if (fabsf(st[i]) > 1e-4f) { stAt = i; break; }
    EXPECT(stAt > 9500 && stAt < 10600, "START 0.5 plays only the second half (last audible %zu, ~10000 samples)", stAt);
    boot(); smp_ext[0].start = 0.1f; smp_ext[0].end = 0.2f; smp_ext[0].loop_f = 1.f; samplerPlay(0, 1.f); auto lp = render(300);   // 19200 samples, region = 2000 samples
    EXPECT(rms(lp, 12000, 19000) > 0.05, "LOOP keeps sounding long after one pass of the 2000-sample region (rms %.3f)", rms(lp, 12000, 19000));
    boot(); smp_ext[0].start = 0.1f; smp_ext[0].end = 0.2f; samplerPlay(0, 1.f); auto nl = render(300);
    EXPECT(rms(nl, 12000, 19000) < 1e-4, "same region without LOOP is silent after the first pass (rms %.5f)", rms(nl, 12000, 19000));
    boot(); smp_ext[0].attack_ms = 100.f; samplerPlay(0, 1.f); auto at = render(100);
    EXPECT(rms(at, 0, 200) < 0.2 * rms(ref, 0, 200) + 1e-6, "ATTACK 100 ms: the first 4 ms are much quieter than the 2 ms default (%.4f vs %.4f)", rms(at, 0, 200), rms(ref, 0, 200));
    boot(); smp_ext[0].end = 0.5f; smp_ext[0].release_ms = 200.f; samplerPlay(0, 1.f); auto rl = render(400);
    size_t tail = 0; for (size_t i = rl.size(); i-- > 0;) if (fabsf(rl[i]) > 1e-4f) { tail = i; break; }
    EXPECT(tail > 10000 + 5000, "RELEASE 200 ms rings out ~9600 samples past the end (last audible %zu)", tail);
    boot(); smp_ext[3].start = 0.3f; smp_ext[5].loop_f = 1.f; samplerSwapSlots(3, 5); EXPECT(smp_ext[5].start == 0.3f && smp_ext[3].loop_f == 1.f, "slot swap carries the extras");
    samplerExtReset(); EXPECT(smp_ext[5].start == 0.f && smp_ext[3].loop_f == 0.f && smp_ext[0].attack_ms == 2.f, "reset (patch load) restores the defaults");
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
