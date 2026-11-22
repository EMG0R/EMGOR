// clock.h — master step clock. AUDIO plane owns it; CONTROL only calls clockTap()/clockSetBpm().
// Sample-accurate: the grid ACCUMULATOR (next_step/step period) is fixed-point —
// integer samples + a Q32.32 fraction (see clock.cpp) — never float. float has a
// 24-bit mantissa, so a float accumulator that adds to itself every block stops
// being sample-exact past ~16.7M samples (~5.8 min uptime) and keeps degrading
// forever after. clk.bpm/clk.step_samples/clk.next_step stay float fields for
// display and for existing external readers (engine/sequencer.cpp reads
// clk.next_step directly); they are derived FROM the fixed-point state each
// tick, never accumulated themselves, so they don't re-introduce the drift.
#pragma once
#include <stdint.h>
#include "core_config.h"

// Generative sequencer rate divisions, relative to the QUARTER note (not the 16th
// grid): a quarter note divided by this many. Triplets (1/8T, 1/16T) cannot be
// reached by dividing the 16th grid — 3 doesn't divide 4 — so the generative
// accumulator in clock.cpp runs independently off the beat period. Index into
// kGenDivNames (clock.cpp) for display.
enum GenDiv {
    GEN_DIV_1_4   = 0,   // quarter / 1
    GEN_DIV_1_8   = 1,   // quarter / 2
    GEN_DIV_1_8T  = 2,   // quarter / 3  (triplet eighth)
    GEN_DIV_1_16  = 3,   // quarter / 4  (default — matches the old fixed 16th behaviour)
    GEN_DIV_1_16T = 4,   // quarter / 6  (triplet sixteenth)
    GEN_DIV_1_32  = 5,   // quarter / 8
};
extern const char* const kGenDivNames[GEN_DIV_COUNT]; // "1/4","1/8","1/8T","1/16","1/16T","1/32"

struct ClockState {
    float    bpm;              // unquantized allowed (tap)
    float    step_samples;     // samples per 16th = SR * 15 / bpm — DISPLAY mirror of step_q, re-derived
                                // whenever step_q changes, never itself accumulated.
    float    next_step;        // absolute sample time of the next 16th — DISPLAY/legacy mirror of
                                // next_step_whole/next_step_frac below, refreshed once per clockTick().
                                // Kept only because engine/sequencer.cpp reads it directly; it carries
                                // the same float-mantissa ceiling past ~16.7M samples that the old
                                // accumulator had (unavoidable in a float field at that magnitude) —
                                // see clockSamplesToNextStep() for an exact fixed-point replacement.
    uint32_t gstep;            // 0 .. loop_len-1 (longest loop across engines)
    uint32_t loop_len;         // in 16ths; max of all engine loops
    float    step_phase;       // 0..1 inside the current 16th (for gates)
    float    swing;            // 0..1
    // shadow clock for HALF/DBL/STOP (FX) — multiplies the *effective* playback rate
    float    varispeed;        // 1.0 normal; slews
    float    varispeed_target;
    // tap (2026-10-05): the analysis itself lives in the shared analyzer
    // (demiurge-transport.h, the same code the Pi transport runs) -- see clockTap().
    // Only the PHASE snap is queued here: the tap's ISR-stamped ABSOLUTE SAMPLE (not a ms
    // wall-clock time that has to be converted through ms_at_block, which is how a tap used
    // to inherit every main-loop stall as phase error).
    bool     snap_pending;     // set by tap; ISR realigns next_step on the next block
    uint32_t snap_sample;      // absolute sample of the press (same wrap-safe domain as sample_now)
    // additive (clock.cpp): absolute sample counter + the wall-clock/sample
    // correspondence needed to convert clockTap()'s ms timestamps into a
    // sample-accurate `next_step` snap from the ISR.
    uint32_t sample_now;         // last sample_now seen by clockTick(), control-plane readable via clockSampleNow()
    uint32_t ms_at_block;        // pmorNowMs() at the start of the current block
    uint32_t sample_at_block;    // sample_now at that same instant
    float    shadow_phase;       // 0..1, varispeed-scaled accumulator behind step_phase

