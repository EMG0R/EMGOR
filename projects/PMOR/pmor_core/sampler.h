// sampler.h — 16 slots × 2 s mono in SDRAM. Record starts ON PRESS, stops on release, with fades.
#pragma once
#include <stdint.h>
#include "core_config.h"
#include "param.h"

struct SmpSlotCfg {                 // patch-serialised
    float pitch;                    // -1..1  → ±1 octave
    float formant;                  // -1..1
    float cutoff;                   // 0..1 (1 = open, default)
    bool  reverse;
};

// Per-slot playback extras (schema PD_SAMPLER 512+slot*8+k). NOT in the patch blob (its layout is frozen; no room): they live in the additive extension record, and reset to the defaults
// below on every patch load / boot unless the patch's extension record (patch_format.h PatchExt, PMOR3/Pnn.EXT) carries them. Defaults reproduce the pre-existing voice bit for bit.
struct SmpSlotExt {
    float start;        // 0..1 of the recording: where playback begins (forward) / the loop restarts (reverse)
    float end;          // 0..1: where it stops (forward) / begins (reverse)
    float loop_f;       // 0/1 enum shadow: loop the start..end region (ends on the next hit of the slot, or after 8 s)
    float attack_ms;    // 1..2000, log; default 2 ms = the old voice-start fade
    float release_ms;   // 1..4000, log; default 2 ms = the old natural-end fade
};
static const float SMP_ATK_DEF_MS = 2.f, SMP_REL_DEF_MS = 2.f;   // the defaults of ATTACK / RELEASE (= the old fixed fades)
extern SmpSlotExt smp_ext[SMP_SLOTS];
void samplerExtReset();

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
void samplerReseed(uint32_t seed);   // pan/width dice (0 = original fixed seed)
// CONTROL plane
bool samplerRecordStart(uint8_t slot);       // false if slot occupied (must delete first)
void samplerRecordStop();
void samplerDelete(uint8_t slot);
// ---- pool audition: a scratch buffer (not a slot) the UI fills from the SD in the background, then plays once.
//      Costs nothing while idle. CONTROL plane writes the buffer and calls Play; the audio plane only reads it.
int16_t* samplerAuditionBuf();                 // SMP_LEN samples, 16-bit mono
void     samplerAuditionPlay(uint32_t len);    // start playing the first `len` samples from the top (len clamped to SMP_LEN)
void     samplerAuditionStop();
bool     samplerAuditionPlaying();
void samplerBlankAll();                      // clear every slot (RAM + length only; no file is touched)
void samplerSwapSlots(uint8_t a, uint8_t b); // exchange two slots: audio, length and per-slot settings
void samplerPlay(uint8_t slot, float vel);   // manual
// AUDIO plane
void samplerFireStep(uint16_t mask);
// Stutter retrigger (fx.cpp's ST4..ST32 / BZZ timer): CHOPS only. The slot's playing voice keeps advancing muted (a shadow playhead); the chops are separate
// voices; when the stutter releases the chops fade out and the shadow fades back in exactly where the sample would be had it never stuttered.
void samplerRetrig(uint16_t mask);
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
const ParamPage* samplerSlotPage(uint8_t slot);
const ParamPage* samplerExtPage(uint8_t slot);   // START END LOOP ATTACK RELEASE   // pitch × formant pair, cutoff, reverse
const ParamPage* samplerMixPage();
