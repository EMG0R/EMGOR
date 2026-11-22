// clock.cpp — sample-accurate master 16th clock. See engine/clock.h for the
// AUDIO/CONTROL plane split. The grid ACCUMULATOR (what actually gets added to
// every block) is Q32.32 fixed point, never float — see the fp_* helpers below
// and clock.h's struct comment for the exact format. clk.bpm/step_samples/
// next_step stay as float fields (derived fresh each tick, never themselves
// accumulated) only because other files read them directly. clockTap()'s
// bookkeeping (and its snap conversion) is the one place that still uses
// wall-clock ms, per the ms_at_block/sample_at_block correspondence — that is
// intentional, not part of the drift bug.
#include <string.h>
#include <math.h>
#include "platform.h"
#include "clock.h"
#include "shared_state.h"
#include "engine_util.h"
#include "demiurge-transport.h"   // shared tap analyzer + 14-bit tempo wire grid (one copy, see Makefile)

ClockState clk;

const char* const kGenDivNames[GEN_DIV_COUNT] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
// divisor of the QUARTER-note period for each GenDiv index, in the same order.
static const uint32_t kGenDivisorOfBeat[GEN_DIV_COUNT] = { 1, 2, 3, 4, 6, 8 };

// STOP (varispeed_target ~= 0.06, the tape-stop floor) gets a slower ~250 ms
// fall so it reads as a real tape grinding to a halt; every other varispeed
// move (HALF/DBL/normal) slews at ~40 ms.
static const float VARISPEED_STOP_TARGET = 0.06f;
static const float VARISPEED_NORMAL_MS   = 40.f;
static const float VARISPEED_STOP_MS     = 250.f;

// -------------------------------------------------------------------------
// fixed-point helpers. Format: Q32.32 unsigned fixed point in a uint64_t —
// real_value = raw / 2^32. "frac" values below are Q0.32 (raw/2^32, always
// in [0,1)) and are used for the swing multiply, which is a fraction of a
// period, not a period itself.
// -------------------------------------------------------------------------
static inline uint64_t fp_from_double(double v) {
    return (uint64_t)(v * 4294967296.0 + 0.5);
}
static inline double fp_to_double(uint64_t q) {
    return (double)q / 4294967296.0;
}
static inline float fp_to_float(uint64_t q) {
    return (float)fp_to_double(q);
}
// Q0.32 fraction from a float known to be in [0,1) (clamped defensively).
static inline uint32_t fp_frac_from_float(float v) {
    if (v < 0.f) v = 0.f;
    if (v >= 1.f) v = 0.999999f;
    return (uint32_t)((double)v * 4294967296.0 + 0.5);
}
// Multiply a Q32.32 value `a` by a Q0.32 fraction `frac_q32` (both unsigned).
// Exact 64-bit integer math, no overflow: `a`'s whole part is always « 2^20
// here (worst case ~720000 samples/16th at 1 BPM), so both cross-term
// products below stay well inside 64 bits.
static inline uint64_t fp_mul_frac(uint64_t a, uint32_t frac_q32) {
    uint64_t a_whole = a >> 32;
    uint64_t a_frac  = a & 0xFFFFFFFFull;
    uint64_t term1   = a_whole * (uint64_t)frac_q32;         // already in Q32.32 units
    uint64_t term2   = (a_frac * (uint64_t)frac_q32) >> 32;  // cross term, Q32.32 units
    return term1 + term2;
}

// Refresh the float display mirror of the master grid's absolute next-step time
// from the fixed-point source. Not itself accumulated, so it never drifts the
// way the old `clk.next_step += step_len` loop did — it's just a fresh cast
// each tick, with the same float-mantissa ceiling any float holding a huge
// absolute sample count has (unavoidable at that magnitude; see clock.h).
static inline void clockRefreshNextStepMirror() {
    clk.next_step = (float)clk.next_step_whole + (float)clk.next_step_frac / 4294967296.f;
}

// Write an absolute grid time (in samples, as a double, always near sample_now)
// into the wrap-safe whole+frac pair.
static void clockSetAbsNextStep(double abs_samples) {
    double whole_d = floor(abs_samples);
    double frac_d  = abs_samples - whole_d;
    clk.next_step_whole = (uint32_t)(int64_t)whole_d;   // intentional 32-bit wrap, matches sample_now
    clk.next_step_frac  = (uint32_t)(frac_d * 4294967296.0 + 0.5);
    clockRefreshNextStepMirror();
}