    // ---- fixed-point master grid accumulator (2026-10: replaces the old float
    // next_step/step_samples as the thing that actually gets added-to every block) ----
    // Format: Q32.32 unsigned fixed point. A single uint64_t `step_q` stores a
    // PERIOD (samples per 16th) as real_value = step_q / 2^32 — exact, doesn't wrap.
    // An absolute GRID TIME (next_step) is instead split into a uint32_t whole-sample
    // part + a uint32_t Q0.32 fractional part, because it must wrap at 2^32 samples
    // in lockstep with `sample_now` (a free-running uint32_t counter owned by
    // engine/audio_callback.cpp) — see clockTick()'s wrap-safe signed-difference
    // comparison instead of `>=` on raw values.
    uint64_t step_q;            // EXACT samples-per-16th, Q32.32. Canonical source of clk.step_samples.
    uint32_t next_step_whole;   // absolute sample time of the next 16th boundary, integer part (wraps with sample_now)
    uint32_t next_step_frac;    // fractional part, Q0.32 (raw/2^32), always in [0, 1) of a sample

    // ---- generative sequencer subdivision (TASK 3) — independent accumulator,
    // same Q32.32-period / whole+frac-absolute discipline as the grid above ----
    uint8_t  gen_div_idx;        // current GenDiv index; persisted by store/patch.cpp later
    uint64_t gen_step_q;         // Q32.32 samples per gen-step at gen_div_idx
    uint32_t gen_next_whole;     // absolute sample time of the next gen-step (wraps with sample_now)
    uint32_t gen_next_frac;      // Q0.32 fraction, as above

    // ---- follower phase-lock (2026-10): CONTROL plane (usb/clock_engine.c,
    // while it holds CLOCK_ROLE_DEMIURGE_MASTER) queues a correction here;
    // AUDIO plane (clockTick()) applies it at the top of the very next block.
    // Same pending-flag discipline as snap_pending/snap_ms above, and for the
    // same reason: next_step_whole/frac (and, for a reanchor, gstep) are a
    // multi-field grid mutation that clockTick() itself reads and writes every
    // block, so writing them directly from the main loop could tear against an
    // in-progress ISR read. Deferring the actual mutation into clockTick() (same
    // thread/context that owns the fields) means there is nothing to tear and
    // nothing to lock — worst case a correction lands one block (~1ms) later
    // than requested, which is inaudible at any tempo this grid runs at.
    bool     phase_reanchor_pending; // HARD: next boundary = NOW, gstep = 0 (0xFA/0xFB Start/Continue)
    bool     phase_nudge_pending;    // SOFT/snap: add phase_nudge_samples to next_step_whole
    int32_t  phase_nudge_samples;    // signed sample correction; valid only while phase_nudge_pending

    // ---- beat anchor for the MIDI clock (2026-11-06, EMGOR: loops on NEPTR must
    // land on PMOR's bar, not just its tempo). AUDIO plane writes, CONTROL plane
    // (usb/clock_engine.c master_tick_poll) reads via clock_engine_grid_beat_anchor().
    // abs16: absolute 16th count, never wraps with the loop; abs16 % 16 == gstep % 16
    // always (bar phase), so SPP = abs16 tells a listener where bar 1 is.
    // beat_*: sample time + abs16 of the latest grid BEAT (gstep % 4 == 0), published
    // seqlock-style: fields first, then beat_seq++.
    uint32_t          abs16;
    volatile uint32_t beat_sample;
    volatile uint32_t beat_abs16;
    volatile uint32_t beat_seq;
    // follower: where the next hard re-anchor puts the grid (SPP + Continue); 0 = bar start (Start)
    uint32_t reanchor_pos;
    uint32_t reanchor_sample;     // IRQ stamp of the Start (valid when reanchor_has_sample): the grid anchors THERE, not at main-loop time
    bool     reanchor_has_sample;
    // follower "clock lead" (ms, -20..+40, default 0): the grid is held this much EARLIER than the followed ticks, compensating the host's output latency. Persisted in QSPI Globals.
    float    follow_lead_ms;
};

extern ClockState clk;

void  clockInit(float bpm);
// AUDIO plane: advance by one block; returns the number of 16th boundaries crossed (0 or 1; catch-up guard).
int   clockTick(uint32_t sample_now);
// CONTROL plane
// Local tap analysis + phase snap. `tap_sample` = the press instant as an ABSOLUTE SAMPLE
// COUNT (stamped in the audio ISR by hw/controls.cpp, see encoderTakePressSample()).
// Returns DT_TAP_IGNORED(0) for a bounce/duplicate, DT_TAP_ARMED(1) when the tap only
// opened a window, DT_TAP_TEMPO(2) when the tempo changed. The tempo adopted is the
// wire-canonical (14-bit quantised) value, so it is exactly what clock_engine broadcasts.
// Only for INTERNAL/MASTER mode -- a SLAVED PMOR sends the tap to the Pi instead
// (engine/transport_link.cpp decides which).
int   clockTap(uint32_t tap_sample);
void  clockSetBpm(float bpm);               // from the parameter editor (caller quantizes)
void  clockSetLoopLen(uint32_t sixteenths); // sequencer recomputes when bars/steps change
// additive (clock.cpp), report per AGENT_RULES:
void      clockUntap();                     // removes the last tap and recomputes bpm from what remains
uint8_t   clockTapCount();                  // taps currently in the analyzer window (0 = none / expired)
uint32_t  clockSampleNow();                 // absolute sample counter, control-plane readable

