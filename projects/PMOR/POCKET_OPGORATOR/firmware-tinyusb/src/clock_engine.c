// clock_engine.c — see clock_engine.h for the contract and the list of
// fixes applied relative to pocket-OpGorator.ino's clock/PO-sync code.
#include "clock_engine.h"

#include <string.h>
#include <strings.h>  // strcasecmp/strncasecmp (POSIX, present in newlib)
#include <stdlib.h>   // atoi() -- BEATLVL CDC command parsing
#include <math.h>
#include <stdio.h>

#include "tusb.h"
#include "audio_format.h"
#include "board_stm32h7.h"  // board_get_now_ms() -- see clock_engine.h's "diag" comment

//--------------------------------------------------------------------+
// Tunables
//--------------------------------------------------------------------+
#define MIDI_PPQN            24.0f
#define PO_PPQN               2.0f

// PO-sync click render
#define CLICK_LEVEL           0.8f                                      // ~0.8 FS
#define CLICK_SHORT_LEN_SAMPLES  ((uint32_t)(0.0025f * AUDIO_SAMPLE_RATE))  // ~2.5ms
#define CLICK_BURST_LEN_SAMPLES  ((uint32_t)(0.015f  * AUDIO_SAMPLE_RATE))  // ~15ms
#define CLICK_BURST_HZ         1000.0f

// PO-sync IN edge detect
#define SYNC_IN_THRESHOLD     0.25f                                       // rising-edge threshold, FS
#define SYNC_IN_REFRACTORY_SAMPLES ((uint32_t)(0.010f * AUDIO_SAMPLE_RATE)) // 10ms refractory
#define SYNC_IN_HYSTERESIS    (SYNC_IN_THRESHOLD * 0.5f)

// PO-sync-in tempo smoothing/plausibility window. Small and separate from
// the USB-MIDI PLL window (fix #2): PO pulses are only 2 PPQN, so a
// 24-tick window would take many bars to lock. Trade-off is documented in
// README -- this is a much weaker filter than the MIDI PLL and, per the
// hardware doc's mic auto-switch behavior on an unpatched input jack, is
// NOT a complete defense against a floating/mic-fed input mistaken for a
// sync pulse. It only ever runs while this device holds OPG_MASTER role
// (see po_sync_in_record_interval below) -- that's the primary defense.
#define PO_IN_WINDOW 4
#define PO_IN_MIN_BPM 20.0f
#define PO_IN_MAX_BPM 400.0f
// Reject a window as "probably noise" if the spread between its biggest
// and smallest interval exceeds this fraction of their average -- a real
// PO/Volca clock is far steadier than that; room noise crossing 0.25 FS
// isn't.
#define PO_IN_MAX_SPREAD_FRAC 0.15f

// USB-MIDI follower PLL window -- matches demiurge-clock.cpp's PLL_WINDOW
// exactly (same >=24 tick rule, same reasoning: USB MIDI jitter is ~1ms,
// which is 5-10% of a tick at 120 BPM, so single-tick reactions are noise).
#define MIDI_PLL_WINDOW 24

// Boot role-listen window. The daemon (demiurge-clock.cpp) re-announces
// its role on CC116 every ~2s; 2.5s guarantees we see at least one full
// cycle (plus margin for the first announce landing right after our
// window opens) before defaulting to a role claim of our own.
#define BOOT_LISTEN_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.5f))

// ~2s role/BPM re-announce cadence, in samples (matches demiurge-clock.cpp).
#define ROLE_ANNOUNCE_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.0f))

// Bounded catch-up: how many ticks clock_engine_poll() will emit in one
// call if it was delayed (mirrors demiurge-clock's own tolerance for loop
// jitter without ever flooding the bus).
#define TICK_CATCHUP_GUARD 8

// ---- Default test beat (added 2026-08-12) ----------------------------
// See clock_engine.h for the full rationale. 4/4 at MIDI_PPQN=24
// ticks/quarter: one bar is 4*24 = 96 ticks, an 8th note is 12 ticks --
// same tick unit the MIDI clock and PO-sync-out click already use, so
// "kick/snare/hat drift" and "MIDI clock drift" are the same measurement.
#define BEAT_TICKS_PER_BAR   96u
#define BEAT_EIGHTH_TICKS    12u
#define BEAT_KICK_TICK_A      0u   // beat 1
#define BEAT_KICK_TICK_B     48u   // beat 3
#define BEAT_SNARE_TICK_A    24u   // beat 2
#define BEAT_SNARE_TICK_B    72u   // beat 4

// Kick: sine with a fast downward pitch sweep + separate (faster) pitch
// envelope, cheap substitute for a "808-style" kick -- no filters needed.
#define BEAT_KICK_FREQ_START_HZ   150.0f
#define BEAT_KICK_FREQ_END_HZ      45.0f
#define BEAT_KICK_AMP_DECAY_MS    180.0f
#define BEAT_KICK_PITCH_DECAY_MS   35.0f
#define BEAT_KICK_LEVEL             1.0f

// Snare/hat: cheap xorshift white noise through a one-pole highpass
// (removes the DC/rumble a raw PRNG carries), differentiated by decay
// time and filter coefficient rather than separate synthesis code.
#define BEAT_SNARE_AMP_DECAY_MS   130.0f
#define BEAT_SNARE_HPF_COEFF        0.97f  // closer to 1 = lower cutoff (fuller snare)
#define BEAT_SNARE_LEVEL            0.55f

#define BEAT_HAT_AMP_DECAY_MS      45.0f
#define BEAT_HAT_HPF_COEFF          0.90f  // brighter/harder highpass than the snare
#define BEAT_HAT_LEVEL               0.35f

#define BEAT_DEFAULT_LEVEL_0_127   80      // ~0.63 of BEAT_LEVEL_MAX -- audible but not dominant

//--------------------------------------------------------------------+
// Shared state
//--------------------------------------------------------------------+
// sample_clock: written ONLY from the audio callback (ISR/callback
// context), read from clock_engine_poll() (main-loop context). A 32-bit
// aligned load/store is a single atomic bus transaction on Cortex-M7, so
// this is safe without a lock; it's the same convention audio_format.h /
// usb_audio_stream.c already use for cross-context float reads.
static volatile uint32_t g_sample_clock = 0;

static volatile sync_mode_t  g_sync_mode  = SY0_STEREO;   // boot default; no persistence yet
static volatile click_wave_t g_click_wave = CLICK_WAVE_SHORT_PULSE;

// clock_role_t values ARE the CC116 wire values (1/2) by construction, so
// "what do we send" and "what state are we in" never need translating.
static volatile clock_role_t g_role = CLOCK_ROLE_OPG_MASTER;

// bpm: single float source of truth (fix #7). No clamped uint8_t mirror
// anywhere in this module; format for display at the point of use.
static volatile float g_bpm = 120.0f;
static volatile bool  g_seq_running = true;

// Default test beat (fix: on by default -- see clock_engine.h). Written
// from CDC command context, read from the audio callback -- same
// cross-context convention as g_bpm/g_seq_running above.
static volatile bool  g_beat_enabled = true;
static volatile float g_beat_level   = (float) BEAT_DEFAULT_LEVEL_0_127 / 127.0f;

