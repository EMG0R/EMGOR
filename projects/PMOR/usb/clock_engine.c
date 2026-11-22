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
#include "usb_descriptors.h"   // EPNUM_MIDI (IRQ-time RX stamping)
#include "board_stm32h7.h"  // board_get_now_ms() -- see clock_engine.h's "diag" comment
#include "demiurge-transport.h"  // 14-bit tempo wire grid (CC120/121), shared with the Pi transport -- see Makefile DEMIURGE_SRC

//--------------------------------------------------------------------+
// Tunables
//--------------------------------------------------------------------+
#define MIDI_PPQN            24.0f
#define GRID_TICKS_PER_BEAT  24u   // integer twin of MIDI_PPQN for the grid-derived tick scheduler
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

// ---- Follower phase-lock (added 2026-10-01) --------------------------
// Separate from the PLL above: the PLL steers TEMPO (averaged over 24 ticks
// before it writes anything); this steers WHERE IN THE BAR that tempo is
// anchored, and has to react every 16th, not every bar, or a Start/Continue
// well into a song would take ages to find the beat. A quarter note is
// MIDI_PPQN (24) ticks and this firmware's grid is 16ths, so 6 ticks = one
// 16th -- the only rate PMOR's gstep/next_step can be compared against.
#define MIDI_TICKS_PER_16TH ((uint32_t) MIDI_PPQN / 4u)   // 6
#define MIDI_TICKS_PER_BEAT ((uint32_t) MIDI_PPQN)        // 24

// Slew coefficient: apply 1/2^PHASE_SLEW_SHIFT of the measured error on every
// 16th-note boundary. Chosen against this bus's OWN measured jitter, not a
// guess -- README's "Measured tick-rate bug" section (second/third debugging
// rounds) found real USB-MIDI Realtime arrival jitter on this exact link to
// be host-scheduling/USB-frame-alignment noise on the order of a tick's
// low-single-digit percent, not multi-ms swings. shift=2 (1/4 per boundary):
//   - damps a single noisy tick's contribution to a QUARTER of its own size
//     before it ever reaches next_step_whole, so one jittery arrival can
//     never itself read as an audible step-timing jump;
//   - still converges fast: the error this slews is always already reduced
//     into (-step/2, +step/2] (see follower_phase_correct()), so its WORST
//     case is exactly half a 16th: error*(3/4)^n drops under 5% of that
//     starting value by n=11 corrections -- 11 sixteenths, under one 4/4 bar
//     (16 sixteenths) at ANY tempo, since this runs once per 16th regardless
//     of BPM, not once per unit time. A cold-start (s_follower_phase_cold)
//     skips this entirely and arrives in one correction instead -- see that
//     flag's comment.
// Integer division truncates toward zero (deliberately, not an arithmetic
// shift -- shifting a negative value is implementation-defined signedness
// behavior in C, division by a power of two is not): the one irreducible
// residual this leaves is up to 3 samples (<0.07ms at 48kHz) of static error
// once |error| drops below 4, which is below any clock this firmware can
// even measure, let alone hear.
#define PHASE_SLEW_SHIFT 2

// Boot role-listen window. The daemon (demiurge-clock.cpp) re-announces
// its role on CC116 every ~2s; 2.5s guarantees we see at least one full
// cycle (plus margin for the first announce landing right after our
// window opens) before defaulting to a role claim of our own.
#define BOOT_LISTEN_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.5f))

// ~2s role/BPM re-announce cadence, in samples (matches demiurge-clock.cpp).
#define ROLE_ANNOUNCE_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.0f))

// Bounded catch-up (TASK 2 fix, 2026-10-01): how many ticks master_tick_poll()
// will attempt to FLUSH in one poll() call if it was delayed. This used to be
// the total budget -- any tick beyond it was silently dropped, which is the
// bug: it leaves s_next_tick_sample advanced past ticks that were never sent,
// a phase offset that never recovers. It's now purely a per-call burst cap so
// a single stall can't flood the USB-MIDI TX FIFO in one shot; any backlog
// beyond this cap stays owed (s_next_tick_sample is NOT advanced past ticks
// that weren't sent) and is retried on the next poll() call, which runs again
// within a sub-ms main-loop pass. CFG_TUD_MIDI_TX_BUFSIZE is 64 bytes = 16
// four-byte packets (tusb_config.h) -- keep this at or below that so a single
// burst here can't itself starve the CC116/CC117/CC118/CC119 sends later in
// the same poll() call.
#define TICK_CATCHUP_BURST_MAX 16

// Hopelessly-behind threshold (TASK 2 fix): more than a full beat owed (24
// PPQN ticks) means catching up by bursting would itself flood the bus with
// a beat's worth of 0xF8 at once, which is its own kind of wrong. Past this,
// give up catching up, resync phase to "now", and send 0xFB Continue so the
// host re-anchors instead of silently sitting at a stale phase.
#define TICK_RESYNC_OWED_TICKS 24

// Silent->sounding is immediate; sounding->silent needs this many ms of
// stable silence before g_seq_running actually flips (TASK 1 fix) -- a
// one-step gap between sequenced hits must not spam 0xFA/0xFC.
#define SOUNDING_SILENT_DEBOUNCE_MS 120u

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
static bool s_gap_realign = false;   // set at a freewheel gap, consumed by the next 0xF8 (re-derive the tick index from the grid phase)
static uint32_t s_iv_min = 0xFFFFFFFFu, s_iv_max = 0;   // tick-to-tick interval (stamp domain), reset on STATUS read
static uint32_t s_tk_ring[25]; static uint32_t s_tk_n = 0; static float s_bpm_est[5]; static uint32_t s_bpm_n = 0;
#define PH_WIN 48u
static int32_t  s_ph_x[PH_WIN];                      // raw (stamp - t0) history, samples (residuals are formed against the CURRENT period at use: see 0xF8)
static uint32_t s_ph_kk[PH_WIN];                     // tick index of each history entry
static uint32_t s_ph_t0 = 0, s_ph_n = 0, s_ph_k = 0, s_ph_est_now = 0; static bool s_ph_have = false, s_ph_ok = false;
static inline uint32_t hires_sample_now(void);
static uint32_t stamp_now(void);   // IRQ-stamped "now" for the packet being parsed (defined with the RX stamping block)

static volatile sync_mode_t  g_sync_mode  = SY0_STEREO;   // boot default; no persistence yet
static volatile click_wave_t g_click_wave = CLICK_WAVE_SHORT_PULSE;

// clock_role_t values ARE the CC116 wire values (1/2) by construction, so
// "what do we send" and "what state are we in" never need translating.
static volatile clock_role_t g_role = CLOCK_ROLE_OPG_MASTER;

// bpm: single float source of truth (fix #7). No clamped uint8_t mirror
// anywhere in this module; format for display at the point of use.
static volatile float g_bpm = 120.0f;
// TASK 1 fix (2026-10-01): used to init `true` with nothing to ever flip it,
// so the very first "did running change" edge test never fired and this
// firmware never once transmitted an initial 0xFA Start. Now starts false
// (genuinely silent until clock_engine_set_sounding() says otherwise) and is
// driven exclusively by the debounced sounding signal while OPG_MASTER (see
// master_tick_poll()) -- or by the bus's own transport while following.
static volatile bool  g_seq_running = false;
static volatile uint32_t s_start_count = 0;   // accepted Start/Continue messages (a Stop->Start faster than one main-loop pass still counts)

// TASK 1: raw (undebounced) "would this currently produce audio" signal,
// written from main.cpp's control-plane loop via clock_engine_set_sounding()
// every iteration -- same cross-context convention as g_bpm. Debounced into
// g_seq_running inside master_tick_poll(), poll-context only.
static volatile bool  g_sounding_raw = false;
static bool    s_sounding_silent_pending    = false;
static uint32_t s_sounding_silent_since_ms  = 0;

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

// TASK 2 diagnostics (2026-10-01): ground truth for the catch-up/resync
// behavior at the bench -- see clock_engine.h's comment on the getters.
static volatile uint32_t g_tick_catchup_count = 0;
static volatile uint32_t g_tick_resync_count  = 0;

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

// ---- Follower phase-lock state (poll-context only -- driven entirely from
// handle_realtime()'s 0xF8/0xFA/0xFB cases below, same context as the PLL
// state just above). s_follower_tick_count is the raw count of ticks since
// the last 0xFA/0xFB, wrapping at MIDI_TICKS_PER_BEAT (not that it needs to:
// it's only ever tested via % MIDI_TICKS_PER_16TH, but bench-readable in diag
// as "where in the beat are we" matters more than keeping it artificially
// small). The counters/error fields exist purely for "diag" -- nothing in the
// correction path reads them back.
static uint32_t s_follower_tick_count          = 0;
// Bench fix (2026-10-01, after first hardware test): `pmor_next - now` does
// NOT reliably sit one whole step_period away from `now` the instant a 16th's
// tick is handled -- clockTick() (audio ISR) and handle_realtime() (main
// loop) are different planes with no enforced ordering between them, so
// whether clockTick() has already consumed the matching boundary by the time
// handle_realtime() looks is genuinely unordered. The raw (pre-reduction)
// value below used to be treated AS the error, which meant it was off by a
// whole step_period roughly as often as not (measured on hardware:
// error=-5889 at step_period=5806 -- almost exactly -1.0 step). The true
// error is raw reduced modulo step_period into (-step/2, +step/2] -- see
// follower_phase_correct(). Both the raw and the reduced value are kept for
// diag (losing the raw one is exactly what would have hidden this bug).
static int32_t  s_follower_phase_error_raw_samples = 0; // pre-reduction: (pmor_next - now) - step_period
static int32_t  s_follower_phase_error_samples     = 0; // reduced into (-step/2, +step/2] -- the ACTUAL phase error
static uint32_t s_follower_phase_error_period      = 1; // the step_period that error was measured against (for the frac16 getter)
static uint32_t s_follower_phase_slew_count        = 0;
static uint32_t s_follower_phase_snap_count        = 0;
// Cold-start flag: true means the NEXT correction should be a full (coefficient-1)
// snap rather than a slewed fraction, because there's no slew history worth
// trusting yet -- a fresh role transition into DEMIURGE_MASTER or a freewheel
// gap large enough to have reset the tempo PLL's own window (same staleness
// signal, reused here) both mean "whatever phase PMOR is sitting at right now
// has no relationship to the bus we're about to start tracking again", so
// arrive immediately instead of slewing in over ~11 sixteenths. Cleared after
// the first correction; every correction after that slews.
static bool     s_follower_phase_cold = true;
// Freewheel-gap detection needs a coarse wall-clock-ish timestamp; we use
// the sample clock itself (48kHz counter) rather than millis() so this
// module has zero dependency on libDaisy's System:: -- 2s of samples is
// 2s of samples regardless of which clock you count with.
#define FREEWHEEL_GAP_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 2.0f))

// ---- clock_mode (2026-10-05, bathtub realizations section 2): INTERNAL / SLAVE / MASTER ------
// NOT a second role scheme -- a VIEW over the existing CC116 role plus bus liveness, so there is
// still exactly one mechanism deciding who owns the clock:
//   MASTER   role == OPG_MASTER and a USB host has enumerated us  (clock out on; we drive the bus)
//   SLAVE    role == DEMIURGE_MASTER and a 0xF8 arrived within BUS_SILENCE_SAMPLES (clock out
//            muted -- no loopback; the bus tempo is the tempo)
//   INTERNAL everything else: standalone/unplugged PMOR, or following a bus that went quiet.
//            The local grid is the clock, local tap + BPM editing work in full, and nobody is
//            owed anything. "Whenever PMOR is unplugged its tap tempo and its clock still function."
// Auto-switch to SLAVE is the existing CC116=2 handshake from the Pi daemon; the ~1 s fallback to
// INTERNAL is BUS_SILENCE_SAMPLES below (the follower PLL's own freewheel is 2 s -- that is for
// holding tempo across a stutter; this is for deciding who is allowed to change it).
#define BUS_SILENCE_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 1.0f))

