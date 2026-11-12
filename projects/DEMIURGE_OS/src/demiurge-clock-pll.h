// src/demiurge-clock-pll.h
//
// Pure-logic core of demiurge-clock's follower-mode PLL (tick-interval
// plausibility gate + outlier-resistant averaging) and its tempo-source
// priority ladder. No ALSA, no threads, no process-global state --
// structs and functions only, so this header can be #include'd both by
// demiurge-clock.cpp (built on the Pi against real ALSA) and by
// testing/clock/*.cpp (built on any dev box, macOS included, with none
// of that). Do NOT duplicate this logic anywhere else -- a copy will
// rot. See testing/clock/ for the test harness and docs/clock.md for
// the protocol writeup.
//
#pragma once

#include <math.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------
// Follower-mode PLL: tick-interval plausibility gate
// ---------------------------------------------------------------------
//
// 24 PPQN MIDI clock: one tick every (60 / bpm / 24) seconds. The gate
// bounds an accepted external tick-to-tick interval to what's physically
// possible for a 24 PPQN clock between MIN_BPM and MAX_BPM -- anything
// outside that is not a tempo, it's either a duplicate byte (too short)
// or a stall (too long).
//
// This MUST agree with the firmware's own gate in
// firmware-tinyusb/src/clock_engine.c (min_iv/max_iv), which computes
// the identical bound in audio samples instead of seconds. If you touch
// either MAX_BPM/MIN_BPM/PPQN here, touch the firmware's copy too.
#define CLOCK_MAX_BPM 400.0
#define CLOCK_MIN_BPM 20.0
#define CLOCK_PPQN    24.0

// Fastest plausible tick (at MAX_BPM) -> shortest legal interval, i.e.
// the floor. At 400 BPM / 24 PPQN that's 6.25 ms -- comfortably above
// PMOR's known duplicate-byte offset (0.3-1.5 ms after the real tick),
// so the floor alone rejects every observed duplicate.
#define CLOCK_PLL_MIN_INTERVAL_S (60.0 / CLOCK_MAX_BPM / CLOCK_PPQN)
// Slowest plausible tick (at MIN_BPM) -> longest legal interval before
// we call it a stall instead of a very slow tempo. 125 ms at 20 BPM.
#define CLOCK_PLL_MAX_INTERVAL_S (60.0 / CLOCK_MIN_BPM / CLOCK_PPQN)

// Output BPM clamp. Separate from the gate above: this is the range the
// rest of the protocol maps CC118/CC119 into (40..400), unchanged by
// this fix.
#define CLOCK_BPM_OUT_MIN 40.0
#define CLOCK_BPM_OUT_MAX 400.0

#define CLOCK_PLL_WINDOW 24

typedef enum {
    CLOCK_PLL_FIRST_TICK,      // no previous tick to compare against yet
    CLOCK_PLL_ACCEPT,          // interval is a plausible real tick
    CLOCK_PLL_REJECT_DUPLICATE, // interval below the floor -- a dup byte
    CLOCK_PLL_STALL            // interval above the ceiling -- a gap
} clock_pll_verdict_t;

static inline clock_pll_verdict_t clock_pll_classify(double dt) {
    if (dt < CLOCK_PLL_MIN_INTERVAL_S) return CLOCK_PLL_REJECT_DUPLICATE;
    if (dt > CLOCK_PLL_MAX_INTERVAL_S) return CLOCK_PLL_STALL;
    return CLOCK_PLL_ACCEPT;
}

static inline const char *clock_pll_verdict_name(clock_pll_verdict_t v) {
    switch (v) {
        case CLOCK_PLL_FIRST_TICK:       return "first-tick";
        case CLOCK_PLL_ACCEPT:           return "accept";
        case CLOCK_PLL_REJECT_DUPLICATE: return "reject-duplicate";
        case CLOCK_PLL_STALL:            return "stall";
        default:                         return "?";
    }
}

typedef struct {
    double intervals[CLOCK_PLL_WINDOW];
    int    count;   // valid samples currently held (caps at CLOCK_PLL_WINDOW)
    int    idx;     // next ring slot to write
    int    filled;  // has the window been filled at least once
} clock_pll_window_t;

static inline void clock_pll_window_init(clock_pll_window_t *w) {
    memset(w, 0, sizeof(*w));
}

// Same as init -- named separately so call sites read as intent (e.g.
// "reset the window on a role flip / after a stall") rather than
// "construct a window", even though the operation is identical.
static inline void clock_pll_window_reset(clock_pll_window_t *w) {
    clock_pll_window_init(w);
}

static inline void clock_pll_window_push(clock_pll_window_t *w, double dt) {
    w->intervals[w->idx % CLOCK_PLL_WINDOW] = dt;
    w->idx++;
    if (w->idx >= CLOCK_PLL_WINDOW) w->filled = 1;
    if (w->count < CLOCK_PLL_WINDOW) w->count++;
}