// next_step_whole/frac += step_len_q (wrap-safe: uint32_t whole add wraps exactly
// in step with sample_now's own free-running wrap).
static inline void clockAddToNextStep(uint64_t step_len_q) {
    uint64_t frac_sum   = (uint64_t)clk.next_step_frac + (step_len_q & 0xFFFFFFFFull);
    clk.next_step_frac  = (uint32_t)frac_sum;
    clk.next_step_whole = clk.next_step_whole + (uint32_t)(step_len_q >> 32) + (uint32_t)(frac_sum >> 32);
}

// gen_next_whole/frac += clk.gen_step_q (same discipline as clockAddToNextStep).
static inline void clockAddToGenNext() {
    uint64_t frac_sum  = (uint64_t)clk.gen_next_frac + (clk.gen_step_q & 0xFFFFFFFFull);
    clk.gen_next_frac  = (uint32_t)frac_sum;
    clk.gen_next_whole = clk.gen_next_whole + (uint32_t)(clk.gen_step_q >> 32) + (uint32_t)(frac_sum >> 32);
}

// samples-per-gen-step at division `idx`, exact: a quarter note is exactly 4
// sixteenths, so step_q<<2 (a left shift) is an exact *4, no precision loss;
// dividing that by the small triplet-aware divisor table only loses < 1/2^32
// of a sample, i.e. nothing.
static inline uint64_t clockComputeGenStepQ(uint8_t idx) {
    uint64_t beat_q = clk.step_q << 2;
    return beat_q / kGenDivisorOfBeat[idx];
}

// Phase-lock: anchor the generative accumulator to the master grid's current
// next-step instant. Called whenever the master grid's anchor or period moves
// (bpm change, tap-tempo snap, or a division change) so the two never walk apart.
static inline void clockGenResync() {
    clk.gen_next_whole = clk.next_step_whole;
    clk.gen_next_frac  = clk.next_step_frac;
}

static void tapReset();   // defined with the tap analyzer below

void clockInit(float bpm) {
    float keep_lead = clk.follow_lead_ms;   // persisted setup value: survives a re-init
    memset(&clk, 0, sizeof(clk));
    clk.follow_lead_ms = keep_lead;
    tapReset();
    clk.bpm = bpm > 1.f ? bpm : 120.f;           // the BPM handover in main() must never see 0
    clk.step_q       = fp_from_double((double)SR * 15.0 / (double)clk.bpm);
    clk.step_samples = fp_to_float(clk.step_q);
    clk.next_step_whole = (uint32_t)(clk.step_q >> 32);
    clk.next_step_frac  = (uint32_t)(clk.step_q & 0xFFFFFFFFull);
    clockRefreshNextStepMirror();
    clk.loop_len = MAX_STEPS_BAR;
    clk.varispeed = clk.varispeed_target = 1.f;
    clk.gen_div_idx = GEN_DIV_1_16;
    clk.gen_step_q  = clockComputeGenStepQ(clk.gen_div_idx);
    clockGenResync();
    g.bpm = clk.bpm;
}

// Beat anchor for the MIDI clock (see ClockState::abs16 / beat_*).
static inline void clockAlignAbs16() {
    int32_t d = (int32_t)(clk.gstep % 16u) - (int32_t)(clk.abs16 % 16u);
    if (d > 7) d -= 16;
    if (d < -8) d += 16;
    clk.abs16 = (uint32_t)((int32_t)clk.abs16 + d);
}
static inline void clockPublishBeat(uint32_t sample) {
    clk.beat_sample = sample;
    clk.beat_abs16  = clk.abs16;
    PMOR_MEM_BARRIER();   // fields visible before the sequence bump
    clk.beat_seq    = clk.beat_seq + 1u;
}

