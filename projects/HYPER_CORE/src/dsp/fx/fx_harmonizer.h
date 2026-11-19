// fx_harmonizer.h -- HYPERTRUMPET V2 harmonizer ("shiftr"). NEW 2026-11-15.
// v1: two FFT (gizmo~) pitch voices A/B + dry, interval ladder idx 0..13 (V1_SCRAPE 4.1). Here: two time-domain
// pitch-synchronous voices (fx_shifter_core.h) sharing ONE input delay line, mono in -> stereo out.
// Ladder (A,B semitones; '-' = voice off): 0:-12,- 1:-12,-7 2:-7,- 3:-12,-5 4:-5,- 5:neutral(dry) 6:-12,+5 7:+5,- 8:+12,+5
//   9:-12,+7 10:+7,- 11:+12,+7 12:+12,- 13: MANUAL (v1 had no handler; here A=STEP A, B=STEP B, pure voices, dry 0).
// Levels as v1: single voice 0.6 (dry 1.0), two-voice 0.5/0.5 with dry 0.5, neutral dry only. STEP A/B add fine
// offsets (st) to the ladder values. DETUNE spreads A up / B down by +-cents. GLIDE slews ratio between intervals.
// Params: MIX INTVL STEP A STEP B LEVEL A LEVEL B DETUNE GLIDE TONE WIDTH VOICES
// Roles: PITCH=INTVL AMT=VOICES TONE=TONE MOD=DETUNE TIME=GLIDE. Actions: MAIN = reset to unison (INTVL 5, steps 0),
//   ALT = next interval (wraps 13 -> 0), CLEAR = same as MAIN.
// Ops/sample: write 1; per active voice ~70 (shifter) + ~8 (tone lp, pan, level, ratio slew); dry/mix ~10 -> ~170 with 2 voices, ~90 with 1.
// Memory: 2048 floats = 8 KB (+ object).
#pragma once
#include "fx_common.h"
#include "fx_shifter_core.h"
namespace hx {

struct FxHarm : Fx {
    ShifterCore core; ShVoice va, vb;
    float rA = 1.f, rB = 1.f, gA = 0.f, gB = 0.f, gD = 1.f, mixCur = 1.f; bool primed = false;
    float mixPrev = 1.f;
    float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK];

    static constexpr int LADDER_N = 14;

