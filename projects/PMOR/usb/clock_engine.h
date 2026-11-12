// clock_engine.h — MIDI clock master/follower + role negotiation + PO
// (Pocket Operator / Volca) 2-PPQN sync engine (SY0-SY5), ported from
// pocket-OpGorator.ino's clock/PO-sync work with the bugs an adversarial
// review found in that .ino FIXED before landing here. See the "Ported
// from the .ino, with fixes" section in README.md for the full list; the
// short version:
//
//   1. Tick scheduling is done entirely in wrap-safe uint32_t sample
//      units (signed-subtraction comparison), not an ever-growing double
//      compared against a wrapping uint32_t -- the .ino's version stops
//      emitting ticks after ~24.9h of uptime.
//   2. Tempo has exactly one writer at a time, chosen by explicit
//      priority (USB-MIDI follower PLL > PO-sync-in > local, the last of
//      which this scaffold doesn't implement -- no encoders here). PO
//      sync input is *never* allowed to steer tempo while this device is
//      following the Demiurge bus clock, and gets its own
//      plausibility+stability gate so mic-fed room noise (see
//      pocket-OpGorator.md's Signal Flow: the MAX4466 auto-switches onto
//      AUDIO_IN_L/R whenever no cable is inserted) can't hijack tempo
//      through an unpatched SY3/SY4/SY5 input.
//   3. Role is NOT re-claimed unconditionally on every boot. This
//      firmware has no verified-retained persistent store on this board
//      (STM32H7 backup-domain SRAM needs VBAT held across power loss,
//      which this board's power-switch wiring does not guarantee -- see
//      README), so instead of persisting-and-maybe-lying about it, the
//      device boots SILENT: it listens for ~2.5s (long enough to catch
//      one full daemon role-announce cycle, see demiurge-clock.cpp's ~2s
//      cadence) before claiming master, and adopts whatever role it
//      hears instead if one arrives first.
//   4. While this device holds MASTER role, ALL incoming transport
//      (Start/Continue/Stop) is ignored -- a master takes no orders from
//      the bus it drives, mirroring the pre-existing (correct) 0xF8
//      self-echo gating.
//   5. PO-sync-out click waveform is runtime-selectable between a short
//      unipolar pulse (default) and a 15ms 1kHz tone burst -- the design
//      spec and the hardware doc describe two different things, and
//      which one actually works through the PCM3060's AC-coupled output
//      into real PO/Volca hardware needs a scope, not a guess.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "tusb_config.h"  // CFG_TUD_CDC, for the CDC-only declarations below