int clockTick(uint32_t sample_now) {
    clk.sample_now      = sample_now;
    clk.sample_at_block = sample_now;
    clk.ms_at_block      = pmorNowMs();

    // ---- varispeed slew: one-pole toward varispeed_target. STOP falls slower.
    // (A one-pole filter coefficient, not a time accumulator — float is fine here.)
    bool  stopping = clk.varispeed_target <= (VARISPEED_STOP_TARGET + 0.005f) &&
                      clk.varispeed > clk.varispeed_target;
    float ms  = stopping ? VARISPEED_STOP_MS : VARISPEED_NORMAL_MS;
    float n   = ms * 0.001f * BLOCK_HZ;
    float a   = (n > 0.5f) ? (1.f - expf(-1.f / n)) : 1.f;
    clk.varispeed += (clk.varispeed_target - clk.varispeed) * a;
    clk.varispeed  = flushDenorm(clk.varispeed);

    // (shadow_phase kept for reference only; step_phase is derived from the MASTER
    //  grid below, so gates/stutters are always exactly where the sequencer is.
    //  HALF/DBL/STOP are handled by re-mapping which step fires, not by bending phase.)

    // ---- follower phase-lock: apply whatever usb/clock_engine.c queued since
    // the last block (see ClockState's phase_reanchor_pending/phase_nudge_pending
    // comment for why this is deferred here instead of mutated directly from the
    // main loop). Reanchor takes priority over a stale queued nudge — a fresh
    // Start/Continue means "forget any in-flight slew", not "slew on top of it".
    if (clk.phase_reanchor_pending) {
        // the boundary that fires now is step `reanchor_pos` (the fire below does gstep++ and publishes the NEW index, like the tap path's "nb - 1"):
        clk.gstep            = clk.loop_len ? (clk.reanchor_pos + clk.loop_len - 1u) % clk.loop_len : 0;
        clk.reanchor_pos     = 0;
        clk.next_step_whole  = sample_now;
        if (clk.reanchor_has_sample) {      // anchor at the Start's own IRQ stamp (a stalled main loop must not shift the grid), at most one step back
            int32_t back = (int32_t)(sample_now - clk.reanchor_sample);
            if (back > 0 && (float)back < clk.step_samples) clk.next_step_whole = clk.reanchor_sample;
        }
        clk.reanchor_has_sample = false;
        clk.next_step_frac   = 0;
        clk.phase_reanchor_pending = false;
        clk.phase_nudge_pending    = false;
        clockGenResync();
    } else if (clk.phase_nudge_pending) {
        clk.next_step_whole = (uint32_t)((int64_t)clk.next_step_whole + (int64_t)clk.phase_nudge_samples);
        clk.phase_nudge_pending = false;
        clockGenResync();
    }

    // ---- tap snap: realign next_step to the tap instant, converting the
    // tap's ms timestamp to an absolute sample time via the correspondence
    // just captured above. Done in double (one-shot, not accumulated) purely
    // because it mixes a ms timestamp in; the result is written back through
    // the fixed-point setter so the grid itself never re-enters float state.
    // ---- TAP = "the beat is NOW" (EMGOR 2026-09-15: the click must trigger the note, the grid
    // then runs on from the click; never a second note around the click, never a burst).
    //   * tap lands in the first half of a step that just fired  -> that step WAS the click:
    //     no refire, the grid is shifted so its boundary sits exactly on the tap.
    //   * otherwise                                              -> the step fires now, on the tap.
    //   In both cases the step at the tap becomes a BEAT (gstep multiple of 4), so what plays
    //   between clicks is the in-between subdivisions and the next click lands on the next beat.
    if (clk.snap_pending) {
        // The press is an ABSOLUTE SAMPLE stamped in the ISR (hw/controls.cpp) -- no wall-clock
        // conversion, so a stalled main loop (OLED page blit, SD write, I2C) no longer shifts
        // the grid anchor by however long it stalled. Usually slightly BEFORE this block.
        int32_t dsamp = (int32_t)(clk.snap_sample - clk.sample_at_block);
        double  tap   = (double)clk.sample_at_block + (double)dsamp;
        double  ss   = fp_to_double(clk.step_q);
        if (ss < 1.0) ss = 1.0;
        double  next_step_abs = (double)clk.next_step_whole + (double)clk.next_step_frac / 4294967296.0;
        double  last_boundary = next_step_abs - ss;
        double  since = tap - last_boundary;
        if (since > -ss * 0.5 && since < ss * 0.5) {          // (negative: a boundary fired between the press and now)
            clockSetAbsNextStep(tap + ss);                                // no refire: the fired step was the click
            uint32_t beat = ((clk.gstep + 2u) / 4u) * 4u;                 // that step is now the beat
            clk.gstep = (clk.loop_len ? beat % clk.loop_len : beat);
            clockAlignAbs16();
            clockPublishBeat((uint32_t)(clk.sample_at_block + (uint32_t)dsamp)); // the beat is the tap
        } else {
            clockSetAbsNextStep(tap);                                     // fire on the tap (this block)
            uint32_t nb = ((clk.gstep / 4u) + 1u) * 4u;                   // the fired step = next beat
            clk.gstep = nb - 1u;                                          // (tick's ++ lands on it)
            if (clk.loop_len && clk.gstep >= clk.loop_len) clk.gstep = clk.loop_len - 1u;
        }
        clk.snap_pending = false;
        clockGenResync();   // tap moved the grid anchor -> keep the generative accumulator locked to it
    }

    // ---- master step: catch-up guard — fire at most one boundary per block;
    // if we are behind by more than one step, resync to now instead of
    // firing a burst. Wrap-safe: compared via signed 32-bit difference against
    // sample_now, never `>=` on raw values, so this is correct across the
    // ~24.8 h wrap of the sample counter, not just within one span of it.
    int     fired = 0;
    int32_t diff  = (int32_t)(sample_now - clk.next_step_whole);
    bool    due   = diff > 0 || (diff == 0 && clk.next_step_frac == 0);
    if (due) {
        uint32_t sw_frac_q32 = fp_frac_from_float(clk.swing * 0.34f);     // up to ~1/3 swing, spec V2 L2020-2025
        uint64_t lng_q = clk.step_q + fp_mul_frac(clk.step_q, sw_frac_q32);
        uint64_t sht_q = (clk.step_q << 1) - lng_q;      // exact: average == step_q regardless of fp_mul rounding
        bool     next_is_odd = ((clk.gstep + 1) & 1u) != 0;
        uint64_t step_len_q  = next_is_odd ? lng_q : sht_q;   // odd steps lengthened
        uint32_t boundary    = clk.next_step_whole;           // the boundary firing now
        clockAddToNextStep(step_len_q);

        clk.gstep++;
        if (clk.gstep >= clk.loop_len) clk.gstep = 0;
        fired = 1;
        clk.abs16++;
        clockAlignAbs16();
        if ((clk.gstep & 3u) == 0u) clockPublishBeat(boundary); // even boundaries are swing-free

        // still behind after firing this one (a tap that fired "now", or a stall)? resync
        // rather than fire a burst: one note per boundary, ever.
        diff = (int32_t)(sample_now - clk.next_step_whole);
        due  = diff > 0 || (diff == 0 && clk.next_step_frac == 0);
        if (due) {
            clk.next_step_whole = sample_now + (uint32_t)(clk.step_q >> 32);
            clk.next_step_frac  = (uint32_t)(clk.step_q & 0xFFFFFFFFull);
        }
    }
    // master step phase 0..1 at the START of this block (effects add i/step_samples per sample).
    // whole_diff is always within one step length of 0 by construction above, so the int32
    // cast is exact (no magnitude-dependent precision loss) regardless of sample_now's size.
    if (clk.step_samples > 0.f) {
        int32_t whole_diff = (int32_t)(clk.next_step_whole - sample_now);
        float   remaining  = (float)whole_diff + (float)clk.next_step_frac / 4294967296.f;
        float   ph = 1.f - remaining / clk.step_samples;
        if (ph < 0.f) ph = 0.f; if (ph > 0.9999f) ph = 0.9999f;
        clk.step_phase = ph;
        clk.shadow_phase = ph;
    }
    clockRefreshNextStepMirror();
    return fired;
}