static inline int clock_pll_cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

// Trimmed mean: sort a COPY of the window, discard the single lowest and
// single highest sample, average the rest. This is what makes the PLL
// outlier-resistant -- one bad sample surviving the gate (or one extreme
// jitter spike) can no longer swing the whole average the way a plain
// mean could (one bad sample in 24 used to shift recovered tempo ~4%).
// Falls back to a plain mean below 3 samples (nothing left to trim).
static inline double clock_pll_trimmed_mean(const clock_pll_window_t *w) {
    int n = w->count;
    if (n <= 0) return 0.0;
    double sorted[CLOCK_PLL_WINDOW];
    memcpy(sorted, w->intervals, sizeof(double) * (size_t)n);
    qsort(sorted, (size_t)n, sizeof(double), clock_pll_cmp_double);
    if (n < 3) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) sum += sorted[i];
        return sum / n;
    }
    double sum = 0.0;
    for (int i = 1; i < n - 1; i++) sum += sorted[i];
    return sum / (n - 2);
}

static inline double clock_pll_bpm_from_interval(double avg_interval_s) {
    if (avg_interval_s <= 0.0) return CLOCK_BPM_OUT_MIN;
    double bpm = 60.0 / (avg_interval_s * CLOCK_PPQN);
    if (bpm < CLOCK_BPM_OUT_MIN) bpm = CLOCK_BPM_OUT_MIN;
    if (bpm > CLOCK_BPM_OUT_MAX) bpm = CLOCK_BPM_OUT_MAX;
    return bpm;
}

// Full PLL state for one follower-mode lock. Time is tracked as
// caller-supplied monotonic seconds (double) rather than struct timespec
// so this header has zero platform dependency; the caller (the daemon,
// or a test) owns the clock source.
typedef struct {
    clock_pll_window_t window;
    double prev_tick_s;
    int    have_prev;
    double bpm;       // last recovered BPM (only meaningful if have_bpm)
    int    have_bpm;
} clock_pll_state_t;

static inline void clock_pll_state_init(clock_pll_state_t *st) {
    clock_pll_window_init(&st->window);
    st->prev_tick_s = 0.0;
    st->have_prev = 0;
    st->bpm = 0.0;
    st->have_bpm = 0;
}

typedef struct {
    clock_pll_verdict_t verdict;
    int    bpm_updated;  // 1 if st->bpm changed as a result of this call
    double bpm;          // st->bpm after this call (valid if st->have_bpm)
} clock_pll_result_t;

// Feed one external-tick timestamp into the PLL. `now_s` must be
// monotonic seconds from a stable epoch (the daemon uses time-since-
// startup to keep double precision excellent indefinitely; tests can
// use anything monotonic).
static inline clock_pll_result_t clock_pll_on_tick(clock_pll_state_t *st, double now_s) {
    clock_pll_result_t res;
    res.bpm_updated = 0;
    res.bpm = st->bpm;

    if (!st->have_prev) {
        res.verdict = CLOCK_PLL_FIRST_TICK;
        st->prev_tick_s = now_s;
        st->have_prev = 1;
        return res;
    }

    double dt = now_s - st->prev_tick_s;
    clock_pll_verdict_t v = clock_pll_classify(dt);
    res.verdict = v;

    if (v == CLOCK_PLL_STALL) {
        // A gap bigger than the slowest plausible tick: don't let it
        // become one insane interval sample dragging the average.
        // Re-anchor and start a fresh window so the *next* tick begins a
        // clean lock instead of the gap corrupting history.
        clock_pll_window_reset(&st->window);
        st->prev_tick_s = now_s;
        return res;
    }

    if (v == CLOCK_PLL_REJECT_DUPLICATE) {
        // A duplicate tick (PMOR's known ~3% double-send bug: the
        // firmware occasionally emits one 0xF8 twice, 0.3-1.5 ms apart).
        // Do NOT re-anchor: if prev_tick_s moved to the duplicate's
        // timestamp, the *next real* interval would be measured from
        // the duplicate and read short, biasing recovered tempo fast --
        // exactly the silent field bug this gate exists to prevent.
        // Leave prev_tick_s alone and drop the sample.
        return res;
    }

    // Accepted: push, re-anchor, recompute once the window has filled at
    // least once (matches the original 24-tick warm-up before any BPM
    // is reported).
    clock_pll_window_push(&st->window, dt);
    st->prev_tick_s = now_s;
    if (st->window.filled) {
        double avg = clock_pll_trimmed_mean(&st->window);
        double bpm = clock_pll_bpm_from_interval(avg);
        st->bpm = bpm;
        st->have_bpm = 1;
        res.bpm = bpm;
        res.bpm_updated = 1;
    }
    return res;
}

