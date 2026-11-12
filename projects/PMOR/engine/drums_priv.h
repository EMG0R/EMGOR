// drums_priv.h — private engine for drums.cpp / drum_sounds.cpp.
// 4 groups x 16 synthesized voices, described by data (SoundDef) and driven by
// ~10 shared synthesis "kinds" (see VKind) so 64 recipes stay implementable
// without 64 bespoke classes. One DrumVoice per pad (DRUM_VOICES == N_PADS).
#pragma once
#include <stdint.h>
#include <math.h>
#include "../config.h"
#include "drums.h"
#include "scale.h"
#include "engine_util.h"

// ==================================================================== consts
static constexpr float DRUM_FS_INV = 1.f / SR;
static constexpr float DRUM_TWOPI  = 6.28318530718f;
static constexpr float DRUM_PI     = 3.14159265359f;

// ==================================================================== primitives

// Cheap swing-type VCA / soft clip: g * s / (1 + |s|) (§0 "swing-type VCA";
// stands in for tanh() everywhere per research doc — cheap, no transcendental).
static inline float drumSwing(float s, float g) { return g * s / (1.f + fabsf(s)); }

// DC blocker (after any rectifier/waveshaper).
struct DcBlock {
    float x1 = 0.f, y1 = 0.f;
    inline float process(float x) {
        float y = x - x1 + 0.995f * y1;
        x1 = x; y1 = flushDenorm(y);
        return y1;
    }
};

// Andrew Simper "Cytomic" zero-delay-feedback state-variable filter: stable
// well past fs/6 (unlike the naive Chamberlin form), coefficients (g,k) only
// need recomputing when the corner/Q change — cheap to hold across a whole hit.
struct Svf {
    float ic1 = 0.f, ic2 = 0.f;
    float a1 = 1.f, a2 = 0.f, a3 = 0.f, k = 1.f;
    void setFc(float fc, float q) {
        if (fc < 20.f) fc = 20.f;
        if (fc > SR * 0.45f) fc = SR * 0.45f;
        if (q < 0.3f) q = 0.3f;
        float g = tanf(DRUM_PI * fc / SR);
        k  = 1.f / q;
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick(float in, float& lp, float& bp, float& hp) {
        float v3 = in - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenorm(2.f * v1 - ic1);
        ic2 = flushDenorm(2.f * v2 - ic2);
        lp = v2; bp = v1; hp = in - k * v1 - v2;
    }
};

// One shared 6-square "808 metal bank" (§4 HATS): integer phase accumulators,
// MSB extraction — a handful of adds/compares, ticked ONCE per sample and
// shared by every hat/cymbal voice this block (mandated CPU mitigation).
struct MetalBank {
    uint32_t phase[6] = {0,0,0,0,0,0};
    uint32_t inc[6]   = {0,0,0,0,0,0};
    void init() {
        static const float freqHz[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f };
        for (int i = 0; i < 6; i++)
            inc[i] = (uint32_t)((freqHz[i] * DRUM_FS_INV) * 4294967296.f);
        for (int i = 0; i < 6; i++) phase[i] = 0;
    }
    inline float tick() {
        float s = 0.f;
        for (int i = 0; i < 6; i++) {
            phase[i] += inc[i];
            s += (phase[i] & 0x80000000u) ? -1.f : 1.f;
        }
        return s * (1.f / 6.f);
    }
};

// ==================================================================== sound descriptors
enum VKind : uint8_t {
    VK_RESON = 0,  // 1-3 bridged-T resonators pinged by a short exciter, optional
                   // attack-fm "bump" + self-fm "sigh" + noise bed + drive/fold.
                   // Kicks (808/606/boom/rumble/reverse), toms/conga/clave/rimshot/
                   // woodblock/bongo, modal snare.
    VK_DUAL,       // two tonal layers (ping resonators OR rounded pitch-swept
                   // oscillators, chosen by useOsc) + a noise layer with an
                   // optional flat "hold". 909-family kicks/toms, most snares.
    VK_SWEEP,      // one or two sine/square pitch-sweep layers + optional noise
                   // click/sweep + optional bitcrush + optional resonant VCF +
                   // optional AM/vibrato. Sub/phone/zap/glitch-free kicks, chirp/
                   // zap/laser/bubble perc, zap snare.
    VK_CLAP,       // band-passed noise, N re-trigger bursts + a smooth tail.
    VK_NOISE,      // plain/BP/HP noise burst with attack/hold/decay.
    VK_METAL,      // taps the shared MetalBank through a BP1->BP2->HP chain
                   // (optionally 2-band for cymbals), optional shared-noise
                   // blend, optional bell "ping", optional delayed 2nd hit,
                   // optional bitcrush. All 808/909/606 hats + cymbal + ride.
    VK_RING,       // ring-modulated square x sine pairs -> BP -> VCA (+ optional
                   // bell ping). Ring-mod hat, metal-ping perc.
    VK_FM,         // 2-op FM (+ optional pitch env, + optional noise layer).
    VK_COWBELL,    // two squares -> double band-pass -> two-stage swing env.
    VK_GLITCH,     // N retriggered ping/noise-tick bursts, decaying per hit.
    VK_LEGACY,     // EMGOR's v2 engine, verbatim: sine (+partial) with one-pole pitch drop,
                   // exp env (per-sample dec), LCG noise (HP for hats), through the v2
                   // mix-stage tanh drive. All 32 KIT/KIT2 sounds.
};

enum PitchMode : uint8_t {
    PM_FIXED = 0,  // f0 (Hz) * pitchRatio(cfg.pitch) — snares/hats/most perc ("ignore key")
    PM_KEYROOT,    // scaleHz(0, 0) * pitchRatio — kicks always track the session root
    PM_TOMDEG,     // scaleHz(scaleTomDegree(rnd), tomOct) * pitchRatio — toms/chirp
};

// One row per sound (4 groups x 16). Plain data; unused fields for a given
// `kind` are simply ignored. Level-matching: `gain` is a static trim — to
// re-tune, render the voice with length=0.5, pitch=0, tone=0.5, accent=1 into
// a scratch buffer, measure the peak, and set gain = 0.891f / peak (§6.2).
struct SoundDef {
    VKind     kind;
    PitchMode pmode    = PM_FIXED;
    uint8_t   tomOct   = 1;