    FxHarm(Arena& a, float s) {
        name = "HARMONIZ"; sr = s;
        P(0, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(1, "INTVL", 0.f, 13.f, 5.f, 1.f, PF_QUANT);
        P(2, "STEP A", -24.f, 24.f, 0.f, 0.01f, PF_BIPOLAR);
        P(3, "STEP B", -24.f, 24.f, 0.f, 0.01f, PF_BIPOLAR);
        P(4, "LEVEL A", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(5, "LEVEL B", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(6, "DETUNE", 0.f, 50.f, 0.f, 0.1f);
        P(7, "GLIDE", 0.f, 500.f, 40.f, 1.f);
        P(8, "TONE", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(9, "WIDTH", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(10, "VOICES", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        setRole(R_PITCH, 1); setRole(R_AMT, 10); setRole(R_TONE, 8); setRole(R_MOD, 6); setRole(R_TIME, 7);
        ok = core.init(a); va.reset(); vb.reset();
    }

    // ladder lookup -> semitones and base gains. aOn/bOn voice flags, dry gain.
    static void ladder(int idx, float& sa, float& sb, float& ga, float& gb, float& gd, bool manual) {
        static const int8_t A[14] = {-12,-12,-7,-12,-5, 0,-12, 5, 12,-12, 7, 12, 12, 0};
        static const int8_t B[14] = {  0, -7, 0, -5, 0, 0,  5, 0,  5,  7, 0,  7,  0, 0};
        static const uint8_t M[14] = {1,2,1,2,1,0,2,1,2,2,1,2,1,3};   // 0 neutral 1 single 2 two 3 manual
        if (idx < 0) idx = 0; if (idx > 13) idx = 13;
        sa = A[idx]; sb = B[idx]; (void)manual;
        switch (M[idx]) {
            case 0: ga = gb = 0.f; gd = 1.f; break;
            case 1: ga = 0.6f; gb = 0.f; gd = 1.f; break;
            case 2: ga = gb = 0.5f; gd = 0.5f; break;
            default: ga = gb = 1.f; gd = 0.f; break;
        }
    }

    bool process(const FxCtx& c, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) { if (!stereoIn) return false; return true; }
        if (!stereoIn) { for (int i = 0; i < n; i++) R[i] = L[i]; }
        for (int i = 0; i < n; i++) { dryL[i] = L[i]; dryR[i] = R[i]; }
        core.setPitch(c.f0, c.conf, c.sr > 0.f ? c.sr : sr, c.valvePitchHz);

        float sa, sb, ba, bb, bd;
        int idx = (int)v[1];
        ladder(idx, sa, sb, ba, bb, bd, idx == 13);
        const bool aOn = ba > 0.f, bOn = bb > 0.f;
        const float vol = sm[10];
        float tA = aOn ? ba * sm[4] * vol : 0.f, tB = bOn ? bb * sm[5] * vol : 0.f;
        float tD = bd;
        float det = sm[6];
        float stA = sa + sm[2] + 0.01f * det, stB = sb + sm[3] - 0.01f * det;
        if (!aOn) stA = 0.f; if (!bOn) stB = 0.f;
        stA = clampf(stA, -24.f, 24.f); stB = clampf(stB, -24.f, 24.f);
        const float rtA = semiToRatio(stA), rtB = semiToRatio(stB);
        const float gl = v[7] < 2.f ? 2.f : v[7];
        const float ra = 1.f - expf(-1.f / (gl * 0.001f * sr));
        const float rl = 1.f - expf(-1.f / (0.010f * sr));                // 10 ms level slew
        const float fc = clampf(700.f * powf(22.f, sm[8]), 200.f, 0.45f * sr);
        const float la = sm[8] > 0.995f ? 1.f : 1.f - expf(-kTwoPi * fc / sr);
        const bool toneOn = sm[8] <= 0.995f;
        const float w = sm[9];
        // equal-power pans: A left-ish, B right-ish
        const float pa = 0.5f - 0.5f * w, pb = 0.5f + 0.5f * w;
        const float aL = cosf(pa * 1.5707963f), aR = sinf(pa * 1.5707963f), bL = cosf(pb * 1.5707963f), bR = sinf(pb * 1.5707963f);

        const float mixT = sm[0];
        if (!primed) { mixCur = mixT; gA = tA; gB = tB; gD = tD; rA = rtA; rB = rtB; primed = true; }
        const float minc = (mixT - mixCur) / (float)n;
        float m = mixCur;

        for (int i = 0; i < n; i++) {
            float x = stereoIn ? 0.5f * (dryL[i] + dryR[i]) : dryL[i];
            core.write(x);
            gA = flush(gA + (tA - gA) * rl); gB = flush(gB + (tB - gB) * rl); gD = flush(gD + (tD - gD) * rl);
            rA += (rtA - rA) * ra; rB += (rtB - rB) * ra;
            float wl = gD * dryL[i], wr = gD * dryR[i];
            if (gA > 1e-5f || tA > 0.f) {
                float y = va.tick(core, rA);
                if (toneOn) { va.lp += (y - va.lp) * la; y = va.lp = flush(va.lp); }
                y *= gA; wl += aL * y; wr += aR * y;
            }
            if (gB > 1e-5f || tB > 0.f) {
                float y = vb.tick(core, rB);
                if (toneOn) { vb.lp += (y - vb.lp) * la; y = vb.lp = flush(vb.lp); }
                y *= gB; wl += bL * y; wr += bR * y;
            }
            L[i] = dryL[i] + (wl - dryL[i]) * m;
            R[i] = dryR[i] + (wr - dryR[i]) * m;
            m += minc;
        }
        mixCur = mixT;
        return true;
    }

    void action(int id, float) override {
        if (id == A_MAIN || id == A_CLEAR) { v[1] = 5.f; v[2] = 0.f; v[3] = 0.f; }
        else if (id == A_ALT) { int i = (int)v[1] + 1; if (i > 13) i = 0; v[1] = (float)i; }
    }
    void reset() override { va.reset(); vb.reset(); }
};
inline Fx* fx_create_harm(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxHarm)); return m ? new (m) FxHarm(a, sr) : nullptr; }
}