// ---- canonical tempo as a number (ch16 CC120 MSB / CC121 LSB) ------------------------------
// While following, PMOR adopts the master's NUMBER. The follower PLL keeps running (it supplies
// phase, and is the tempo fallback for foreign gear) but its interval-average tempo is ignored
// while a number is fresh -- a PLL tempo measures the sender's USB scheduling jitter, not its clock.
#define WIRE_TEMPO_FRESH_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 3.0f))
static uint8_t  s_wire_msb = 0;
static bool     s_wire_have_msb = false;
static bool     s_wire_have = false;
static uint32_t s_wire_sample = 0;
static uint16_t s_wire_sent_last = 0xFFFF;        // last 14-bit value we transmitted (master)
static uint32_t s_wire_sent_sample = 0;
static bool     s_wire_sent_any = false;
static bool wire_tempo_fresh(void) {
  return s_wire_have && (uint32_t)(g_sample_clock - s_wire_sample) <= WIRE_TEMPO_FRESH_SAMPLES;
}

// ---- Clock-ownership election state (bench fix, 2026-10-01) ---------
// Fixes the observed bug: PMOR used to re-announce CC116=OPG_MASTER
// unconditionally every ~2s (the boot-default fallback and the ~2s
// re-announce both just broadcast g_role verbatim with nothing ever
// correcting g_role itself), even while silent -- which overrode the
// Pi-side transport-aware election every single cycle. See
// clock_engine.h's comment above clock_engine_set_sounding() /
// clock_engine_human_gesture() and this file's handle_cc16() cc==115
// branch for the two inputs this feeds on. All of this is poll-context
// only (driven from clock_engine_poll(), never the audio callback).
//
// CC115 ("NEPTR holds a loop") receive state. PMOR never transmits CC115
// itself (handle_cc16's cc==115 branch is the only place this is written),
// so unlike CC116/CC119 there is no self-echo to filter -- every CC115
// byte seen on the bus is genuinely from NEPTR.
static bool     g_remote_loop_held             = false; // last CC115 value interpreted
static uint32_t g_remote_loop_last_heard_sample = 0;
static bool     g_remote_loop_have_heard        = false;

// NEPTR re-announces CC115 every ~2s (ungated, same cadence as its role/BPM
// announce per the protocol doc) -- 6s is three missed announces in a row,
// a generous margin against normal bus jitter/a dropped packet or two
// without taking long to notice NEPTR actually went away.
#define REMOTE_LOOP_STALE_SAMPLES ((uint32_t) (AUDIO_SAMPLE_RATE * 6.0f))

// Local "is PMOR itself making sound" debounce, role-independent. This is
// the SAME 120ms debounce TASK 1 already added (SOUNDING_SILENT_DEBOUNCE_MS,
// via s_sounding_silent_pending/since_ms below) -- just computed once per
// clock_engine_poll() call regardless of current role, rather than only
// while OPG_MASTER, because the election logic below needs to see local
// sounding edges even while PMOR is currently following the bus. No second
// timer: master_tick_poll() now just reads this instead of recomputing it.
static bool     s_local_sounding = false;

// Edge-detect anchor for the election below (RULE 5: only decide on the
// silent->sounding edge, never on every poll while already sounding, or a
// mid-performance CC115 change would flap the role).
static bool     s_role_election_prev_sounding = false;

// Set true by clock_engine_human_gesture() (TASK 4: a deliberate tap always
// wins, "regardless of CC115 or anything else" per spec) and cleared by
// apply_role() the next time role genuinely transitions to DEMIURGE_MASTER
// (whether that's the Pi explicitly taking master back over CC116, or a
// future design adding its own override) -- see clock_role_election_poll()'s
// RULE 1 exception below for why: a tapped master claim is sticky and must
// survive PMOR going silent afterward (tapping tempo doesn't itself make
// noise), but is not sticky against an explicit hand-back.
static bool     s_master_by_gesture = false;

// ---- Master tick scheduling (fix #1: wrap-safe uint32_t only) -------
static uint32_t s_next_tick_sample __attribute__((unused)); // superseded by the grid-derived scheduler (2026-11-06)
static bool     s_tick_armed = false;
// grid-derived tick state (2026-11-06, master_tick_poll)
static uint32_t s_tk_seq = 0;          // last grid beat consumed
static uint32_t s_tk_beat_sample = 0;  // sample time of the current beat (tick 0)
static uint32_t s_tk_in_beat = 24;     // ticks already sent for the current beat
static bool     s_start_pending = false; // Start deferred to the next grid beat
static bool     s_tk_pred = false;     // current beat's tick 0 was sent on a prediction
static uint32_t s_tk_beat_abs16 = 0;   // abs16 of the current beat
static uint32_t s_last_role_announce_sample = 0;
static bool     s_have_role_announce_anchor = false;
// TASK 1 fix: was `= true`, which combined with g_seq_running's old `true`
// init meant the very first edge test (g_seq_running != s_last_seq_running_
// sent) could never be true -- no initial Start ever went out. Now matches
// g_seq_running's new false default: the first real transition to sounding
// is guaranteed to be seen as an edge and transmitted.
static bool     s_last_seq_running_sent = false; // what we last told the bus via 0xFA/0xFC

// TASK 3 fix (2026-10-01): exact (unrounded, long-run-average-correct) tick
// period, carried as a whole-sample part (s_next_tick_sample advances by
// this every tick, same as before) plus a Q0.32 fixed-point fractional
// remainder accumulator. The old code rounded samples_per_tick to the
// nearest whole sample ONCE and used that rounded value for every tick --
// at any BPM where 60*SR/(bpm*24) isn't a whole number (i.e. almost any
// un-quantized tap-tempo value), the rounding error compounds every single
// tick, without bound, so the emitted MIDI clock's average tempo silently
// drifts away from the grid's own (unrounded) tempo forever. Carrying the
// remainder here means the LONG-RUN AVERAGE period is exact (Q32 precision
// is ~2.3e-10 samples/tick of residual error -- not a real-world drift
// source) even though each individual tick is still a whole number of
// samples (required -- s_next_tick_sample is a uint32_t sample count).
//
// TASK 2 fix (2026-10-01): the paragraph above used to end here with a TODO --
// engine/clock.h didn't expose the grid's own exact step period yet, so this
// derived an independently-computed (if individually exact) period from g_bpm.
// That's now fixed: clock_engine_grid_step_q32() (usb/clock_engine.h, defined
// in engine/audio_callback.cpp -- a C-linkage adapter around engine/clock.h's
// C++ clockStepSamplesQ32(), since this file is plain C and can't call a
// C++-mangled symbol) returns the EXACT Q32.32 samples-per-16th value
// engine/clock.cpp's own grid accumulator adds at every boundary -- not a
// second computation of an equivalent number, THE SAME number. 24 PPQN means
// samples-per-tick = (samples-per-16th << 2) / 24 (a quarter is exactly 4
// sixteenths, so the <<2 is exact); see master_tick_poll() below for the
// all-integer derivation (no double in the critical path, so no precision is
// lost converting a value this large through a 53-bit mantissa). The emitted
// MIDI clock and the audible grid now provably cannot slip against each other
// at any BPM, for any uptime -- one source of truth, not two.
static uint32_t s_tick_frac_accum = 0; // Q0.32 fractional samples owed, carries across ticks

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

// Live subsystem probes, defined in POCKETGORATOR_CODE.cpp. Used to turn the
// heavy main-loop subsystems on/off at runtime so we can watch which one
// kills USB without reflashing (each enable auto-reverts, so USB always
// comes back and CDC/DFU stay reachable).
extern void pmor_probe_set(const char* what, int on);
#endif  // CFG_TUD_CDC