    float f0  = 200.f;      // nominal Hz (PM_FIXED) or unpitched-engine base
    float f1r = 0.f;        // layer-2 freq ratio to f0 (0 disables layer 2)
    float f2r = 0.f;        // layer-3 freq ratio (bongo only)
    float t60_0 = 0.15f, t60_1 = 0.10f, t60_2 = 0.08f;   // seconds, nominal (x length ratio)
    float g0 = 1.f, g1 = 0.f, g2 = 0.f;

    float clickMs = 0.f, clickBump = 1.7f;   // attack-fm bump: f*clickBump -> f over clickMs
    float selfFm  = 0.f;                     // 0..1, sigh depth tied to the running envelope
    bool  useOsc  = false;                   // VK_DUAL: false=ping resonator, true=rounded osc+pitch env
    float holdMs  = 0.f;                     // VK_DUAL / VK_NOISE flat hold before decay

    bool  sweepLog    = false;               // VK_SWEEP: log-linear (constant semitone/ms) vs exp-approach
    float sweepStartR = 1.f, sweepMs = 20.f; // start = f0*sweepStartR -> f0
    bool  vcf = false; float vcfStartHz = 3000.f, vcfEndHz = 300.f, vcfQ = 0.4f;
    bool  useSquare = false;                 // VK_SWEEP: square/saw core instead of sine (laser)
    float amHz = 0.f, amDepth = 0.f;         // VK_SWEEP: tremolo (laser "pew-pew")
    float vibHz = 0.f, vibDepth = 0.f;       // VK_SWEEP: vibrato (bubble)
    float lpTrackMult = 0.f;                 // VK_SWEEP: 0=off, else post LP tracks freq*mult

    bool  nBp = false;                       // noise filter: bandpass vs highpass
    float nF = 2000.f, nQ = 1.f, nSweepTo = 0.f;  // 0 = no sweep
    float nT60 = 0.05f, nHoldMs = 0.f, nAtkMs = 0.5f, nLvl = 0.f;

    uint8_t bN = 0; float bGapMs = 10.f, bDurMs = 4.f;          // VK_CLAP bursts
    float tailT60 = 0.1f, tailAtkMs = 5.f, tailLvl = 0.5f;      // VK_CLAP tail

    bool  crush = false; float crushBits = 8.f, crushHz = 20000.f;

    bool  twoBand = false;                                       // VK_METAL cymbal 2-band
    float bp1 = 3440.f, bp1Q = 2.5f, bp2 = 7100.f, bp2Q = 2.5f, hpF = 6000.f, hpQ = 0.7f;
    float noiseMix = 0.f;                                        // VK_METAL: blend in shared noise (909-style)
    float pingF0 = 0.f, pingF1 = 0.f, pingT60 = 0.3f, pingLvl = 0.f;  // VK_METAL/VK_RING bell ping
    float delay2Ms = 0.f, delay2Lvl = 0.f;                       // VK_METAL delayed 2nd hit (pedal chick)