// ---- Ground-truth diagnostic counters (see clock_engine.h) ----------
// Deliberately dumb: plain monotonic counts, no derived math, no "this is
// now the calibrated rate" claims. The 2026-08-11 SOF-based calibrator
// that used to live here computed g_effective_sample_rate from these same
// SOF timestamps and fed it into tick-scheduling math -- it's gone (see
// clock_engine.h's retraction comment) because (a) hardware proved it
// never actually fired/updated anything, and (b) a follow-up hardware
// measurement proved the premise it was built to fix (an off-nominal
// audio sample rate) was itself wrong. What's left is instrumentation
// ONLY: a caller reads these, correlates two reads against ITS OWN wall
// clock, and derives whatever rate it needs -- this firmware never again
// hands back a number it labeled "calibrated" without having measured it.
static volatile uint32_t g_tick_count               = 0;
static volatile uint32_t g_audio_cb_count           = 0;
static volatile uint32_t g_audio_frames_total       = 0;
static volatile uint32_t g_midi_tx_hw_events        = 0;
static volatile uint32_t g_midi_tx_hw_bytes         = 0;
// Call-site counters (2026-08-12, third debugging round) -- incremented
// at the exact point clock_engine.c ASKS TinyUSB to send a packet, split
// by message type so a duplicate-CC bug can't hide behind a correct
// realtime count or vice versa. Compared against g_midi_tx_hw_events
// (the TX-completion count one layer down, in midi_device.c): if they're
// EQUAL, this file is calling tud_midi_packet_write() more often than it
// should (a clock_engine.c bug); if hw_events is HIGHER, TinyUSB/the DCD
// is completing more transfers than this file ever asked for (a
// TinyUSB/DCD-side re-arm or re-send bug). See "diag" below and README's
// "Measured tick-rate bug" for the full writeup.
static volatile uint32_t g_midi_packets_written_realtime = 0;
static volatile uint32_t g_midi_packets_written_cc       = 0;

// Raw DWC2-driver-level ground truth (2026-08-12, third debugging round)
// -- defined in the vendored lib/tinyusb/src/portable/synopsys/dwc2/
// dcd_dwc2.c, one layer BELOW even the TX-complete hook above: counts
// activity on the USB-MIDI IN endpoint at the point THIS PROJECT'S DCD
// driver (a) schedules ("kicks") a transfer and (b) physically pushes
// bytes into the hardware TX FIFO for it. See dcd_dwc2.c's own comment
// block for the full rationale and exactly what each layer's agreement/
// disagreement with the others proves.
extern volatile uint32_t g_dwc2_midi_in_kicks;
extern volatile uint32_t g_dwc2_midi_in_kick_bytes;
extern volatile uint32_t g_dwc2_midi_in_fifo_pushes;
extern volatile uint32_t g_dwc2_midi_in_fifo_bytes;

// ---- Boot role-listen state machine (fix #3) ------------------------
typedef enum { BOOT_LISTENING = 0, BOOT_RESOLVED } boot_phase_t;
static volatile boot_phase_t g_boot_phase = BOOT_LISTENING;

//--------------------------------------------------------------------+
// PO-sync OUT: click render state (audio-callback-only, no cross-context
// sharing needed -- plain statics, not volatile).
//--------------------------------------------------------------------+
static double   s_po_phase_samples  = 0.0;   // running phase within one 8th note, in samples
static uint32_t s_click_counter     = 0;     // >0 while a click is being rendered (counts down)
static uint32_t s_click_total_len   = 0;     // length the current click render started at (for burst phase calc)
static float    s_burst_phase       = 0.0f;  // running radians for the tone-burst renderer
static bool     s_po_repeater_fire  = false; // SY5: re-emit a click on OUT L for the IN edge just seen

//--------------------------------------------------------------------+
// Default test beat: sequencer + voice render state (audio-callback-
// only). Sequencer phase reuses the SAME wrap-safe next-tick-sample
// scheduling pattern as master_tick_poll()'s MIDI tick emission (see
// clock_engine.h) but is deliberately independent of MIDI role/transport
// gating other than g_seq_running -- it runs whenever the audio callback
// runs, so it works standalone with no bus and no host, which is the
// point.
//--------------------------------------------------------------------+
static uint32_t s_beat_next_tick_sample = 0;
static bool     s_beat_tick_armed       = false;
static uint32_t s_beat_tick_idx         = 0;      // 0..BEAT_TICKS_PER_BAR-1

// Precomputed per-sample decay multipliers (set once in clock_engine_init()
// via expf() -- audio-callback-only after that, no per-sample transcendental
// calls beyond the kick's sinf() and the noise voices' one-pole filter).
static float s_beat_kick_amp_decay_coeff   = 0.0f;
static float s_beat_kick_pitch_decay_coeff = 0.0f;
static float s_beat_snare_amp_decay_coeff  = 0.0f;
static float s_beat_hat_amp_decay_coeff    = 0.0f;

// Kick voice (sine + pitch sweep).
static float s_beat_kick_amp_env   = 0.0f;  // 0 == idle
static float s_beat_kick_pitch_env = 0.0f;
static float s_beat_kick_phase     = 0.0f;

// Snare/hat voices (noise through a one-pole highpass). Filter state
// (z/y) is deliberately left running between hits rather than reset --
// it's a linear filter, and resetting it would just add a click of its
// own at the next trigger.
static float s_beat_snare_amp_env = 0.0f;
static float s_beat_snare_hp_z    = 0.0f;
static float s_beat_snare_hp_y    = 0.0f;
static float s_beat_hat_amp_env   = 0.0f;
static float s_beat_hat_hp_z      = 0.0f;
static float s_beat_hat_hp_y      = 0.0f;

// Cheap xorshift32 PRNG for the noise voices -- deterministic, no libc
// rand() dependency, no allocation.
static uint32_t s_beat_noise_state = 0x20260812u; // any nonzero seed

// One mono sample per frame of the CURRENT block's rendered beat (kick +
// snare + hat, level-scaled, BEFORE the SY-mode click-channel-protection
// gating applied to the analog outputs below) -- main.cpp reads this via
// clock_engine_mix_beat_into_capture() to add the beat into the USB
// capture stream too, independent of which analog output channel(s) it
// actually reached (see clock_engine.h "default test beat").
static float s_beat_block[AUDIO_BLOCK_SAMPLES];

//--------------------------------------------------------------------+
// PO-sync IN: rising-edge detect state (audio-callback-only).
//--------------------------------------------------------------------+
static bool     s_sync_in_armed          = true;
static uint32_t s_sync_in_refractory_ctr = 0;
static uint32_t s_sync_in_last_edge      = 0;
static bool     s_sync_in_have_last_edge = false;
static uint32_t s_po_in_intervals[PO_IN_WINDOW];
static uint8_t  s_po_in_count = 0;
static uint8_t  s_po_in_idx   = 0;

//--------------------------------------------------------------------+
// USB-MIDI follower PLL state (poll-context only -- all touched from
// clock_engine_poll(), never from the audio callback).
//--------------------------------------------------------------------+
static uint32_t s_midi_intervals[MIDI_PLL_WINDOW];
static uint8_t  s_midi_interval_count = 0;
static uint8_t  s_midi_interval_idx   = 0;
static uint32_t s_midi_interval_sum   = 0;
static uint32_t s_last_rx_tick_sample = 0;
static bool     s_have_last_rx_tick   = false;
// Freewheel-gap detection needs a coarse wall-clock-ish timestamp; we use
// the sample clock itself (48kHz counter) rather than millis() so this
// module has zero dependency on libDaisy's System:: -- 2s of samples is
// 2s of samples regardless of which clock you count with.
#define FREEWHEEL_GAP_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.0f))

// ---- Master tick scheduling (fix #1: wrap-safe uint32_t only) -------
static uint32_t s_next_tick_sample;
static bool     s_tick_armed = false;
static uint32_t s_last_role_announce_sample = 0;
static bool     s_have_role_announce_anchor = false;
static bool     s_last_seq_running_sent = true; // what we last told the bus via 0xFA/0xFC

//--------------------------------------------------------------------+
// Small MIDI byte-stream parser (poll context). Realtime bytes (>=0xF8)
// are handled immediately and never disturb running status, per spec --
// they can legally appear in the middle of another message.
//--------------------------------------------------------------------+
static uint8_t s_rx_status = 0;
static uint8_t s_rx_data[2];
static uint8_t s_rx_data_count = 0;