void clockSetBpm(float bpm) {
    if (bpm < 1.f) return;
    // The WIRE VALUE IS CANONICAL (DEMIURGE_OS docs/clock.md "Tempo is a number"): whoever
    // sets a tempo adopts the 14-bit-quantised value it will broadcast, so PMOR, the Pi and
    // NEPTR hold the identical number. Single choke point: taps, the BPM editor, patch load
    // and the bus-follower all arrive here. (Below 40 BPM the wire grid cannot represent it.)
    if (bpm >= (float)DT_WIRE_BPM_MIN && bpm <= (float)DT_WIRE_BPM_MAX) bpm = (float)dt_bpm_canon((double)bpm);
    double old_ss = fp_to_double(clk.step_q);
    double new_ss = (double)SR * 15.0 / (double)bpm;
    // keep phase: rescale the remaining time to next_step by the tempo ratio
    // instead of resetting it, so a live BPM nudge never clicks/jumps a step.
    // whole_diff is exact (next_step is always within one step of sample_now),
    // so this rescale carries none of the old float-subtraction's precision loss.
    if (old_ss > 0.0) {
        int32_t whole_diff = (int32_t)(clk.next_step_whole - clk.sample_now);
        double  remaining  = (double)whole_diff + (double)clk.next_step_frac / 4294967296.0;
        remaining *= (new_ss / old_ss);
        clockSetAbsNextStep((double)clk.sample_now + remaining);
    }
    clk.bpm = bpm;
    clk.step_q       = fp_from_double(new_ss);
    clk.step_samples = fp_to_float(clk.step_q);
    g.bpm = bpm;

    clk.gen_step_q = clockComputeGenStepQ(clk.gen_div_idx);
    clockGenResync();   // re-derive from the same anchor so gen never walks away from the grid
}