    float fmRatio = 1.5f, fmIdx0 = 6.f, fmIdx1 = 0.f, fmMs = 30.f;

    uint8_t retrigN = 0; float retrigGapMs = 12.f, retrigMul = 0.7f; bool retrigNoise = false;

    float drive = 0.f; bool fold = false;    // output soft-clip / wavefold amount

    bool  forceReverse = false;              // (unused now — REVERSE is a per-pad setting, never a sound)
    // VK_LEGACY (v2 DrumDef {f, amp, dec, noiz, drop, pan, part})
    float lAmp = 1.f, lDec = 0.999f, lNoiz = 0.f, lDrop = 1.f, lPart = 0.f;
    float gain = 1.f;                        // static level-match trim (applied AFTER saturation)
    float satDrive = 0.f;                    // >0: v2-style tanh(x*satDrive) before gain — all kicks share this character
};

extern const SoundDef DRUM_DEFS[DG_COUNT][DRUM_SOUNDS];

// ==================================================================== voice
struct DrumVoice {
    enum VState : uint8_t { VS_IDLE = 0, VS_ACTIVE, VS_CHOKE };
    VState   state = VS_IDLE;
    uint8_t  group = 0, idxInGrp = 0, soundIdx = 0;
    const SoundDef* def = nullptr;

    uint32_t age = 0;
    float    lenRatio = 1.f;
    bool     reverse  = false;
    float    totalLen = 0.3f;
    float    accent   = 1.f;
    float    fBase    = 100.f;

    float    panL = 0.7071f, panR = 0.7071f;

    // choke (retrigger) state — drums.cpp owns the "what to trigger next" side
    // (pad-indexed pending arrays); the voice only knows how to fade itself out.
    uint32_t chokeLeft = 0, chokeTotal = 1;

    // resonators (up to 3)
    float ry1[3] = {0.f,0.f,0.f}, ry2[3] = {0.f,0.f,0.f};
    float rR[3]  = {0.f,0.f,0.f}, rCw[3] = {0.f,0.f,0.f};
    float rNorm[3] = {1.f,1.f,1.f};   // sin(w)/exciter-sum: makes a ping peak ~= accent, not 1/sin(w)
    uint32_t quietRun = 0;           // consecutive samples below the idle threshold

    // oscillator phases (radians, wrapped) + integer phases (square/saw cores)
    float    ph0 = 0.f, ph1 = 0.f, ph2 = 0.f;
    uint32_t iph0 = 0u, iph1 = 0u, iph2 = 0u;

    float    ampCache = 0.f;

    Svf svfA, svfB, svfC, svfD;
    DcBlock dc;

    float    crushHold = 0.f, crushPhase = 0.f, crushLevels = 256.f;

    // TONE colour: a shared, uniform "brightness" mapping (§6.1 comment: tone
    // is each voice's most useful single colour knob) applied as a generic
    // post-filter for every kind except VK_METAL/VK_COWBELL/VK_FM, which wire
    // `tone` directly into their own doc-specific parameter instead.
    float    tone = 0.5f, toneHz = 4000.f;
    bool     toneHp = false;

    uint32_t rnd = 1u;

    void  trigger(const SoundDef& d, const DrumPadCfg& cfg, float vel,
                  uint8_t grp, uint8_t idxGrp, uint8_t sIdx, uint32_t& seed);
    float process(float metal, float noise);
    void  startChoke(uint32_t ms);

  private:
    float env(float t60, float holdS = 0.f) const;
    float genExp(float start, float end, float Tms) const;
    float genLog(float start, float end, float Tms) const;
    void  setReson(int slot, float freq, float t60);
    float tickReson(int slot, float x);
    float pingExc(float amp) const;
    float shape(float x, const SoundDef* d) const;
    float noiseLayer(float noiseSample, const SoundDef* d);
    float render(const SoundDef* d, float metal, float noise);

    float procReson(const SoundDef* d, float noise);
    float procDual(const SoundDef* d, float noise);
    float procSweep(const SoundDef* d, float noise);
    float procClap(const SoundDef* d, float noise);
    float procNoise(const SoundDef* d, float noise);
    float procMetal(const SoundDef* d, float metal, float noise);
    float procRing(const SoundDef* d);
    float procFm(const SoundDef* d, float noise);
    float procCowbell(const SoundDef* d);
    float procGlitch(const SoundDef* d, float noise);
    float procLegacy(const SoundDef* d, float noise);
    float legF = 0.f, legFt = 0.f, legHatLp = 0.f;   // VK_LEGACY state
};