// dfu trigger — see README "buttonless DFU (MUST PRESERVE)". Ported
// verbatim in spirit from PocketPassthru's CDC-watch, adapted to this
// project's line-buffered CDC console (main.cpp's PollCdc accumulates
// raw CDC bytes into a line and hands complete lines to
// clock_engine_handle_cdc_line() below): any line containing "dfu"
// (case-sensitive, matching the original byte-triplet scan) sets this
// flag; main.cpp polls clock_engine_dfu_requested() once per loop and
// performs the actual System::ResetToBootloader() call itself (that call
// is C++-only libDaisy API, so it can't live in this .c file). Only
// meaningful in builds with a CDC interface at all (CFG_TUD_CDC) -- the
// MIDI-only/MIDI+audio isolation builds have no CDC console, so they have
// no software DFU trigger either; use the physical BOOT+RESET buttons on
// those builds (see README "isolation builds have no buttonless DFU").
#if CFG_TUD_CDC
static volatile bool s_dfu_request = false;

bool clock_engine_dfu_requested(void) { return s_dfu_request; }
#endif  // CFG_TUD_CDC

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void midi_send_realtime(uint8_t status) {
  // Fix (2026-08-11, second debugging round): this used to go through
  // tud_midi_stream_write(0, &b, 1), which is a byte-oriented API backed
  // by a small state machine (midi_driver_stream_t) shared with EVERY OTHER
  // tud_midi_stream_write() call on this interface -- including
  // midi_send_cc16() below. Two problems with that, found while chasing
  // the measured "host receives ~2.3% more MIDI Clock bytes than this
  // firmware's own tick_count" bug:
  //
  //   1. Wrong CIN. tud_midi_n_stream_write()'s CIN classifier (see
  //      midi_device.c) only recognizes SysEx-start / 2-byte-syscom /
  //      3-byte-syscom as special System messages; every OTHER 0xFx status
  //      byte -- including every Real-Time message this firmware sends
  //      (0xF8 Clock, 0xFA Start, 0xFC Stop) -- falls through to its
  //      generic "else" branch and gets encoded with CIN 0x5
  //      (MIDI_CIN_SYSEX_END_1BYTE, "SysEx ends with 1 data, OR 1-byte
  //      SYSTEM COMMON message"). Per the USB MIDI 1.0 spec (Table 4-1),
  //      System REAL-TIME messages are a different class from System
  //      Common and are meant to use CIN 0xF (MIDI_CIN_1BYTE_DATA,
  //      literally named "Single Byte"), not 0x5. Sending Real-Time
  //      status under a CIN documented for SysEx-tail/System-Common is a
  //      genuine spec violation, however tolerant most hosts are of it in
  //      practice.
  //   2. Shared mutable state across message types. Because
  //      tud_midi_n_stream_write()'s stream->index/total only resets to 0
  //      once a message is fully flushed, any call that leaves that state
  //      mid-message (e.g. the 3-byte CC path in midi_send_cc16() below,
  //      if a future change ever lets it be interrupted) would corrupt the
  //      framing of the NEXT unrelated call -- including a Real-Time byte
  //      arriving mid-CC. Real-Time messages are legally allowed to
  //      interleave with anything per the MIDI spec; sharing byte-stream
  //      state between them and channel messages is fragile by
  //      construction.
  //
  // tud_midi_packet_write() bypasses the byte-stream state machine
  // entirely -- we hand it one already-complete, already-correctly-CIN'd
  // 4-byte USB-MIDI Event Packet, so there is no shared state to
  // corrupt and no CIN classifier to get it wrong.
  uint8_t packet[4] = {
    (uint8_t) ((0u << 4) | MIDI_CIN_1BYTE_DATA), // cable 0, CIN 0xF
    status,
    0,
    0,
  };
  // Call-site instrumentation (2026-08-12, third debugging round) --
  // incremented here, at the single call site for EVERY realtime message
  // (0xF8/0xFA/0xFC) this file ever sends, regardless of whether the write
  // reports success. We deliberately do NOT branch on the return value or
  // retry on failure -- see the "no retry-on-busy" note in README's
  // "Measured tick-rate bug" section for why a retry loop here would be
  // exactly the kind of bug that could double-send.
  g_midi_packets_written_realtime++;
  tud_midi_packet_write(packet);
}

static void midi_send_cc16(uint8_t cc, uint8_t val) {
  // Also moved to the packet API (see midi_send_realtime() above for the
  // full rationale) so NOTHING on this interface uses the byte-stream
  // state machine any more -- one less shared-state surface to reason
  // about between this and the Real-Time sends.
  uint8_t status = (uint8_t) (0xB0 | 0x0F);
  uint8_t packet[4] = {
    (uint8_t) ((0u << 4) | MIDI_CIN_CONTROL_CHANGE), // cable 0, CIN 0xB
    status,
    cc,
    (uint8_t) (val & 0x7F),
  };
  g_midi_packets_written_cc++; // see midi_send_realtime()'s instrumentation comment
  tud_midi_packet_write(packet);
}

//--------------------------------------------------------------------+
// PO-sync-in tempo write, gated by role (fix #2) + plausibility/stability
// smoothing over a short window (fix #2, mic-noise mitigation).
//--------------------------------------------------------------------+
static void po_sync_in_record_interval(uint32_t interval_samples) {
  // Priority gate: PO-sync-in may only steer tempo while THIS device is
  // the bus master. While following the Demiurge bus (CLOCK_ROLE_
  // DEMIURGE_MASTER), the USB-MIDI PLL owns g_bpm exclusively -- an
  // unpatched SY3/SY4/SY5 input picking up mic noise must never be able
  // to fight the bus clock.
  if (g_role != CLOCK_ROLE_OPG_MASTER) return;

  float bpm = (60.0f * ((float) AUDIO_SAMPLE_RATE)) / (PO_PPQN * (float) interval_samples);
  if (bpm < PO_IN_MIN_BPM || bpm > PO_IN_MAX_BPM) return; // implausible, drop

  s_po_in_intervals[s_po_in_idx] = interval_samples;
  s_po_in_idx = (uint8_t)((s_po_in_idx + 1) % PO_IN_WINDOW);
  if (s_po_in_count < PO_IN_WINDOW) s_po_in_count++;
  if (s_po_in_count < PO_IN_WINDOW) return; // window not full yet

  uint32_t lo = s_po_in_intervals[0], hi = s_po_in_intervals[0];
  uint64_t sum = 0;
  for (uint8_t i = 0; i < PO_IN_WINDOW; i++) {
    uint32_t v = s_po_in_intervals[i];
    if (v < lo) lo = v;
    if (v > hi) hi = v;
    sum += v;
  }
  float avg = (float) sum / (float) PO_IN_WINDOW;
  float spread = (float) (hi - lo) / avg;
  if (spread > PO_IN_MAX_SPREAD_FRAC) return; // too jittery to trust -- likely not a real pulse train

  float new_bpm = (60.0f * ((float) AUDIO_SAMPLE_RATE)) / (PO_PPQN * avg);
  g_bpm = clampf(new_bpm, PO_IN_MIN_BPM, PO_IN_MAX_BPM);
}

//--------------------------------------------------------------------+
// Click rendering (per-sample, called from clock_engine_process_audio)
//--------------------------------------------------------------------+
static inline float render_click_sample(void) {
  if (s_click_counter == 0) return 0.0f;

  float sample;
  if (g_click_wave == CLICK_WAVE_TONE_BURST) {
    sample = CLICK_LEVEL * sinf(s_burst_phase);
    s_burst_phase += (2.0f * (float) M_PI * CLICK_BURST_HZ) / (float) AUDIO_SAMPLE_RATE;
    if (s_burst_phase > 2.0f * (float) M_PI) s_burst_phase -= 2.0f * (float) M_PI;
  } else {
    sample = CLICK_LEVEL; // short unipolar-level plateau
  }
  s_click_counter--;
  return sample;
}

