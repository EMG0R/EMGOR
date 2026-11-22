// synth_private.h — private to synth.cpp / synth_algos.cpp. Voice model,
// wavetables, Karplus buffers, and the per-algorithm note-on/render entry
// points. Not part of the public interface (synth.h); nothing outside these
// two .cpp files should include this.
#pragma once
#include <stdint.h>
#include <math.h>
#include "../config.h"
#include "synth.h"
#include "engine_util.h"

// ---------------------------------------------------------------- wavetables
#define WT_LEN 256
#define WT_N   8
extern float SDRAM_BSS synWtbl[WT_N][WT_LEN];
void synBuildWavetables();      // fills synWtbl; called once from synthInit()

// ---------------------------------------------------------------- Karplus-Strong delay lines
// One per voice slot; sized for the lowest note we can trigger (F3-ish and
// below with the -1 perf octave), SDRAM per rule 6 (> 8KB).
#define KP_LEN 900
extern float SDRAM_BSS synKpBuf[SYN_VOICES][KP_LEN];

// ---------------------------------------------------------------- shared osc helpers
// Cheap parabolic sine, ph 0..1 -> approx sin(2*pi*ph), good enough for a synth voice.
static inline float synSin(float ph) {
    float x = ph + ph - 1.f;
    return 4.f * x * (1.f - fabsf(x));
}
static inline void synWrap01(float& ph) {
    if (ph >= 1.f) ph -= (float)(int)ph;
    else if (ph < 0.f) ph += 1.f - (float)(int)ph;
}
static inline float synPolyBlep(float t, float dt) {
    if (dt <= 0.f) return 0.f;
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// ---------------------------------------------------------------- voice
struct Voice {
    bool     stolen   = false;   // being force-released by the polyphony limiter
    uint32_t note_id  = 0;       // monotonically increasing: oldest-steal + pan slot order
    uint8_t  algo     = 0;
    uint8_t  pad      = 0xFF;    // manual-play owner pad, 0xFF = sequencer/no owner
    float    note_hz  = 220.f;
    float    pan      = 0.5f;

    ADSR ampEnv;
    ADSR filtEnv;

    // pitch envelope: pitch_mul multiplies note_hz; glides toward pitch_target
    // at pitch_rate (units/sample). 0.5 = an octave down, 1.0 = written pitch.
    float pitch_mul    = 1.f;
    float pitch_target = 1.f;
    float pitch_rate   = 0.f;
    bool  pitchGlideEnable = true;   // THIEF may disable per-note (re-rolled character)

    // per-voice Chamberlin SVF state + block-rate coefficients
    float svf_lp = 0.f, svf_bp = 0.f;
    float svf_f  = 0.2f, svf_q = 1.f;

    // sequencer gate: counts down then forces release. -1 = held (manual play / no auto release).
    int32_t gate_samples = -1;
    // sequenced/arp notes: the swell attack may not be slower than this one-pole coef (Thief) —
    // 0 = no cap (hand-played). Keeps long-note mode from starving the arp/generator of voices.
    float   atk_min_coef = 0.f;

    // shared oscillator scratch, meaning is per-algorithm (see synth_algos.cpp)
    float phase[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
    float ratio[5] = {1.f, 1.f, 1.f, 1.f, 1.f};
    int   osc_type = 0;
    float aux[6]   = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    int   iaux[3]  = {0, 0, 0};

    // ADD algorithm: 8 additive partials (phase, per-note detune, precomputed gain)
    float apart[8]  = {};
    float adrift[8] = {};
    float pgain[8]  = {};
};

extern Voice synVoices[SYN_VOICES];

// per-note algorithm setup (rolls Thief's character, sets ratios/aux state)
void  synAlgoNoteOn(Voice& v, const SynPreset& pr, uint32_t& rng);
// per-sample raw oscillator render (pre-filter); vidx needed for per-voice Karplus buffer
float synAlgoRender(int vidx, Voice& v, const SynPreset& pr, float phaseInc, float envNorm, uint32_t& rng);
