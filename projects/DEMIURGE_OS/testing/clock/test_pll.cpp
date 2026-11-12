// testing/clock/test_pll.cpp
//
// Standalone test for the PLL + tempo-source-ladder logic in
// src/demiurge-clock-pll.h. Pure logic, no ALSA -- runs on macOS (or
// anywhere) via `testing/clock/run.sh`. Includes the SAME header the
// daemon includes; there is no separate copy of the algorithm to drift.
//
// Every test asserts a numeric result and prints it. A non-zero exit
// code means a test failed.

#include "../../src/demiurge-clock-pll.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <random>

static int g_failures = 0;
static int g_tests = 0;

#define CHECK(cond, ...) do { \
    g_tests++; \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

static double tick_interval_s(double bpm) {
    return 60.0 / bpm / CLOCK_PPQN;
}

// Feed a clean, exact tick train at `bpm` for `n_ticks` and return the
// PLL's final recovered BPM (0 if it never locked).
static double run_clean_train(double bpm, int n_ticks, clock_pll_state_t *st_out = nullptr) {
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    double t = 0.0;
    double dt = tick_interval_s(bpm);
    double last_bpm = 0.0;
    for (int i = 0; i < n_ticks; i++) {
        clock_pll_result_t res = clock_pll_on_tick(&st, t);
        if (res.bpm_updated) last_bpm = res.bpm;
        t += dt;
    }
    if (st_out) *st_out = st;
    return last_bpm;
}

// ---------------------------------------------------------------------
// Test 1: clean 120 BPM tick train recovers 120.00 +/- 0.1
// ---------------------------------------------------------------------
static void test1_clean_120() {
    double bpm = run_clean_train(120.0, 200);
    printf("[test 1] clean 120 BPM train -> recovered %.4f BPM\n", bpm);
    CHECK(fabs(bpm - 120.0) < 0.1, "expected 120 +/- 0.1, got %.4f", bpm);
}

// ---------------------------------------------------------------------
// Test 2: 3%% duplicates injected at +0.3ms and +1.5ms -- still 120 +/- 0.1
//
// Also proves the regression: run the SAME duplicate-injected train
// through the OLD gate (dt > 0.0005 && dt < 2.0, plain mean) and show it
// fails -- i.e. this is a real regression test, not a tautology.
// ---------------------------------------------------------------------
struct SimTick { double t; };

static std::vector<SimTick> build_duplicate_train(double bpm, int n_ticks, double dup_fraction) {
    std::vector<SimTick> out;
    double dt = tick_interval_s(bpm);
    double t = 0.0;
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> pick01(0.0, 1.0);
    // Alternate the duplicate offset between 0.3ms and 1.5ms so both
    // ends of the spec'd range get covered.
    int dup_variant = 0;
    for (int i = 0; i < n_ticks; i++) {
        out.push_back({t});
        if (pick01(rng) < dup_fraction) {
            double offset = (dup_variant % 2 == 0) ? 0.0003 : 0.0015;
            dup_variant++;
            out.push_back({t + offset});
        }
        t += dt;
    }
    return out;
}

// Returns {final recovered bpm, worst (max abs error from `expect_bpm`)
// bpm seen at any point in the run}. The worst-case figure is what
// proves the bug/fix: a transient bias that gets diluted out of the
// trailing window by the time the run ends would otherwise hide as a
// clean-looking final value.
struct RunResult { double final_bpm; double worst_bpm; };

static RunResult run_train_new_pll(const std::vector<SimTick> &train, double expect_bpm) {
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    RunResult r{0.0, expect_bpm};
    for (auto &s : train) {
        clock_pll_result_t res = clock_pll_on_tick(&st, s.t);
        if (res.bpm_updated) {
            r.final_bpm = res.bpm;
            if (fabs(res.bpm - expect_bpm) > fabs(r.worst_bpm - expect_bpm)) r.worst_bpm = res.bpm;
        }
    }
    return r;
}