//--------------------------------------------------------------------+
// Helpers
//--------------------------------------------------------------------+
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// Returns whether TinyUSB actually accepted the packet (tud_midi_packet_
// write()'s own return value -- false means the TX FIFO didn't have the 4
// bytes free, i.e. real backpressure). Added for TASK 2 (catch-up): the
// bulk-tick loop in master_tick_poll() uses this to know when to STOP
// trying to flush backlog this call rather than silently losing track of
// how much got sent -- the call-site instrumentation below is unchanged
// (still counts every call regardless of success, per its own longstanding
// documented invariant -- see that comment) and is a different, deliberate
// thing from this return value.
static bool midi_send_realtime(uint8_t status) {
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
  return tud_midi_packet_write(packet);
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

// Song Position Pointer (0xF2, 14-bit position in 16ths), System Common 3-byte, CIN 0x3.
static void midi_send_spp(uint32_t abs16) {
  uint16_t pos = (uint16_t) (abs16 & 0x3FFFu);
  uint8_t packet[4] = {
    (uint8_t) ((0u << 4) | MIDI_CIN_SYSCOM_3BYTE), // cable 0, CIN 0x3
    0xF2,
    (uint8_t) (pos & 0x7Fu),
    (uint8_t) ((pos >> 7) & 0x7Fu),
  };
  g_midi_packets_written_realtime++;
  tud_midi_packet_write(packet);
}

// Canonical tempo as a NUMBER: ch16 CC120 (MSB) then CC121 (LSB), back to back so the pair
// is never split by another message. `bpm` is quantised to the 14-bit wire grid here; callers
// that ORIGINATE a tempo (tap, editor) already adopted the quantised value in clockSetBpm().
static void midi_send_tempo14(float bpm) {
  uint16_t w = dt_bpm_to_wire14((double) bpm);
  midi_send_cc16(120, dt_wire14_msb(w));
  midi_send_cc16(121, dt_wire14_lsb(w));
  s_wire_sent_last   = w;
  s_wire_sent_sample = g_sample_clock;
  s_wire_sent_any    = true;
}
// A BPM-editor sweep changes the wire value every few ms; don't flood the endpoint.
#define WIRE_TEMPO_MIN_GAP_SAMPLES ((uint32_t)(AUDIO_SAMPLE_RATE * 0.05f))

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
// Follower phase-lock (added 2026-10-01, re-derived after first hardware
// test) -- called from handle_realtime()'s 0xF8 case, ONLY while g_role ==
// CLOCK_ROLE_DEMIURGE_MASTER, every 6th incoming tick (one 16th). Measures
// how far PMOR's own grid anchor (engine/clock.cpp, read via
// clock_engine_grid_next_step_sample()) is from where this 16th's arrival
// says it should be, then either arrives in one shot (cold start) or slews a
// quarter of that error in (jitter-rejecting). `now` is the sample time this
// 16th's tick arrived, in the SAME g_sample_clock domain handle_realtime()
// already reads elsewhere.
//
// BENCH BUG (first hardware test, fixed here): `pmor_next - now` is NOT
// reliably one whole step_period at this instant. clockTick() (audio ISR)
// and handle_realtime() (main loop) are different, unordered planes -- the
// main loop can observe a 16th's tick either just BEFORE or just AFTER
// clockTick() has consumed the matching grid boundary, with no guaranteed
// ordering between the two. The raw value below is therefore ambiguous by up
// to a whole step_period; measured on hardware as error=-5889 samples at
// step_period=5806 (-1.01 step) while PMOR was actually locked to within
// ~83 samples. Fix: reduce the raw value modulo step_period into the
// symmetric range (-step/2, +step/2] -- a value in that range is correct
// regardless of which side of the ISR/main-loop race `now` landed on, so the
// result no longer depends on an ordering this firmware can't guarantee.
//--------------------------------------------------------------------+
static int32_t  s_ferr_ring[256];          // |phase error| (samples) of the last 256 corrections
static uint32_t s_ferr_n = 0;
void clock_engine_ferr_reset(void) { s_ferr_n = 0; }
// mean / 99th percentile / max of the follower phase error in MICROSECONDS over the last <=256 corrections (one per 16th)
uint32_t clock_engine_ferr_stats(uint32_t* mean_us, uint32_t* p99_us, uint32_t* max_us) {
  uint32_t n = s_ferr_n < 256u ? s_ferr_n : 256u;
  if (n == 0) { *mean_us = *p99_us = *max_us = 0; return 0; }
  int32_t v[256]; uint64_t sum = 0;
  for (uint32_t i = 0; i < n; i++) { v[i] = s_ferr_ring[i]; sum += (uint32_t) v[i]; }
  for (uint32_t i = 1; i < n; i++) { int32_t k = v[i]; uint32_t j = i; while (j > 0 && v[j - 1] > k) { v[j] = v[j - 1]; j--; } v[j] = k; }   // insertion sort (control plane, <=256)
  uint32_t i99 = (n * 99u) / 100u; if (i99 >= n) i99 = n - 1u;
  *mean_us = (uint32_t) (((sum / n) * 1000000ull) / AUDIO_SAMPLE_RATE);
  *p99_us  = (uint32_t) (((uint64_t) v[i99] * 1000000ull) / AUDIO_SAMPLE_RATE);
  *max_us  = (uint32_t) (((uint64_t) v[n - 1] * 1000000ull) / AUDIO_SAMPLE_RATE);
  return n;
}
static void follower_phase_correct(uint32_t now) {
  // SAME source of truth master_tick_poll() uses for its own tick period --
  // see clock_engine_grid_step_q32()'s header comment. Never a second,
  // independently-derived period.
  uint64_t grid_step_q32 = clock_engine_grid_step_q32();       // Q32.32, samples per 16th
  uint32_t step_period    = (uint32_t) (grid_step_q32 >> 32);  // whole samples per 16th, PMOR's OWN current period
  if (step_period == 0) return; // guard: clock.cpp clamps bpm >= 1, so this shouldn't happen

  if (clock_engine_grid_reanchor_pending()) return;   // the grid is about to be re-anchored (Start/Continue): measuring against the OLD grid would queue a nudge the re-anchor then wipes
  uint32_t pmor_next     = clock_engine_grid_next_step_sample();
  int32_t  diff_to_next  = (int32_t) (pmor_next - now); // wrap-safe signed distance, now -> PMOR's next boundary
  int32_t  step_i        = (int32_t) step_period;

  // Pre-reduction value -- kept ONLY for diag (see s_follower_phase_error_raw_samples'
  // comment: this is what exposed the framing bug and must stay visible so the
  // next one doesn't hide the same way). Never fed into the correction itself.
  int32_t raw = diff_to_next - step_i + clock_engine_follow_lead_samples();   // target: boundary = tick - lead
  s_follower_phase_error_raw_samples = raw;

  // Reduce into (-step/2, +step/2]. C's % is sign-preserving (a negative raw
  // gives a negative or zero remainder), so both bounds need their own check
  // -- not an else-if, see the fix writeup above for why a single branch
  // isn't enough.
  int32_t err = raw % step_i;
  if (err > step_i / 2)       err -= step_i;
  if (err <= -(step_i / 2))   err += step_i;
  s_follower_phase_error_samples = err;
  s_ferr_ring[s_ferr_n & 255u] = err < 0 ? -err : err; s_ferr_n++;      // lock-quality statistics (STATUS ferr=)
  s_follower_phase_error_period  = step_period;

  if (s_follower_phase_cold) {
    // First correction after a role flip into DEMIURGE_MASTER, or after a
    // freewheel gap long enough to have reset the tempo PLL's own averaging
    // window (see the FREEWHEEL_GAP_SAMPLES reset below, which also sets this
    // flag): there is no slew history worth trusting, so arrive at the
    // already-reduced (therefore always <= half a 16th) error in one shot
    // rather than spending ~11 sixteenths creeping in from a cold anchor.
    clock_engine_grid_phase_nudge(-err);   // SIGN: err = (grid boundary after the tick) - (tick); a boundary that is LATE (err>0) must move EARLIER
    s_follower_phase_snap_count++;
    s_follower_phase_cold = false;
  } else {
    int32_t correction = err / (1 << PHASE_SLEW_SHIFT); // truncating divide -- see PHASE_SLEW_SHIFT comment
    if (correction != 0) {
      clock_engine_grid_phase_nudge(-correction);
      s_follower_phase_slew_count++;
    }
  }
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
// True once clock_engine_process_audio() has ever run: it then owns the
// g_sample_clock advance, and clock_engine_note_audio_block() must not
// double-advance. Builds that skip process_audio() leave this false and the
// note call becomes the sole clock source.
static volatile bool s_process_audio_seen = false;

void clock_engine_process_audio(float const *const *in, float *const *out, size_t n_frames) {
  s_process_audio_seen = true;
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

// ---- STUDIO / EDIT mode (docs/PROTOCOL.md section 6): PMOR is a pure follower of Live's USB MIDI clock.
// Role pinned to DEMIURGE_MASTER (the existing follower PLL + phase-lock), no election, no role/tap CCs on ch16,
// no clock out; 0xF2 SPP is honoured with Continue; Stop halts the sequencer (see pmorSetTransportRun in main).
static uint8_t  s_policy       = 0;      // clock-role policy: 0 AUTO, 1 INTERNAL, 2 FOLLOW, 3 MASTER (hw/studio.cpp pushes the persisted user choice)
static uint32_t s_role_since   = 0;      // g_sample_clock when the current role was adopted
static bool     s_studio       = false;
static uint16_t s_spp16        = 0;      // last Song Position Pointer (16ths)
static bool     s_spp_valid    = false;
static void (*s_transport_cb)(bool) = 0;
static void transport_notify(bool run) { if (s_transport_cb) s_transport_cb(run); }
void clock_engine_set_transport_cb(void (*cb)(bool run)) { s_transport_cb = cb; }
static void apply_role(clock_role_t new_role, bool announce) {
  if (new_role == g_role && g_boot_phase == BOOT_RESOLVED) return; // idempotent, no-op
  g_role = new_role;
  if (new_role == CLOCK_ROLE_OPG_MASTER) transport_notify(true);   // its own clock again: nothing is stopped by a bus transport any more
  s_role_since = g_sample_clock;
  g_boot_phase = BOOT_RESOLVED;
  if (new_role == CLOCK_ROLE_DEMIURGE_MASTER) {
    // A genuine (non-idempotent, see the early return above) transition to
    // follower clears a prior tap-gesture's sticky master claim -- see
    // s_master_by_gesture's own comment. A tap only overrides SILENCE-driven
    // demotion (clock_role_election_poll()'s RULE 1); it doesn't make this
    // device permanently deaf to an explicit hand-back of the clock.
    s_master_by_gesture = false;
  }
  // Reset PLL/freewheel state on every role change so a stale window from
  // a previous stint can't leak into the new one (mirrors demiurge-
  // clock.cpp's role-flip reset exactly).
  s_have_last_rx_tick = false;
  s_midi_interval_count = 0;
  s_midi_interval_idx = 0;
  s_midi_interval_sum = 0;
  s_follower_tick_count = 0; s_ph_have = false; s_tk_n = 0; s_bpm_n = 0; // same reasoning: a stale beat-position from a
                             // previous stint must not leak into the new one
  s_follower_phase_cold = true; // arrive in one shot on the first correction of the new stint, don't slew in
  s_tick_armed = false; // re-derive tick-emission phase from "now" if we're now master
  s_wire_have = false;      // a tempo NUMBER from a previous follower stint is not fresh authority in this one
  s_wire_have_msb = false;
  if (announce) {
    midi_send_cc16(116, (uint8_t) new_role);
  }
  s_have_role_announce_anchor = true;
  s_last_role_announce_sample = g_sample_clock;
}

static void handle_cc16(uint8_t cc, uint8_t val) {
  if (cc == 116 && val == (uint8_t) CLOCK_ROLE_DEMIURGE_MASTER && (s_policy == 1 || s_policy == 3)) return;   // user pinned MASTER/INTERNAL: no bus claim may take the clock
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
    if (g_role == CLOCK_ROLE_DEMIURGE_MASTER && !wire_tempo_fresh()) {
      // demiurge-clock.cpp:213 mapping, matched exactly. Legacy 7-bit (2.8 BPM/step): only used
      // while no 14-bit CC120/121 pair is fresh (a daemon predating the number, or foreign gear).
      float bpm = 40.0f + (val / 127.0f) * 360.0f;
      g_bpm = clampf(bpm, 40.0f, 400.0f);
    }
  } else if (cc == 120) {
    s_wire_msb = (uint8_t) (val & 0x7F);       // MSB first; applied when the LSB lands
    s_wire_have_msb = true;
  } else if (cc == 121) {
    // Tempo as a number. Only meaningful while following -- as master this is our own broadcast
    // bouncing back on the bidirectional bus (same self-echo gate as CC119/0xF8).
    if (s_wire_have_msb && g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
      g_bpm = clampf((float) dt_wire14_to_bpm(dt_wire14_join(s_wire_msb, val)), 40.0f, 400.0f);
      s_wire_have = true;
      s_wire_sample = g_sample_clock;
    }
  } else if (cc == 115) {
    // NEPTR's "I hold a loop" announce (new 2026-10, bench clock-ownership
    // fix -- see this file's "Clock-ownership election state" block).
    // Ungated by role on NEPTR's side -- sent every ~2s AND on change
    // regardless of who NEPTR currently thinks is master -- so just record
    // it unconditionally here too; the gating happens on the READ side
    // (remote_loop_considered_held(), clock_role_election_poll()), not here.
    // 127 = at least one looper channel holds audio, 0 = all empty; treat
    // anything >=64 as "held" (same threshold convention as CC117's
    // transport bit) rather than requiring an exact 127.
    g_remote_loop_held = (val >= 64);
    g_remote_loop_last_heard_sample = g_sample_clock;
    g_remote_loop_have_heard = true;
  } else if (cc == 20) {
    // Local test hook (NOT part of the Demiurge protocol): SY mode select.
    if (val <= 5) g_sync_mode = (sync_mode_t) val;
  } else if (cc == 21) {
    // Local test hook: click waveform select.
    g_click_wave = (val >= 64) ? CLICK_WAVE_TONE_BURST : CLICK_WAVE_SHORT_PULSE;
  }
}

static void (*s_cc_cb)(uint8_t ch, uint8_t cc, uint8_t val) = 0;
void clock_engine_set_cc_cb(void (*cb)(uint8_t ch, uint8_t cc, uint8_t val)) { s_cc_cb = cb; }
static void handle_complete_message(uint8_t status, uint8_t const *data, uint8_t count) {
  (void) count;
  uint8_t hi  = status & 0xF0;
  uint8_t ch  = status & 0x0F;
  if (status == 0xF2) { // Song Position Pointer (14-bit, 16ths): remembered for the next Continue (STUDIO/EDIT)
    s_spp16 = (uint16_t)((data[0] & 0x7F) | ((uint16_t)(data[1] & 0x7F) << 7)); s_spp_valid = true;
    return;
  }
  if (hi == 0xB0 && ch == 0x0F && !s_studio) { // Control Change, channel 16 (NEPTR roles; ignored in STUDIO/EDIT)
    handle_cc16(data[0], data[1]);
  }
  if (hi == 0xB0 && ch != 0x0F && s_cc_cb) s_cc_cb(ch, data[0], data[1]);   // any other channel's CC (FX over MIDI, docs/PROTOCOL.md)
  // Other message types (notes, PC, etc.) aren't part of this protocol;
  // this firmware has no sequencer/synth engine in this scaffold to
  // route them to, so they're simply not acted on.
}

// The ONE transport-start path of the follower: Start, Continue(+SPP) and the CDC JOIN request all run it.
//   at_pos/pos16 : re-anchor the grid at that 16th (Continue+SPP) or at bar 1 now (Start / plain Continue)
//   next_tick_idx: tick index (since that anchor) of the NEXT F8 to be processed; 16th boundaries are indices 0, 6, 12, ...
//   reset_tempo  : Start/Continue restart the tempo estimator; JOIN keeps it (the stream has been running)
static uint32_t s_run_left = 0;
// A Start/Continue that shares a USB transfer with the clock ticks behind it was back-dated like a tick in that batch (one period per
// packet after it). It precedes tick 0 by only ~20 samples, so only the ticks BEYOND the first count: give one period back.
static uint32_t start_stamp(void) {
  uint32_t st = stamp_now();
  if (s_run_left > 0) st += (uint32_t) ((60.0 * (double) AUDIO_SAMPLE_RATE) / ((double) MIDI_PPQN * (double) g_bpm));
  return st;
}
static float s_seed_bpm = 0.0f;   // host-supplied tempo hint (TEMPO): seeds the estimator at a cold Start
void clock_engine_tempo_seed(float bpm) {
  if (bpm < 40.0f || bpm > 400.0f) return;
  s_seed_bpm = bpm;
  bool streaming = s_have_last_rx_tick && (uint32_t) (hires_sample_now() - s_last_rx_tick_sample) < (uint32_t) (AUDIO_SAMPLE_RATE / 5);   // an F8 within the last 200 ms
  if (!streaming || !g_seq_running) g_bpm = bpm;   // nothing to lock to (or stopped): the grid takes it at once; while F8 streams the PLL rules and only the hint is kept
}
static void follower_start_at(bool at_pos, uint16_t pos16, uint32_t next_tick_idx, bool reset_tempo, bool has_stamp, uint32_t stamp) {
  g_seq_running = true;
  transport_notify(true);
  s_last_seq_running_sent = true;   // bus-driven; don't re-echo it as our own edge
  // Phase-lock (2026-10-01): "the bar starts HERE" is exactly what Start/Continue mean -- hard re-anchor instead of slewing toward it,
  // and reset the tick counter so the very next 16th-worth of ticks is what the phase-lock measures against.
  if (has_stamp) clock_engine_grid_reanchor_stamped(at_pos ? pos16 : 0u, stamp);
  else if (at_pos) clock_engine_grid_reanchor_at(pos16);
  else        clock_engine_grid_reanchor_now();
  s_spp_valid = false;
  clock_engine_ferr_reset();
  s_follower_tick_count = next_tick_idx; s_ph_have = false;
  s_gap_realign = false;          // a Start/Continue defines the tick phase: nothing left to re-derive
  s_follower_phase_cold = true;   // the first 16th-boundary tick after Start snaps the phase in ONE shot (the re-anchor above lands at main-loop time, which can be a whole loop pass after the 0xFA's IRQ stamp)
  if (reset_tempo) { s_tk_n = 0; s_bpm_n = 0; if (s_seed_bpm > 0.0f) { for (int i = 0; i < 5; i++) s_bpm_est[i] = s_seed_bpm; s_bpm_n = 5; } }   // a seeded cold Start: the first real estimates only replace the hint one by one (median of 5)
}

// ---- CDC JOIN (docs/PROTOCOL.md): a host that connects while the DAW already plays gets no 0xFA. JOIN <spp16> says "the 0xF8 nearest in
// time to this request's arrival is tick 0 of 16th <spp16>, running". Pending when that tick has not arrived yet.
static bool     s_join_pending = false, s_join_done = false;
static uint16_t s_join_spp = 0; static uint32_t s_join_req = 0, s_join_deadline = 0; static int32_t s_join_late_us = 0;
static uint32_t s_cdc_stamp = 0; static bool s_cdc_stamp_valid = false;      // IRQ stamp of the last CDC OUT transfer (the request line)
static void join_apply(uint32_t tick_stamp, uint32_t next_tick_idx) {
  follower_start_at(true, s_join_spp, next_tick_idx, false, false, 0u);
  s_follower_phase_cold = true;                                      // the next 16th-boundary tick snaps the phase in one shot
  s_join_late_us = (int32_t) (((int64_t) (int32_t) (s_join_req - tick_stamp) * 1000000) / AUDIO_SAMPLE_RATE);
  s_join_pending = false; s_join_done = true;
}
int clock_engine_join_begin(uint16_t spp16) {
  if (g_role != CLOCK_ROLE_DEMIURGE_MASTER || !s_have_last_rx_tick) return -1;
  uint32_t nowS = hires_sample_now();
  if ((int32_t) (nowS - s_last_rx_tick_sample) > (int32_t) (AUDIO_SAMPLE_RATE / 2)) return -1;          // no live F8 stream (>500 ms)
  double per = (60.0 * (double) AUDIO_SAMPLE_RATE) / ((double) MIDI_PPQN * (double) g_bpm);
  s_join_req = s_cdc_stamp_valid ? s_cdc_stamp : nowS; s_cdc_stamp_valid = false;
  s_join_spp = spp16 & 0x3FFFu; s_join_done = false;
  int32_t dR = (int32_t) (s_join_req - s_last_rx_tick_sample);          // request time relative to the last tick we processed
  if ((double) dR > per * 0.5) {                                        // the NEXT tick is nearer and has not arrived: decide when it does
    s_join_pending = true; s_join_deadline = nowS + (uint32_t) AUDIO_SAMPLE_RATE / 2u; return 0;
  }
  // nearest among the last ticks we hold (ring of tick stamps): up to 2 ticks back, tick j-back has next index j+1
  uint32_t best_j = 0; int32_t best = dR < 0 ? -dR : dR; uint32_t have = s_tk_n < 25u ? s_tk_n : 25u;
  for (uint32_t j = 1; j <= 2u && j < have; j++) {
    uint32_t st = s_tk_ring[(s_tk_n - 1u - j) % 25u]; int32_t d = (int32_t) (s_join_req - st); if (d < 0) d = -d;
    if (d < best) { best = d; best_j = j; }
  }
  uint32_t tick_st = s_tk_ring[(s_tk_n - 1u - best_j) % 25u];
  join_apply(tick_st, best_j + 1u);
  return 0;
}
int clock_engine_join_poll(int32_t* late_us) {                         // 1 applied, 0 waiting, -2 gave up
  if (s_join_done) { *late_us = s_join_late_us; return 1; }
  if (s_join_pending && (int32_t) (hires_sample_now() - s_join_deadline) > 0) { s_join_pending = false; return -2; }
  return s_join_pending ? 0 : -2;
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
        uint32_t now = stamp_now();   // IRQ-stamped arrival time of THIS tick
        if (s_have_last_rx_tick && (int32_t) (now - s_last_rx_tick_sample) > (int32_t) FREEWHEEL_GAP_SAMPLES) {   // SIGNED: a stamp slightly EARLIER than the previous tick's (bunched/reordered IRQ stamps) used to wrap to ~2^32 and read as a 24 h gap -> spurious PLL reset + cold phase snap
          // Stale window (freewheel gap exceeded) -- start a fresh lock
          // rather than averaging across the gap.
          s_have_last_rx_tick = false;
          s_midi_interval_count = 0;
          s_midi_interval_idx = 0;
          s_midi_interval_sum = 0;
          // Same staleness signal for the phase-lock: whatever phase PMOR
          // drifted to during the gap has no relationship to the bus we're
          // about to resume tracking -- arrive in one shot on the next
          // correction instead of slewing in from a cold anchor.
          s_follower_phase_cold = true; s_ph_have = false; s_tk_n = 0; s_bpm_n = 0;
          s_gap_realign = true;   // the tick count is lost (an unknown number of ticks went by): re-derive this tick's index from PMOR's own grid below
        }
        if (s_have_last_rx_tick) {
          uint32_t interval = now - s_last_rx_tick_sample; // wrap-safe unsigned subtraction
          if (interval < s_iv_min) s_iv_min = interval; if (interval > s_iv_max && interval < 48000u) s_iv_max = interval;
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
            if (s_midi_interval_count >= 6) {      // was a full 24-tick window (a whole beat): with IRQ stamps 6 intervals are already clean
              float avg = (float) s_midi_interval_sum / (float) s_midi_interval_count;
              // Symbolic PLL constant (fix #6): bpm = 60*SR/(PPQN*avg),
              // not a magic 120000.0f literal.
              float bpm = (60.0f * ((float) AUDIO_SAMPLE_RATE)) / (MIDI_PPQN * avg);
              (void) bpm;   // superseded by the bunching-robust estimator below (tempo = median of 5 elapsed-window estimates)
            }
          }
        }
        s_last_rx_tick_sample = now;
        s_have_last_rx_tick = true;

        // ---- Phase-lock (2026-10-01): separate loop from the tempo PLL
        // above -- that one only writes g_bpm once its 24-tick window fills;
        // this one needs the raw tick stream, reacting every 16th (6 ticks),
        // or a mid-song Start/Continue would take many bars to find the beat.
        // Counted independently of the PLL's window/index bookkeeping so a
        // freewheel-gap reset of the PLL's averaging window above can never
        // skip or double a phase-lock boundary.
        bool join_tick = false;
        if (s_join_pending) { join_apply(now, 0u); join_tick = true; }   // this F8 is the nearest tick to the JOIN request: it is tick 0 of the 16th
        uint32_t tick_idx = s_follower_tick_count;   // 0 for the first clock after Start/Continue: the 16th boundaries are tick indices 0, 6, 12, ...
        s_follower_tick_count++;
        {   // tempo: elapsed time over the last <=24 ticks (immune to bunched/short intervals, which the old per-interval window REJECTED and so mis-scaled by ~4 %),
            // median of the last 5 estimates (a stamp that is late by one bunch only skews one estimate)
            s_tk_ring[s_tk_n % 25u] = now; s_tk_n++;
            if (s_tk_n >= 3u) {   // learn the tempo from the 3rd tick on (2 intervals): a cold Start must not run a whole beat at the old tempo; the median/longer window refine it
              uint32_t cnt = s_tk_n < 25u ? s_tk_n : 25u;
              uint32_t el = now - s_tk_ring[(s_tk_n - cnt) % 25u];
              if (el > 0) {
                float est = (60.0f * (float) AUDIO_SAMPLE_RATE * (float) (cnt - 1u)) / ((float) MIDI_PPQN * (float) el);
                s_bpm_est[s_bpm_n % 5u] = est; s_bpm_n++;
                if (s_bpm_n >= 1u) {
                  float t[5]; uint32_t m = s_bpm_n < 5u ? s_bpm_n : 5u;
                  for (uint32_t i = 0; i < m; i++) t[i] = s_bpm_est[i];
                  for (uint32_t i = 1; i < m; i++) { float k = t[i]; uint32_t j = i; while (j > 0 && t[j - 1] > k) { t[j] = t[j - 1]; j--; } t[j] = k; }
                  if (!wire_tempo_fresh() && !s_gap_realign) g_bpm = clampf(t[m / 2], 40.0f, 400.0f);   // the NUMBER outranks the PLL. (Held at the pre-gap tempo while a freewheel resume is being re-locked: the first elapsed-time estimates after a pause are bunched garbage, measured 100 BPM at 124, and the grid phase the re-lock reads would wander with them.)
                }
              }
            }
        }
        // One-way-delay phase estimator (2026-11-07). A USB host may deliver several ticks in ONE transfer (measured on the Pi's
        // ALSA path: tick intervals 0.25..27 ms around a 25 ms nominal), so a tick's IRQ stamp is only ever LATE, never early.
        // The true tick time is therefore the lower envelope of (stamp - k*period): a sliding minimum over ~2 beats.
        {
          if (!s_ph_have) { s_ph_t0 = now; s_ph_k = 0; s_ph_n = 0; s_ph_have = true; }
          double per = (60.0 * (double) AUDIO_SAMPLE_RATE) / ((double) MIDI_PPQN * (double) g_bpm);   // samples per tick (PLL tempo)
          // Store RAW (stamp - t0) and the tick index; recompute residuals against the oldest window entry with the CURRENT period. (Storing
          // (stamp - k*period_at_the_time) made any tempo wobble scale with k, which grows without bound since Start: macbert's sim, ferr
          // mean 26.8 ms / p99 59.9 ms from 0.5 ms of jitter. Residuals are now relative within one ~2-beat window.)
          uint32_t idx = s_ph_n % PH_WIN;
#ifdef FOLLOWER_LEGACY
          s_ph_x[idx] = (int32_t) ((double) (int32_t) (now - s_ph_t0) - (double) s_ph_k * per); s_ph_kk[idx] = 0;   // old: residual vs the period AT THAT TIME
#else
          s_ph_x[idx] = (int32_t) (now - s_ph_t0); s_ph_kk[idx] = s_ph_k;
#endif
          uint32_t kcur = s_ph_k;
          s_ph_n++; s_ph_k++;
          uint32_t cnt = s_ph_n < PH_WIN ? s_ph_n : PH_WIN;
          uint32_t ref = (s_ph_n - cnt) % PH_WIN;
          double mn = 1e18;
#ifdef FOLLOWER_LEGACY
          { int32_t m0 = s_ph_x[0]; for (uint32_t i = 1; i < cnt; i++) if (s_ph_x[i] < m0) m0 = s_ph_x[i]; s_ph_est_now = s_ph_t0 + (uint32_t) (int32_t) ((double) kcur * per + (double) m0); }
#else
          for (uint32_t i = 0; i < cnt; i++) { double xi = (double) (s_ph_x[i] - s_ph_x[ref]) - (double) (s_ph_kk[i] - s_ph_kk[ref]) * per; if (xi < mn) mn = xi; }
          s_ph_est_now = s_ph_t0 + (uint32_t) (int32_t) ((double) s_ph_x[ref] + (double) (kcur - s_ph_kk[ref]) * per + mn);
#endif
          s_ph_ok = (cnt >= 12);
        }
        if (s_gap_realign && s_ph_ok) {
          // FREEWHEEL RESUME without a Start/Continue/SPP (>= 2 s of silence): the tick count no longer says where in the 16th a tick falls, and the cold snap would
          // pull PMOR's boundary onto whichever tick the stale count calls a multiple of 6 -- up to 5 ticks (130 ms at 124 BPM) from the sender's real 16th until the next
          // Start/SPP. The grid free-ran through the gap at the same tempo, so ITS phase says which of the 6 ticks of a 16th this one is. Decided once the one-way-delay
          // estimator has a settled tick time (12 ticks; a single IRQ stamp can be 2-3 ticks late when the host bunches ticks after a pause), not on the first tick.
          s_gap_realign = false;
          uint32_t sp = (uint32_t) (clock_engine_grid_step_q32() >> 32);
          if (sp >= 6u && !clock_engine_grid_reanchor_pending()) {
            int32_t d = (int32_t) (clock_engine_grid_next_step_sample() - s_ph_est_now) + clock_engine_follow_lead_samples();            // this tick (estimated true time) -> PMOR's next 16th boundary
            int32_t m = d % (int32_t) sp; if (m < 0) m += (int32_t) sp;
            uint32_t pos = (sp - (uint32_t) m) % sp;                                                // samples since PMOR's last boundary
            uint32_t j = (uint32_t) (((uint64_t) pos * MIDI_TICKS_PER_16TH + sp / 2u) / sp) % MIDI_TICKS_PER_16TH;   // nearest tick of the 16th
            s_follower_tick_count = j + 1u; tick_idx = j;
            s_follower_phase_cold = true;                                                           // snap the (sub-tick) remainder in one shot
          }
        }
#ifdef FOLLOWER_LEGACY
        if ((s_follower_tick_count % MIDI_TICKS_PER_16TH) == 0) {
#else
        if ((tick_idx % MIDI_TICKS_PER_16TH) == 0 && !join_tick && !s_gap_realign) {   // (the JOIN tick itself re-anchored the grid at this instant: next correction at index 6)
#endif   // (was: count AFTER the increment, i.e. the 6th tick = index 5: PMOR's boundary sat one tick early)
          follower_phase_correct(s_ph_ok ? s_ph_est_now : now);
        }
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
        s_start_count++;
        s_last_seq_running_sent = true; // bus-driven; don't re-echo it as our own edge

        // Phase-lock (2026-10-01): "the bar starts HERE" is exactly what
        // Start/Continue mean -- hard re-anchor instead of slewing toward it,
        // and reset the tick counter so the very next 16th-worth of ticks
        // (not whatever phase was mid-beat before) is what the phase-lock
        // measures against.
        follower_start_at(s_studio && status == 0xFB && s_spp_valid, s_spp16, 0u, true, true, start_stamp());   // Continue at the stated position, or Start / plain Continue: bar 1
      }
      break;

    case 0xFC: // Stop
      if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) {
        g_seq_running = false;
        transport_notify(false);
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
  if (s_studio) return;
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
// Clock-ownership election (bench fix, 2026-10-01) -- see the state block
// above (before "Master tick scheduling") for CC115/sounding/gesture state,
// and clock_engine.h for the user-facing contract this implements.
//--------------------------------------------------------------------+

// Safe-default read of g_remote_loop_held: "never heard" and "stale" both
// collapse to "no loop held" (RULE 3's "unknown" case) -- the safe default
// when the Pi is silent/absent/not-yet-booted is that PMOR CAN lead, not
// that it's locked out of leading by a loop-hold claim nobody can still
// vouch for.
static bool remote_loop_considered_held(void) {
  if (!g_remote_loop_have_heard) return false;
  uint32_t age = g_sample_clock - g_remote_loop_last_heard_sample; // wrap-safe unsigned subtraction
  if (age > REMOTE_LOOP_STALE_SAMPLES) return false; // stale -- same safe default as never-heard
  return g_remote_loop_held;
}

// See s_local_sounding's own comment: same debounce TASK 1 already added,
// just pulled out of master_tick_poll()'s OPG_MASTER-only gate so it keeps
// running (and keeps seeing edges) while following the bus too.
static void local_sounding_poll(void) {
  bool raw = g_sounding_raw;
  if (raw) {
    s_sounding_silent_pending = false;
    s_local_sounding = true;
  } else if (s_local_sounding) {
    uint32_t now_ms = board_get_now_ms();
    if (!s_sounding_silent_pending) {
      s_sounding_silent_pending = true;
      s_sounding_silent_since_ms = now_ms;
    } else if ((now_ms - s_sounding_silent_since_ms) >= SOUNDING_SILENT_DEBOUNCE_MS) {
      s_local_sounding = false;
      s_sounding_silent_pending = false;
    }
  }
}

// Decision table (see clock_engine.h and README for the user-facing
// writeup):
//   silent                                   -> DEMIURGE_MASTER (defer)   [RULE 1]
//   silent->sounding edge, remote holds loop -> DEMIURGE_MASTER (pick up) [RULE 2]
//   silent->sounding edge, remote has none   -> OPG_MASTER (lead)         [RULE 3]
//   tap tempo (clock_engine_human_gesture)   -> OPG_MASTER, always        [RULE 4]
//   already sounding, no edge                -> unchanged (no re-decide) [RULE 5]
// RULE 1 has one exception: a sticky tap-gesture claim (s_master_by_gesture)
// is not undone by silence alone -- see that flag's own comment.
// apply_role() is idempotent (no-op when the role isn't actually changing),
// so re-asserting RULE 1 every poll while silent cannot itself cause
// flapping; only a genuine edge or an explicit CC115/gesture change can
// move the role at all.
static void clock_role_election_poll(void) {
  if (s_studio) return;
  if (g_boot_phase != BOOT_RESOLVED) return; // boot_listen_poll owns role until resolved
  // AUTOMATIC clock role (2026-11-08, EMGOR): PMOR is the clock MASTER (continuous 0xF8 + Start/Stop/SPP, the relay locks NEPTR to it).
  // It follows a bus clock only while one is actually arriving, and takes the clock back ~2 s after it stops. The old rules that
  // handed the clock to the Pi whenever PMOR was silent (or NEPTR held a loop) are gone: they latched PMOR in follow with no clock arriving.
  if (g_role == CLOCK_ROLE_DEMIURGE_MASTER && s_policy != 2) {
    uint32_t last = s_have_last_rx_tick ? s_last_rx_tick_sample : s_role_since;
    if ((uint32_t) (g_sample_clock - last) > (uint32_t) (AUDIO_SAMPLE_RATE * 2.0f)) apply_role(CLOCK_ROLE_OPG_MASTER, true);
  }
  s_role_election_prev_sounding = s_local_sounding;
}

//--------------------------------------------------------------------+
// Master tick scheduling (fix #1: wrap-safe uint32_t, fix #6 symbolic)
//--------------------------------------------------------------------+
static void master_tick_poll(void) {
  if (g_boot_phase != BOOT_RESOLVED || g_role != CLOCK_ROLE_OPG_MASTER) {
    s_tick_armed = false; // re-arm fresh whenever we (re-)become master
    // NOTE: the silence debounce (s_sounding_silent_pending/since_ms) is no
    // longer reset here on a role change -- it's now owned by
    // local_sounding_poll(), which runs every clock_engine_poll() call
    // regardless of role (see s_local_sounding's comment for why: the
    // clock-ownership election needs to see local sounding edges even
    // while we're following the bus), so there's no stale per-role timing
    // left behind to clean up here any more.
    return;
  }

  // ---- TASK 1: debounce "is this instrument making sound" into
  // g_seq_running. Immediate on silent->sounding (Start should fire right
  // when something would actually be heard); ~120ms of stable silence
  // required before sounding->silent (so a one-step gap between hits can't
  // spam Start/Stop). This is the ONLY writer of g_seq_running while
  // OPG_MASTER -- while following the bus, handle_realtime() owns it
  // instead (see clock_engine_set_sounding()'s header comment). The actual
  // debounce now lives in local_sounding_poll() (role-independent, see its
  // comment) -- this just mirrors the already-debounced result into
  // g_seq_running while we're the one driving transport from it.
  g_seq_running = s_local_sounding;

  // Transport (2026-11-06): Stop goes out at once. Start is deferred to the next
  // grid beat (s_start_pending, see the tick block below) and goes out as
  // SPP(abs16) + Continue -- or Start when abs16 is 0 -- so a listener always
  // knows WHERE in the bar the clock is, not just that it is running.
  if (g_seq_running != s_last_seq_running_sent) {
    s_last_seq_running_sent = g_seq_running;
    if (g_seq_running) {
      s_start_pending = true;
      s_tick_armed    = false;
    } else {
      midi_send_realtime(0xFC);
      midi_send_cc16(117, 0); // transport, so a host can see it without parsing realtime
      s_start_pending = false;
    }
  }

  uint32_t now = g_sample_clock;

  // Tempo as a number: immediately on any change (a tap must reach the Pi and the displays NOW,
  // not at the next ~2 s announce), rate-limited so a BPM-editor sweep cannot flood the endpoint.
  {
    uint16_t w = dt_bpm_to_wire14((double) g_bpm);
    if (w != s_wire_sent_last &&
        (!s_wire_sent_any || (uint32_t) (now - s_wire_sent_sample) >= WIRE_TEMPO_MIN_GAP_SAMPLES)) {
      midi_send_tempo14(g_bpm);
    }
  }
  // TASK 2/3: exact (unrounded) mean tick period, derived ENTIRELY in 64-bit
  // fixed-point integer math from the grid's own Q32.32 samples-per-16th value
  // -- see s_tick_frac_accum's comment above. No double anywhere in this path:
  // grid_step_q32 is already Q32.32 (raw/2^32 = exact samples-per-16th); <<2 is
  // an exact *4 (a quarter is exactly 4 sixteenths); /24 is the only lossy step,
  // and it loses at most 2^-32 samples/tick (same residual the old g_bpm-double
  // path accepted deliberately) -- a single 64-bit integer divide, not a
  // round-trip through a 53-bit double mantissa.
  uint64_t grid_step_q32  = clock_engine_grid_step_q32();   // Q32.32, samples per 16th (clock.cpp's own grid period)
  uint64_t tick_period_q32 = (grid_step_q32 << 2) / (uint64_t) MIDI_PPQN; // Q32.32, samples per MIDI tick (24 PPQN)
  uint32_t samples_per_tick_int = (uint32_t) (tick_period_q32 >> 32); // whole-sample part every tick advances by
  if (samples_per_tick_int == 0) samples_per_tick_int = 1; // guard div-by-zero-ish stall (same intent as the old guard)
  // Fractional remainder as a Q0.32 fixed-point fraction (2^32 "ticks" = 1
  // full sample) -- carried in s_tick_frac_accum and only ever rounds UP
  // into an extra whole sample on overflow, so the average over many ticks
  // converges on the true (unrounded) period with no bounded-but-nonzero
  // bias the way a naive round-half-up-every-tick scheme would have.
  uint32_t samples_per_tick_frac_q32 = (uint32_t) (tick_period_q32 & 0xFFFFFFFFull);

  // ---- 0xF8 from the GRID (2026-11-06) ---------------------------------------
  // Every tick is derived from the sequencer's own beats (clock.cpp publishes
  // each grid beat: sample + abs16): tick k of a beat is due at
  // beat_sample + k * tick_period. Exactly 24 per beat, tick 0 ON the beat, so a
  // tap (which moves the grid) moves the clock with it -- before, the ticks ran
  // on their own free timer and a tap left them a fraction of a beat off.
  // At each new beat: a deferred Start goes out first (SPP + Continue/Start);
  // every bar an SPP re-states the position; if the previous beat could not
  // finish its 24 (tap speed-up, stall) the remainder is dropped and the beat
  // is announced with SPP + Continue instead, so no listener ever counts a
  // wrong number of ticks into a beat.
  (void) samples_per_tick_frac_q32;
  {
    uint32_t bseq, bsample, babs16;
    clock_engine_grid_beat_anchor(&bseq, &bsample, &babs16);
    if (!s_tick_armed) {                 // (re)entering master or (re)starting: wait for a fresh beat
      s_tk_seq = bseq;
      s_tk_in_beat = GRID_TICKS_PER_BEAT;          // nothing owed until the next beat arrives
      s_tk_beat_sample = bsample;                  // anchor the beat prediction to the latest REAL grid beat (a stale anchor made
      s_tk_beat_abs16  = babs16;                   //   a role change after a long follower stint burst thousands of catch-up ticks, measured 2026-11-07)
      s_tk_pred = false;
      s_tick_armed = true;
    }
    // Half a tick, in samples: how far a published grid beat may sit from the
    // predicted one and still be "the same beat" (re-pin only, no transport msg).
    int32_t half_tick = (int32_t) (tick_period_q32 >> 33);
    bool repinned = false;
    const bool tick_on = !s_studio;      // master clock is CONTINUOUS: ticks do not depend on the sequencer sounding (Start/Stop still do)
    if (tick_on && bseq != s_tk_seq && s_tk_pred && !s_start_pending) {
      int32_t e = (int32_t) (bsample - s_tk_beat_sample);
      if (e > -half_tick && e < half_tick) {        // prediction confirmed: re-pin to the grid
        s_tk_seq = bseq;
        s_tk_beat_sample = bsample;
        s_tk_beat_abs16 = babs16;
        s_tk_pred = false;
        repinned = true;
      }
    }
    if (!repinned && tick_on && bseq != s_tk_seq) {
      // A grid beat that does not match the predicted one (tap / tempo jump), or
      // the first beat after Start. If tick 0 was already sent on a prediction
      // that turned out wrong, the beat is re-announced with SPP + Continue.
      bool mispredicted = s_tk_pred && !s_start_pending;
      s_tk_pred = false;
      bool short_beat = ((s_tk_in_beat < GRID_TICKS_PER_BEAT) && !s_start_pending) || mispredicted;
      uint32_t owed = GRID_TICKS_PER_BEAT - s_tk_in_beat;
      if (short_beat && owed <= TICK_CATCHUP_BURST_MAX / 4u) {   // a few late ticks: finish the beat
        while (s_tk_in_beat < GRID_TICKS_PER_BEAT && midi_send_realtime(0xF8)) { g_tick_count++; g_tick_catchup_count++; s_tk_in_beat++; }
        short_beat = (s_tk_in_beat < GRID_TICKS_PER_BEAT);
      }
      s_tk_seq = bseq;
      s_tk_beat_sample = bsample;
      s_tk_beat_abs16 = babs16;
      s_tk_in_beat = 0;
      if (s_start_pending) {
        midi_send_spp(babs16);
        midi_send_realtime((babs16 & 0x3FFFu) == 0u ? 0xFA : 0xFB);
        midi_send_cc16(117, 127);
        s_start_pending = false;
      } else if (short_beat && g_seq_running) {
        g_tick_resync_count++;
        midi_send_spp(babs16);
        midi_send_realtime(0xFB);        // Continue at the stated position
      } else if ((babs16 & 15u) == 0u) {
        midi_send_spp(babs16);           // once per bar: where bar 1 is (unpredicted beat)
      }
    }
    // Predict the next beat: tick 0 goes out ON TIME from the schedule instead of
    // waiting for clock.cpp to publish the beat (which lands ~1 audio block late
    // and made every tick 0 ~3 ms late, measured 2026-11-06). The published beat
    // then re-pins the schedule above.
    if (tick_on && !s_start_pending && s_tk_in_beat >= GRID_TICKS_PER_BEAT) {
      uint32_t nb = s_tk_beat_sample + (uint32_t) (((uint64_t) GRID_TICKS_PER_BEAT * tick_period_q32) >> 32);
      if ((int32_t) (now - nb) >= 0) {
        s_tk_beat_sample = nb;
        s_tk_in_beat = 0;
        s_tk_pred = true;
        s_tk_beat_abs16 += 4u;
        if ((s_tk_beat_abs16 & 15u) == 0u) midi_send_spp(s_tk_beat_abs16); // once per bar
      }
    }
    if (tick_on && !s_start_pending) {
      uint32_t guard = 0;
      while (s_tk_in_beat < GRID_TICKS_PER_BEAT && guard < TICK_CATCHUP_BURST_MAX) {
        uint32_t due = s_tk_beat_sample + (uint32_t) (((uint64_t) s_tk_in_beat * tick_period_q32) >> 32);
        if ((int32_t) (now - due) < 0) break;
        if (!midi_send_realtime(0xF8)) break; // FIFO backpressure -- retried next poll
        g_tick_count++;
        if (guard > 0) g_tick_catchup_count++;
        s_tk_in_beat++;
        guard++;
      }
    }
  }

  if (!s_have_role_announce_anchor || (now - s_last_role_announce_sample) >= ROLE_ANNOUNCE_SAMPLES) {
    s_last_role_announce_sample = now;
    s_have_role_announce_anchor = true;
    midi_send_cc16(116, (uint8_t) g_role);
    uint8_t v = (uint8_t) clampf(((g_bpm - 40.0f) / 360.0f) * 127.0f, 0.0f, 127.0f);
    midi_send_cc16(119, v);
    midi_send_tempo14(g_bpm);                      // the canonical number, alongside the legacy 7-bit
    midi_send_cc16(117, g_seq_running ? 127 : 0); // TASK 1: re-announce transport alongside role/BPM
  }
}

// Follower also re-announces its role every ~2s, same as the daemon does
// regardless of its own role (demiurge-clock.cpp's tick_thread announces
// unconditionally) -- keeps a late-plugged UI/daemon converging quickly
// even when this device isn't the one driving ticks.
// Transport announced while FOLLOWING. Mirrors NEPTR's own ungated CC115:
// "am I making sound" is a FACT about this instrument, not a function of who
// owns the clock, so it must reach the bus in BOTH roles.
//
// Why this exists (bench-observed 2026-10-01): every CC117 send used to live
// inside master_tick_poll(), which returns early unless OPG_MASTER. The moment
// PMOR correctly deferred the clock it also went silent about transport, and
// the Pi's clock_status.conf decayed to `pmor_transport = unknown`. That breaks
// the election outright: RULE 2 has PMOR DEFER when NEPTR holds a loop, so if
// deferring also hides the fact that PMOR started playing, the daemon can never
// learn it -- the only way PMOR revealed "I'm sounding" was by claiming master,
// which is exactly what RULE 2 tells it not to do.
//
// NOTE the value is s_local_sounding, NOT g_seq_running. While following,
// g_seq_running is the BUS's transport (owned by handle_realtime()); echoing it
// back would just tell the Pi what the Pi already told us. The daemon wants to
// know whether THIS instrument is making sound, which is s_local_sounding.
static bool     s_follower_transport_sent  = false;
static bool     s_have_follower_transport_anchor = false;

static void follower_role_announce_poll(void) {
  if (s_studio) return;
  if (g_boot_phase != BOOT_RESOLVED || g_role != CLOCK_ROLE_DEMIURGE_MASTER) return;
  uint32_t now = g_sample_clock;

  // change-triggered: a real transition reaches the daemon immediately rather
  // than waiting out the ~2s re-announce.
  if (!s_have_follower_transport_anchor || s_local_sounding != s_follower_transport_sent) {
    s_follower_transport_sent = s_local_sounding;
    s_have_follower_transport_anchor = true;
    midi_send_cc16(117, s_local_sounding ? 127 : 0);
  }

  if (!s_have_role_announce_anchor || (now - s_last_role_announce_sample) >= ROLE_ANNOUNCE_SAMPLES) {
    s_last_role_announce_sample = now;
    s_have_role_announce_anchor = true;
    midi_send_cc16(116, (uint8_t) g_role);
    midi_send_cc16(117, s_local_sounding ? 127 : 0);  // keep pmor_transport fresh on the Pi
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
  // TASK 1 fix: starts SILENT, not running -- see g_seq_running's own
  // comment at its declaration for why the old `true` default meant this
  // firmware never transmitted its first real Start.
  g_seq_running = false;
  g_sounding_raw = false;
  s_sounding_silent_pending = false;
  s_sounding_silent_since_ms = 0;
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

  s_follower_tick_count              = 0;
  s_follower_phase_error_raw_samples = 0;
  s_follower_phase_error_samples     = 0;
  s_follower_phase_error_period      = 1;
  s_follower_phase_slew_count        = 0;
  s_follower_phase_snap_count        = 0;
  s_follower_phase_cold              = true;

  g_remote_loop_held = false;
  g_remote_loop_last_heard_sample = 0;
  g_remote_loop_have_heard = false;
  s_local_sounding = false;
  s_role_election_prev_sounding = false;
  s_master_by_gesture = false;

  s_tick_armed = false;
  s_tick_frac_accum = 0;
  s_have_role_announce_anchor = false;
  // TASK 1 fix: mirrors g_seq_running's new false default -- the first real
  // silent->sounding transition is now guaranteed to be seen as an edge.
  s_last_seq_running_sent = g_seq_running;

  s_rx_status = 0;
  s_rx_data_count = 0;
#if CFG_TUD_CDC
  s_dfu_request = false;
#endif

  g_tick_count                    = 0;
  g_tick_catchup_count            = 0;
  g_tick_resync_count             = 0;
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

// ---- IRQ-time stamping of incoming USB-MIDI (2026-11-07) -----------------------------------------------
// Timestamps used to be taken when the main loop got round to the bytes (up to several ms late, bunched).
// Now the USB IRQ (patched dcd_dwc2: dcd_rx_complete_hook) stamps every finished MIDI OUT transfer with a
// high-resolution sample time: g_sample_clock at the start of the current audio block + the timer ticks since then
// converted to samples. tud_midi_rx_cb() (tud_task context) attaches that stamp to every packet of the transfer;
// clock_engine_poll() then feeds the parser one packet at a time with the packet's own stamp as "now".
extern uint32_t pmor_tick(void);
extern uint32_t pmor_tick_hz(void);
static volatile uint32_t s_blk_tick = 0;                 // timer tick at the last audio-block start
static volatile uint32_t s_blk_n = 64;                   // frames of that block
static volatile uint32_t s_irq_stamp[32];                // ISR -> main: one entry per finished MIDI OUT transfer
static volatile uint8_t  s_irq_w = 0, s_irq_r = 0;
static uint32_t s_pkt_stamp[64];                         // main: one stamp per queued 4-byte packet
static uint8_t  s_pkt_w = 0, s_pkt_r = 0;
static uint32_t s_fifo_prev_packets = 0;
static uint32_t s_rx_stamp = 0; static bool s_rx_stamp_valid = false;
static uint32_t s_run_stamp = 0, s_run_len = 0;   // (s_run_left is declared above start_stamp) packets sharing one IRQ stamp (one USB transfer)
uint32_t clock_engine_iv_stats(uint32_t* min_us, uint32_t* max_us) {
  *min_us = s_iv_max ? (uint32_t) (((uint64_t) s_iv_min * 1000000ull) / AUDIO_SAMPLE_RATE) : 0; *max_us = (uint32_t) (((uint64_t) s_iv_max * 1000000ull) / AUDIO_SAMPLE_RATE);
  s_iv_min = 0xFFFFFFFFu; s_iv_max = 0; return 0;
}
static uint32_t s_irq_count = 0, s_irq_dropped = 0, s_cb_count = 0, s_pk_count = 0, s_nostamp = 0; static int32_t s_late_max = 0, s_late_min = 0;
void clock_engine_rx_stats(uint32_t* irq, uint32_t* cb, uint32_t* pk, uint32_t* nostamp, int32_t* late_max_us, int32_t* late_min_us) {
  *irq = s_irq_count; *cb = s_cb_count; *pk = s_pk_count; *nostamp = s_nostamp;
  *late_max_us = (int32_t) (((int64_t) s_late_max * 1000000) / AUDIO_SAMPLE_RATE); *late_min_us = (int32_t) (((int64_t) s_late_min * 1000000) / AUDIO_SAMPLE_RATE);
  s_late_max = -1000000; s_late_min = 1000000;
}

static inline uint32_t hires_sample_now(void) {
  uint32_t dt = pmor_tick() - s_blk_tick;
  uint32_t hz = pmor_tick_hz();
  uint32_t frac = (uint32_t) (((uint64_t) dt * (uint64_t) AUDIO_SAMPLE_RATE) / (uint64_t) hz);
  if (frac > s_blk_n) frac = s_blk_n;   // cannot be more than one block unless the audio ISR stalled
  // g_sample_clock was advanced by this block BEFORE s_blk_tick was taken, while the grid (clock.cpp) works in the block-START domain:
  // subtract the block so a tick stamped now and the grid boundary of this block share one time base (was a constant +1 block = 1.33 ms late).
#ifdef FOLLOWER_LEGACY
  return g_sample_clock + frac;                       // regression-test only: the pre-fix behaviour (+1 block)
#else
  return g_sample_clock - s_blk_n + frac;
#endif
}
void dcd_rx_complete_hook(uint8_t epnum) {              // USB IRQ context
  if (epnum == EPNUM_CDC_DATA) { s_cdc_stamp = hires_sample_now(); s_cdc_stamp_valid = true; return; }   // JOIN request arrival time
  if (epnum != EPNUM_MIDI) return;
  uint8_t w = s_irq_w;
  if ((uint8_t) (w - s_irq_r) >= 32u) { s_irq_dropped++; return; }
  s_irq_stamp[w & 31u] = hires_sample_now();
  s_irq_w = (uint8_t) (w + 1u);
  s_irq_count++;
}
void tud_midi_rx_cb(uint8_t itf) {                      // tud_task context, after the transfer landed in the FIFO
  (void) itf; s_cb_count++;
  uint32_t stamp = (s_irq_r != s_irq_w) ? s_irq_stamp[s_irq_r & 31u] : hires_sample_now();
  if (s_irq_r != s_irq_w) s_irq_r = (uint8_t) (s_irq_r + 1u);
  uint32_t avail = tud_midi_available() / 4u;           // packets now queued
  while (s_fifo_prev_packets < avail) {
    if ((uint8_t) (s_pkt_w - s_pkt_r) < 64u) { s_pkt_stamp[s_pkt_w & 63u] = stamp; s_pkt_w++; }
    s_fifo_prev_packets++;
  }
}
static uint8_t cin_len(uint8_t cin) {
  switch (cin) { case 0x5: case 0xF: return 1; case 0x2: case 0x6: case 0xC: case 0xD: return 2; case 0x0: case 0x1: return 0; default: return 3; }
}
static uint32_t stamp_now(void) { return s_rx_stamp_valid ? s_rx_stamp : g_sample_clock - s_blk_n; }

void clock_engine_poll(void) {
  uint8_t pkt[4];
  while (tud_midi_packet_read(pkt)) {
    if (s_fifo_prev_packets) s_fifo_prev_packets--;
    s_pk_count++;
    if (s_pkt_r != s_pkt_w) {
      uint32_t st = s_pkt_stamp[s_pkt_r & 63u];
      // BUNCHING: a host may deliver several ticks in ONE transfer; they all carry that transfer's IRQ stamp, so every tick but the last is
      // late by (ticks after it) * period. Packets sharing a stamp are such a run: back-date the i-th of L by (L-1-i) periods (tempo from the PLL).
#ifdef FOLLOWER_LEGACY
      s_run_left = 1; s_run_len = 1; s_run_stamp = st;
      if (0) {
#else
      if (!(s_run_left > 0 && st == s_run_stamp)) {
#endif
        uint32_t L = 1; for (uint8_t k = (uint8_t) (s_pkt_r + 1u); k != s_pkt_w && L < 8u; k++) { if (s_pkt_stamp[k & 63u] == st) L++; else break; }
        s_run_stamp = st; s_run_left = L; s_run_len = L;
      }
      double per_s = (60.0 * (double) AUDIO_SAMPLE_RATE) / ((double) MIDI_PPQN * (double) g_bpm);
      uint32_t i_in_run = s_run_len - s_run_left; s_run_left--;
      s_rx_stamp = st - (uint32_t) ((double) (s_run_len - 1u - i_in_run) * per_s);
      s_pkt_r++; s_rx_stamp_valid = true; int32_t late = (int32_t) (g_sample_clock - s_rx_stamp); if (late > s_late_max) s_late_max = late; if (late < s_late_min) s_late_min = late; } else { s_rx_stamp_valid = false; s_nostamp++; }
    uint8_t n = cin_len(pkt[0] & 0x0F);
    for (uint8_t i = 0; i < n; i++) midi_rx_byte(pkt[1 + i]);
  }
  s_rx_stamp_valid = false;
  if (tud_midi_available() == 0) { s_fifo_prev_packets = 0; s_pkt_r = s_pkt_w; }   // re-sync the per-packet stamp queue with an empty FIFO

  local_sounding_poll();       // update s_local_sounding before anything reads it this cycle
  boot_listen_poll();
  clock_role_election_poll();  // may change g_role before master_tick_poll/announce run this same cycle
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
  if (strncmp(line, "T", 1) == 0 && strlen(line) >= 4) {
    int on = (strstr(line, "0") == NULL);
    if (strncmp(line, "TRel", 4) == 0)  { pmor_probe_set("trel", on); cdc_reply("probe trel\r\n"); return; }
    if (strncmp(line, "TKeys", 5) == 0) { pmor_probe_set("keys", on); cdc_reply("probe keys\r\n"); return; }
    if (strncmp(line, "TOled", 5) == 0) { pmor_probe_set("oled", on); cdc_reply("probe oled\r\n"); return; }
    if (strncmp(line, "TIntro", 6) == 0) { pmor_probe_set("intro", on); cdc_reply("probe intro\r\n"); return; }
    if (strncmp(line, "TLoop", 5) == 0) { pmor_probe_set("loop", on); cdc_reply("probe loop\r\n"); return; }
    if (strncmp(line, "TAll", 4) == 0)  { pmor_probe_set("all",  on); cdc_reply("probe all\r\n");  return; }
  }
  // Surgical audio×OLED hooks (2026-08-16): run from this same main-loop
  // context. OB1 = blit one page, OBF = blit full frame, ASTOP/ASTART =
  // audio engine off/on. Reply FIRST so the answer is queued even if the
  // blit wedges USB — the death itself is the data.
  {
    extern void pmor_oled_test(int what);
    extern void pmor_audio_ctl(int on);
    extern void pmor_oled_contrast(int level);
    if (strncmp(line, "OB1", 3) == 0)    { cdc_reply("blit page\r\n");  pmor_oled_test(1); return; }
    if (strncmp(line, "OBF", 3) == 0)    { cdc_reply("blit full\r\n");  pmor_oled_test(2); return; }
    if (strncmp(line, "OBB", 3) == 0)    { cdc_reply("blit black\r\n"); pmor_oled_test(3); return; }
    if (strncmp(line, "OBW", 3) == 0)    { cdc_reply("blit white\r\n"); pmor_oled_test(4); return; }
    if (strncmp(line, "OBD", 3) == 0)    { cdc_reply("blit dark\r\n");  pmor_oled_test(5); return; }
    if (strncmp(line, "OBC", 3) == 0)    { cdc_reply("blit checker\r\n"); pmor_oled_test(6); return; }
    if (strncmp(line, "OBH", 3) == 0)    { cdc_reply("blit half\r\n");    pmor_oled_test(7); return; }
    if (strncmp(line, "OBS", 3) == 0)    { cdc_reply("blit slow\r\n");    pmor_oled_test(8); return; }
    {
      extern void pmor_pb5_test(int high);
      if (strncmp(line, "OPH", 3) == 0)  { cdc_reply("pb5 high 10ms\r\n"); pmor_pb5_test(1); return; }
      if (strncmp(line, "OPL", 3) == 0)  { cdc_reply("pb5 low 10ms\r\n");  pmor_pb5_test(0); return; }
      {
        extern void pmor_sine_set(int on);
        if (strncmp(line, "SINE1", 5) == 0) { cdc_reply("sine on\r\n");  pmor_sine_set(1); return; }
        if (strncmp(line, "SINE0", 5) == 0) { cdc_reply("sine off\r\n"); pmor_sine_set(0); return; }
      }
    }
    if (strncmp(line, "OC0", 3) == 0)    { cdc_reply("contrast 1\r\n");   pmor_oled_contrast(0x01); return; }
    if (strncmp(line, "OC1", 3) == 0)    { cdc_reply("contrast 16\r\n");  pmor_oled_contrast(0x10); return; }
    if (strncmp(line, "OC9", 3) == 0)    { cdc_reply("contrast 143\r\n"); pmor_oled_contrast(0x8F); return; }
    if (strncmp(line, "ASTOP", 5) == 0)  { cdc_reply("audio off\r\n");  pmor_audio_ctl(0); return; }
    if (strncmp(line, "ASTART", 6) == 0) { cdc_reply("audio on\r\n");   pmor_audio_ctl(1); return; }
  }
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
    char buf[200];
    snprintf(buf, sizeof(buf),
             "sy=%d wave=%d role=%s mode=%s bpm=%d bpm_x100=%ld wire=%d running=%d boot=%s beat=%s beatlvl=%d\r\n",
             (int) g_sync_mode,
             (int) g_click_wave,
             (g_role == CLOCK_ROLE_OPG_MASTER) ? "opg_master" : "demiurge_master",
             clock_engine_clock_mode_name(),
             (int) (g_bpm + 0.5f),
             (long) (g_bpm * 100.0f + 0.5f),
             (int) wire_tempo_fresh(),
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
    // TASK 2 diagnostics added here (tick_catchup_count/tick_resync_count):
    // ground truth for "did the catch-up burst ever need to run beyond the
    // first tick" and "did we ever give up and resync" -- see
    // clock_engine.h's comment on the getters for the full rationale.
    // Clock-ownership election fields (2026-10-01 bench fix) added here:
    // role/local_sounding/master_by_gesture are the exact inputs/output of
    // clock_role_election_poll()'s decision table, and remote_loop_held/
    // remote_loop_have_heard/remote_loop_stale are CC115's receive state --
    // see remote_loop_considered_held() for how the last two collapse to
    // "no loop" when either is true. Together this line is the whole
    // decision visible at once: role is explained by whichever of
    // local_sounding/remote_loop_held/master_by_gesture drove it.
    char buf[768];
    bool remote_loop_stale = g_remote_loop_have_heard &&
        ((g_sample_clock - g_remote_loop_last_heard_sample) > REMOTE_LOOP_STALE_SAMPLES);
    snprintf(buf, sizeof(buf),
             "sysnow_ms=%lu sample_clock=%lu "
             "audio_cb_count=%lu audio_frames_total=%lu "
             "tick_count=%lu tick_catchup_count=%lu tick_resync_count=%lu "
             "running=%d sounding_raw=%d midi_packets_written_realtime=%lu "
             "midi_packets_written_cc=%lu midi_tx_hw_events=%lu "
             "midi_tx_hw_bytes=%lu dwc2_midi_kicks=%lu "
             "dwc2_midi_kick_bytes=%lu dwc2_midi_fifo_pushes=%lu "
             "dwc2_midi_fifo_bytes=%lu "
             "role=%s local_sounding=%d master_by_gesture=%d "
             "remote_loop_held=%d remote_loop_have_heard=%d remote_loop_stale=%d "
             "follower_tick_count=%lu follower_phase_error_raw_samples=%ld "
             "follower_phase_error_samples=%ld follower_phase_error_frac16_permille=%ld "
             "follower_phase_slew_count=%lu follower_phase_snap_count=%lu\r\n",
             (unsigned long) board_get_now_ms(),
             (unsigned long) g_sample_clock,
             (unsigned long) g_audio_cb_count,
             (unsigned long) g_audio_frames_total,
             (unsigned long) g_tick_count,
             (unsigned long) g_tick_catchup_count,
             (unsigned long) g_tick_resync_count,
             (int) g_seq_running,
             (int) g_sounding_raw,
             (unsigned long) g_midi_packets_written_realtime,
             (unsigned long) g_midi_packets_written_cc,
             (unsigned long) g_midi_tx_hw_events,
             (unsigned long) g_midi_tx_hw_bytes,
             (unsigned long) g_dwc2_midi_in_kicks,
             (unsigned long) g_dwc2_midi_in_kick_bytes,
             (unsigned long) g_dwc2_midi_in_fifo_pushes,
             (unsigned long) g_dwc2_midi_in_fifo_bytes,
             (g_role == CLOCK_ROLE_OPG_MASTER) ? "opg_master" : "demiurge_master",
             (int) s_local_sounding,
             (int) s_master_by_gesture,
             (int) g_remote_loop_held,
             (int) g_remote_loop_have_heard,
             (int) remote_loop_stale,
             (unsigned long) clock_engine_get_follower_tick_count(),
             (long) s_follower_phase_error_raw_samples,
             (long) s_follower_phase_error_samples,
             (long) clock_engine_get_follower_phase_error_frac16_permille(),
             (unsigned long) s_follower_phase_slew_count,
             (unsigned long) s_follower_phase_snap_count);
    cdc_reply(buf);
  } else if (line[0] != '\0') {
    cdc_reply("? (try HELP, SY0-SY5, WAVE0, WAVE1, BEAT ON, BEAT OFF, BEATLVL n, STATUS, RATE, DIAG)\r\n");
  }
}
#endif  // CFG_TUD_CDC

sync_mode_t  clock_engine_get_sync_mode(void)  { return g_sync_mode; }
void         clock_engine_set_sync_mode(sync_mode_t m) { if ((int)m >= 0 && (int)m <= (int)SY5_PASS) g_sync_mode = m; }
click_wave_t clock_engine_get_click_wave(void) { return g_click_wave; }
clock_role_t clock_engine_get_role(void)       { return g_role; }
float        clock_engine_get_bpm(void)        { return g_bpm; }

// clock_mode: see the block comment above BUS_SILENCE_SAMPLES.
static bool bus_clock_fresh(void) {
  if (g_role != CLOCK_ROLE_DEMIURGE_MASTER || !s_have_last_rx_tick) return false;
  return (uint32_t) (g_sample_clock - s_last_rx_tick_sample) <= BUS_SILENCE_SAMPLES;
}
pmor_clock_mode_t clock_engine_clock_mode(void) {
  if (g_role == CLOCK_ROLE_DEMIURGE_MASTER) return bus_clock_fresh() ? PMOR_CLKMODE_SLAVE : PMOR_CLKMODE_INTERNAL;
  return tud_midi_mounted() ? PMOR_CLKMODE_MASTER : PMOR_CLKMODE_INTERNAL;
}
const char *clock_engine_clock_mode_name(void) {
  switch (clock_engine_clock_mode()) {
    case PMOR_CLKMODE_SLAVE:  return "slave";
    case PMOR_CLKMODE_MASTER: return "master";
    default:                  return "internal";
  }
}
// A tap on this device while SLAVED: the Pi analyses it (once) and broadcasts the tempo.
// ch16 CC112, value = age in ms since the press (ISR-stamped), so the Pi can reconstruct the
// press instant as (its receive time - age) no matter how long this loop stalled before sending.
void clock_engine_send_tap_event(uint8_t age_ms) {
  midi_send_cc16(112, age_ms > 127 ? 127 : age_ms);
}

// See clock_engine.h for the full contract. Single-writer discipline
// preserved (fix #2's rule, extended): while we're OPG_MASTER, the
// instrument's gridTick()/step_ms is the ONLY thing allowed to write
// g_bpm -- this call is main.cpp's sole entry point for that, called from
// loop/task context same as every other g_bpm writer in this file. A no-op
// while DEMIURGE_MASTER protects the follower PLL's exclusive ownership
// (same priority rule po_sync_in_record_interval() already enforces).
void clock_engine_set_local_bpm(float bpm) {
  if (g_role != CLOCK_ROLE_OPG_MASTER) return;
  if (bpm < 40.0f)  bpm = 40.0f;
  if (bpm > 400.0f) bpm = 400.0f;
  g_bpm = bpm;
}
bool         clock_engine_is_running(void)     { return g_seq_running; }
bool         clock_engine_role_resolved(void)  { return g_boot_phase == BOOT_RESOLVED; }

// TASK 1: see clock_engine.h for the full contract. Poll-context write
// (main.cpp calls this once per main-loop iteration, same cross-context
// convention as clock_engine_set_local_bpm() above) -- the debounce itself
// lives in master_tick_poll() so it only ever drives g_seq_running while
// OPG_MASTER; while following the bus, this value is recorded but ignored
// (handle_realtime()'s 0xFA/0xFB/0xFC own g_seq_running in that role, same
// priority rule the rest of this file already enforces for tempo).
void clock_engine_set_sounding(bool sounding) {
  g_sounding_raw = sounding;
}

// TASK 4: see clock_engine.h for the full contract.
void clock_engine_human_gesture(float bpm) {
  if (s_studio) return;   // a tap in STUDIO/EDIT must not claim the bus
  if (bpm < 40.0f)  bpm = 40.0f;
  if (bpm > 400.0f) bpm = 400.0f;
  // A tap always wins the role too, not just the tempo -- apply_role() is a
  // no-op if we're already OPG_MASTER (idempotent, see its own comment), so
  // this is safe to call unconditionally. announce=false: we send CC116
  // ourselves below, immediately, rather than relying on apply_role()'s own
  // announce path, so it lands in the SAME immediate burst as the BPM CCs
  // instead of being a separate write.
  apply_role(CLOCK_ROLE_OPG_MASTER, false);
  // RULE 4 (clock_role_election_poll()): sticky until an explicit hand-back
  // (apply_role() clears it on a genuine transition to DEMIURGE_MASTER) --
  // a tap must not get silently undone the next poll cycle just because
  // tapping tempo isn't itself "making sound" (see s_master_by_gesture).
  s_master_by_gesture = true;
  g_bpm = bpm;
  s_tick_armed = false;     // re-derive tick phase from "now" under the new tempo
  s_tick_frac_accum = 0;    // no fractional debt carried across a tempo jump this abrupt
  uint8_t v = (uint8_t) clampf(((g_bpm - 40.0f) / 360.0f) * 127.0f, 0.0f, 127.0f);
  midi_send_cc16(116, (uint8_t) CLOCK_ROLE_OPG_MASTER); // claim master immediately
  midi_send_cc16(118, v);                               // BPM steer
  midi_send_cc16(119, v);                                // BPM broadcast
  midi_send_tempo14(g_bpm);                              // canonical number (g_bpm is already wire-quantised by clockSetBpm())
  // Count this as this round's re-announce too, so the ~2s cadence doesn't
  // immediately re-send the same thing a few ms later.
  s_last_role_announce_sample = g_sample_clock;
  s_have_role_announce_anchor = true;
}

uint32_t clock_engine_get_tick_catchup_count(void) { return g_tick_catchup_count; }
uint32_t clock_engine_get_tick_resync_count(void)  { return g_tick_resync_count; }

uint32_t clock_engine_get_follower_tick_count(void)              { return s_follower_tick_count % MIDI_TICKS_PER_BEAT; }
int32_t  clock_engine_get_follower_phase_error_raw_samples(void) { return s_follower_phase_error_raw_samples; }
int32_t  clock_engine_get_follower_phase_error_samples(void)     { return s_follower_phase_error_samples; }
int32_t  clock_engine_get_follower_phase_error_frac16_permille(void) {
  uint32_t period = s_follower_phase_error_period ? s_follower_phase_error_period : 1u;
  // Integer math only (see the header comment for why: %f is a no-op in this
  // build's newlib-nano). (int64_t) product first so the *1000 can't overflow
  // a 32-bit int even at the largest step_period this grid ever runs at
  // (~720000 samples/16th at 1 BPM, per clock.h's own fp_mul_frac comment).
  int64_t scaled = (int64_t) s_follower_phase_error_samples * 1000;
  return (int32_t) (scaled / (int64_t) period);
}
uint32_t clock_engine_get_follower_phase_slew_count(void) { return s_follower_phase_slew_count; }
uint32_t clock_engine_get_follower_phase_snap_count(void) { return s_follower_phase_snap_count; }

uint32_t clock_engine_get_audio_cb_count(void)      { return g_audio_cb_count; }
uint32_t clock_engine_get_audio_frames_total(void)  { return g_audio_frames_total; }
uint32_t clock_engine_get_midi_tx_hw_events(void)   { return g_midi_tx_hw_events; }
uint32_t clock_engine_get_midi_tx_hw_bytes(void)    { return g_midi_tx_hw_bytes; }
uint32_t clock_engine_get_midi_packets_written_realtime(void) { return g_midi_packets_written_realtime; }
uint32_t clock_engine_get_midi_packets_written_cc(void)       { return g_midi_packets_written_cc; }

void clock_engine_note_audio_block(uint32_t n_frames) {
  g_audio_cb_count++;
  g_audio_frames_total += n_frames;
  // In builds that never route audio through clock_engine_process_audio()
  // (the PMOR instrument has its own synth path), THIS call is the sole
  // advancer of g_sample_clock. Without it the boot-listen window never
  // elapses (boot=listening forever), master_tick_poll() never resolves,
  // and the device emits zero 0xF8 ticks — the host app shows no BPM.
  // Guarded so builds that DO call process_audio() don't double-advance.
  if (!s_process_audio_seen) g_sample_clock += n_frames;
  s_blk_n = n_frames; s_blk_tick = pmor_tick();   // anchor for the hi-res MIDI RX stamps (hires_sample_now)
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

// ---- STUDIO / EDIT follower mode (docs/PROTOCOL.md section 6) ----------------------------------
void clock_engine_set_studio(bool on) {
  if (on == s_studio) return;
  s_studio = on;
  s_spp_valid = false;
  if (on) {
    apply_role(CLOCK_ROLE_DEMIURGE_MASTER, false);   // follow Live; no CC116 announce
    g_seq_running = false; s_last_seq_running_sent = false;   // stopped until Live sends Start/Continue
    transport_notify(false);
  } else {
    apply_role(CLOCK_ROLE_OPG_MASTER, false);        // back to the standalone default (no announce either)
  }
}
bool clock_engine_studio(void) { return s_studio; }
uint32_t clock_engine_start_count(void) { return s_start_count; }

void clock_engine_set_policy(uint8_t mode) {      // 0 AUTO, 1 INTERNAL, 2 FOLLOW, 3 MASTER (persisted user choice; AUTO unless the user picked one)
  s_policy = mode & 3;
  if (s_policy == 1 || s_policy == 3) { if (!s_studio && g_role != CLOCK_ROLE_OPG_MASTER) apply_role(CLOCK_ROLE_OPG_MASTER, true); }
}
uint8_t clock_engine_policy(void) { return s_policy; }
