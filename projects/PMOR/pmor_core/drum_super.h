// drum_super.h -- the drum synth engine. One SuperDef (136 floats = every synthesis parameter) per pad drives one SuperVoice. A preset is a
// SoundDef (the frozen recipe table / SD preset files) mapped onto a SuperDef by superFromDef(); once loaded the pad owns the params and every one
// is editable. Layers are gated by a level (0 = skipped, no compute).
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "drums_priv.h"

static inline float sTanh(float z) { if (z > 3.f) return 1.f; if (z < -3.f) return -1.f; float z2 = z * z; return z * (27.f + z2) / (27.f + 9.f * z2); }
static inline float mixf(float a, float b, float m) { return (1.f - m) * a + m * b; }

// Svf with a coefficient memo: setFc() of identical (fc, q) is skipped (tanf + divide); results are bit-identical to recomputing.
struct SvfC : Svf { float lf = -1.f, lq = -1.f; inline void set(float fc, float q) { if (fc == lf && q == lq) return; lf = fc; lq = q; setFc(fc, q); } };

struct SuperDef {
    // ---- pitch: base freq = exp-weighted blend of three trackers (KEY root, random scale degree, fixed Hz)
    float f0 = 200.f, pKey = 0.f, pTom = 0.f, pFix = 1.f, tomOct = 1.f, tune = 1.f, rndMix = 0.f;
    // ---- time
    float atkMs = 0.4f, atkLin = 0.f, lenTrack = 0.f, lenNom = 0.3f;
    // ---- RES: 3 ping resonators (+click bump on #0, self-FM)
    float rG[3] = {0, 0, 0}, rT[3] = {0.15f, 0.10f, 0.08f}, rF1 = 0.f, rF2 = 0.f;
    float rClickMs = 0.f, rBump = 1.7f, rSelf = 0.f, rFollow = 0.f;
    // ---- GLITCH: retriggered ping / noise-tick bursts
    float gG = 0.f, gN = 1.f, gGapMs = 12.f, gMul = 0.7f, gNoise = 0.f, gT = 0.15f;
    // ---- OSC: A (sine/rounded/square, pitch sweep exp<->log, AM, vib, partial) + B (sine/rounded, follows)
    float oG0 = 0.f, oG1 = 0.f, oT0 = 0.15f, oT1 = 0.10f, oF1r = 0.f, oStartR = 1.f, oMs = 20.f, oLog = 0.f;
    float oRound = 0.f, oSquare = 0.f, oAmHz = 0.f, oAmDepth = 0.f, oVibHz = 0.f, oVibDepth = 0.f;
    float oBStart = 1.f, oBMs = 20.f, oBRound = 0.f, oVel = 0.f, oPartMix = 0.f, oPart = 0.f;
    float vcfMix = 0.f, vcfStart = 3000.f, vcfEnd = 300.f, vcfQ = 0.4f, lpMix = 0.f, lpMult = 0.f;
    // ---- NOISE (shared by every group): env, SVF BP<->HP, optional 1-pole HP in front, sweep, velocity, per-sample jitter
    float nLvl = 0.f, nF = 2000.f, nQ = 1.f, nBp = 0.f, nSweepTo = 0.f, nSweepMs = 60.f, nT60 = 0.05f, nHold = 0.f;
    float nAtk = 0.5f, nHp1 = 0.f, nVel = 0.f, nJit = 0.f;
    // ---- CLAP
    float cG = 0.f, cN = 1.f, cGap = 10.f, cDur = 4.f, cF = 2000.f, cQ = 1.f, cTailT = 0.1f, cTailAtk = 5.f, cTailLvl = 0.5f;
    // ---- METAL (808/909 bank through BP1,BP2 -> HP), ping, delayed 2nd hit
    float mG = 0.f, mTA = 0.15f, mTB = 0.f, mgA = 1.f, mgB = 0.f, mTwo = 0.f;
    float mBp1 = 3440.f, mBp1Q = 2.5f, mBp2 = 7100.f, mBp2Q = 2.5f, mHpF = 6000.f, mHpQ = 0.7f, mNoiseMix = 0.f, mTone = 1.f;
    float pingF0 = 0.f, pingF1 = 0.f, pingT = 0.3f, pingLvl = 0.f, d2Ms = 0.f, d2Lvl = 0.f;
    // ---- COWBELL
    float bG = 0.f, bF2r = 1.481f, bQ = 5.f, bTA = 0.015f, bTB = 0.6f, bGain = 1.5f;
    // ---- FM (+ generative per-hit re-roll, mix 0..1)
    float fG = 0.f, fRatio = 1.5f, fIdx0 = 6.f, fIdx1 = 0.f, fMs = 30.f, fT60 = 0.12f, fBump = 1.f, fClickMs = 0.f;
    float fGen = 0.f, fChaosP = 0.f, fChaosI = 0.f, fChaosD = 0.f, fChaosDrop = 0.f;
    // ---- output stage (every optional stage = a mix whose preset values are 0/1)
    float crushMix = 0.f, crushBits = 8.f, crushHz = 20000.f;
    float drive = 0.f, driveMix = 0.f, foldMix = 0.f, dcR = 0.995f;
    float toneMix = 0.f, toneHp = 0.f, satMix = 0.f, satIn = 1.f, gain = 1.f;
    // ---- depth stage (was xSnap/xBody/xDrive/xCrush) + its per-group constants
    float xSnap = 0.f, xBody = 0.f, xDrive = 0.f, xCrush = 0.f, xSnapMs = 3.f, xBodyFix = 0.f, xBodyRatio = 0.5f;
    // ---- pad
    float pan = 0.5f, panRnd = 0.f;
};
static_assert(sizeof(SuperDef) % sizeof(float) == 0, "SuperDef is a flat float block (patch EXT, schema)");
#define SDEF_FLOATS (sizeof(SuperDef) / sizeof(float))