// TASK 2 (2026-10): exact step period for an external consumer (usb/clock_engine.c's
// MIDI clock) that must not slip against the audio-rate grid over long uptime.
// Returns clk.step_q verbatim: Q32.32 fixed point, samples-per-16th-note.
//   real_samples_per_16th = (double)clockStepSamplesQ32() / 4294967296.0
// This is the EXACT value clockTick() adds to the grid at every boundary — no
// rounding beyond the one-time float/double -> fixed conversion done inside
// clockInit()/clockSetBpm() when bpm changes, so a consumer deriving any other
// tick rate (24 PPQN, MIDI clock, etc.) from it can free-run for days without
// drifting a sample against the audio grid. To get samples-per-quarter-note,
// multiply the real value by 4 (or shift the raw Q32.32 integer left by 2 — exact,
// no precision loss, since a quarter note is exactly 4 sixteenths).
uint64_t  clockStepSamplesQ32(void);

// TASK 3 (2026-10): generative sequencer subdivision, phase-locked to the master
// grid (re-anchored on every clockSetBpm() and every tap-tempo resync, so the two
// accumulators never walk apart).
void clockSetGenDiv(uint8_t idx);  // clamps to 0..GEN_DIV_COUNT-1; recomputes gen_step_q and re-anchors
int  clockGenTick(void);           // call once per audio block, AFTER clockTick(); returns steps fired this block (usually 0 or 1)

// ---- Follower phase-lock API (2026-10): lets an external follower clock
// (usb/clock_engine.c, while it holds CLOCK_ROLE_DEMIURGE_MASTER) pull this
// grid's PHASE into alignment with a reference clock without sacrificing the
// sample-accurate accumulator above. clockSetBpm() already handles TEMPO
// (phase-preserving by design); these three handle WHERE in the bar that
// tempo is anchored. All units are raw samples in the same wrap-safe uint32_t
// domain as clk.sample_now/next_step_whole — never seconds/ms, so a caller
// never needs a sample-rate constant. CONTROL-plane calls (same convention as
// clockTap()/clockSetBpm()): they only ever queue a correction; clockTick()
// (AUDIO plane) performs the actual mutation on the next block — see the
// phase_reanchor_pending/phase_nudge_pending fields' comment in ClockState
// for why. clock.h has no notion of "role": gating this to only run while
// actually following an external clock is the caller's job (usb/clock_engine.c
// gates every one of these calls on g_role == CLOCK_ROLE_DEMIURGE_MASTER).

// Read the grid's own upcoming 16th-boundary instant (absolute sample time,
// same value clk.next_step_whole holds) — the one piece of grid state an
// external follower needs to compute its own phase error against a reference.
uint32_t clockNextStepSample(void);

// HARD re-anchor: the grid restarts at step 0 RIGHT NOW. For 0xFA Start /
// 0xFB Continue, where MIDI transport semantics are "the bar starts here,
// forget whatever phase came before" — not a thing to slew toward. Also
// re-anchors the generative subdivision accumulator (clockGenResync()), same
// as every other grid-anchor-moving call in clock.cpp, so it never drifts
// from the master grid it rides on.
void clockReanchorNow(void);
void clockReanchorAt(uint32_t pos16);   // SPP+Continue: like clockReanchorNow() but resumes at 16th pos16 (mod loop length)

// SOFT correction: add `correction_samples` (signed, positive = push the next
// boundary later / PMOR was running ahead; negative = pull it earlier / PMOR
// was behind) to the grid's anchor. The caller decides the magnitude — pass a
// small fraction of a measured error for a slewed correction that filters bus
// jitter, or the full error for a one-shot snap when the error is too large to
// slew sensibly (see usb/clock_engine.c's follower phase-lock for both uses).
// gstep is left untouched either way — only the anchor moves, so this never
// skips or repeats a step, it just nudges when the next one lands.
void clockReanchorStamped(uint32_t pos16, uint32_t stamp);   // Start/Continue: re-anchor at the 0xFA's IRQ stamp
void clockPhaseNudge(int32_t correction_samples);

// helpers
static inline float clockStepMs() { return 15000.f / clk.bpm; }
