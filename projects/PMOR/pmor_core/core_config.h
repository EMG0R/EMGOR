// core_config.h -- constants of the hardware-free core (pmor_core/). Pure #defines: no includes beyond
// <stdint.h>. Pins, calibration and UI feel stay in the root config.h (which includes this file).
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- audio
#define SR              48000.0f
#define BLOCK           64
#define BLOCK_HZ        (SR / BLOCK)          // 750 Hz control rate inside the ISR


// ---------------------------------------------------------------- behaviour knobs the core reads
#define PERF_ACCEL_FILTER 0       // 0 = NO filter on the synth PERF card (EMGOR 2026-09-15: removed for now); 1 = accel Y opens/closes it
#define TAP_GUARD_STEPS 0.45f     // a hand tap within this many 16ths of a sequenced hit of the same pad = one note, not two

// ---------------------------------------------------------------- sequencer
#define MAX_BARS        16
#define MAX_STEPS_BAR   16
#define MAXSTEPS        (MAX_BARS * MAX_STEPS_BAR)   // 256
#define N_PADS          16
#define N_LANES         16

// generative sequencer rate divisions (engine/clock.h GenDiv enum): 1/4, 1/8,
// 1/8T, 1/16, 1/16T, 1/32 — see clock.h for the enum and clock.cpp for the table.
#define GEN_DIV_COUNT   6

// ---------------------------------------------------------------- engines
#define SYN_VOICES      12         // 8 playable + 4 releasing (EMGOR 2026-09-15: 16 glitched the board)
#define SYN_MAX_POLY    8
#define SYN_OCTAVES     5
#define DRUM_CLASSIC_SOUNDS 16    // the frozen kit per engine (golden-exact)
#define DRUM_SOUNDS     24        // presets per engine (16 classic + 8 stage-2)
#define DRUM_VOICES     16
#define SMP_SLOTS       16
#define SMP_LEN         96000u    // 2 s mono per slot
#define ENGINE_VOL_MAX  1.25f

// ---------------------------------------------------------------- FX space
// FX = the v2 banks: 8 x 16 (see engine/fx.h)
#define FX_FADE_MS      6         // anti-click on every engage/release
#define DIST_XFADE_MS   5

// ---------------------------------------------------------------- output
#define MASTER_UNITY    0.2848f   // old pad 9 (75 %) — the new 100 %
#define CLIP_THRESH_DB  -4.4f     // Fruity-style knee default

// ---------------------------------------------------------------- patches
#define PATCH_PAGES     4
#define PATCH_SLOTS     (PATCH_PAGES * 16)   // 64
#define PATCH_MAGIC_V7  0x4F504737u          // 'OPG7' (7g: per-pad drum VOL added) -- legacy, load-only
#define PATCH_MAGIC     0x4F504738u          // 'OPG8' (v8: + drum engine depth knobs)

// ---------------------------------------------------------------- SDRAM placement
// Hardware: .sdram_bss section (SDRAM is NOT zeroed at reset, each module's init() clears its buffers).
// Host build (-DPMOR_HOST): plain zeroed BSS.
#ifdef PMOR_HOST
#define SDRAM_BSS
#else
#define SDRAM_BSS __attribute__((section(".sdram_bss")))
#endif