static inline void start_click(void) {
  uint32_t len = (g_click_wave == CLICK_WAVE_TONE_BURST) ? CLICK_BURST_LEN_SAMPLES : CLICK_SHORT_LEN_SAMPLES;
  s_click_counter   = len;
  s_click_total_len = len;
  s_burst_phase     = 0.0f;
}

//--------------------------------------------------------------------+
// Default test beat: voice render + sequencer (per-sample, called from
// clock_engine_process_audio -- see the state block above for why this
// is independent of MIDI role/transport gating other than g_seq_running).
//--------------------------------------------------------------------+
static inline float beat_noise_sample(void) {
  // xorshift32 -- cheap, deterministic, no libc rand() dependency.
  uint32_t x = s_beat_noise_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_beat_noise_state = x;
  return ((float) (int32_t) x) / 2147483648.0f; // ~[-1, 1)
}

static inline float render_beat_kick_sample(void) {
  if (s_beat_kick_amp_env < 0.0005f) { s_beat_kick_amp_env = 0.0f; return 0.0f; }
  float freq = BEAT_KICK_FREQ_END_HZ + (BEAT_KICK_FREQ_START_HZ - BEAT_KICK_FREQ_END_HZ) * s_beat_kick_pitch_env;
  s_beat_kick_phase += (2.0f * (float) M_PI * freq) / (float) AUDIO_SAMPLE_RATE;
  if (s_beat_kick_phase > 2.0f * (float) M_PI) s_beat_kick_phase -= 2.0f * (float) M_PI;
  float sample = BEAT_KICK_LEVEL * s_beat_kick_amp_env * sinf(s_beat_kick_phase);
  s_beat_kick_amp_env   *= s_beat_kick_amp_decay_coeff;
  s_beat_kick_pitch_env *= s_beat_kick_pitch_decay_coeff;
  return sample;
}

static inline void start_beat_kick(void) {
  s_beat_kick_amp_env   = 1.0f;
  s_beat_kick_pitch_env = 1.0f;
  s_beat_kick_phase     = 0.0f;
}

// Shared noise-voice renderer for snare/hat -- same one-pole highpass
// shape, differentiated by decay coefficient/filter coefficient/level
// (the actual per-voice state lives in the caller, passed by pointer, so
// snare and hat can decay independently and overlap without a shared
// "voice slot").
static inline float render_beat_noise_voice(float *amp_env, float amp_decay_coeff,
                                             float *hp_z, float *hp_y, float hp_coeff,
                                             float level) {
  if (*amp_env < 0.0005f) { *amp_env = 0.0f; return 0.0f; }
  float x = beat_noise_sample();
  float y = hp_coeff * (*hp_y + x - *hp_z);
  *hp_z = x;
  *hp_y = y;
  float sample = level * (*amp_env) * y;
  *amp_env *= amp_decay_coeff;
  return sample;
}

static inline void beat_sequencer_trigger(uint32_t tick_idx) {
  if (tick_idx % BEAT_EIGHTH_TICKS == 0) {
    s_beat_hat_amp_env = 1.0f;
  }
  if (tick_idx == BEAT_KICK_TICK_A || tick_idx == BEAT_KICK_TICK_B) {
    start_beat_kick();
  }
  if (tick_idx == BEAT_SNARE_TICK_A || tick_idx == BEAT_SNARE_TICK_B) {
    s_beat_snare_amp_env = 1.0f;
  }
}

//--------------------------------------------------------------------+
// Public: audio-callback-context processing
//--------------------------------------------------------------------+
void clock_engine_process_audio(float const *const *in, float *const *out, size_t n_frames) {
  float bpm_now = g_bpm; // single atomic 32-bit read, see g_sample_clock comment above
  sync_mode_t mode = g_sync_mode;
  bool beat_on = g_beat_enabled;
  float beat_level = g_beat_level;
  // 2 PPQN == one pulse per 8th note. samples/quarter = 60*SR/bpm; an 8th
  // is half that, i.e. 30*SR/bpm -- same PO_PPQN-derived quantity as
  // po_sync_in_record_interval()'s tempo recovery, just inverted.
  double samples_per_eighth = (30.0 * (double) AUDIO_SAMPLE_RATE) / (double) bpm_now;

  // Same tick unit master_tick_poll() uses for MIDI clock emission (24
  // PPQN) -- see clock_engine.h "default test beat": this is what makes
  // hearing the beat drift THE SAME measurement as the MIDI clock
  // drifting, not a separate/independent timer.
  double samples_per_midi_tick = ((double) AUDIO_SAMPLE_RATE * 60.0) / ((double) bpm_now * (double) MIDI_PPQN);
  uint32_t samples_per_midi_tick_u32 = (uint32_t) (samples_per_midi_tick + 0.5);
  if (samples_per_midi_tick_u32 == 0) samples_per_midi_tick_u32 = 1;

  // SY-mode click-channel protection for the analog outputs (fix: "do
  // not clobber a sync-click channel") is enforced directly in the
  // per-case output routing switch below -- SY1/SY5 write click_sample
  // verbatim (unmixed) to L, SY2 to R, and only the OTHER channel in
  // those modes gets `+ beat_sample`; SY0/SY3/SY4 have no OUT click at
  // all, so both channels get the beat. USB capture (see
  // clock_engine_mix_beat_into_capture() below) is NOT gated by this --
  // it always gets the beat, since it has no analog sync-click channel
  // to protect in the first place.

  float const *in_l = in[0];
  float const *in_r = in[1];
  float *out_l = out[0];
  float *out_r = out[1];

  for (size_t i = 0; i < n_frames; i++) {
    float inL = in_l[i];
    float inR = in_r[i];

    // ---- PO-sync IN: rising-edge detect (SY3/SY4/SY5) ----
    bool do_edge_detect = (mode == SY3_IN_L || mode == SY4_IN_R || mode == SY5_PASS);
    float sync_in_sample = (mode == SY4_IN_R) ? inR : inL; // SY3/SY5 watch L, SY4 watches R
    if (do_edge_detect) {
      if (s_sync_in_refractory_ctr > 0) {
        s_sync_in_refractory_ctr--;
      } else if (s_sync_in_armed && sync_in_sample >= SYNC_IN_THRESHOLD) {
        s_sync_in_armed = false;
        s_sync_in_refractory_ctr = SYNC_IN_REFRACTORY_SAMPLES;
        uint32_t now_sample = g_sample_clock + (uint32_t) i;
        if (s_sync_in_have_last_edge) {
          uint32_t interval = now_sample - s_sync_in_last_edge; // wrap-safe: unsigned subtraction
          if (interval > 0) po_sync_in_record_interval(interval);
        }
        s_sync_in_last_edge = now_sample;
        s_sync_in_have_last_edge = true;
        s_po_phase_samples = 0.0; // pulse advances the clock phase by an 8th note
        if (mode == SY5_PASS) s_po_repeater_fire = true;
      } else if (sync_in_sample < SYNC_IN_HYSTERESIS) {
        s_sync_in_armed = true;
      }
    }

    // ---- PO-sync OUT: click phase/render (SY1/SY2 master, SY5 repeater) ----
    bool click_out_active = (mode == SY1_OUT_L || mode == SY2_OUT_R);
    if (click_out_active) {
      s_po_phase_samples += 1.0;
      if (s_po_phase_samples >= samples_per_eighth) {
        s_po_phase_samples -= samples_per_eighth;
        start_click();
      }
    }
    if (s_po_repeater_fire) {
      start_click();
      s_po_repeater_fire = false;
    }
    float click_sample = render_click_sample();

    // ---- Default test beat: sequencer advance + voice render ----
    // Wrap-safe next-tick scheduling, same pattern as master_tick_poll()
    // (see that function's own comment for why signed-subtraction
    // comparison against a free-running uint32_t is correct forever).
    uint32_t beat_now_sample = g_sample_clock + (uint32_t) i;
    if (!s_beat_tick_armed) {
      s_beat_next_tick_sample = beat_now_sample;
      s_beat_tick_armed = true;
    }
    if (beat_on && g_seq_running && (int32_t) (beat_now_sample - s_beat_next_tick_sample) >= 0) {
      beat_sequencer_trigger(s_beat_tick_idx);
      s_beat_tick_idx = (s_beat_tick_idx + 1) % BEAT_TICKS_PER_BAR;
      s_beat_next_tick_sample += samples_per_midi_tick_u32;
    }
    float beat_sample = beat_on ? beat_level * (
        render_beat_kick_sample() +
        render_beat_noise_voice(&s_beat_snare_amp_env, s_beat_snare_amp_decay_coeff,
                                 &s_beat_snare_hp_z, &s_beat_snare_hp_y, BEAT_SNARE_HPF_COEFF,
                                 BEAT_SNARE_LEVEL) +
        render_beat_noise_voice(&s_beat_hat_amp_env, s_beat_hat_amp_decay_coeff,
                                 &s_beat_hat_hp_z, &s_beat_hat_hp_y, BEAT_HAT_HPF_COEFF,
                                 BEAT_HAT_LEVEL)
      ) : 0.0f;
    s_beat_block[i] = beat_sample; // for clock_engine_mix_beat_into_capture() -- USB always gets it

    // ---- Output routing per SY mode ----
    switch (mode) {
      case SY1_OUT_L: {
        float mono = 0.5f * (inL + inR);
        out_l[i] = click_sample;
        out_r[i] = mono + beat_sample; // L carries the sync click -- protected, see beat_protect_l
        break;
      }
      case SY2_OUT_R: {
        float mono = 0.5f * (inL + inR);
        out_l[i] = mono + beat_sample; // R carries the sync click -- protected, see beat_protect_r
        out_r[i] = click_sample;
        break;
      }
      case SY3_IN_L: // sync consumed on IN L; audio passes from IN R (mono, both outs)
        out_l[i] = inR + beat_sample;
        out_r[i] = inR + beat_sample;
        break;
      case SY4_IN_R: // sync consumed on IN R; audio passes from IN L (mono, both outs)
        out_l[i] = inL + beat_sample;
        out_r[i] = inL + beat_sample;
        break;
      case SY5_PASS:
        out_l[i] = click_sample; // sync repeater out -- protected, see beat_protect_l
        out_r[i] = inR + beat_sample; // mono audio chain
        break;
      case SY0_STEREO:
      default:
        out_l[i] = inL + beat_sample;
        out_r[i] = inR + beat_sample;
        break;
    }
  }

  g_sample_clock += (uint32_t) n_frames; // wraps at 2^32 samples (~24.9h @ 48kHz); every
                                          // consumer of g_sample_clock uses wrap-safe signed-
                                          // subtraction comparisons, so wrap itself is harmless.
}