// ---- exposure: every field is a schema parameter. layer = page; PF_LOG on time/frequency; discrete counts quantised.
enum SdLayer : uint8_t { SL_PITCH, SL_TIME, SL_RES, SL_GLITCH, SL_OSC, SL_NOISE, SL_CLAP, SL_METAL, SL_COWBELL, SL_FM, SL_OUT, SL_DEPTH, SL_PAN, SL_COUNT };
extern const char* const SDEF_LAYER_NAMES[SL_COUNT];
struct SdParamInfo { const char* name; uint16_t off; float mn, mx; uint8_t flags; uint8_t layer; };   // off = float index into SuperDef
extern const SdParamInfo SDEF_PARAMS[];
extern const size_t SDEF_NPARAMS;
// The four engines (KICK SNARE HAT PERC) are one core with four exposed parameter sets: each engine shows the fields its 24 presets actually use (plus the
// shared pitch / output / depth / pan stage), under engine-specific names. Ids stay the global field index k (stable, sparse per engine).
void        sdefInitEngines();                       // computes the used-field sets from the preset table (control plane, drumsInit)
bool        sdefUsed(int grp, size_t k);             // is field k part of engine grp's parameter set
const char* sdefName(int grp, size_t k);             // the engine's name for field k (SDEF_PARAMS name when not renamed)
extern const char* const SDEF_ENGINE_NAMES[4];

struct SuperVoice {
    enum VState : uint8_t { VS_IDLE = 0, VS_ACTIVE, VS_CHOKE };
    VState state = VS_IDLE;
    SuperDef dd; const SuperDef* def = nullptr;   // dd = hot copy taken at trigger: edits never glitch a sounding hit
    uint8_t group = 0;
    uint32_t age = 0;
    float lenRatio = 1.f, lenMul = 1.f, totalLen = 0.3f, accent = 1.f, fBase = 100.f, pRatio = 1.f;
    bool reverse = false;
    float panL = 0.7071f, panR = 0.7071f;
    uint32_t chokeLeft = 0, chokeTotal = 1, quietRun = 0;
    // resonators: 0..2 RES, 3 GLITCH
    float ry1[4] = {}, ry2[4] = {}, rR[4] = {}, rCw[4] = {}, rNorm[4] = {};
    float ph0 = 0.f, ph1 = 0.f, mph0 = 0.f, mph1 = 0.f, fph0 = 0.f, fph1 = 0.f;
    uint32_t iph0 = 0u, cph0 = 0u, cph1 = 0u;
    float ampCache = 0.f;
    // depth stage
    bool xOn = false; float xSnap = 0.f, xBody = 0.f, xDrive = 0.f, xCrush = 0.f;
    float xSnapEnv = 0.f, xSnapCoef = 0.f, xSnapLp = 0.f, xBodyHz = 0.f, xBodyPh = 0.f, xHold = 0.f; uint32_t xHoldN = 0u, xHoldLen = 1u;
    // FM effective (nominal blended with the per-hit roll)
    float eFc = 100.f, eRatio = 1.5f, eIdx0 = 6.f, eIdx1 = 0.f, eMs = 30.f, eT60 = 0.12f, eBump = 1.f, eClick = 0.f;
    SvfC svfN, svfCl, svfM1, svfM2, svfMH, svfCb1, svfCb2, svfV, svfL, svfT;
    float tNowS = 0.f, tEffS = 0.f, rLastF[4] = {-1.f, -1.f, -1.f, -1.f}, rLastT[4] = {-1.f, -1.f, -1.f, -1.f};   // per-sample time (hoisted out of env/genExp/genLog); setReson memo
    DcBlock dc;
    float crushHold = 0.f, crushPhase = 0.f, crushLevels = 256.f;
    float tone = 0.5f, toneHz = 4000.f;
    float hpLp = 0.f;
    uint32_t rnd = 1u;

