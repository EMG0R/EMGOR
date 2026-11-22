// fx_mood.h -- HYPERTRUMPET V2 "MOOD" random pitch-shift gate (v1 p MOOD, supervp.trans~). NEW 2026-11-15.
// One shifter voice (fx_shifter_core.h, same core as the harmonizer) at a ladder semitone [-24,-17,-12,-9,0,+7,+12,+19,+24].
// Every step (RATE ms, v1: 16th note = 125 ms @120 bpm): roll r in 0..1; r > MOOD -> wet target 0, else 1; the wet gain ramps
// linearly over RAMP ms (v1 100 ms line~). AUTO (0..1) = probability that a step also picks a random ladder index (v1 AUTO
// toggle: index = random 9); 0 = joystick/PITCH index. Deterministic LCG seeded by SEED (reseeds when SEED changes).
// Params: MIX PITCH MOOD RATE AUTO RAMP SEED. Roles: PITCH=PITCH AMT=MOOD TIME=RATE MOD=AUTO.
// Actions: MAIN = force a wet step now, ALT = re-roll immediately, CLEAR = wet off.
// Ops/sample: ~75 when wet is active (voice) + ~12 (step clock, ramp, mix); ~12 when gated off (voice skipped). Memory 8 KB.
#pragma once
#include "fx_common.h"
#include "fx_shifter_core.h"
namespace hx {

struct FxMood : Fx {
    ShifterCore core; ShVoice voice;
    float wet = 0.f, wetT = 0.f, ratio = 1.f, mixCur = 1.f; bool primed = false;
    float stepCnt = 0.f; uint32_t rng = 1u; int lastSeed = -1;
    int curIdx = 6;
    // statistics (public, for tests / UI)
    uint32_t steps = 0, onSteps = 0, idxCount[9] = {0};
    float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK], mono[HX_MAXBLOCK];
    static constexpr int LADDER[9] = {-24, -17, -12, -9, 0, 7, 12, 19, 24};

    FxMood(Arena& a, float s) {
        name = "MOOD"; sr = s;
        P(0, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(1, "PITCH", 0.f, 8.f, 6.f, 1.f, PF_QUANT);
        P(2, "MOOD", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(3, "RATE", 30.f, 500.f, 125.f, 0.f, PF_LOG);
        P(4, "AUTO", 0.f, 1.f, 0.f, 0.f, PF_PERCENT);
        P(5, "RAMP", 10.f, 300.f, 100.f, 1.f);
        P(6, "SEED", 0.f, 999.f, 1.f, 1.f, PF_INT | PF_QUANT);
        P(7, "BITS", 4.f, 16.f, 16.f, 1.f, PF_INT | PF_QUANT);       // abl.device.redux~ bit depth on the shifted voice. v1: the redux outputs go to unconnected *~ stages (PERFRMNC p MOOD), so v1 is inaudible = 16 (off) is the faithful default
        setRole(R_PITCH, 1); setRole(R_AMT, 2); setRole(R_TIME, 3); setRole(R_MOD, 4);
        ok = core.init(a); voice.reset(); reseed(1);
    }
    void reseed(int s) { rng = 0x9E3779B9u * (uint32_t)(s + 1) + 12345u; for (int i = 0; i < 4; i++) rnd01(rng); lastSeed = s; stepCnt = 0.f; }

    void rollStep() {
        float r = rnd01(rng);
        wetT = (r > v[2]) ? 0.f : 1.f;
        if (v[4] > 0.f && rnd01(rng) < v[4]) { int k = (int)(rnd01(rng) * 9.f); if (k > 8) k = 8; curIdx = k; }
        else curIdx = (int)v[1];
        steps++; if (wetT > 0.5f) { onSteps++; idxCount[curIdx]++; }
    }

    bool process(const FxCtx& c, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if ((int)v[6] != lastSeed) reseed((int)v[6]);
        for (int i = 0; i < n; i++) { dryL[i] = L[i]; dryR[i] = stereoIn ? R[i] : L[i]; mono[i] = stereoIn ? 0.5f * (L[i] + R[i]) : L[i]; }
        core.setPitch(c.f0, c.conf, c.sr > 0.f ? c.sr : sr, c.valvePitchHz);
        const float stepLen = sm[3] * 0.001f * sr;
        const float rampInc = 1.f / (sm[5] * 0.001f * sr);
        const float rl = 1.f - expf(-1.f / (0.015f * sr));
        const float mixT = sm[0];
        if (!primed) { mixCur = mixT; primed = true; curIdx = (int)v[1]; ratio = semiToRatio((float)LADDER[curIdx]); }
        const float qs = v[7] < 15.5f ? exp2f(v[7] - 1.f) : 0.f;      // redux bit depth (0 = off)
        const float minc = (mixT - mixCur) / (float)n; float m = mixCur;
        for (int i = 0; i < n; i++) {
            if (stepCnt <= 0.f) { rollStep(); stepCnt += stepLen; }
            stepCnt -= 1.f;
            core.write(mono[i]);
            if (wet < wetT) { wet += rampInc; if (wet > wetT) wet = wetT; } else if (wet > wetT) { wet -= rampInc; if (wet < wetT) wet = wetT; }
            float rt = semiToRatioTab((float)LADDER[curIdx]);
            ratio += (rt - ratio) * rl;
            float out = dryL[i], outR = dryR[i];
            if (wet > 0.f || wetT > 0.f) {
                float y = voice.tick(core, ratio) * wet; if (qs > 0.f) y = floorf(y * qs + 0.5f) / qs;
                // wet replaces the dry in proportion to the gate (v1: line~ 0/1 crossfade between shifted and dry)
                float g = wet; float wl = dryL[i] * (1.f - g) + y, wr = dryR[i] * (1.f - g) + y;
                out = dryL[i] + (wl - dryL[i]) * m; outR = dryR[i] + (wr - dryR[i]) * m;
            }
            L[i] = out; R[i] = outR; m += minc;
        }
        mixCur = mixT;
        return stereoIn;   // mono in -> mono out (wet is mono); R written equal to L when mono-in for hosts that read it
    }
    // ratio cache: 9 ladder entries, computed once per call chain
    float semiToRatioTab(float s) { static const float T[9] = {0.25f, 0.37162f, 0.5f, 0.59460f, 1.f, 1.49831f, 2.f, 2.97302f, 4.f}; (void)s; return T[curIdx]; }

    void action(int id, float) override {
        if (id == A_MAIN) { wetT = 1.f; } else if (id == A_ALT) { stepCnt = 0.f; } else if (id == A_CLEAR) { wetT = 0.f; }
    }
    void reset() override { voice.reset(); wet = wetT = 0.f; }
};
constexpr int FxMood::LADDER[9];
inline Fx* fx_create_mood(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxMood)); return m ? new (m) FxMood(a, sr) : nullptr; }
}