//--------------------------------------------------------------------+
// MIDI RX byte handling (poll context)
//--------------------------------------------------------------------+
static uint8_t midi_status_data_len(uint8_t status) {
  uint8_t hi = status & 0xF0;
  if (hi == 0xC0 || hi == 0xD0) return 1; // Program Change, Channel Pressure
  return 2; // Note On/Off, Poly AT, CC, Pitch Bend
}

static void apply_role(clock_role_t new_role, bool announce) {
  if (new_role == g_role && g_boot_phase == BOOT_RESOLVED) return; // idempotent, no-op
  g_role = new_role;
  g_boot_phase = BOOT_RESOLVED;
  // Reset PLL/freewheel state on every role change so a stale window from
  // a previous stint can't leak into the new one (mirrors demiurge-
  // clock.cpp's role-flip reset exactly).
  s_have_last_rx_tick = false;
  s_midi_interval_count = 0;
  s_midi_interval_idx = 0;
  s_midi_interval_sum = 0;
  s_tick_armed = false; // re-derive tick-emission phase from "now" if we're now master
  if (announce) {
    midi_send_cc16(116, (uint8_t) new_role);
  }
  s_have_role_announce_anchor = true;
  s_last_role_announce_sample = g_sample_clock;
}

static void handle_cc16(uint8_t cc, uint8_t val) {
  if (cc == 116) {
    // Role negotiation. Self-echo mitigation: this device (and the
    // daemon) both re-announce their CURRENT role every ~2s, so hearing
    // our own broadcast bounce back on the bidirectional bus is
    // guaranteed. apply_role() is a no-op when new_role == g_role, which
    // is exactly the condition an echo of our own announcement satisfies
    // -- no separate "from self" flag is needed or possible (USB-MIDI
    // carries no source-client identity the way ALSA seq does on the Pi
    // side). This also structurally prevents role oscillation: a role
    // only ever changes in response to a value that DIFFERS from what we
    // currently hold, and the only way to see a differing value is a
    // genuine flip from the other side.
    if (val == (uint8_t) CLOCK_ROLE_OPG_MASTER) {
      apply_role(CLOCK_ROLE_OPG_MASTER, false);
    } else if (val == (uint8_t) CLOCK_ROLE_DEMIURGE_MASTER) {
      apply_role(CLOCK_ROLE_DEMIURGE_MASTER, false);
    }
  } else if (cc == 119) {
    // BPM broadcast from the bus -- only meaningful (and only applied)
    // while we're following it. Gating on role here is what keeps this
    // from fighting our own CC119 broadcast when we're master (we'd hear
    // our own echo otherwise) -- same self-echo pattern as CC116/0xF8.
    if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
      // demiurge-clock.cpp:213 mapping, matched exactly.
      float bpm = 40.0f + (val / 127.0f) * 360.0f;
      g_bpm = clampf(bpm, 40.0f, 400.0f);
    }
  } else if (cc == 20) {
    // Local test hook (NOT part of the Demiurge protocol): SY mode select.
    if (val <= 5) g_sync_mode = (sync_mode_t) val;
  } else if (cc == 21) {
    // Local test hook: click waveform select.
    g_click_wave = (val >= 64) ? CLICK_WAVE_TONE_BURST : CLICK_WAVE_SHORT_PULSE;
  }
}

static void handle_complete_message(uint8_t status, uint8_t const *data, uint8_t count) {
  (void) count;
  uint8_t hi  = status & 0xF0;
  uint8_t ch  = status & 0x0F;
  if (hi == 0xB0 && ch == 0x0F) { // Control Change, channel 16
    handle_cc16(data[0], data[1]);
  }
  // Other message types (notes, PC, etc.) aren't part of this protocol;
  // this firmware has no sequencer/synth engine in this scaffold to
  // route them to, so they're simply not acted on.
}

