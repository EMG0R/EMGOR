// fx_chorus.h -- HYPERTRUMPET V2 3-voice ensemble chorus. Own work (Emory Smith / EMGOR).
// v1 (V1_SCRAPE 4.7): Ableton chorus-ensemble, mod 0.3, rate 0.9 Hz, +3 dB gain, mix 100 % when on (pad 8 latch).
// Method: one mono delay ring (stereo input is summed to mono for the wet path; dry stays stereo), three Hermite taps at
// DELAY*(0.8, 1.0, 1.2) ms, each modulated by sinPar of one shared phasor offset by 0/120/240 degrees, depth = DEPTH*0.45*base.
// Voices are panned left/centre/right by WIDTH, sum -> small feedback (FB, <= 0.5, written back into the line) and wet HP + LP
// (one-pole each) -> GAIN dB -> MIX. Output is always stereo.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxChorus : Fx {
    enum { P_MIX = 0, P_RATE, P_DEPTH, P_DELAY, P_FB, P_WIDTH, P_GAIN, P_HP, P_LP };
    DelayLine dl; float ph = 0.f, hpL = 0, hpR = 0, lpL = 0, lpR = 0, fbS = 0;
    DryWet dwL, dwR; float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK], wetL[HX_MAXBLOCK], wetR[HX_MAXBLOCK];
    volatile bool clearReq = false;
    bool satFb = false;      // opt-in (MODBLOCK): soft-clip the feedback term inside the loop (cubic clamp, no division, knee at 2x so the cubic term is -12 dB lower: 3rd harmonic -87 dB at a -10 dBFS sine), so high FB sings instead of running away
    FxChorus(Arena& a, float s) {
        name = "CHORUS"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_RATE, "RATE", 0.1f, 15.f, 0.9f, 0.f, PF_LOG);
        P(P_DEPTH, "DEPTH", 0.f, 1.f, 0.3f, 0.f, PF_PERCENT);
        P(P_DELAY, "DELAY", 5.f, 30.f, 15.f, 0.f);          // ms (voices at 0.8x/1.0x/1.2x)
        P(P_FB, "FB", 0.f, 0.5f, 0.f, 0.f, PF_PERCENT);
        P(P_WIDTH, "WIDTH", 0.f, 1.f, 0.8f, 0.f, PF_PERCENT);
        P(P_GAIN, "GAIN", -12.f, 6.f, 3.f, 0.f);             // dB
        P(P_HP, "HP", 20.f, 2000.f, 80.f, 0.f, PF_LOG);
        P(P_LP, "LP", 1000.f, 20000.f, 12000.f, 0.f, PF_LOG);
        setRole(R_AMT, P_DEPTH); setRole(R_TIME, P_RATE); setRole(R_TONE, P_LP); setRole(R_MOD, P_WIDTH); setRole(R_FB, P_FB);
        if (!dl.init(a, (uint32_t)(0.05f * s) + 8)) ok = false;
    }
    void reset() override { clearReq = true; }
    void action(int id, float) override { if (id == A_CLEAR) clearReq = true; }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (clearReq) { clearReq = false; memset(dl.buf, 0, dl.size() * 4); hpL = hpR = lpL = lpR = fbS = 0.f; }
        if (!stereoIn) memcpy(R, L, n * 4);
        memcpy(dryL, L, n * 4); memcpy(dryR, R, n * 4);
        const float inc = sm[P_RATE] / sr, base = sm[P_DELAY] * 0.001f * sr, dep = sm[P_DEPTH] * 0.45f, fb = sm[P_FB], W = sm[P_WIDTH];
        const float bs[3] = { base * 0.8f, base, base * 1.2f };
        // voice pans: left / centre / right, equal-power; centre sits at +-0 so width only spreads the outer voices
        float gl[3], gr[3]; const float pans[3] = { -1.f, 0.f, 1.f };
        for (int v = 0; v < 3; v++) { float a = (pans[v] * W + 1.f) * 0.25f * kPi; gl[v] = cosf(a) * 0.8165f; gr[v] = sinf(a) * 0.8165f; }
        const float gain = dbToLin(sm[P_GAIN]);
        const float hpA = 1.f - expf(-kTwoPi * sm[P_HP] / sr), lpA = 1.f - expf(-kTwoPi * clampf(sm[P_LP], 1000.f, 0.45f * sr) / sr);
        const float maxD = (float)(dl.size() - 8);
        // budget: voice lengths, integer delays and Hermite weights are computed once per SB = 8 samples (the delay moves < 0.01 sample per step at 15 Hz / full depth)
        const int SB = 8; uint32_t id[3]; float w0[3], w1[3], w2[3], w3[3];
        for (int s0 = 0; s0 < n; s0 += SB) {
            const int sb = (n - s0 < SB) ? (n - s0) : SB;
            for (int v = 0; v < 3; v++) {
                float D = bs[v] * (1.f + dep * sinPar(ph + (float)v * (1.f / 3.f)));
                D = clampf(D, 2.f, maxD) - 1.f;
                uint32_t li = (uint32_t)D; float f = D - (float)li, f2 = f * f, f3 = f2 * f;
                id[v] = li; w0[v] = -0.5f * f + f2 - 0.5f * f3; w1[v] = 1.f - 2.5f * f2 + 1.5f * f3; w2[v] = 0.5f * f + 2.f * f2 - 1.5f * f3; w3[v] = -0.5f * f2 + 0.5f * f3;
            }
            ph += inc * (float)sb; ph -= (float)(int)ph;
            for (int i = s0; i < s0 + sb; i++) {
                float sL = 0.f, sR = 0.f, mono = 0.f;
                for (int v = 0; v < 3; v++) {
                    const uint32_t p = dl.w - 1 - id[v], m = dl.mask;
                    float y = w0[v] * dl.buf[(p + 1) & m] + w1[v] * dl.buf[p & m] + w2[v] * dl.buf[(p - 1) & m] + w3[v] * dl.buf[(p - 2) & m];
                    sL += y * gl[v]; sR += y * gr[v]; mono += y;
                }
                dl.write(flush(0.5f * (dryL[i] + dryR[i]) + (satFb ? fb * 2.f * cubicClip(mono * (1.f / 6.f)) : fb * mono * (1.f / 3.f))));
                hpL += (sL - hpL) * hpA; hpR += (sR - hpR) * hpA;
                float aL = sL - hpL, aR = sR - hpR;
                lpL += (aL - lpL) * lpA; lpR += (aR - lpR) * lpA;
                wetL[i] = lpL * gain; wetR[i] = lpR * gain;
            }
        }
        hpL = flush(hpL); hpR = flush(hpR); lpL = flush(lpL); lpR = flush(lpR);          // denormal guard once per block (the states decay slowly, a block is 1 ms)
        dwL.apply(L, dryL, wetL, n, sm[P_MIX]); dwR.apply(R, dryR, wetR, n, sm[P_MIX]);
        return true;
    }
};
inline Fx* fx_create_chorus(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxChorus)); return m ? new (m) FxChorus(a, sr) : nullptr; }
}
