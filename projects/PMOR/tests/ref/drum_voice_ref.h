// drum_voice_ref.h -- the original drum voice (reference for tests/test_drumsynth.cpp). See drum_voice_ref.cpp.
#pragma once
#include "drums_priv.h"
// ==================================================================== voice
struct RefVoice {
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

    // engine depth stage (all zero/false for the classic kit => the code path is skipped entirely)
    bool     xOn = false;
    float    xSnap = 0.f, xBody = 0.f, xDrive = 0.f, xCrush = 0.f;
    float    xSnapEnv = 0.f, xSnapCoef = 0.f, xSnapLp = 0.f;
    float    xBodyHz = 0.f, xBodyPh = 0.f;
    float    xHold = 0.f; uint32_t xHoldN = 0u, xHoldLen = 1u;
    // VK_GENFM per-hit rolls
    float    gfFc = 200.f, gfRatio = 1.f, gfIdx = 4.f, gfDec = 0.2f, gfDrop = 1.f;

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
    float procGenFm(const SoundDef* d, float noise);
    float applyExtras(float raw, float noise);
    float legF = 0.f, legFt = 0.f, legHatLp = 0.f;   // VK_LEGACY state
};