static void handle_realtime(uint8_t status) {
  switch (status) {
    case 0xF8: // Timing Clock
      // Fix (kept correct from the .ino): only ever feeds the follower
      // PLL while we're actually following. While we're master, this is
      // guaranteed to include our own 0xF8 echoing back over the
      // bidirectional bus -- feeding that into the PLL would be
      // self-referential nonsense at best and a feedback runaway at
      // worst. Gating on role, not on any notion of "did I send this",
      // is sufficient and is the same pattern CC116/CC119 use above.
      if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
        uint32_t now = g_sample_clock;
        if (s_have_last_rx_tick && (now - s_last_rx_tick_sample) > FREEWHEEL_GAP_SAMPLES) {
          // Stale window (freewheel gap exceeded) -- start a fresh lock
          // rather than averaging across the gap.
          s_have_last_rx_tick = false;
          s_midi_interval_count = 0;
          s_midi_interval_idx = 0;
          s_midi_interval_sum = 0;
        }
        if (s_have_last_rx_tick) {
          uint32_t interval = now - s_last_rx_tick_sample; // wrap-safe unsigned subtraction
          // Reject obviously-implausible intervals before they ever enter
          // the averaging window (matches demiurge-clock.cpp's own dt
          // sanity bound, expressed in samples instead of seconds).
          uint32_t min_iv = (uint32_t) (AUDIO_SAMPLE_RATE * 60.0f / 400.0f / MIDI_PPQN);
          uint32_t max_iv = (uint32_t) (AUDIO_SAMPLE_RATE * 60.0f / 20.0f  / MIDI_PPQN);
          if (interval >= min_iv && interval <= max_iv) {
            if (s_midi_interval_count < MIDI_PLL_WINDOW) {
              s_midi_intervals[s_midi_interval_count++] = interval;
              s_midi_interval_sum += interval;
            } else {
              s_midi_interval_sum -= s_midi_intervals[s_midi_interval_idx];
              s_midi_intervals[s_midi_interval_idx] = interval;
              s_midi_interval_sum += interval;
              s_midi_interval_idx = (uint8_t) ((s_midi_interval_idx + 1) % MIDI_PLL_WINDOW);
            }
            if (s_midi_interval_count >= MIDI_PLL_WINDOW) {
              float avg = (float) s_midi_interval_sum / (float) MIDI_PLL_WINDOW;
              // Symbolic PLL constant (fix #6): bpm = 60*SR/(PPQN*avg),
              // not a magic 120000.0f literal.
              float bpm = (60.0f * ((float) AUDIO_SAMPLE_RATE)) / (MIDI_PPQN * avg);
              g_bpm = clampf(bpm, 40.0f, 400.0f);
            }
          }
        }
        s_last_rx_tick_sample = now;
        s_have_last_rx_tick = true;
      }
      break;

    case 0xFA: // Start
    case 0xFB: // Continue
      // Fix #4: transport is only ever accepted while following. While
      // master, ALL incoming transport is ignored -- a master takes no
      // orders from the bus it drives, including a delayed echo of its
      // own prior Start/Stop. This mirrors the pre-existing (and
      // correct) 0xF8 self-echo gate above.
      if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
        g_seq_running = true;
        s_last_seq_running_sent = true; // bus-driven; don't re-echo it as our own edge
      }
      break;

    case 0xFC: // Stop
      if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
        g_seq_running = false;
        s_last_seq_running_sent = false;
      }
      break;

    default:
      break; // 0xF9/0xFD undefined, 0xFE Active Sense, 0xFF Reset -- no-op
  }
}

static void midi_rx_byte(uint8_t b) {
  if (b >= 0xF8) {
    handle_realtime(b);
    return; // realtime bytes never touch running status (MIDI spec)
  }
  if (b & 0x80) {
    s_rx_status = b;
    s_rx_data_count = 0;
    return;
  }
  if (s_rx_status == 0) return; // data byte with no running status yet -- drop
  if (s_rx_data_count < 2) s_rx_data[s_rx_data_count++] = b;
  uint8_t needed = midi_status_data_len(s_rx_status);
  if (s_rx_data_count >= needed) {
    handle_complete_message(s_rx_status, s_rx_data, s_rx_data_count);
    s_rx_data_count = 0; // keep s_rx_status for running-status repeats
  }
}

//--------------------------------------------------------------------+
// Boot role-listen state machine (fix #3)
//--------------------------------------------------------------------+
static void boot_listen_poll(void) {
  if (g_boot_phase != BOOT_LISTENING) return;
  // apply_role() above already flips g_boot_phase to BOOT_RESOLVED the
  // moment a genuine CC116 arrives (via handle_cc16 -> apply_role), so
  // reaching here means nothing has been heard yet this call.
  if (g_sample_clock >= BOOT_LISTEN_SAMPLES) {
    // Nothing claimed the bus in the listen window -- default to
    // OpGorator master (matches the design spec's stated default) and
    // announce it once.
    apply_role(CLOCK_ROLE_OPG_MASTER, true);
  }
}

//--------------------------------------------------------------------+
// Master tick scheduling (fix #1: wrap-safe uint32_t, fix #6 symbolic)
//--------------------------------------------------------------------+
static void master_tick_poll(void) {
  if (g_boot_phase != BOOT_RESOLVED || g_role != CLOCK_ROLE_OPG_MASTER) {
    s_tick_armed = false; // re-arm fresh whenever we (re-)become master
    return;
  }

  if (g_seq_running != s_last_seq_running_sent) {
    midi_send_realtime(g_seq_running ? 0xFA : 0xFC);
    s_last_seq_running_sent = g_seq_running;
    if (g_seq_running) s_tick_armed = false; // resync tick phase to "now" on Start
  }

  uint32_t now = g_sample_clock;
  double samples_per_tick = ((double) ((float) AUDIO_SAMPLE_RATE) * 60.0) / ((double) g_bpm * (double) MIDI_PPQN);
  uint32_t samples_per_tick_u32 = (uint32_t) (samples_per_tick + 0.5);
  if (samples_per_tick_u32 == 0) samples_per_tick_u32 = 1; // guard div-by-zero-ish stall

  if (!s_tick_armed) {
    s_next_tick_sample = now + samples_per_tick_u32;
    s_tick_armed = true;
  }

  // Wrap-safe: (int32_t)(now - target) >= 0 is true iff `now` has reached
  // or passed `target` in modular arithmetic, and stays correct forever
  // regardless of how many times the uint32_t sample counter has wrapped
  // -- unlike the .ino's `double nextMidiTickSample` compared directly
  // against a wrapping uint32_t, which silently stops matching once the
  // double exceeds 2^32 (~24.9h uptime @ 48kHz).
  int guard = 0;
  while ((int32_t) (now - s_next_tick_sample) >= 0 && guard++ < TICK_CATCHUP_GUARD) {
    if (g_seq_running) {
      midi_send_realtime(0xF8);
      g_tick_count++;
    }
    s_next_tick_sample += samples_per_tick_u32;
  }

  if (!s_have_role_announce_anchor || (now - s_last_role_announce_sample) >= ROLE_ANNOUNCE_SAMPLES) {
    s_last_role_announce_sample = now;
    s_have_role_announce_anchor = true;
    midi_send_cc16(116, (uint8_t) g_role);
    uint8_t v = (uint8_t) clampf(((g_bpm - 40.0f) / 360.0f) * 127.0f, 0.0f, 127.0f);
    midi_send_cc16(119, v);
  }
}