// ---------------------------------------------------------------------
// Tempo-source priority ladder
// ---------------------------------------------------------------------
//
// Multiple things can ask the clock to change tempo. Lowest index wins
// when more than one is "fresh" (seen within CLOCK_SRC_RECENCY_WINDOW_S):
//
//   0. HUMAN_GESTURE  -- a deliberate PMOR tap-tempo gesture (CC116=2
//                        role-claim immediately followed by a CC118
//                        steer -- see CLOCK_GESTURE_WINDOW_S below and
//                        demiurge-clock.cpp's CC118 handler for how the
//                        two are correlated). Outranks EVERYTHING,
//                        including a live external-MIDI PLL lock: if
//                        you just tapped a new tempo by hand, that's
//                        authoritative over whatever the loop or an
//                        external clock was doing a moment ago -- the
//                        loop is expected to varispeed to meet it, not
//                        the other way around.
//   1. EXTERNAL_MIDI  -- recovered 0xF8 PLL lock (PMOR / role=follower)
//   2. CC118_STEER    -- e.g. chase_bliss_clock.orc's looper-tempo push
//                        (the ROUTINE case -- a CC118 NOT adjacent to a
//                        CC116 role-claim, see HUMAN_GESTURE above)
//   3. LINK           -- Ableton Link (only consulted when role==master)
//   4. LIVE_CONF_BPM  -- startup `--bpm=` from live.conf; one-shot, never
//                        re-applied at runtime, so it never needs a
//                        recency check of its own.
//
// Below HUMAN_GESTURE, this is a SAFETY NET, not a second arbitration
// system: CC118 senders like chase_bliss_clock.orc already self-gate (it
// only writes CC118 while ITS OWN local arbiter believes the loop, not
// DEMIURGE, is authoritative -- see chase_bliss_clock.orc's "MASTER CLOCK
// COORDINATION" block, ~line 444-524 as of 2026-08). Under normal
// operation the ladder should rarely have to block anything below rung 0;
// it exists so a misbehaving or unaware sender can't yank tempo out from
// under a live external MIDI clock lock -- and so a deliberate human tap
// always can.
typedef enum {
    CLOCK_SRC_HUMAN_GESTURE = 0,
    CLOCK_SRC_EXTERNAL_MIDI = 1,
    CLOCK_SRC_CC118_STEER   = 2,
    CLOCK_SRC_LINK          = 3,
    CLOCK_SRC_LIVE_CONF_BPM = 4,
    CLOCK_SRC_COUNT
} clock_tempo_source_t;

// Recency window: a lower-priority source is blocked while a strictly
// higher-priority one has been heard within this many seconds. Matches
// chase_bliss_clock.orc's own local arbiter (`iChaseBlissExtWindow =
// 2.0`) so the two systems agree on what "fresh" means. HUMAN_GESTURE
// uses this same window too (a tap stays authoritative for 2s, long
// enough to cover the loop's own quantized varispeed catch-up) -- only
// the much shorter CLOCK_GESTURE_WINDOW_S below governs how a gesture is
// *detected* in the first place.
#define CLOCK_SRC_RECENCY_WINDOW_S 2.0

// How close a CC118 steer must land after a CC116=2 role-claim for the
// two to be treated as ONE human gesture (tap-tempo) rather than two
// unrelated events. 300ms is generous for two MIDI messages a firmware
// sends back-to-back on the same USB-MIDI write (sub-millisecond in
// practice) while staying well clear of the false-positive case: PMOR's
// CC116 re-announce cadence is ~2s (demiurge-clock.cpp's own role
// re-announce, mirrored by the firmware per clock_engine.h), so even a
// routine re-announce landing right before an unrelated routine CC118
// from chase_bliss_clock.orc has only a 300ms/2000ms ~= 15% chance of
// overlapping, and a false "gesture" classification in that case is
// harmless anyway -- a routine CC118 just gets treated as higher
// priority for one write, not a wrong value.
#define CLOCK_GESTURE_WINDOW_S 0.3

static inline const char *clock_tempo_source_name(clock_tempo_source_t s) {
    switch (s) {
        case CLOCK_SRC_HUMAN_GESTURE: return "gesture";
        case CLOCK_SRC_EXTERNAL_MIDI: return "external-midi";
        case CLOCK_SRC_CC118_STEER:   return "cc118";
        case CLOCK_SRC_LINK:          return "link";
        case CLOCK_SRC_LIVE_CONF_BPM: return "live.conf";
        default:                      return "?";
    }
}

// May a tempo request from `candidate` take effect right now, given
// `last_seen[]` (monotonic seconds, any stable epoch, same units as
// `now_s`) for every source? Blocked iff some strictly higher-priority
// source (lower enum value) has been heard within the recency window.
static inline int clock_tempo_source_may_apply(
        clock_tempo_source_t candidate,
        const double last_seen[CLOCK_SRC_COUNT],
        double now_s) {
    for (int s = 0; s < (int)candidate; s++) {
        if (now_s - last_seen[s] < CLOCK_SRC_RECENCY_WINDOW_S) return 0;
    }
    return 1;
}
