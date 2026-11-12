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

// ---- Getters (for CDC status / future OLED port) --------------------
sync_mode_t  clock_engine_get_sync_mode(void);
click_wave_t clock_engine_get_click_wave(void);
clock_role_t clock_engine_get_role(void);
float        clock_engine_get_bpm(void);
bool         clock_engine_is_running(void);
bool         clock_engine_role_resolved(void);

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