// Reimplementation of the OLD (pre-fix) gate + plain-mean average, kept
// ONLY here in the test file to prove the regression -- this is
// deliberately NOT shared with the daemon (the daemon no longer has this
// code; that's the whole point of the fix). Reproduces both pre-fix
// bugs: the loose 0.5ms/2s gate, the plain (untrimmed) mean, AND the
// unconditional re-anchor on every tick (accepted or not).
static RunResult run_train_old_gate(const std::vector<SimTick> &train, double expect_bpm) {
    static const int OLD_WINDOW = 24;
    double intervals[OLD_WINDOW];
    int idx = 0;
    bool have_prev = false;
    double prev = 0.0;
    RunResult r{0.0, expect_bpm};
    for (auto &s : train) {
        if (have_prev) {
            double dt = s.t - prev;
            if (dt > 0.0005 && dt < 2.0) {
                intervals[idx % OLD_WINDOW] = dt;
                idx++;
                if (idx >= OLD_WINDOW) {
                    double sum = 0.0;
                    for (int i = 0; i < OLD_WINDOW; i++) sum += intervals[i];
                    double avg = sum / OLD_WINDOW;
                    double bpm = 60.0 / (avg * 24.0);
                    r.final_bpm = bpm;
                    if (fabs(bpm - expect_bpm) > fabs(r.worst_bpm - expect_bpm)) r.worst_bpm = bpm;
                }
            }
        }
        prev = s.t; // OLD BUG: unconditional re-anchor, even on reject/accept alike
        have_prev = true;
    }
    return r;
}

static void test2_duplicates() {
    auto train = build_duplicate_train(120.0, 400, 0.03);
    RunResult newr = run_train_new_pll(train, 120.0);
    RunResult oldr = run_train_old_gate(train, 120.0);
    printf("[test 2] 120 BPM + 3%% duplicates (+0.3/+1.5ms):\n");
    printf("         new gate -> final %.4f BPM, worst-case %.4f BPM\n", newr.final_bpm, newr.worst_bpm);
    printf("         old gate -> final %.4f BPM, worst-case %.4f BPM (%.2f%% fast bias)\n",
           oldr.final_bpm, oldr.worst_bpm, (oldr.worst_bpm - 120.0) / 120.0 * 100.0);
    CHECK(fabs(newr.final_bpm - 120.0) < 0.1, "expected final 120 +/- 0.1 with new gate, got %.4f", newr.final_bpm);
    CHECK(fabs(newr.worst_bpm - 120.0) < 0.1, "expected worst-case 120 +/- 0.1 with new gate, got %.4f", newr.worst_bpm);
    // Regression proof: the OLD gate must show a real fast bias
    // somewhere in the run (>= 0.1 BPM off) -- if it didn't, this test
    // wouldn't actually be exercising the bug described in FIX 1.
    CHECK(fabs(oldr.worst_bpm - 120.0) >= 0.1,
          "old gate never showed a bias >= 0.1 BPM -- test is not exercising the bug (got worst-case %.4f)",
          oldr.worst_bpm);
}

// ---------------------------------------------------------------------
// Test 3: jittered train (+/-1ms uniform, realistic USB MIDI) -> within 0.5%
// ---------------------------------------------------------------------
static void test3_jitter() {
    double bpm = 120.0;
    double dt = tick_interval_s(bpm);
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    std::mt19937 rng(777);
    std::uniform_real_distribution<double> jitter(-0.001, 0.001);
    double t = 0.0;
    double last_bpm = 0.0;
    for (int i = 0; i < 300; i++) {
        double jittered_t = t + jitter(rng);
        clock_pll_result_t res = clock_pll_on_tick(&st, jittered_t);
        if (res.bpm_updated) last_bpm = res.bpm;
        t += dt;
    }
    double pct_err = fabs(last_bpm - bpm) / bpm * 100.0;
    printf("[test 3] jittered (+/-1ms) 120 BPM train -> recovered %.4f BPM (%.4f%% error)\n",
           last_bpm, pct_err);
    CHECK(pct_err < 0.5, "expected < 0.5%% error, got %.4f%%", pct_err);
}

// ---------------------------------------------------------------------
// Test 4: tempo change 120 -> 140 settles within a bounded number of ticks
// ---------------------------------------------------------------------
static void test4_tempo_change() {
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    double t = 0.0;
    // Lock at 120 first.
    double dt120 = tick_interval_s(120.0);
    for (int i = 0; i < 200; i++) {
        clock_pll_on_tick(&st, t);
        t += dt120;
    }
    // Now switch to 140 and count ticks until recovered BPM is within
    // 0.5 BPM of 140 and STAYS there.
    double dt140 = tick_interval_s(140.0);
    int settle_tick = -1;
    const int MAX_TICKS = 100;
    for (int i = 0; i < MAX_TICKS; i++) {
        clock_pll_result_t res = clock_pll_on_tick(&st, t);
        t += dt140;
        if (res.bpm_updated && fabs(res.bpm - 140.0) < 0.5 && settle_tick < 0) {
            settle_tick = i;
        }
        if (res.bpm_updated && fabs(res.bpm - 140.0) >= 0.5) {
            settle_tick = -1; // not settled yet / regressed
        }
    }
    printf("[test 4] tempo change 120 -> 140 settled within %d ticks (bound %d, window %d)\n",
           settle_tick, MAX_TICKS, CLOCK_PLL_WINDOW);
    CHECK(settle_tick >= 0 && settle_tick < MAX_TICKS,
          "expected settle within %d ticks, got %d", MAX_TICKS, settle_tick);
}

