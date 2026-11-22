// drums.h — 16 pads × 16 synthesized sounds, quadrant-grouped. No samples.
// Pad layout (key index = row*4+col, row 0 = top):
//   TL quadrant (rows 0-1, cols 0-1) = PERC     TR (rows 0-1, cols 2-3) = HATS
//   BL quadrant (rows 2-3, cols 0-1) = KICKS    BR (rows 2-3, cols 2-3) = SNARES
#pragma once
#include <stdint.h>
#include "../config.h"
#include "param.h"

enum DrumGroup : uint8_t { DG_KICK = 0, DG_SNARE, DG_HAT, DG_PERC, DG_COUNT };
extern const char* const DRUM_GROUP_NAMES[DG_COUNT];
DrumGroup drumPadGroup(uint8_t pad);
uint8_t   drumPadIndexInGroup(uint8_t pad);   // 0..3

struct DrumPadCfg {                  // patch-serialised, per pad
    uint8_t sound;                   // 0..15 within the pad's group bank (0 classic → 15 wonky)
    float   length;                  // 0..1, scales every envelope time together (0.5 = as designed)
    float   pitch;                   // -1..1 (semitones ±12 for pitched sounds)
    float   tone;                    // 0..1 colour
    bool    reverse;                 // time-reversed envelopes
    float   vol;                     // per-pad volume 0..2 (1 = as tuned); joystick U/D on the focused pad = +-10 %
};

struct DrumsState {
    DrumPadCfg pad[N_PADS];
    float group_vol[DG_COUNT];       // 0..1.25
    float volume;                    // engine, 0..1.25
    // performance (not serialised): joystick direction FX handled by fx module, not here
};
extern DrumsState drums;

extern const char* const DRUM_SOUND_NAMES[DG_COUNT][DRUM_SOUNDS];

void drumsInit();
// AUDIO plane
void drumsTrigger(uint8_t pad, float vel);          // safe from ISR; retrigger = fast 3-5 ms choke then restart
void drumsProcess(float* outL, float* outR, size_t n);
// CONTROL plane
void drumsPreview(uint8_t pad);                     // manual pad hit (calls drumsTrigger via a lock-free flag)
// editor
const ParamPage* drumsPadPage(uint8_t pad);         // length, pitch, tone (+ reverse as enum)
const ParamPage* drumsMixPage();                    // 4 group volumes
