// fx.h — the FX menu: the PMOR_V2 punch-in banks, exactly. 8 banks (v2's FX1..FX8) x 16 pads;
// the joystick L/R flick steps through the banks. Every effect is FSR-modulated (v2 rule).
// Automation spans the LONGEST engine loop. DSP lives verbatim in fx_v2.cpp; host logic in fx.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "core_config.h"

#define FX_BANKS 8
#define FX_TOTAL (FX_BANKS * 16)       // effect id = bank * 16 + pad
static inline uint8_t fxId(uint8_t bank, uint8_t pad) { return (uint8_t)(bank * 16 + pad); }

struct FxState {
    // CONTROL writes
    volatile uint16_t held[FX_BANKS];       // physically pressed pads per bank (v2 fx_held..fx8_held)
    volatile uint8_t  view;                 // bank showing on the trellis / OLED (v2 fx_bank)
    // automation (patch-serialised) — v2 fx_auto[8][16] made loop-long
    uint16_t auto_mask[MAXSTEPS][FX_BANKS]; // recorded held banks per 16th
    uint8_t  auto_fsr[MAXSTEPS];            // 0..255
    bool     auto_rec;                      // recording gesture active (joystick button held on the FX page)
    bool     auto_enabled;
    // external (MIDI-in) layer, session-only, never serialised and never recorded: a Live clip driving FX over ch5/ch6 (docs/PROTOCOL.md "FX over MIDI")
    volatile uint16_t ext[FX_BANKS];        // engage bits set from outside
    volatile uint16_t ovr[FX_BANKS];        // FXs whose PMOR automation loop is paused (an outside edit took over) until the next physical hold of that FX
    volatile float    ext_fsr;              // amount macro set from outside (0..1), raises the effective FSR like the recorded auto_fsr does
    // per-bank macro params (schema PD_FX 130..159; session-live, not in patches): the DSP has ONE FSR macro and one lidar macro per bank
    float    bank_amt[FX_BANKS];            // floor under the FSR macro the bank's effects read (0 = follow the live FSR)
    float    bank_lid[FX_BANKS];            // floor under the lidar macro
    float    bank_mix[FX_BANKS];            // bank wet/dry (1 = untouched; bank 1's chain is spread over the master chain: not exposed)
    // stutter shadow-clock shared with the sequencer
    float    varispeed_req;                 // 1, 0.5, 2, 0.06 (STOP) — clock.cpp slews to it
    // performance-page directional throws for drums/sampler (separate from the FX page)
    volatile float perf_up, perf_right, perf_down, perf_left;   // 0..1 amounts from the joystick
    volatile uint8_t perf_engine;           // Engine currently in performance, or 0xFF
    // current distortion algorithm for the drum/sampler perf DOWN throw, patch-serialised (DistAlgo)
    uint8_t dist_algo;
    // FX-page accel tilt filter position 0..1 (v2 cut_t) — frozen off the page
    volatile float tilt_t;
};
extern FxState fx;

void fxInit();
void fxReseed(uint32_t seed);   // random streams of the whole FX chain (0 = original fixed seeds)
// AUDIO plane
void fxOnStep(uint32_t gstep, uint16_t* fire_masks /*[ENG_COUNT], may be modified for stutters*/);
void fxProcess(float* L, float* R, size_t n, float fsr, float lidar, float accel_x, bool on_fx_page);
void fxProcessPerf(float* L, float* R, size_t n);   // drum/sampler perf-page throws
typedef void (*FxRetrigCb)(const uint16_t* masks /*[ENG_COUNT]*/);
void fxSetRetrigCallback(FxRetrigCb cb);
// CONTROL plane
void fxPress(uint8_t id);
void fxRelease(uint8_t id);
void fxReleaseAll();
void fxSetView(uint8_t bank);
bool fxAnyHeld();
bool fxStutterActive();                   // an ST4/8/16/32 or BZZ is retriggering (audio plane read)
uint16_t fxEffectiveBank(uint8_t bank);      // effective engage mask of a bank right now (held | external | automation of the current step)
void     fxEffectiveAll(uint16_t out[8], float* fsr);   // all banks + the FSR macro in ONE pass (fxMidiPoll: was 8 + 1 separate passes)
float    fxEffectiveFsr();                    // effective FSR macro (live | automation | external)
void     fxExtSet(uint8_t id, bool on);       // outside engage edit: takes the FX over from the loop until the next physical hold
bool fxIsHeld(uint8_t id);
const char* fxName(uint8_t id);
// v2 live latch (L1792): a pad played by hand between steps feeds the stutter/BZZ registers,
// so a stutter always has material even with nothing sequenced. CONTROL or AUDIO plane.
void fxNoteLive(uint8_t engine, uint8_t pad);
// distortion algorithm set (drum perf DOWN); switching crossfades 5 ms
enum DistAlgo : uint8_t { DIST_TANH_ASYM = 0, DIST_FOLD, DIST_FUZZ, DIST_WARM, DIST_FL, DIST_CRUSH, DIST_COUNT };
extern const char* const DIST_NAMES[DIST_COUNT];