// Follower also re-announces its role every ~2s, same as the daemon does
// regardless of its own role (demiurge-clock.cpp's tick_thread announces
// unconditionally) -- keeps a late-plugged UI/daemon converging quickly
// even when this device isn't the one driving ticks.
static void follower_role_announce_poll(void) {
  if (g_boot_phase != BOOT_RESOLVED || g_role != CLOCK_ROLE_DEMIURGE_MASTER) return;
  uint32_t now = g_sample_clock;
  if (!s_have_role_announce_anchor || (now - s_last_role_announce_sample) >= ROLE_ANNOUNCE_SAMPLES) {
    s_last_role_announce_sample = now;
    s_have_role_announce_anchor = true;
    midi_send_cc16(116, (uint8_t) g_role);
  }
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+
void clock_engine_init(void) {
  g_sample_clock = 0;
  g_sync_mode = SY0_STEREO;
  g_click_wave = CLICK_WAVE_SHORT_PULSE;
  g_role = CLOCK_ROLE_OPG_MASTER;
  g_bpm = 120.0f;
  g_seq_running = true;
  g_boot_phase = BOOT_LISTENING;

  s_po_phase_samples = 0.0;
  s_click_counter = 0;
  s_po_repeater_fire = false;
  s_sync_in_armed = true;
  s_sync_in_refractory_ctr = 0;
  s_sync_in_have_last_edge = false;
  memset(s_po_in_intervals, 0, sizeof(s_po_in_intervals));
  s_po_in_count = 0;
  s_po_in_idx = 0;

  memset(s_midi_intervals, 0, sizeof(s_midi_intervals));
  s_midi_interval_count = 0;
  s_midi_interval_idx = 0;
  s_midi_interval_sum = 0;
  s_have_last_rx_tick = false;

  s_tick_armed = false;
  s_have_role_announce_anchor = false;
  s_last_seq_running_sent = g_seq_running;

  s_rx_status = 0;
  s_rx_data_count = 0;
#if CFG_TUD_CDC
  s_dfu_request = false;
#endif

  g_tick_count                    = 0;
  g_audio_cb_count                = 0;
  g_audio_frames_total            = 0;
  g_midi_tx_hw_events             = 0;
  g_midi_tx_hw_bytes              = 0;
  g_midi_packets_written_realtime = 0;
  g_midi_packets_written_cc       = 0;

  // Default test beat: on by default (fix -- see clock_engine.h), level
  // reset to the tuned default every init so a stale CDC-set level
  // doesn't survive an unexpected re-init.
  g_beat_enabled = true;
  g_beat_level   = (float) BEAT_DEFAULT_LEVEL_0_127 / 127.0f;

  s_beat_tick_armed = false;
  s_beat_tick_idx   = 0;
  s_beat_kick_amp_env = s_beat_kick_pitch_env = s_beat_kick_phase = 0.0f;
  s_beat_snare_amp_env = s_beat_snare_hp_z = s_beat_snare_hp_y = 0.0f;
  s_beat_hat_amp_env   = s_beat_hat_hp_z   = s_beat_hat_hp_y   = 0.0f;
  memset(s_beat_block, 0, sizeof(s_beat_block));

  // Per-sample decay multipliers r such that an envelope starting at 1.0
  // and multiplied by r every sample reaches 1/e after decay_ms of audio
  // -- computed once here (expf() is fine at init, never called per-
  // sample) rather than baked in as a magic float literal per voice.
  s_beat_kick_amp_decay_coeff   = expf(-1000.0f / (BEAT_KICK_AMP_DECAY_MS   * (float) AUDIO_SAMPLE_RATE));
  s_beat_kick_pitch_decay_coeff = expf(-1000.0f / (BEAT_KICK_PITCH_DECAY_MS * (float) AUDIO_SAMPLE_RATE));
  s_beat_snare_amp_decay_coeff  = expf(-1000.0f / (BEAT_SNARE_AMP_DECAY_MS  * (float) AUDIO_SAMPLE_RATE));
  s_beat_hat_amp_decay_coeff    = expf(-1000.0f / (BEAT_HAT_AMP_DECAY_MS    * (float) AUDIO_SAMPLE_RATE));
}

void clock_engine_poll(void) {
  uint8_t b;
  while (tud_midi_stream_read(&b, 1) == 1) {
    midi_rx_byte(b);
  }

  boot_listen_poll();
  master_tick_poll();
  follower_role_announce_poll();
}

// Everything below is CDC-only: the isolation builds (MIDI-only, MIDI+
// audio) have no CDC interface at all (CFG_TUD_CDC 0), so there is no
// console to drive these from -- main.cpp simply never calls
// clock_engine_handle_cdc_line() in those variants (no PollCdc loop).
#if CFG_TUD_CDC
static void cdc_reply(char const *s) {
  tud_cdc_write(s, strlen(s));
  tud_cdc_write_flush();
}

void clock_engine_handle_cdc_line(char const *line) {
  // Buttonless-DFU trigger (MUST PRESERVE -- see PocketPassthru-
  // FLASHING.md): PocketPassthru watches raw CDC RX for the byte
  // sequence "dfu" and reboots into ROM DFU with no button. This
  // firmware's CDC console is line-buffered (main.cpp accumulates bytes
  // until '\n'), so the trigger here is "the line contains dfu"
  // (case-sensitive substring, same as the original's byte-triplet
  // scan) rather than a raw 3-byte scan -- same user-facing behavior
  // (`printf 'dfu' > port`), same guarantee. main.cpp polls
  // clock_engine_dfu_requested() every loop iteration and performs the
  // actual System::ResetToBootloader(STM) call (C++-only libDaisy API,
  // so it can't live in this .c file).
  if (strstr(line, "dfu") != NULL) {
    s_dfu_request = true;
    cdc_reply("DFU requested -- rebooting to bootloader\r\n");
    return;
  }

  if (strncasecmp(line, "sy", 2) == 0 && line[2] >= '0' && line[2] <= '5' && line[3] == '\0') {
    g_sync_mode = (sync_mode_t) (line[2] - '0');
    cdc_reply("OK\r\n");
  } else if (strcasecmp(line, "wave0") == 0) {
    g_click_wave = CLICK_WAVE_SHORT_PULSE;
    cdc_reply("OK\r\n");
  } else if (strcasecmp(line, "wave1") == 0) {
    g_click_wave = CLICK_WAVE_TONE_BURST;
    cdc_reply("OK\r\n");
  } else if (strcasecmp(line, "beat on") == 0 || strcasecmp(line, "beaton") == 0) {
    g_beat_enabled = true;
    cdc_reply("OK\r\n");
  } else if (strcasecmp(line, "beat off") == 0 || strcasecmp(line, "beatoff") == 0) {
    g_beat_enabled = false;
    cdc_reply("OK\r\n");
  } else if (strncasecmp(line, "beatlvl", 7) == 0) {
    // "BEATLVL <0-127>" -- same 0-127 convention as the CC20/CC21 local
    // test hooks and CC119's BPM broadcast elsewhere in this file, so
    // one mental model covers every knob this firmware exposes.
    int v = atoi(line + 7);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    clock_engine_set_beat_level_0_127((uint8_t) v);
    cdc_reply("OK\r\n");
  } else if (strcasecmp(line, "status") == 0) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "sy=%d wave=%d role=%s bpm=%d running=%d boot=%s beat=%s beatlvl=%d\r\n",
             (int) g_sync_mode,
             (int) g_click_wave,
             (g_role == CLOCK_ROLE_OPG_MASTER) ? "opg_master" : "demiurge_master",
             (int) (g_bpm + 0.5f),
             (int) g_seq_running,
             (g_boot_phase == BOOT_RESOLVED) ? "resolved" : "listening",
             g_beat_enabled ? "on" : "off",
             (int) (g_beat_level * 127.0f + 0.5f));
    cdc_reply(buf);
  } else if (strcasecmp(line, "help") == 0) {
    cdc_reply(
      "SY0-SY5      sync mode (see README PO-sync table)\r\n"
      "WAVE0/WAVE1  PO-sync click waveform (short pulse / tone burst)\r\n"
      "BEAT ON/OFF  default test beat (kick/snare/hat, on by default)\r\n"
      "BEATLVL n    beat mix level, 0-127 (default 80 -- see BEAT_DEFAULT_LEVEL_0_127)\r\n"
      "STATUS       dump current state\r\n"
      "RATE         assumed sample rate + tick-emission call count\r\n"
      "DIAG         ground-truth instrumentation (see README)\r\n"
      "dfu          reboot to bootloader (buttonless DFU)\r\n");
  } else if (strcasecmp(line, "rate") == 0) {
    // 2026-08-11, second debugging round: this used to also report a
    // "calibrated" rate_hz that never actually got calibrated (see
    // clock_engine.h's retraction comment) -- removed rather than kept as
    // a comforting fake number. assumed_hz is exactly what it says: the
    // AUDIO_SAMPLE_RATE compile-time constant this firmware assumes and
    // uses for all tick-scheduling math, nothing more. tick_count is the
    // ground-truth count of 0xF8 emission call sites (see master_tick_poll
    // -- exactly one call site, counter incremented in lockstep with every
    // call). Use "diag" for the fuller instrumentation set.
    char buf[160];
    snprintf(buf, sizeof(buf),
             "assumed_hz=%u sy=%d role=%s bpm=%d tick_count=%lu\r\n",
             (unsigned) AUDIO_SAMPLE_RATE,
             (int) g_sync_mode,
             (g_role == CLOCK_ROLE_OPG_MASTER) ? "opg_master" : "demiurge_master",
             (int) (g_bpm + 0.5f),
             (unsigned long) g_tick_count);
    cdc_reply(buf);
  } else if (strcasecmp(line, "diag") == 0) {
    // Ground-truth instrumentation (added 2026-08-11/12 across the second
    // and third debugging rounds -- see clock_engine.h's counter-group
    // comment for what each field proves). Take two reads separated by a
    // known host-side wall-clock interval to derive real rates for each
    // layer instead of trusting any firmware-side "calibration":
    //   sysnow_ms per real second      -> is SysTick/HCLK itself accurate
    //   sample_clock per real second   -> is the audio/SAI domain accurate
    //   audio_frames_total/audio_cb_count -> is the block size really 48
    //     (confirmed exactly 48.0000 over a 31-minute run, third round)
    // The duplicate-0xF8 investigation (third round) needs three counters
    // read TOGETHER, not derived from wall-clock rate at all:
    //   midi_packets_written_realtime + midi_packets_written_cc
    //     -> how many times clock_engine.c itself CALLED
    //        tud_midi_packet_write() (a call-site count, "what we asked
    //        TinyUSB to do")
    //   midi_tx_hw_events / midi_tx_hw_bytes
    //     -> how many USB-MIDI IN transfers TinyUSB's own driver reported
    //        COMPLETING, and their byte counts (midi_device.c's
    //        tud_midi_tx_complete_cb hook -- "what TinyUSB says it did").
    //        NOTE: this only increments while a host is actively draining
    //        the endpoint; expect 0 immediately after boot/flash before
    //        a host has enumerated and started polling.
    // packets_written_realtime+cc == midi_tx_hw_events (bytes/4) would
    // mean clock_engine.c asked for exactly what got sent (no duplication
    // here); if hw_events is HIGHER than what was written, the
    // duplication is downstream of this file, inside TinyUSB/the DCD --
    // in which case dwc2_kicks/dwc2_kick_bytes (this project's DCD driver
    // scheduling a transfer, dcd_edpt_xfer_fifo()) vs
    // dwc2_fifo_pushes/dwc2_fifo_bytes (that same driver physically
    // pushing bytes into the hardware TX FIFO, epin_write_tx_fifo() --
    // see that function's own comment for why it can legitimately run
    // more than once per kick, and what a REAL over-push looks like
    // there) narrows it further, all the way down to "the wire".
    char buf[400];
    snprintf(buf, sizeof(buf),
             "sysnow_ms=%lu sample_clock=%lu "
             "audio_cb_count=%lu audio_frames_total=%lu "
             "tick_count=%lu midi_packets_written_realtime=%lu "
             "midi_packets_written_cc=%lu midi_tx_hw_events=%lu "
             "midi_tx_hw_bytes=%lu dwc2_midi_kicks=%lu "
             "dwc2_midi_kick_bytes=%lu dwc2_midi_fifo_pushes=%lu "
             "dwc2_midi_fifo_bytes=%lu\r\n",
             (unsigned long) board_get_now_ms(),
             (unsigned long) g_sample_clock,
             (unsigned long) g_audio_cb_count,
             (unsigned long) g_audio_frames_total,
             (unsigned long) g_tick_count,
             (unsigned long) g_midi_packets_written_realtime,
             (unsigned long) g_midi_packets_written_cc,
             (unsigned long) g_midi_tx_hw_events,
             (unsigned long) g_midi_tx_hw_bytes,
             (unsigned long) g_dwc2_midi_in_kicks,
             (unsigned long) g_dwc2_midi_in_kick_bytes,
             (unsigned long) g_dwc2_midi_in_fifo_pushes,
             (unsigned long) g_dwc2_midi_in_fifo_bytes);
    cdc_reply(buf);
  } else if (line[0] != '\0') {
    cdc_reply("? (try HELP, SY0-SY5, WAVE0, WAVE1, BEAT ON, BEAT OFF, BEATLVL n, STATUS, RATE, DIAG)\r\n");
  }
}
#endif  // CFG_TUD_CDC