void clockSetLoopLen(uint32_t sixteenths) {
    // The master loop (= the FX automation length) is TWICE the longest engine loop, capped at
    // MAXSTEPS (EMGOR 2026-09-15: "if the main thing is 1 bar the FX get 2 bars to work with").
    // Engines run their own `gstep % their_len`, so they are unaffected.
    uint32_t n = sixteenths ? sixteenths : 1;
    n *= 2u;
    if (n > MAXSTEPS) n = MAXSTEPS;
    clk.loop_len = n;
    if (clk.gstep >= clk.loop_len) clk.gstep = 0;
}

// ---------------------------------------------------------------- tap tempo
// The analysis is the SHARED analyzer in demiurge-transport.h -- the same code the Pi
// transport (demiurge-clock) runs, so a standalone PMOR and a Pi-analysed tap feel
// identical. Rules (all measured in DEMIURGE_OS/testing/transport): a tap < 120 ms after
// the previous one is a bounce/duplicate and is dropped; a gap > 2 s starts a new window;
// tempo = least-squares fit over the last 6 taps (live from tap 2); a >40 % interval jump
// restarts the window. Timestamps are the ISR's ABSOLUTE SAMPLE count, unwrapped here to
// microseconds (the counter is 32-bit and wraps every ~24.8 h; taps are seconds apart).
static dt_tap_t s_tap;
static bool     s_tap_have_last = false;
static uint32_t s_tap_last_sample = 0;
static int64_t  s_tap_abs_samples = 0;

static inline int64_t tapSampleToUs(uint32_t sample) {
    if (!s_tap_have_last) { s_tap_abs_samples = 0; s_tap_have_last = true; }
    else s_tap_abs_samples += (int32_t)(sample - s_tap_last_sample);    // signed, wrap-safe
    s_tap_last_sample = sample;
    return (int64_t)((double)s_tap_abs_samples * (1000000.0 / (double)SR));
}

static void tapReset() { dt_tap_reset(&s_tap); s_tap_have_last = false; s_tap_abs_samples = 0; }

int clockTap(uint32_t tap_sample) {
    double bpm = 0.0;
    int r = dt_tap_feed(&s_tap, tapSampleToUs(tap_sample), &bpm);
    if (r == DT_TAP_IGNORED) return r;               // bounce: no tempo change, no phase snap
    if (r == DT_TAP_TEMPO) clockSetBpm((float)bpm);  // canonicalised inside clockSetBpm
    // EVERY accepted tap (the first one too) is a beat: the ISR realigns / fires on it next block.
    clk.snap_sample  = tap_sample;
    clk.snap_pending = true;
    return r;
}

void clockUntap() {
    double bpm = 0.0;
    if (dt_tap_untap(&s_tap, &bpm) == DT_TAP_TEMPO) clockSetBpm((float)bpm);   // < 2 taps left: leave bpm alone
}

