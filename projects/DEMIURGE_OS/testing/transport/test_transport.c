// test_transport.c -- measures src/demiurge-transport.h (tap analyzer + 14-bit tempo wire grid).
// Host-only (no ALSA, no audio, no hardware). Build/run: bash run.sh
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "demiurge-transport.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// deterministic gaussian jitter (Box-Muller on a fixed LCG) so results are reproducible
static uint32_t rs = 12345u;
static double urand(void) { rs = rs * 1664525u + 1013904223u; return ((rs >> 8) + 0.5) / 16777216.0; }
static double grand(void) { return sqrt(-2.0 * log(urand())) * cos(6.283185307179586 * urand()); }

// the algorithm PMOR/clock.cpp used before this change: mean of consecutive intervals,
// window of TAP_MAX_N = 5 -- kept here only as the measured baseline.
static double old_mean_interval_bpm(const int64_t *t, int n) {
    int64_t sum = 0; for (int i = 1; i < n; i++) sum += t[i] - t[i - 1];
    return 60000000.0 / ((double)sum / (double)(n - 1));
}

int main(void) {
    // ---- 1. wire grid
    int bad_rt = 0; double max_q = 0.0;
    for (int w = 0; w <= DT_WIRE14_MAX; w++) {
        double bpm = dt_wire14_to_bpm((uint16_t)w);
        if (dt_bpm_to_wire14(bpm) != w) bad_rt++;
        uint8_t m = dt_wire14_msb((uint16_t)w), l = dt_wire14_lsb((uint16_t)w);
        if (dt_wire14_join(m, l) != w || m > 127 || l > 127) bad_rt++;
    }
    CHECK(bad_rt == 0, "wire14 round trip failed for %d values", bad_rt);
    for (double b = 40.0; b <= 400.0; b += 0.00137) {
        double e = fabs(dt_bpm_canon(b) - b); if (e > max_q) max_q = e;
    }
    CHECK(max_q <= 0.5 * 360.0 / 16383.0 + 1e-9, "quantisation error %.6f exceeds half a step", max_q);
    CHECK(fabs(dt_bpm_canon(dt_bpm_canon(123.456)) - dt_bpm_canon(123.456)) < 1e-12, "canon not idempotent");
    CHECK(dt_bpm_to_wire14(10.0) == 0 && dt_bpm_to_wire14(999.0) == DT_WIRE14_MAX, "wire clamp");
    CHECK(dt_bpm_to_wire14(NAN) == 0, "wire NaN");
    printf("wire grid: 16384/16384 round-trip, max quantisation error %.5f BPM (half step = %.5f)\n", max_q, 0.5 * 360.0 / 16383.0);
    printf("  120.00 -> wire %u -> %.4f ; 123.456 -> %.4f\n", dt_bpm_to_wire14(120.0), dt_bpm_canon(120.0), dt_bpm_canon(123.456));

    // ---- 2. clean taps
    {
        dt_tap_t s; dt_tap_reset(&s); double bpm = 0; int r;
        r = dt_tap_feed(&s, 1000000, &bpm); CHECK(r == DT_TAP_ARMED, "first tap should arm, got %d", r);
        r = dt_tap_feed(&s, 1500000, &bpm); CHECK(r == DT_TAP_TEMPO && fabs(bpm - 120.0) < 1e-6, "2nd tap -> 120 BPM live, got %d %.4f", r, bpm);
        for (int i = 2; i < 12; i++) r = dt_tap_feed(&s, 1000000 + 500000LL * i, &bpm);
        CHECK(fabs(bpm - 120.0) < 1e-6, "steady 120 drifted: %.6f", bpm);
        CHECK(s.n <= DT_TAP_MAX, "window overflow");
    }
    // ---- 3. duplicates / bounce ignored
    {
        dt_tap_t s; dt_tap_reset(&s); double bpm = 0;
        dt_tap_feed(&s, 0, &bpm); dt_tap_feed(&s, 500000, &bpm);
        int r = dt_tap_feed(&s, 500000 + 900, &bpm);       // 0.9 ms later: the 3% duplicate-byte case
        CHECK(r == DT_TAP_IGNORED && s.n == 2, "0.9 ms duplicate not ignored (r=%d n=%d)", r, s.n);
        r = dt_tap_feed(&s, 500000 + 119999, &bpm);
        CHECK(r == DT_TAP_IGNORED, "just under min gap should be ignored");
        r = dt_tap_feed(&s, 1000000, &bpm); CHECK(r == DT_TAP_TEMPO && fabs(bpm - 120.0) < 1e-6, "duplicate poisoned the tempo: %.4f", bpm);
    }
    // ---- 4. expiry, outlier restart, clamp, untap
    {
        dt_tap_t s; dt_tap_reset(&s); double bpm = 0; int r;
        dt_tap_feed(&s, 0, &bpm); dt_tap_feed(&s, 500000, &bpm); dt_tap_feed(&s, 1000000, &bpm);
        r = dt_tap_feed(&s, 1000000 + 2500000, &bpm);
        CHECK(r == DT_TAP_ARMED && s.n == 1, "expiry should restart window (r=%d n=%d)", r, s.n);
        dt_tap_reset(&s);
        for (int i = 0; i < 5; i++) dt_tap_feed(&s, 500000LL * i, &bpm);   // 120
        r = dt_tap_feed(&s, 500000LL * 4 + 250000, &bpm);                   // 0.5x interval = deliberate jump to 240 BPM
        CHECK(r == DT_TAP_TEMPO && fabs(bpm - 240.0) < 1e-6, "outlier should restart to the new interval, got %.3f", bpm);
        dt_tap_reset(&s);
        dt_tap_feed(&s, 0, &bpm); r = dt_tap_feed(&s, 1900000, &bpm);       // 31.6 BPM -> clamp 40
        CHECK(r == DT_TAP_TEMPO && bpm == DT_TAP_BPM_MIN, "slow clamp, got %.3f", bpm);
        dt_tap_reset(&s);
        dt_tap_feed(&s, 0, &bpm); r = dt_tap_feed(&s, 130000, &bpm);        // 461 BPM -> clamp 300
        CHECK(r == DT_TAP_TEMPO && bpm == DT_TAP_BPM_MAX, "fast clamp, got %.3f", bpm);
        dt_tap_reset(&s);
        for (int i = 0; i < 4; i++) dt_tap_feed(&s, 500000LL * i, &bpm);
        dt_tap_feed(&s, 500000LL * 4 + 100000, &bpm);   // stray late tap (inside 40 %, joins the window)
        r = dt_tap_untap(&s, &bpm);
        CHECK(r == DT_TAP_TEMPO && fabs(bpm - 120.0) < 1e-6, "untap should restore 120, got %.4f", bpm);
        dt_tap_reset(&s); CHECK(dt_tap_untap(&s, &bpm) == DT_TAP_IGNORED, "untap on empty");
    }
    // ---- 5. the measured improvement: jittery human taps, new fit vs old end-to-end mean
    {
        const double sigmas_ms[] = { 5.0, 10.0, 20.0 };
        const double true_bpm[]  = { 90.0, 120.0, 150.0 };
        printf("tap accuracy over 2000 trials per cell (5 taps, gaussian jitter on every tap):\n");
        printf("  true  jitter   old(rms err)  new(rms err)   old(max)  new(max)\n");
        for (int bi = 0; bi < 3; bi++) for (int si = 0; si < 3; si++) {
            double se_old = 0, se_new = 0, mx_old = 0, mx_new = 0; int trials = 2000;
            for (int k = 0; k < trials; k++) {
                int64_t t[5]; dt_tap_t s; dt_tap_reset(&s); double bpm = 0;
                double beat_us = 60000000.0 / true_bpm[bi];
                for (int i = 0; i < 5; i++) {
                    t[i] = (int64_t)(1000000.0 + beat_us * i + grand() * sigmas_ms[si] * 1000.0);
                    dt_tap_feed(&s, t[i], &bpm);
                }
                double eo = old_mean_interval_bpm(t, 5) - true_bpm[bi];
                double en = bpm - true_bpm[bi];
                se_old += eo * eo; se_new += en * en;
                if (fabs(eo) > mx_old) mx_old = fabs(eo);
                if (fabs(en) > mx_new) mx_new = fabs(en);
            }
            double ro = sqrt(se_old / trials), rn = sqrt(se_new / trials);
            printf("  %5.1f  %4.0fms   %8.3f      %8.3f      %7.2f   %7.2f\n", true_bpm[bi], sigmas_ms[si], ro, rn, mx_old, mx_new);
            CHECK(rn < ro, "fit not better than end-to-end mean at %.0f BPM / %.0f ms", true_bpm[bi], sigmas_ms[si]);
        }
    }
    // ---- 6. OSC pool wire
    {
        unsigned char b[128]; int src; double age;
        size_t n = dt_osc_encode(b, sizeof b, "/transport/tap", "if", 1, 12.5f);
        CHECK(n == 28, "tap datagram length %zu (want 16+4+8=28)", n);
        CHECK(dt_osc_parse_tap(b, n, &src, &age) && src == 1 && fabs(age - 12.5) < 1e-6, "tap round trip src=%d age=%f", src, age);
        // byte-exact against the OSC 1.0 spec: "/transport/tempo" ",f" 120.0 (0x42F00000)
        n = dt_osc_encode(b, sizeof b, "/transport/tempo", "f", 0, 120.0f);
        static const unsigned char want[] = { '/','t','r','a','n','s','p','o','r','t','/','t','e','m','p','o',0,0,0,0,
                                              ',','f',0,0, 0x42,0xF0,0,0 };
        CHECK(n == sizeof want && memcmp(b, want, n) == 0, "tempo datagram not byte-exact (len %zu)", n);
        n = dt_osc_encode(b, sizeof b, "/transport/start", "", 0, 0.f);
        CHECK(n == 24 && b[20] == ',' , "start datagram len %zu", n);
        n = dt_osc_encode(b, 8, "/transport/tempo", "f", 0, 1.f);
        CHECK(n == 0, "overflow not refused");
        // inbound variants
        n = dt_osc_encode(b, sizeof b, "/transport/tap", "", 0, 0.f);
        CHECK(dt_osc_parse_tap(b, n, &src, &age) && src == 2 && age == 0.0, "no-arg tap -> other/0");
        n = dt_osc_encode(b, sizeof b, "/transport/tap", "i", 0, 0.f);
        CHECK(dt_osc_parse_tap(b, n, &src, &age) && src == 0 && age == 0.0, "int-only tap -> pmor/0");
        n = dt_osc_encode(b, sizeof b, "/transport/tap", "if", 9, 3.f);
        CHECK(dt_osc_parse_tap(b, n, &src, &age) && src == 2, "out-of-range source clamps to other");
        n = dt_osc_encode(b, sizeof b, "/transport/tempo", "f", 0, 120.f);
        CHECK(!dt_osc_parse_tap(b, n, &src, &age), "wrong address accepted as tap");
        unsigned char junk[24] = {0}; memcpy(junk, "/transport/tap", 14); junk[16] = ',';
        junk[17] = 'i'; junk[18] = 'f'; junk[19] = 'f';   // type tag claims 3 args but no room / no NUL
        CHECK(!dt_osc_parse_tap(junk, 20, &src, &age), "truncated datagram accepted");
        printf("OSC: encode byte-exact vs OSC 1.0, parse round trip, rejects wrong address / truncated\n");
    }
    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
