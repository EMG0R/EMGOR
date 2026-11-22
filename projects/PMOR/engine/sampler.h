// sampler.h — 16 slots × 2 s mono in SDRAM. Record starts ON PRESS, stops on release, with fades.
#pragma once
#include <stdint.h>
#include "../config.h"
#include "param.h"

struct SmpSlotCfg {                 // patch-serialised
    float pitch;                    // -1..1  → ±1 octave
    float formant;                  // -1..1
    float cutoff;                   // 0..1 (1 = open, default)
    bool  reverse;
};

struct SamplerState {
    SmpSlotCfg slot[SMP_SLOTS];
    uint32_t   len[SMP_SLOTS];      // samples recorded (0 = empty)
    float      volume;              // 0..1.25
    float      width, width_prob;   // as synth
    float      polyphony;           // voices, same 2/3 pan rule as synth
    // transient state (not serialised)
    int8_t     rec_slot;            // -1 = idle
    volatile uint32_t rec_pos;
    bool       audio_dirty;         // needs SD save
};
extern SamplerState smp;
extern float SDRAM_BSS smp_buf[SMP_SLOTS][SMP_LEN];

void samplerInit();
// CONTROL plane
bool samplerRecordStart(uint8_t slot);       // false if slot occupied (must delete first)
void samplerRecordStop();
void samplerDelete(uint8_t slot);
void samplerPlay(uint8_t slot, float vel);   // manual
// AUDIO plane
void samplerFireStep(uint16_t mask);
void samplerProcess(const float* inL, float* outL, float* outR, size_t n);
// TASK 3 (2026-10): "is any voice still sounding" for main.cpp's computeSounding()
// (MIDI transport gating) -- voice state (SmpVoice/`voices[]`) is otherwise private
// to sampler.cpp. Counts every voice NOT in V_FREE, V_FADEOUT (a ringing tail)
// included -- a long one-shot still fading out IS still sounding, which is exactly
// the gap this closes. Backed by a single `volatile uint8_t` updated only at the
// two points a voice actually enters/leaves V_FREE (both inside samplerProcess()'s
// audio-ISR context) -- same cross-context convention as clk.sample_now /
// usb/clock_engine.c's g_sample_clock: one aligned volatile read/write is atomic on
// Cortex-M7, so the control-loop read here needs no lock. Read from the control
// loop, not the ISR.
int samplerActiveVoices(void);
// editor
const ParamPage* samplerSlotPage(uint8_t slot);   // pitch × formant pair, cutoff, reverse
const ParamPage* samplerMixPage();