uint8_t clockTapCount() { return s_tap.n; }

uint32_t clockSampleNow() { return clk.sample_now; }

uint64_t clockStepSamplesQ32(void) { return clk.step_q; }

void clockSetGenDiv(uint8_t idx) {
    if (idx >= GEN_DIV_COUNT) idx = GEN_DIV_COUNT - 1;
    clk.gen_div_idx = idx;
    clk.gen_step_q  = clockComputeGenStepQ(idx);
    clockGenResync();   // division changed -> re-anchor so it doesn't inherit stale phase
}

// ---- Follower phase-lock API (2026-10): see engine/clock.h for the full
// contract. All three are CONTROL-plane calls — they only read/queue state;
// clockTick() (AUDIO plane, above) performs the one-and-only mutation of
// next_step_whole/frac/gstep from these queues, so there is nothing here for
// an ISR to race against.
uint32_t clockNextStepSample(void) { return clk.next_step_whole; }

void clockReanchorAt(uint32_t pos16) {
    clk.reanchor_pos = pos16;
    clk.phase_reanchor_pending = true;
}
void clockReanchorNow(void) {
    clk.reanchor_pos = 0;
    clk.phase_reanchor_pending = true;
}

void clockReanchorStamped(uint32_t pos16, uint32_t stamp) {
    clk.reanchor_sample = stamp; clk.reanchor_has_sample = true;
    clk.reanchor_pos = pos16;
    clk.phase_reanchor_pending = true;
}

void clockPhaseNudge(int32_t correction_samples) {
    clk.phase_nudge_samples = correction_samples; // write the payload FIRST,
    clk.phase_nudge_pending = true;               // then the flag that makes clockTick() consume it —
                                                   // same ordering convention as clockTap()'s snap_ms/snap_pending.
}

// ---- C-linkage adapters for usb/clock_engine.c (plain C — can't call these
// C++-mangled symbols directly). Mirrors the clock_engine_grid_step_q32()
// adapter pattern documented in usb/clock_engine.h; declared there, defined
// here since this file is the one that actually owns the state they touch.
extern "C" {
uint32_t clock_engine_grid_next_step_sample(void) { return clockNextStepSample(); }
int32_t  clock_engine_follow_lead_samples(void) { float l = clk.follow_lead_ms; if (!(l >= -20.f && l <= 40.f)) l = 0.f; return (int32_t)(l * ((float)SR / 1000.f) + (l >= 0.f ? 0.5f : -0.5f)); }
void     clock_engine_grid_reanchor_now(void)      { clockReanchorNow(); }
bool     clock_engine_grid_reanchor_pending(void) { return clk.phase_reanchor_pending; }
void     clock_engine_grid_reanchor_at(uint32_t pos16) { clockReanchorAt(pos16); }
void     clock_engine_grid_reanchor_stamped(uint32_t pos16, uint32_t stamp) { clockReanchorStamped(pos16, stamp); }
void     clock_engine_grid_phase_nudge(int32_t correction_samples) { clockPhaseNudge(correction_samples); }
// Latest grid beat (seqlock read; retries if the ISR published mid-read).
void     clock_engine_grid_beat_anchor(uint32_t *seq, uint32_t *sample, uint32_t *abs16) {
    uint32_t s0, s1, smp, a;
    do {
        s0  = clk.beat_seq;
        PMOR_MEM_BARRIER();
        smp = clk.beat_sample;
        a   = clk.beat_abs16;
        PMOR_MEM_BARRIER();
        s1  = clk.beat_seq;
    } while (s0 != s1);
    *seq = s0; *sample = smp; *abs16 = a;
}
}

int clockGenTick(void) {
    // Same wrap-safe, catch-up-guarded shape as the master step above, except a
    // gen-step period can legitimately be shorter than a block (e.g. 1/32 at high
    // BPM), so more than one can fire in a block; cap the loop defensively so a
    // corrupt/near-zero period can never spin forever.
    int fired = 0;
    while (fired < 8) {
        int32_t diff = (int32_t)(clk.sample_now - clk.gen_next_whole);
        bool    due  = diff > 0 || (diff == 0 && clk.gen_next_frac == 0);
        if (!due) break;
        clockAddToGenNext();
        fired++;
    }
    return fired;
}