// ---------------------------------------------------------------------
// Test 5: tick stall > ceiling then resume -> re-anchors, no wedge
// ---------------------------------------------------------------------
static void test5_stall_resume() {
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    double t = 0.0;
    double dt = tick_interval_s(120.0);
    for (int i = 0; i < 200; i++) {
        clock_pll_on_tick(&st, t);
        t += dt;
    }
    double bpm_before_stall = st.bpm;

    // Stall: gap far past the ceiling (125ms) -- device unplugged for 5s.
    t += 5.0;
    clock_pll_result_t stall_res = clock_pll_on_tick(&st, t);
    bool stalled_correctly = (stall_res.verdict == CLOCK_PLL_STALL) && (stall_res.bpm_updated == 0);

    // Resume at 120 BPM after the stall -- must re-lock cleanly, not
    // wedge on a corrupted window.
    t += dt;
    double last_bpm = 0.0;
    for (int i = 0; i < 200; i++) {
        clock_pll_result_t res = clock_pll_on_tick(&st, t);
        if (res.bpm_updated) last_bpm = res.bpm;
        t += dt;
    }
    printf("[test 5] stall (%d verdict=%s bpm_updated=%d) then resume -> recovered %.4f BPM (was %.4f before stall)\n",
           stall_res.verdict == CLOCK_PLL_STALL, clock_pll_verdict_name(stall_res.verdict),
           stall_res.bpm_updated, last_bpm, bpm_before_stall);
    CHECK(stalled_correctly, "expected stall to be classified STALL with no bpm update");
    CHECK(fabs(last_bpm - 120.0) < 0.1, "expected re-lock to 120 +/- 0.1 after stall, got %.4f", last_bpm);
}

