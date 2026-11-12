// shared_state.h — the ONLY cross-plane globals. Each field says which plane writes it.
//   CONTROL plane = main loop (may block: I2C, OLED, SD).
//   AUDIO plane   = AudioCallback + everything it calls (never blocks, never I2C).
// Rule: control writes, audio reads (or the reverse) — never both write the same field.
#pragma once
#include <stdint.h>
#include "core_config.h"

struct SharedState {
    // ---- sensors (CONTROL writes, AUDIO reads)
    volatile float fsr;          // 0..1 scaled (FsrScale), median-filtered
    volatile float lidar;        // 0..1 (0 = far / none, 1 = touching), smoothed
    volatile float accel_x;      // g, -1..1 typical tilt
    volatile float accel_y;
    volatile float accel_z;
    volatile bool  trellis_ok;

    // ---- joystick (AUDIO plane polls ADC at 750 Hz and writes; CONTROL reads)
    volatile float joy_x;        // -1..1, deadzoned, centre-calibrated
    volatile float joy_y;
    volatile bool  joy_sw;       // raw level, debounced by nav
    volatile bool  joy_calibrated;
    volatile uint16_t joy_raw_x, joy_raw_y;   // for DASH

    // ---- pads (CONTROL writes on trellis events, AUDIO reads)
    volatile uint16_t pads_held;     // bitmask of physically held pads
    volatile uint16_t syn_chord;
    volatile uint8_t  pad_engine;    // nav: which engine the pads currently belong to (Engine), 0xFF = none (FX/PATCHES/...)
    volatile bool     on_fx_page;    // nav: the FX menu is showing (the accel tilt filter only moves then)
    // live-play de-doubling (EMGOR 2026-09-15): a hand tap and a sequenced hit of the same pad within
    // TAP_GUARD of each other must sound ONCE. tap_at = last manual hit, seq_hit_at = last sequenced
    // hit, in absolute samples, per engine x pad.
    volatile uint32_t tap_at[3][16];
    volatile uint32_t seq_hit_at[3][16];     // synth PERF chord memory: the pad combination of the last press-run (survives release)

    // ---- metering (AUDIO writes, CONTROL reads)
    volatile float out_amp;      // post-clipper peak follower 0..1, for screensaver/particles/meter
    volatile float cpu_load;     // 0..1, last block

    // ---- transport (AUDIO writes, CONTROL reads)
    volatile uint32_t gstep;     // global 16th index within the longest loop
    volatile uint32_t bar;       // current bar
    volatile float    bpm;       // effective (unquantized when tapped)

    // ---- clock link (2026-10-05; CONTROL writes in engine/transport_link.cpp, CONTROL reads)
    volatile float    bpm_shown;     // the tempo the UI shows: RECEIVED transport tempo when slaved, else the local clock
    volatile uint8_t  clock_mode;    // pmor_clock_mode_t: 0 internal, 1 slave, 2 master
    volatile uint32_t tap_sent_ms;   // System::GetNow() when a tap was last handed to the Pi (OLED flash)
    // sample instant of each pad's latest press, stamped the moment the keypad poll SAW it (not when
    // the UI event is processed) -- what loop tap-in quantises against (see seqRecordHit()).
    volatile uint32_t pad_press_sample[16];
};

extern SharedState g;