sync_mode_t  clock_engine_get_sync_mode(void)  { return g_sync_mode; }
click_wave_t clock_engine_get_click_wave(void) { return g_click_wave; }
clock_role_t clock_engine_get_role(void)       { return g_role; }
float        clock_engine_get_bpm(void)        { return g_bpm; }
bool         clock_engine_is_running(void)     { return g_seq_running; }
bool         clock_engine_role_resolved(void)  { return g_boot_phase == BOOT_RESOLVED; }

uint32_t clock_engine_get_audio_cb_count(void)      { return g_audio_cb_count; }
uint32_t clock_engine_get_audio_frames_total(void)  { return g_audio_frames_total; }
uint32_t clock_engine_get_midi_tx_hw_events(void)   { return g_midi_tx_hw_events; }
uint32_t clock_engine_get_midi_tx_hw_bytes(void)    { return g_midi_tx_hw_bytes; }
uint32_t clock_engine_get_midi_packets_written_realtime(void) { return g_midi_packets_written_realtime; }
uint32_t clock_engine_get_midi_packets_written_cc(void)       { return g_midi_packets_written_cc; }

void clock_engine_note_audio_block(uint32_t n_frames) {
  g_audio_cb_count++;
  g_audio_frames_total += n_frames;
}

//--------------------------------------------------------------------+
// Default test beat: public API
//--------------------------------------------------------------------+
bool  clock_engine_beat_enabled(void)   { return g_beat_enabled; }
float clock_engine_get_beat_level(void) { return g_beat_level; }

void clock_engine_set_beat_enabled(bool enabled) { g_beat_enabled = enabled; }

void clock_engine_set_beat_level_0_127(uint8_t v) {
  if (v > 127) v = 127;
  g_beat_level = (float) v / 127.0f;
}

// Call once per Daisy audio callback, AFTER clock_engine_process_audio()
// (which is what actually renders this block's beat into s_beat_block).
// Adds the same mono beat mix into a caller-supplied copy of the raw
// input, for pushing to the USB capture stream -- see clock_engine.h.
// NOT gated by SY-mode click-channel protection (see
// clock_engine_process_audio()'s comment on that): USB capture has no
// analog sync-click channel to protect, so it always gets the beat.
void clock_engine_mix_beat_into_capture(float const *in_l, float const *in_r,
                                         float *out_l, float *out_r,
                                         size_t n_frames) {
  if (n_frames > AUDIO_BLOCK_SAMPLES) n_frames = AUDIO_BLOCK_SAMPLES; // s_beat_block's fixed size
  for (size_t i = 0; i < n_frames; i++) {
    float b = s_beat_block[i];
    out_l[i] = in_l[i] + b;
    out_r[i] = in_r[i] + b;
  }
}

// Ground-truth USB-MIDI TX instrumentation (see midi_device.c/.h's
// tud_midi_tx_complete_cb -- a Pocket-OpGorator-only hook added to the
// vendored TinyUSB copy, not upstream). Fires once per completed TX
// endpoint transfer with the literal byte count the hardware sent -- this
// is what proves or disproves TinyUSB's own TX path as the source of any
// gap between clock_engine's tick_count (an emission CALL count) and what
// a host actually observes on the wire.
void tud_midi_tx_complete_cb(uint8_t itf, uint32_t xferred_bytes) {
  (void) itf;
  g_midi_tx_hw_events++;
  g_midi_tx_hw_bytes += xferred_bytes;
}
uint32_t     clock_engine_get_tick_count(void)             { return g_tick_count; }