// ---------------------------------------------------------------------
// Test 6: priority ladder
// ---------------------------------------------------------------------
static void test6_priority_ladder() {
    double last_seen[CLOCK_SRC_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double now = 100.0;

    // External MIDI clock is fresh (just ticked). HUMAN_GESTURE (index 0)
    // is still at its zero-init epoch, i.e. "never seen" relative to
    // `now` -- which is exactly what every other sub-test below relies on
    // too (a last_seen of 0.0 reads as "stale" once `now` is 100.0+).
    last_seen[CLOCK_SRC_EXTERNAL_MIDI] = now;
    int allowed_while_fresh = clock_tempo_source_may_apply(CLOCK_SRC_CC118_STEER, last_seen, now);
    printf("[test 6a] CC118 while external clock fresh (dt=0s) -> may_apply=%d\n", allowed_while_fresh);
    CHECK(allowed_while_fresh == 0, "CC118 must be blocked while external clock is fresh");

    // Advance past the recency window (2.0s).
    double later = now + CLOCK_SRC_RECENCY_WINDOW_S + 0.5;
    int allowed_after_stale = clock_tempo_source_may_apply(CLOCK_SRC_CC118_STEER, last_seen, later);
    printf("[test 6b] CC118 after external clock stale (dt=%.1fs) -> may_apply=%d\n",
           later - now, allowed_after_stale);
    CHECK(allowed_after_stale == 1, "CC118 must be allowed once external clock is stale");

    // EXTERNAL_MIDI is never blocked by anything BELOW it on the ladder
    // (CC118/Link/live.conf) -- but it IS outranked by HUMAN_GESTURE,
    // which stays at its zero-init "never seen" value here, so this call
    // exercises "nothing fresher is outranking it right now", not "cannot
    // ever be outranked" (see test6d below for the gesture case).
    int ext_always = clock_tempo_source_may_apply(CLOCK_SRC_EXTERNAL_MIDI, last_seen, now);
    CHECK(ext_always == 1, "EXTERNAL_MIDI must not be blocked when no gesture is fresh");

    // Link is blocked by a fresh CC118 too (priority 2 < priority 3).
    double last_seen2[CLOCK_SRC_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};
    last_seen2[CLOCK_SRC_CC118_STEER] = now;
    int link_blocked = clock_tempo_source_may_apply(CLOCK_SRC_LINK, last_seen2, now);
    printf("[test 6c] Link while CC118 fresh -> may_apply=%d\n", link_blocked);
    CHECK(link_blocked == 0, "Link must be blocked while CC118 is fresh");

    // 6d: HUMAN_GESTURE is the new top rung -- a fresh gesture must block
    // EXTERNAL_MIDI, even though EXTERNAL_MIDI itself blocks everything
    // below it. This is the "a deliberate tap outranks a live PLL lock"
    // requirement: without this rung, a tap-tempo gesture arriving while
    // DEMIURGE is actively following PMOR's own clock ticks would be
    // silently dropped by the EXTERNAL_MIDI-vs-CC118 rule above.
    double last_seen3[CLOCK_SRC_COUNT] = {0.0, 0.0, 0.0, 0.0, 0.0};
    last_seen3[CLOCK_SRC_HUMAN_GESTURE] = now;
    int ext_blocked_by_gesture = clock_tempo_source_may_apply(CLOCK_SRC_EXTERNAL_MIDI, last_seen3, now);
    printf("[test 6d] EXTERNAL_MIDI while a human gesture is fresh -> may_apply=%d\n", ext_blocked_by_gesture);
    CHECK(ext_blocked_by_gesture == 0, "a fresh human gesture must outrank EXTERNAL_MIDI");

    // 6e: HUMAN_GESTURE itself is never blocked by anything (it is rung 0).
    int gesture_always = clock_tempo_source_may_apply(CLOCK_SRC_HUMAN_GESTURE, last_seen3, now);
    CHECK(gesture_always == 1, "HUMAN_GESTURE must never be blocked");
}

// ---------------------------------------------------------------------
// Test 7: realistic PMOR tick-stream simulation over a long window.
//
// Models the ACTUAL measured hardware behaviour, all three effects
// stacked on the same stream simultaneously (not one-at-a-time like
// tests 2/3 above):
//   - 24 PPQN @ 120 BPM nominal
//   - ~3% of ticks duplicated, duplicate arriving 0.3-1.5ms after the
//     original (measured range)
//   - +/-1ms uniform USB-frame jitter on EVERY tick's arrival time
//     (applied to the real tick, not the duplicate's offset)
//   - occasional USB batches: ~1% of ticks land in a compressed burst of
//     2-3 back-to-back arrivals (8-15ms apart -- above the duplicate
//     floor at 400 BPM/6.25ms, but faster than the true 120 BPM/20.8ms
//     tick), immediately followed by one compensating longer gap so the
//     stream's TRUE average rate is unaffected -- this is what a USB
//     transfer stall-then-flush looks like at the daemon's receive
//     timestamp.
// Run over a long window (minutes of ticks) to catch slow drift the
// short tests miss: track BPM every time it updates and assert the
// worst-case deviation across the WHOLE run, not just the final value.
// ---------------------------------------------------------------------
// Model: `true_t` is the ideal, never-perturbed 24 PPQN schedule (what
// SHOULD arrive, and what the next non-burst tick resyncs to). `last_emit`
// is the timestamp of the most recent tick actually pushed. Ordinary
// ticks emit at true_t + jitter. A burst instead emits the next
// `burst_n` ticks compressed to `burst_gap_lo..burst_gap_hi` seconds
// after `last_emit` -- and does NOT touch true_t. Since the burst ticks
// land ahead of where true_t expects them, the FIRST post-burst ordinary
// tick (real_t = true_t + jitter) lands one long catch-up gap after
// `last_emit`, automatically repaying the compression -- exactly the
// "USB stalls, then flushes several ticks, then catches up" shape, with
// the long-run average rate untouched by construction (burst ticks never
// move true_t).
// `mode == kBurstNearSimultaneous`: burst extras are emitted as tiny
// offsets off the SAME true tick (like a fatter duplicate) -- true_t is
// untouched by them, exactly like the existing 3% duplicate mechanism,
// just with more copies. This models "several ticks arrive in one USB
// transfer" as-received-nearly-coincident.
//
// `mode == kBurstSpacedAheadOfSchedule`: burst extras are emitted
// `burst_gap` apart, anchored to `last_emit` (NOT true_t) -- so they
// run ahead of the ideal schedule, and the first post-burst ordinary
// tick (anchored back to true_t) pays the compression back as one long
// catch-up interval. This is the adversarial shape used by 7b.
enum BurstMode { kBurstNearSimultaneous, kBurstSpacedAheadOfSchedule };

static std::vector<SimTick> build_realistic_pmor_train(
        double bpm, double duration_s, double burst_prob,
        double burst_gap_lo, double burst_gap_hi, int burst_n_max,
        BurstMode mode) {
    std::vector<SimTick> out;
    double dt = tick_interval_s(bpm);
    std::mt19937 rng(2026);
    std::uniform_real_distribution<double> pick01(0.0, 1.0);
    std::uniform_real_distribution<double> jitter(-0.001, 0.001);
    std::uniform_real_distribution<double> burst_gap(burst_gap_lo, burst_gap_hi);
    int dup_variant = 0;

    double true_t = 0.0;
    double last_emit = 0.0;
    int burst_remaining = 0;
    bool first = true;
    while (true_t < duration_s) {
        double real_t;
        if (burst_remaining > 0 && mode == kBurstSpacedAheadOfSchedule) {
            real_t = last_emit + burst_gap(rng);
            burst_remaining--;
        } else {
            real_t = first ? 0.0 : true_t + jitter(rng);
        }
        first = false;
        out.push_back({real_t});
        last_emit = real_t;

        // 3% duplicate, 0.3/1.5ms alternating (same as test 2).
        if (pick01(rng) < 0.03) {
            double offset = (dup_variant % 2 == 0) ? 0.0003 : 0.0015;
            dup_variant++;
            out.push_back({real_t + offset});
        }

        // burst_prob chance: the NEXT 2..burst_n_max ticks land as a
        // burst. In kBurstNearSimultaneous mode, emit them right here as
        // tiny same-true-tick offsets (true_t untouched, just like the
        // duplicate mechanism above but with more copies).
        if (burst_remaining == 0 && burst_n_max >= 2 && pick01(rng) < burst_prob) {
            int n = 2 + (int)(pick01(rng) * (burst_n_max - 1));
            if (mode == kBurstNearSimultaneous) {
                for (int b = 0; b < n; b++) {
                    out.push_back({real_t + burst_gap(rng)});
                }
            } else {
                burst_remaining = n;
            }
        }

        true_t += dt;
    }
    return out;
}

static double run_train_worst_and_final(const std::vector<SimTick> &train, double bpm, int warmup_updates, double *final_out) {
    clock_pll_state_t st;
    clock_pll_state_init(&st);
    double worst_bpm = bpm;
    double final_bpm = 0.0;
    int updates = 0;
    for (auto &s : train) {
        clock_pll_result_t res = clock_pll_on_tick(&st, s.t);
        if (res.bpm_updated) {
            updates++;
            final_bpm = res.bpm;
            if (updates > warmup_updates && fabs(res.bpm - bpm) > fabs(worst_bpm - bpm)) {
                worst_bpm = res.bpm;
            }
        }
    }
    if (final_out) *final_out = final_bpm;
    return worst_bpm;
}

// 7a: the REALISTIC measured case -- dup fraction/offsets and jitter as
// measured, plus USB-batch bursts modeled as near-simultaneous arrivals
// (well under the 6.25ms duplicate floor, like a fatter duplicate --
// this is genuinely what "several ticks arrive in one USB transfer"
// looks like at 24 PPQN/120 BPM: the transfer's ticks were essentially
// coincident, not evenly spaced 8-15ms apart). Expect tight (<0.1%)
// accuracy since the gate rejects the whole burst as duplicates.
static void test7a_realistic_pmor_stream() {
    const double bpm = 120.0;
    const double duration_s = 180.0; // 3 simulated minutes
    auto train = build_realistic_pmor_train(bpm, duration_s, /*burst_prob=*/0.01,
                                             /*gap_lo=*/0.0005, /*gap_hi=*/0.003, /*n_max=*/3,
                                             kBurstNearSimultaneous);
    double final_bpm = 0.0;
    double worst_bpm = run_train_worst_and_final(train, bpm, /*warmup_updates=*/2, &final_bpm);
    double final_pct = fabs(final_bpm - bpm) / bpm * 100.0;
    double worst_pct = fabs(worst_bpm - bpm) / bpm * 100.0;
    printf("[test 7a] realistic PMOR stream (3%% dup + +/-1ms jitter + near-simultaneous USB bursts, %.0fs, %zu raw ticks) ->\n"
           "          final (long-run/settled) %.4f BPM (%.4f%% err), worst-case SINGLE-WINDOW TRANSIENT %.4f BPM (%.4f%% err)\n",
           duration_s, train.size(), final_bpm, final_pct, worst_bpm, worst_pct);
    // Long-run/settled accuracy: the spec target (0.1%) -- this is what
    // "recovered BPM" means for any purpose that isn't sample-accurate
    // (UI display, retuning other gear, etc.).
    CHECK(final_pct < 0.1, "expected final BPM within 0.1%% of %.1f, got %.4f (%.4f%% err)", bpm, final_bpm, final_pct);
    // Worst-case is measured across EVERY window update in the whole
    // 3-minute run, not just the end -- so it also captures the brief
    // single-window transient right after a burst before the next
    // window rolls the outlier out. Measured ~0.5% with this stream;
    // 1.0% is a regression guard rail with headroom, not a claim that
    // 1.0% is fine -- if this creeps up, the trimmed-mean window or
    // burst behaviour changed and is worth a fresh look.
    CHECK(worst_pct < 1.0, "worst-case single-window transient regressed past the 1%% guard rail: %.4f%% (was ~0.5%% when this test was written)", worst_pct);
}

// 7b: ADVERSARIAL burst stress test -- same stream, but bursts spaced
// 8-15ms apart (above the 6.25ms duplicate floor, so each burst tick is
// individually ACCEPTED as a plausible-but-wrong interval, not rejected
// as a duplicate). This is a genuine, real finding from building this
// simulation: clock_pll_trimmed_mean() only discards ONE lowest and ONE
// highest sample per 24-tick window -- it is single-outlier-resistant,
// not multi-outlier-resistant. A burst of >=2 fast-but-plausible samples
// landing in the same window is NOT fully filtered and produces a real,
// if transient (self-correcting within ~1 window), BPM spike. This is
// reported, not silently loosened away: if real PMOR hardware bursts
// turn out to land in this 8-15ms-apart band (rather than the <3ms
// near-simultaneous band 7a models), expect brief recovered-BPM spikes
// on hardware. Bounded assertion here just guards against a REGRESSION
// making it worse than currently measured, not a claim this is fine.
static void test7b_adversarial_burst_stress() {
    const double bpm = 120.0;
    const double duration_s = 180.0;
    auto train = build_realistic_pmor_train(bpm, duration_s, /*burst_prob=*/0.01,
                                             /*gap_lo=*/0.008, /*gap_hi=*/0.015, /*n_max=*/3,
                                             kBurstSpacedAheadOfSchedule);
    double final_bpm = 0.0;
    double worst_bpm = run_train_worst_and_final(train, bpm, /*warmup_updates=*/2, &final_bpm);
    double final_pct = fabs(final_bpm - bpm) / bpm * 100.0;
    double worst_pct = fabs(worst_bpm - bpm) / bpm * 100.0;
    printf("[test 7b] ADVERSARIAL burst stress (8-15ms-spaced multi-tick bursts, %.0fs, %zu raw ticks) ->\n"
           "          final %.4f BPM (%.4f%% err), worst-case TRANSIENT %.4f BPM (%.4f%% err)\n"
           "          NOTE: worst-case is a known limitation of single-outlier trimmed-mean filtering,\n"
           "          not a bug -- see the comment above this test. Watch for it on real hardware.\n",
           duration_s, train.size(), final_bpm, final_pct, worst_bpm, worst_pct);
    // The long-run average must still be excellent (bursts are self-
    // cancelling by construction) even though the transient can spike.
    CHECK(final_pct < 0.1, "expected final (long-run) BPM within 0.1%% of %.1f, got %.4f (%.4f%% err)", bpm, final_bpm, final_pct);
    // Regression guard on the transient, not a claim it's acceptable --
    // current measured worst-case is ~12%; fail loudly if it gets worse.
    CHECK(worst_pct < 20.0, "adversarial burst transient regressed past the 20%% guard rail: %.4f%% (was ~12%% when this test was written)", worst_pct);
}

// ---------------------------------------------------------------------
// Test 8: start/stop and role-flip churn.
//
// Simulates CC116 flipping repeatedly (device plugged/unplugged/
// replugged) by calling clock_pll_state_init() -- exactly what
// demiurge-clock.cpp's input_thread does on every accepted role flip
// (see src/demiurge-clock.cpp ~line 463) -- interleaved with clean
// lock runs, mid-stream stalls (clock disappearing entirely), and
// resumes. Asserts: no wedge (always keeps making progress), no
// NaN/negative/zero BPM ever escapes once locked, PLL state resets
// cleanly on every role change (have_bpm/window back to empty), and a
// stall freewheels -- keeps the last good BPM rather than zeroing it.
// ---------------------------------------------------------------------
static bool bpm_is_sane(double bpm) {
    return std::isfinite(bpm) && bpm > 0.0;
}

static void test8_role_flip_churn() {
    std::mt19937 rng(99);
    std::uniform_real_distribution<double> bpm_pick(60.0, 180.0);
    int role_flips = 0;
    int stalls = 0;
    int sane_checks = 0;
    bool any_insane = false;

    for (int round = 0; round < 25; round++) {
        clock_pll_state_t st;
        clock_pll_state_init(&st);
        role_flips++;
        // Fresh state after a role flip must report no lock yet.
        CHECK(st.have_bpm == 0, "round %d: expected have_bpm==0 immediately after role-flip reset", round);

        double bpm = bpm_pick(rng);
        double dt = tick_interval_s(bpm);
        double t = 0.0;
        double last_bpm = 0.0;
        // Lock cleanly.
        for (int i = 0; i < 60; i++) {
            clock_pll_result_t res = clock_pll_on_tick(&st, t);
            if (res.bpm_updated) {
                last_bpm = res.bpm;
                sane_checks++;
                if (!bpm_is_sane(res.bpm)) any_insane = true;
            }
            t += dt;
        }
        CHECK(last_bpm > 0.0, "round %d: expected a clean lock before churn, got %.4f", round, last_bpm);

        // Mid-stream: device disappears (a stall -- gap way past the
        // ceiling), simulating unplug without an explicit role flip
        // (the daemon has no way to know it was unplugged until either
        // a timeout policy elsewhere fires or a replug/role-flip
        // arrives -- this PLL layer's job is just to not corrupt state
        // across the gap).
        t += 3.0 + bpm_pick(rng) / 100.0; // several seconds of silence
        stalls++;
        clock_pll_result_t stall_res = clock_pll_on_tick(&st, t);
        CHECK(stall_res.verdict == CLOCK_PLL_STALL, "round %d: expected STALL verdict on device-disappear gap", round);
        // Freewheel: bpm_updated must NOT fire on a stall, and the
        // last-known BPM must survive untouched (st.bpm, not res.bpm --
        // res.bpm mirrors st.bpm either way, this just documents both).
        CHECK(stall_res.bpm_updated == 0, "round %d: stall must not report a bpm update", round);
        CHECK(fabs(st.bpm - last_bpm) < 1e-9, "round %d: freewheel must hold the exact last-locked BPM across a stall, was %.4f now %.4f", round, last_bpm, st.bpm);
        if (!bpm_is_sane(st.bpm)) any_insane = true;

        // Replug: device comes back at a (possibly different) tempo,
        // WITHOUT a role-flip reset this time -- the window it re-locks
        // through is the post-stall (reset-by-stall) window, proving
        // the stall's own reset was clean too, independent of CC116.
        double bpm2 = bpm_pick(rng);
        double dt2 = tick_interval_s(bpm2);
        t += dt2;
        double relock_bpm = 0.0;
        for (int i = 0; i < 60; i++) {
            clock_pll_result_t res = clock_pll_on_tick(&st, t);
            if (res.bpm_updated) {
                relock_bpm = res.bpm;
                sane_checks++;
                if (!bpm_is_sane(res.bpm)) any_insane = true;
            }
            t += dt2;
        }
        CHECK(fabs(relock_bpm - bpm2) < 0.1, "round %d: expected re-lock to %.4f +/- 0.1 after stall+replug, got %.4f", round, bpm2, relock_bpm);
    }

    printf("[test 8] role-flip/stall/replug churn: %d role-flips, %d stalls, %d sane bpm checks, any_insane=%d\n",
           role_flips, stalls, sane_checks, any_insane);
    CHECK(!any_insane, "expected every reported BPM across all churn to be finite and > 0, saw at least one violation");
}

// ---------------------------------------------------------------------
// Test 9: adversarial/fuzz input -- garbage timestamps must never
// produce a NaN/negative BPM that then poisons the window for future
// good ticks. In practice `now_s` comes from CLOCK_MONOTONIC on the
// daemon (never NaN, never goes backwards), so this is a defense-in-
// depth check rather than a reachable field bug -- but it's cheap and
// the header has zero platform dependency, so nothing stops a future
// caller (a test, a sim, a refactor) from feeding it something odd.
// ---------------------------------------------------------------------
static void test9_fuzz_garbage_input() {
    // 9a: a backwards-time tick (dt < 0) must classify as
    // REJECT_DUPLICATE (dt < MIN_INTERVAL, since MIN_INTERVAL > 0), not
    // silently accepted.
    {
        clock_pll_state_t st;
        clock_pll_state_init(&st);
        clock_pll_on_tick(&st, 10.0);
        clock_pll_result_t res = clock_pll_on_tick(&st, 5.0); // time went backwards
        printf("[test 9a] backwards-time tick -> verdict=%s bpm_updated=%d\n",
               clock_pll_verdict_name(res.verdict), res.bpm_updated);
        CHECK(res.verdict == CLOCK_PLL_REJECT_DUPLICATE, "expected backwards dt to be rejected as REJECT_DUPLICATE, got %s",
              clock_pll_verdict_name(res.verdict));
        CHECK(res.bpm_updated == 0, "backwards-time tick must not update bpm");
    }

    // 9b: a NaN arrival timestamp. dt becomes NaN; both gate comparisons
    // (dt < MIN, dt > MAX) are false for NaN in IEEE 754, so this falls
    // through to CLOCK_PLL_ACCEPT and gets pushed into the window --
    // documenting the actual (not necessarily desired) behaviour rather
    // than assuming a specific one, since fixing this is out of scope
    // unless it's reachable. Flagged loudly if it poisons a locked BPM.
    {
        clock_pll_state_t st;
        clock_pll_state_init(&st);
        double bpm_before = run_clean_train(120.0, 200, &st);
        (void)bpm_before;
        double nan_t = std::nan("");
        clock_pll_result_t res = clock_pll_on_tick(&st, nan_t);
        bool bpm_poisoned = res.bpm_updated && !std::isfinite(res.bpm);
        printf("[test 9b] NaN arrival timestamp -> verdict=%s bpm_updated=%d bpm=%.4f poisoned=%d\n",
               clock_pll_verdict_name(res.verdict), res.bpm_updated, res.bpm, bpm_poisoned);
        if (bpm_poisoned) {
            fprintf(stderr, "NOTE (not a hard failure): clock_pll_on_tick(NaN) is classified %s and "
                    "produces a non-finite bpm -- the gate does not special-case NaN. Unreachable in "
                    "practice (CLOCK_MONOTONIC never yields NaN) but worth knowing if this header is "
                    "ever fed anything other than a monotonic clock.\n",
                    clock_pll_verdict_name(res.verdict));
        }
        // Recovery: feed 24 more clean ticks after the NaN poke and
        // confirm the window eventually flushes the NaN out and
        // re-locks to a sane value -- this is the part that actually
        // matters (a transient bad sample must not wedge the daemon
        // forever).
        double t = 1000.0;
        double dt = tick_interval_s(120.0);
        double recovered = 0.0;
        for (int i = 0; i < CLOCK_PLL_WINDOW + 2; i++) {
            clock_pll_result_t r = clock_pll_on_tick(&st, t);
            if (r.bpm_updated) recovered = r.bpm;
            t += dt;
        }
        printf("[test 9b] after 26 clean ticks post-NaN -> recovered %.4f BPM\n", recovered);
        CHECK(std::isfinite(recovered) && recovered > 0.0, "expected recovery to a finite, positive BPM after the window flushes the NaN sample, got %.4f", recovered);
    }

    // 9c: an exactly-zero dt (two ticks that timestamp identically) --
    // must be rejected the same as any too-short interval, never
    // treated as a valid 0-length tick.
    {
        clock_pll_state_t st;
        clock_pll_state_init(&st);
        clock_pll_on_tick(&st, 42.0);
        clock_pll_result_t res = clock_pll_on_tick(&st, 42.0);
        printf("[test 9c] zero-dt tick -> verdict=%s bpm_updated=%d\n",
               clock_pll_verdict_name(res.verdict), res.bpm_updated);
        CHECK(res.verdict == CLOCK_PLL_REJECT_DUPLICATE, "expected dt==0 to be REJECT_DUPLICATE, got %s", clock_pll_verdict_name(res.verdict));
        CHECK(res.bpm_updated == 0, "zero-dt tick must not update bpm");
    }
}

int main() {
    printf("== demiurge-clock PLL / ladder test suite ==\n");
    printf("CLOCK_PLL_MIN_INTERVAL_S = %.6f s (%.2f ms, floor at %.0f BPM)\n",
           CLOCK_PLL_MIN_INTERVAL_S, CLOCK_PLL_MIN_INTERVAL_S * 1000.0, CLOCK_MAX_BPM);
    printf("CLOCK_PLL_MAX_INTERVAL_S = %.6f s (%.1f ms, ceiling at %.0f BPM)\n",
           CLOCK_PLL_MAX_INTERVAL_S, CLOCK_PLL_MAX_INTERVAL_S * 1000.0, CLOCK_MIN_BPM);
    printf("\n");

    test1_clean_120();
    test2_duplicates();
    test3_jitter();
    test4_tempo_change();
    test5_stall_resume();
    test6_priority_ladder();
    test7a_realistic_pmor_stream();
    test7b_adversarial_burst_stress();
    test8_role_flip_churn();
    test9_fuzz_garbage_input();

    printf("\n%d/%d checks passed\n", g_tests - g_failures, g_tests);
    if (g_failures > 0) {
        printf("%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