    void trigger(const SuperDef& d, const DrumPadCfg& cfg, float vel, uint8_t grp, uint8_t idxGrp, uint32_t& seed);
    float process(float metal, float noise);
    void startChoke(uint32_t ms) { chokeTotal = (uint32_t)(ms * 0.001f * SR); if (chokeTotal < 8u) chokeTotal = 8u; chokeLeft = chokeTotal; state = VS_CHOKE; }

    // ---- verbatim helpers
    float env(float t60, float holdS = 0.f) const {
        t60 *= lenMul; holdS *= lenMul;
        if (t60 < 0.003f) t60 = 0.003f;
        float tau = t60 * (1.f / 6.9078f);
        const float tNow = tNowS, tEff = tEffS;
        float e = (tEff <= holdS) ? 1.f : fastExp(-(tEff - holdS) / tau);
        if (!reverse) {
            const float atk = def->atkMs * 0.001f;
            if (tNow < atk) e *= (def->atkLin > 0.f) ? (tNow / atk) : (0.5f - 0.5f * cosf(DRUM_PI * (tNow / atk)));
        } else {
            const float rel = 0.005f; float remain = totalLen - tNow;
            if (remain < rel) { float g = remain > 0.f ? remain / rel : 0.f; e *= 0.5f - 0.5f * cosf(DRUM_PI * g); }
        }
        return e;
    }
    float genExp(float start, float end, float Tms) const {
        if (Tms < 0.5f) Tms = 0.5f; Tms *= lenMul;
        float tau = (Tms * 0.001f) / 5.f; const float tEff = tEffS;
        return end + (start - end) * fastExp(-tEff / tau);
    }
    float genLog(float start, float end, float Tms) const {
        if (Tms < 0.5f) Tms = 0.5f; Tms *= lenMul;
        float T = Tms * 0.001f; float tEff = tEffS; if (tEff > T) tEff = T;
        float u = 1.f - tEff / T; if (end < 1.f) end = 1.f; if (start < 1.f) start = 1.f;
        return end * fastExp(u * logf(start / end));
    }
    void setReson(int s, float freq, float t60) {
        if (age != 0u && (age & 7u) != 0u) return;
        if (freq == rLastF[s] && t60 == rLastT[s]) return;   // same inputs, same coefficients
        rLastF[s] = freq; rLastT[s] = t60;
        if (freq < 20.f) freq = 20.f; if (freq > SR * 0.45f) freq = SR * 0.45f; if (t60 < 0.005f) t60 = 0.005f;
        float Q = (t60 * 4.f * freq) * 0.4545455f; if (Q < 0.5f) Q = 0.5f;
        float w = DRUM_TWOPI * freq / SR;
        rR[s] = fastExp(-DRUM_PI * freq / (Q * SR)); rCw[s] = cosf(w); rNorm[s] = sinf(w) * (1.f / 14.4f);
    }
    float tickReson(int s, float x) {
        float y = 2.f * rR[s] * rCw[s] * ry1[s] - rR[s] * rR[s] * ry2[s] + x;
        ry2[s] = flushDenorm(ry1[s]); ry1[s] = flushDenorm(y); return y * rNorm[s];
    }
    float pingExc(float amp) const { if (age > 32u) return 0.f; float t = (float)age * DRUM_FS_INV; return amp * fastExp(-t / 0.0003f); }

    float noiseLayer(float noise);
    float render(float metal, float noise);
    float applyExtras(float raw, float noise);
};

// SoundDef (preset recipe) -> SuperDef. idxInGrp = the pad's slot 0..3 in its group (hat pan).
void superFromDef(SuperDef& out, const SoundDef& d, int grp, int idxInGrp);
