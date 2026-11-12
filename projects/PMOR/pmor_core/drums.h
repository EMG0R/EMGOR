// drums.h — 16 pads × 16 synthesized sounds, quadrant-grouped. No samples.
// Pad layout (key index = row*4+col, row 0 = top):
//   TL quadrant (rows 0-1, cols 0-1) = PERC     TR (rows 0-1, cols 2-3) = HATS
//   BL quadrant (rows 2-3, cols 0-1) = KICKS    BR (rows 2-3, cols 2-3) = SNARES
#pragma once
#include <stdint.h>
#include "core_config.h"
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

// ---- engines. A DrumGroup IS an engine (KICK / SNARE / HAT / PERC). Each has DRUM_SOUNDS presets:
//      0..15  = the classic kit (frozen: golden-render exact), 16.. = stage-2 presets (DRUM_CLASSIC_SOUNDS marks the split).
//      Each engine also has DENG_PARAMS live depth knobs (drums.eng[g].x[k], schema ids PD_DRUMS 272+g*8+k), all 0 = neutral.
//      Per-voice depth = preset's own value + the knob, so classic presets are untouched at knob = 0.
#define DENG_PARAMS 4
struct DrumEngCfg { float x[DENG_PARAMS]; };      // runtime, not in the patch blob (frozen layout): see report
extern const char* const DRUM_ENGINE_PARAM_NAMES[DG_COUNT][DENG_PARAMS];   // KICK: CLICK SUB DRIVE CRUSH ...

struct DrumsState {
    DrumEngCfg eng[DG_COUNT];
    DrumPadCfg pad[N_PADS];
    float group_vol[DG_COUNT];       // 0..1.25
    float volume;                    // engine, 0..1.25
    // performance (not serialised): joystick direction FX handled by fx module, not here
};
extern DrumsState drums;

// Deep sound-recipe knobs (host editor): per ENGINE GROUP, multipliers on the fields of the selected sound's recipe (SoundDef) at trigger time. All 1.0 = neutral
// (the built-in recipe is used untouched: bit-identical); otherwise the voice gets a modified copy. Not in the patch blob: saved in the patch extension record.
// Schema PD_DRUMS 320 + group*16 + k, group DRUM.DEEP<g>. Live value storage is SDRAM (see drums.cpp).
#define DDEEP_PARAMS 8
extern float drum_deep[DG_COUNT][DDEEP_PARAMS];
extern const char* const DRUM_DEEP_NAMES[DDEEP_PARAMS];   // PITCH DECAY LAYER2 NOISEF NOISEQ NOISEDEC SWEEP FM
void  drumsDeepReset();                                    // all 1.0
const ParamPage* drumsDeepPage(uint8_t group);

extern const char* const DRUM_SOUND_NAMES[DG_COUNT][DRUM_SOUNDS];

// SD preset files (store/sdlayout): the recipe blob of a built-in sound, and installing a file's copy over it (boot only, once per row).
size_t      drumsDefSize();
const void* drumsDefBlob(uint8_t group, uint8_t sound);
bool        drumsDefInstall(uint8_t group, uint8_t sound, const void* blob, size_t n);
bool        drumsDefInstalled(uint8_t group, uint8_t sound);

// User drum presets (SD file entries 'u'): a base sound + the engine's 4 depth knobs. Browser reads these from RAM only.
#define DRUM_USER_SLOTS 8
struct DrumUserEntry { uint8_t base; float k[4]; char name[10]; };
void        drumsUserSet(uint8_t group, uint8_t n, uint8_t base, const float k[4], const char* name);   // CONTROL plane
bool        drumsUserGet(uint8_t group, uint8_t n, DrumUserEntry* out);
int         drumsUserNextFree(uint8_t group);                 // -1 if full

// ---- drum synth engine: every pad owns a SuperDef (drum_super.h): all synthesis parameters, loaded from the pad's preset when the sound changes.
float*       drumsSynthParamPtr(uint8_t pad, uint16_t k);   // live value k (index into SDEF_PARAMS), nullptr out of range
bool         drumsPadCustom(uint8_t pad);                    // params differ from the pad's preset (patch EXT stores only these pads)
void         drumsPadReload(uint8_t pad);                    // next use reloads the preset's params (re-select / old patch)
const float* drumsPadSynthGet(uint8_t pad);                  // the pad's SDEF_FLOATS floats
void         drumsPadSynthSet(uint8_t pad, const float* v);  // patch load: the pad's current preset, then these params on top (clamped; NaN/out of range keep the preset value)
void         drumsCycStats(uint32_t* max_cyc, uint32_t* cyc_per_voice_sample, uint32_t* blocks, uint32_t* voice_samples);   // DWT probe of the voice loop (blocks with a voice running); reading resets the window
extern float g_drumBenchF;

void drumsInit();
int  drumsActiveVoices();                           // voices not idle (the only ones that cost CPU)
const ParamPage* drumsEnginePage(uint8_t group);    // the engine's depth knobs (4 Params)
uint8_t drumGroupFirstPad(uint8_t group);            // a pad that belongs to the group (browser / audition)
// AUDIO plane
void drumsTrigger(uint8_t pad, float vel);          // safe from ISR; retrigger = fast 3-5 ms choke then restart
void drumsProcess(float* outL, float* outR, size_t n);
// CONTROL plane
void drumsPreview(uint8_t pad);                     // manual pad hit (calls drumsTrigger via a lock-free flag)
// editor
const ParamPage* drumsPadPage(uint8_t pad);         // length, pitch, tone (+ reverse as enum)
const ParamPage* drumsMixPage();                    // 4 group volumes