#ifdef __cplusplus
extern "C" {
#endif

// ---- PO / Volca sync modes, exactly pocket-OpGorator.md's SY0-SY5 table.
typedef enum {
  SY0_STEREO = 0,  // no sync; audio L/R passthrough (bit-exact)
  SY1_OUT_L,       // master: click out on L, mono audio on R
  SY2_OUT_R,       // master: click out on R, mono audio on L
  SY3_IN_L,        // slave: edge-detect on L, mono audio (from R) on both outs
  SY4_IN_R,        // slave: edge-detect on R, mono audio (from L) on both outs
  SY5_PASS,        // passthrough: sync repeater on L, mono audio chain on R
} sync_mode_t;

// Click waveform for SY1/SY2/SY5 render -- see README "PO sync click
// waveform" for why this needs to be A/B-tested against real hardware.
typedef enum {
  CLICK_WAVE_SHORT_PULSE = 0,  // ~2.5ms unipolar-level plateau (default)
  CLICK_WAVE_TONE_BURST  = 1,  // ~15ms 1kHz tone burst (pocket-OpGorator.md wording)
} click_wave_t;

// Clock role. Values chosen to match ch16 CC116 exactly (see
// demiurge-clock.cpp): 1 = OpGorator is master (this firmware drives the
// bus), 2 = Demiurge is master (this firmware follows the bus).
typedef enum {
  CLOCK_ROLE_OPG_MASTER      = 1,
  CLOCK_ROLE_DEMIURGE_MASTER = 2,
} clock_role_t;

// ---- Lifecycle -----------------------------------------------------
// Call once from main(), after usb_audio_stream_init()/tusb_init() but
// before hw.StartAudio() -- it only initializes plain state, no hardware
// side effects, but keeping the ordering matches every other _init() in
// this project.
void clock_engine_init(void);

// Call once per Daisy audio callback (audio ISR/callback context -- must
// not block, must not allocate, must finish in well under 1ms). Advances
// the sample clock, runs PO-sync-in edge detection, PO-sync-out click
// rendering, and the default test beat's sequencer + voice synthesis
// (see "Default test beat" below), and routes `in` -> `out` per the
// current SY mode. n_frames MUST equal AUDIO_BLOCK_SAMPLES (48) -- same
// contract as usb_audio_stream_push_block().
void clock_engine_process_audio(float const *const *in, float *const *out, size_t n_frames);

// Call once per main-loop iteration (loop/task context, NOT the audio
// ISR). Drains incoming USB-MIDI bytes (clock/transport/CC116/CC119 +
// this firmware's local test-hook CCs), runs the boot role-listen state
// machine, and -- once resolved to OPG_MASTER -- emits 24 PPQN 0xF8 +
// Start/Stop + the ~2s role/BPM re-announce.
void clock_engine_poll(void);

// CDC-only (compiled/declared only when this build has a CDC interface --
// see CFG_TUD_CDC in tusb_config.h). The MIDI-only / MIDI+audio isolation
// builds have neither; use the physical BOOT+RESET buttons on those.
#if CFG_TUD_CDC
// Feeds one line of CDC input (NUL-terminated, no trailing \r\n) to the
// local test console. Recognizes "SY0".."SY5", "WAVE0"/"WAVE1", "ROLE",
// "STATUS" (case-insensitive); writes a short human-readable reply back
// over the CDC port. See README "testing" for the full command list.
void clock_engine_handle_cdc_line(char const *line);

// Buttonless-DFU trigger flag -- see README "buttonless DFU (MUST
// PRESERVE)". Set by clock_engine_handle_cdc_line() when a CDC line
// contains "dfu"; main.cpp polls this once per loop and performs the
// actual reboot-to-bootloader call (C++-only libDaisy API).
bool clock_engine_dfu_requested(void);
#endif  // CFG_TUD_CDC

// ---- clock_mode (2026-10-05, bathtub realizations section 2) --------------------------------
// internal / slave / master, as a VIEW over the existing CC116 role + bus liveness (see the
// block comment above BUS_SILENCE_SAMPLES in clock_engine.c). Not a second role scheme.
typedef enum {
  PMOR_CLKMODE_INTERNAL = 0,  // standalone, or following a bus that has been silent > ~1 s
  PMOR_CLKMODE_SLAVE    = 1,  // following the Pi's clock: clock out muted, bus tempo is THE tempo
  PMOR_CLKMODE_MASTER   = 2,  // driving the bus (USB host enumerated): clock out on
} pmor_clock_mode_t;
pmor_clock_mode_t clock_engine_clock_mode(void);
const char       *clock_engine_clock_mode_name(void);   // "internal" / "slave" / "master"
// Tap while SLAVED -> the Pi (ch16 CC112, value = press age in ms, 0..127). See clock_engine.c.
void clock_engine_send_tap_event(uint8_t age_ms);

// ---- Getters (for CDC status / future OLED port) --------------------
sync_mode_t  clock_engine_get_sync_mode(void);
void         clock_engine_set_sync_mode(sync_mode_t m);   // v3: GLOBALS CLOCK param
click_wave_t clock_engine_get_click_wave(void);
clock_role_t clock_engine_get_role(void);
float        clock_engine_get_bpm(void);
bool         clock_engine_is_running(void);
bool         clock_engine_role_resolved(void);

// ---- Host-instrument tempo integration (added for the POCKETGORATOR_CODE
// merge, 2026-08-12) -----------------------------------------------------
// This device's gridTick() sequencer is the ONE tempo authority in the
// merged firmware -- there must never be two independent clocks. Call once
// per main-loop iteration, BEFORE clock_engine_poll():
//   - While clock_engine_get_role() == CLOCK_ROLE_OPG_MASTER (the default),
//     call clock_engine_set_local_bpm() every iteration with the BPM
//     derived from the instrument's own `step_ms` (60000/(step_ms*4)).
//     master_tick_poll() then emits 24 PPQN 0xF8 / Start / Stop / CC116 /
//     CC119 FROM that value -- it does not run its own free-running tempo.
//     No-ops (silently) if role != OPG_MASTER, so it is always safe to call
//     unconditionally from the main loop without an extra role check at
//     the call site.
//   - While role == CLOCK_ROLE_DEMIURGE_MASTER (CC116=2 received), incoming
//     0xF8 drives g_bpm via the existing follower PLL in clock_engine.c
//     (unchanged) -- the caller should instead read clock_engine_get_bpm()
//     every iteration and write the instrument's `step_ms` FROM it
//     (step_ms = 60000/(bpm*4), clamped to the instrument's existing
//     60..500ms grid-step range) so the grid clock follows the bus.
void clock_engine_set_local_bpm(float bpm);

// ---- "Is this instrument making sound" signal (TASK 1, added 2026-10-01)
// ---------------------------------------------------------------------
// g_seq_running used to be hardwired true from boot forever (the grid
// free-ran with nothing ever gating it), so this firmware's very first
// edge-detect of "running changed" never fired and it never transmitted an
// initial 0xFA Start -- any host that waits for a Start before treating
// the bus as live would wait forever. Call this once per main-loop
// iteration (main.cpp), BEFORE clock_engine_poll(), with whatever main.cpp
// determines the grid would currently actually produce audio -- any
// enabled lane with a programmed/generative pattern, a lane with pads
// currently held, or a live sampler voice. An empty pattern with the grid
// spinning is SILENT, not running.
//
// Debounced internally (see master_tick_poll() in clock_engine.c):
// silent->sounding is immediate (so Start fires exactly when something
// would be heard); sounding->silent requires ~120ms of stable silence
// first, so a one-step gap between hits can't spam Start/Stop. The
// debounced result drives g_seq_running/0xFA/0xFC -- but ONLY while this
// device holds CLOCK_ROLE_OPG_MASTER; while following the Demiurge bus,
// transport comes from the bus (0xFA/0xFB/0xFC via handle_realtime()),
// never from local sounding state, same priority rule this file already
// enforces for tempo (fix #2). Safe to call unconditionally regardless of
// role, same as clock_engine_set_local_bpm().
void clock_engine_set_sounding(bool sounding);

// ---- Human tempo gesture (TASK 4, added 2026-10-01) -------------------
// A tap on PMOR's own grid (engine/clock.cpp's clockTap(), surfaced to
// main.cpp as a change in clk.tap_n) is a deliberate human gesture: call
// this once, from main.cpp, the moment it observes a new tap land. It
// outranks everything, including an existing follower relationship --
// a human tapping tempo on the instrument in their hands always wins:
//   - if this device was following the Demiurge bus (CLOCK_ROLE_
//     DEMIURGE_MASTER), it immediately claims OPG_MASTER (a deliberate
//     tap always wins the role, not just the tempo);
//   - g_bpm is set to `bpm` directly (bypassing the normal single-writer
//     gate in clock_engine_set_local_bpm(), since a gesture is explicitly
//     allowed to override whichever writer currently thinks it owns
//     tempo) and the tick phase is re-armed to "now" under the new tempo;
//   - CC116 (role=OPG_MASTER), CC118 (BPM steer) and CC119 (BPM broadcast)
//     are sent immediately, rather than waiting for the ~2s re-announce
//     cadence -- the Pi needs to hear about a deliberate tempo change
//     right away, not up to 2s late.
// `bpm` is clamped to the usual 40..400 range, same as every other g_bpm
// writer in this file.
void clock_engine_human_gesture(float bpm);

// ---- Single clock source of truth (TASK 2, added 2026-10-01) ----------
// engine/clock.h (C++) now exposes clockStepSamplesQ32() -- the EXACT Q32.32
// samples-per-16th period clock.cpp's own grid accumulator adds at every
// boundary. This .c file (clock_engine.c) cannot call a C++-mangled symbol
// directly, so engine/audio_callback.cpp (which already includes clock.h,
// and already wraps this header in `extern "C" {}`) defines this one small
// C-linkage adapter and this header declares it, giving clock_engine.c a
// legal way to read the SAME fixed-point value engine/clock.cpp's grid uses
// -- no duplicate/independent period derivation, so the emitted MIDI clock
// (24 PPQN) and the audible 16th grid can never slip against each other at
// any BPM, by construction, rather than by both computing an equivalent
// value from g_bpm twice.
// Returns: raw Q32.32 fixed point, samples-per-16th-note (verbatim
// clk.step_q). real_samples_per_16th = (double)raw / 4294967296.0;
// samples-per-quarter = raw << 2 (exact, a quarter is exactly 4 sixteenths).
uint64_t clock_engine_grid_step_q32(void);

// ---- Follower phase-lock (added 2026-10-01) ----------------------------
// engine/clock.h now also exposes clockNextStepSample()/clockReanchorNow()/
// clockPhaseNudge() -- the grid's PHASE control surface (clockSetBpm() only
// ever handled tempo, phase-preserving by design, which is exactly why a
// follower used to match the bus's tempo but never its downbeat). These three
// are the same kind of C-linkage adapter as clock_engine_grid_step_q32()
// above, just defined directly in engine/clock.cpp (which already owns the
// state) instead of engine/audio_callback.cpp -- same reason either way: this
// .c file can't call a C++-mangled symbol. Used by this file's own follower
// phase-lock (see handle_realtime()'s 0xF8/0xFA/0xFB cases), gated the same
// way everything else in this file that steers the grid while following is
// gated: only while g_role == CLOCK_ROLE_DEMIURGE_MASTER.
int32_t  clock_engine_follow_lead_samples(void);   // follower clock lead (QSPI global, ms) in samples; + = grid earlier than the followed ticks
uint32_t clock_engine_grid_next_step_sample(void); // adapter for clockNextStepSample()
void     clock_engine_grid_reanchor_now(void);     // adapter for clockReanchorNow()
void     clock_engine_grid_phase_nudge(int32_t correction_samples); // adapter for clockPhaseNudge()
void     clock_engine_grid_reanchor_stamped(uint32_t pos16, uint32_t stamp);   // re-anchor at the Start's IRQ stamp
bool     clock_engine_grid_reanchor_pending(void);   // a Start/Continue re-anchor is queued for the next audio block
void     clock_engine_grid_reanchor_at(uint32_t pos16);       // adapter for clockReanchorAt()
// STUDIO/EDIT: pure follower of the USB-MIDI clock (no role election, no ch16 role/tap CCs, SPP honoured).
void     clock_engine_set_studio(bool on);
int      clock_engine_join_begin(uint16_t spp16);            // CDC JOIN: 0 accepted (applied now or at the next F8), -1 no live F8 stream / not following
int      clock_engine_join_poll(int32_t* late_us);           // 1 applied (late_us = request time minus the chosen tick), 0 waiting, -2 gave up
void     clock_engine_set_policy(uint8_t mode);   // 0 AUTO 1 INTERNAL 2 FOLLOW 3 MASTER: AUTO = master, follows only while a bus clock arrives, back to master ~2 s after it stops
uint8_t  clock_engine_policy(void);
uint32_t clock_engine_ferr_stats(uint32_t* mean_us, uint32_t* p99_us, uint32_t* max_us);   // follower phase error stats (us) over the last <=256 corrections; returns count
void     clock_engine_ferr_reset(void);
uint32_t clock_engine_iv_stats(uint32_t* min_us, uint32_t* max_us);   // follower tick interval min/max (us) since last read
void     clock_engine_rx_stats(uint32_t* irq, uint32_t* cb, uint32_t* pk, uint32_t* nostamp, int32_t* late_max_us, int32_t* late_min_us);   // MIDI RX stamping diagnostics (reads+resets late min/max)
bool     clock_engine_studio(void);
uint32_t clock_engine_start_count(void);
void     clock_engine_set_cc_cb(void (*cb)(uint8_t ch, uint8_t cc, uint8_t val));   // CCs on channels 1-15 (ch 0-based), main-loop context
void     clock_engine_tempo_seed(float bpm);   // host TEMPO request: seed the follower tempo (see docs/PROTOCOL.md TEMPO)
// Followed-transport notification (called from the 0xFA/0xFB/0xFC handlers and on role / STUDIO changes, main-loop context): run=true when the
// transport runs or PMOR is its own master again, false from Stop (and on entering STUDIO) until the next Start/Continue.
void     clock_engine_set_transport_cb(void (*cb)(bool run));                 // accepted Start/Continue messages since boot (seed CC110-113 trigger)
// Latest grid beat: sequence (changes every beat), its sample time, its absolute
// 16th (bar phase = abs16 % 16). master_tick_poll() derives every 0xF8 from this.
void     clock_engine_grid_beat_anchor(uint32_t *seq, uint32_t *sample, uint32_t *abs16);

// ---- Follower phase-lock diagnostics (added 2026-10-01) ----------------
// Ground truth for "is PMOR's grid actually locked to the bus", surfaced in
// the "diag" CDC command alongside the existing tick-catchup/resync counters:
//   - tick count since the last 0xFA/0xFB (0..23, resets with transport) --
//     where in the beat the follower's 6-tick/24-tick counting currently is.
//   - the last RAW phase error measured (before the slew coefficient or a
//     hard-snap was applied), in samples and as a fraction of one 16th --
//     this is what "how far out of lock are we" actually means on the bench.
//   - how many corrections were slewed vs. snapped, so a stuck-oscillating
//     or constantly-snapping follower is visible without a scope.
uint32_t clock_engine_get_follower_tick_count(void);        // ticks since the last Start/Continue, 0..23
// Error BEFORE modulo-reduction into (-step/2, +step/2] -- i.e. literally
// (pmor_next - now) - step_period. Kept separately and deliberately NOT used
// for correction: it is ambiguous by up to a whole step_period, because
// clockTick() (audio ISR) and this file's handle_realtime() (main loop) have
// no guaranteed ordering against each other -- see follower_phase_correct()'s
// "BENCH BUG" comment for the hardware measurement that found this
// (error=-5889 samples at step_period=5806, i.e. -1.01 step, while PMOR was
// actually locked to ~83 samples). Reported anyway because it's exactly the
// field that exposed that bug -- losing it would make the next framing issue
// invisible on the bench.
int32_t  clock_engine_get_follower_phase_error_raw_samples(void);
int32_t  clock_engine_get_follower_phase_error_samples(void); // the ACTUAL phase error: raw reduced into (-step/2, +step/2]
// Same reduced error as a fraction of one 16th, in THOUSANDTHS (permille of a
// 16th) rather than a float: this build links --specs=nano.specs with no
// -u _printf_float, so newlib-nano's snprintf silently drops/ignores %f --
// a float getter here would report a value that then prints as nothing in
// "diag" (exactly what happened the first time this was tried). 500 = half a
// 16th, -250 = a quarter of a 16th early, etc.
int32_t  clock_engine_get_follower_phase_error_frac16_permille(void);
uint32_t clock_engine_get_follower_phase_slew_count(void);   // # of soft (coefficient-scaled) corrections applied
uint32_t clock_engine_get_follower_phase_snap_count(void);   // # of cold-start (full-error) corrections applied

// ---- Tick catch-up / resync diagnostics (TASK 2, added 2026-10-01) ----
// master_tick_poll()'s tick-emission loop used to silently DROP any tick
// beyond TICK_CATCHUP_GUARD (8) if the main loop stalled -- e.g. an SD
// patch save or a long OLED/I2C transaction -- leaving a permanent phase
// offset against the grid that never recovered (s_next_tick_sample kept
// advancing past the ticks that never got sent). It's now a genuine
// catch-up: owed ticks are retried across poll() calls (bounded per call
// only by USB-MIDI FIFO backpressure, not silently discarded), UNLESS the
// backlog exceeds a full beat (>24 ticks, i.e. more than 24 PPQN owed), in
// which case this firmware gives up catching up, resyncs its phase to
// "now", and sends 0xFB Continue so the host re-anchors instead of
// sitting at a stale phase. These two counters are the bench-observable
// ground truth for how often each path actually fires; also surfaced in
// the "DIAG" CDC command.
uint32_t clock_engine_get_tick_catchup_count(void); // ticks sent beyond the first (expected) tick in a poll() call
uint32_t clock_engine_get_tick_resync_count(void);  // times the backlog was abandoned + 0xFB Continue sent

// ---- Sample-rate self-calibration: RETRACTED 2026-08-11, second
// debugging round, and CONFIRMED WRONG (not just unproven) 2026-08-12,
// third round. The original SOF-calibration mechanism this comment used
// to document was a NO-OP on real hardware -- CDC "rate" always read back
// exactly rate_hz=48000.00 (the seed constant, never anything else),
// which means tud_sof_cb() was never actually correcting it. A direct
// hardware measurement (device's own tick_count vs host wall-clock time,
// correlated over a CDC round trip) then proved the ORIGINAL PREMISE
// wrong too -- the audio/SAI domain runs at ~48014 Hz, matching stock
// libDaisy's known PLL3 arithmetic almost exactly (+0.03%). The third
// round confirmed this decisively over a 31-minute continuous run:
// audio_frames_total/audio_cb_count = exactly 48.0000, no drift, no
// stalls. There never was a real audio-domain clock error to calibrate
// against SOF, and the SOF mechanism itself never worked (sof_count read
// 0 for the device's entire uptime, even after 31 minutes) -- so it has
// been deleted entirely (not kept as a non-functional diagnostic; see
// clock_engine.c's tud_sof_cb() history for a plausible root cause,
// noted for the record but not chased: an upstream TinyUSB usbd.c path
// -- a direct `dcd_sof_enable(rhport, false)` on a SECOND
// SET_CONFIGURATION -- can permanently desync the SOF interrupt from the
// `sof_consumer` bitmask tracked in software; unrelated to this
// project's actual bug and out of scope to fix here).
//
// The real ~2.2-2.3% symptom is a USB-MIDI transmit DUPLICATION bug, not
// a rate/timing bug at all -- see README "Measured tick-rate bug" for the
// full three-round writeup, including the host-side inter-arrival
// histogram that proved it (tick spacing is correct at 120.046 BPM;
// ~3% of ticks arrive as an exact duplicate with a ~0ms gap) and the
// device-side counters below that localized it to this firmware's own
// transmit path, not CoreMIDI. master_tick_poll() uses the
// AUDIO_SAMPLE_RATE compile-time constant directly, and this firmware
// never reports a "calibrated" number it did not actually measure.
uint32_t clock_engine_get_tick_count(void);

// ---- Ground-truth diagnostic counters (added 2026-08-11/12 across the
// second and third debugging rounds) -- all plain monotonic counts, no
// derived math, so a caller correlating two reads against host wall-clock
// time can compute real rates itself instead of trusting a firmware-side
// "calibration", AND (the third round's addition) so a caller can compare
// counters AT THE SAME INSTANT across layers to localize exactly where a
// packet gets duplicated:
//   - audio_cb_count / audio_frames_total: Daisy audio-callback
//                        invocations and cumulative frame count -- lets a
//                        caller check audio_frames_total/audio_cb_count
//                        against the expected 48-sample block size.
//                        Confirmed exactly 48.0000 over 31 minutes.
//   - midi_packets_written_realtime / midi_packets_written_cc: call-site
//                        counts, incremented in midi_send_realtime() /
//                        midi_send_cc16() (clock_engine.c) at the exact
//                        point this file calls tud_midi_packet_write() --
//                        "what clock_engine.c asked TinyUSB to send".
//   - midi_tx_hw_events / midi_tx_hw_bytes: USB-MIDI TX endpoint transfer
//                        completions and their byte counts, taken from
//                        TinyUSB's own xfer-complete callback (see
//                        tud_midi_tx_complete_cb() in midi_device.c) --
//                        "what TinyUSB's class driver says it sent".
//                        NOTE (confirmed 2026-08-12): this only
//                        increments once a host is actively draining the
//                        endpoint -- reads 0 immediately after boot/flash
//                        before a host has enumerated, which is expected,
//                        not a sign the hook is broken.
// A fourth layer (dwc2_midi_kicks/kick_bytes/fifo_pushes/fifo_bytes,
// "what this project's own DCD driver scheduled/pushed to hardware") is
// defined in the vendored lib/tinyusb/src/portable/synopsys/dwc2/
// dcd_dwc2.c and read directly by clock_engine.c's "diag" handler via
// extern -- see that file's own comment block. All four layers are
// exposed together via the "diag" CDC command (one machine-parseable
// line): (packets_written_realtime + packets_written_cc) vs
// midi_tx_hw_events/4 vs dwc2_midi_kicks vs dwc2_midi_fifo_pushes/bytes
// is the comparison chain that finds which layer duplicates a packet.
uint32_t clock_engine_get_audio_cb_count(void);
uint32_t clock_engine_get_audio_frames_total(void);
uint32_t clock_engine_get_midi_tx_hw_events(void);
uint32_t clock_engine_get_midi_tx_hw_bytes(void);
uint32_t clock_engine_get_midi_packets_written_realtime(void);
uint32_t clock_engine_get_midi_packets_written_cc(void);

// Call once per Daisy audio callback (audio ISR/callback context), in
// addition to (not instead of) clock_engine_process_audio() -- separate
// call so the isolation builds (which don't route audio through
// clock_engine_process_audio() at all) can still report this counter.
void clock_engine_note_audio_block(uint32_t n_frames);

// ---- Default test beat (added 2026-08-12) ----------------------------
// A built-in synthesized 4/4 kick/snare/hat pattern (no samples, no SD --
// simple oscillator/filtered-noise synthesis) so this device proves its
// own audio output AND clock correctness in one listen, with zero
// external gear: plug in USB power, hear the beat. It is driven off the
// SAME sample-accurate tick unit (24 PPQN, matching MIDI_PPQN) that
// master_tick_poll() uses for MIDI clock emission -- kick on beats 1/3,
// snare on 2/4, hats on every 8th note (12 ticks) -- so hearing the beat
// drift IS the tempo-accuracy test, not a second independent claim about
// timing.
//
// ON BY DEFAULT (clock_engine_init() sets this): the point is that the
// device proves itself out of the box without needing a CDC command
// first. Toggle via the CDC commands "BEAT ON" / "BEAT OFF" and set the
// mix level via "BEATLVL <0-127>" (see clock_engine_handle_cdc_line()
// and "HELP").
//
// Routing (see clock_engine_process_audio()): mixed additively into
// whatever the current SY mode already routes to the Daisy's analog
// outputs, EXCEPT a channel currently carrying the PO-sync-out click
// (SY1/SY5 -> L, SY2 -> R) is left untouched so the beat can never
// corrupt a sync pulse a real PO/Volca is listening for. The USB capture
// stream (see clock_engine_mix_beat_into_capture(), called from
// main.cpp's AudioCallback) always gets the beat mixed into both
// channels regardless of SY mode -- it has no analog sync-click channel
// to protect -- so the pattern can be verified either by ear at the
// Daisy's own outputs or by recording the USB capture on the host.
//
// Cost: pure oscillator/noise-through-one-pole-filter synthesis, no
// allocation, no blocking, computed inline in the SAME per-sample loop
// clock_engine_process_audio() already runs for PO-sync click
// rendering -- no separate audio-callback-adjacent cost center.
bool  clock_engine_beat_enabled(void);
float clock_engine_get_beat_level(void); // 0.0-1.0
void  clock_engine_set_beat_enabled(bool enabled);
void  clock_engine_set_beat_level_0_127(uint8_t v); // clamps to 0-127

// Call once per Daisy audio callback, AFTER clock_engine_process_audio()
// (that call is what renders this block's beat samples internally).
// Writes `in_l[i] + beat, in_r[i] + beat` into the caller-supplied
// out_l/out_r arrays (n_frames capped to AUDIO_BLOCK_SAMPLES) -- intended
// use is building the buffer handed to usb_audio_stream_push_block() so
// the USB capture stream carries the beat too, independent of which
// analog output channel(s) the SY-mode routing actually sent it to.
void clock_engine_mix_beat_into_capture(float const *in_l, float const *in_r,
                                         float *out_l, float *out_r,
                                         size_t n_frames);

#ifdef __cplusplus
}
#endif
