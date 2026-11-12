// synth.h — polysynth. AUDIO plane renders; CONTROL plane edits SynthState (all floats, patch-serialised).
#pragma once
#include <stdint.h>
#include "../config.h"
#include "param.h"

// Algorithms the Daisy can carry alongside drums + FX (each = one preset with its own params).
enum SynAlgo : uint8_t {
    ALG_THIEF = 0,   // Mid-Air-Thief stack: saw/tri stacks + FM plucks, random per-note character (plain-saw branch REMOVED)
    ALG_FM,          // 2-op FM
    ALG_WAVE,        // wavetable morph (8 tables)
    ALG_PLUCK,       // Karplus-Strong
    ALG_SUPER,       // 5-saw supersaw
    ALG_CZ,          // phase distortion
    ALG_FOLD,        // sine → wavefolder
    ALG_ADD,         // 8-partial additive
    ALG_COUNT
};
extern const char* const SYN_ALGO_NAMES[ALG_COUNT];

struct Adsr { float a, d, s, r; };   // seconds, seconds, 0..1, seconds

struct SynPreset {                   // one per algorithm, all patch-serialised
    float p[4];                      // algorithm-specific (names from synAlgoParamPage)
    Adsr  amp;
    float cutoff;                    // Hz, keytracked
    float resonance;                 // 0..1
    Adsr  filt;  float filt_amt;     // envelope → cutoff, -1..1
    float glide_in, glide_out;       // pitch env: seconds gliding INTO the note / OUT of it on release
    float pitch_amt;                 // semitones the glide starts from / ends at; 0 = no pitch env (default)
    float width, width_prob;         // 0..1 generative pan width × probability
};

struct SynthState {
    uint8_t   algo;
    SynPreset preset[ALG_COUNT];
    float     volume;                // 0..1.25
    // live performance inputs (CONTROL writes; not serialised)
    float     perf_octave;           // -1, 0, +1 … from joystick Y; glides
    float     perf_env_macro;        // 0..1 plucky → ambient (accel X)
    float     perf_cutoff;           // 0..1 override when perf_active (accel X per spec; see note in synth.cpp)
    bool      perf_active;
    float     master_prob;           // from joystick X in performance
    volatile float wash_coef;        // FX WASH (v2 L1880-1886): >0 forces every voice release to this one-pole coef
};
extern SynthState syn;

void  synthInit();
// CONTROL plane
void  synthNoteOn(uint8_t pad, float vel);         // manual play (pad → scale note)
void  synthNoteOff(uint8_t pad);
void  synthAllOff();
// AUDIO plane
void  synthFireStep(uint16_t mask, uint32_t gstep); // from sequencer: fires notes with generative octave/pan rules
void  synthRetrigLast(uint16_t mask);               // stutter/BZZ: re-fire the EXACT last note (pitch + character) per pad
void  synthProcess(float* outL, float* outR, size_t n);   // adds into the buffers
// editor pages
const ParamPage* synAlgoParamPage(uint8_t algo);   // the 4 algorithm knobs
const ParamPage* synEnvPage();                     // ADSR (2×2 pairs)
const ParamPage* synFilterPage();                  // cutoff×res pair, filt env, amount
const ParamPage* synPitchPage();                   // glide in × out pair
const ParamPage* synWidthPage();                   // width × prob pair
