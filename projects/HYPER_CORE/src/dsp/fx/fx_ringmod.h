// fx_ringmod.h -- HYPERTRUMPET V2 registry id "ring": ring modulator. Replaces v1 abl.dsp.ringmod~ (V1_SCRAPE 4.13:
// FSR -> 0..800 Hz through a 200 ms glide; mix 0/1 while pad 5 is held). Own algorithm, no third-party code.
// Carrier: phase accumulator (float), SHAPE sine (sinPar, err < 1e-3) / triangle / square (polyBLEP at both edges).
// FREQ glides per sample (GLIDE ms, one pole) so FSR stepping is click-free. TRACK mode: carrier = ctx.f0 * RATIO
// (holds the last tracked value when unvoiced); RATIO 1 = harmonic, 1.414 / 2.76 etc = inharmonic metallic.
// DRIVE soft-saturates the input before multiplication, DEPTH blends AM<->ring (out = x*(1-d + d*c)), TONE = post 1-pole LP.
// A_HOLD (pad 5 momentary): once received, the wet path is gated: 1 => ramps in over 10 ms, 0 => ramps out over 10 ms.
// Before the first A_HOLD the gate is open (effect is "always on" = MIX).
#pragma once
#include "fx_common.h"
namespace hx {
struct FxRingmod : Fx {
    enum { P_MIX, P_FREQ, P_GLIDE, P_DRIVE, P_SHAPE, P_TRACK, P_RATIO, P_DEPTH, P_TONE };
    float ph = 0.f, fcur = 100.f, lastTrack = 220.f, gate = 1.f, gateT = 1.f, mixCur = 1.f, lpz[2] = {0.f, 0.f};
    bool gateMode = false, primed = false;
    FxRingmod(Arena&, float s) {
        name = "RINGMOD"; sr = s;
        static const char* const shp[] = {"SINE", "TRI", "SQR"};
        static const char* const trk[] = {"FREE", "TRACK"};
        P(P_MIX,   "MIX",   0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_FREQ,  "FREQ",  1.f, 2000.f, 200.f, 0.f, PF_LOG);
        P(P_GLIDE, "GLIDE", 1.f, 2000.f, 200.f, 0.f, PF_LOG);
        P(P_DRIVE, "DRIVE", 0.f, 24.f, 0.f);
        P(P_SHAPE, "SHAPE", 0.f, 2.f, 0.f, 1.f, PF_ENUM | PF_QUANT, shp);
        P(P_TRACK, "TRACK", 0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT, trk);
        P(P_RATIO, "RATIO", 0.25f, 8.f, 1.f, 0.f, PF_LOG);
        P(P_DEPTH, "DEPTH", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_TONE,  "TONE",  500.f, 20000.f, 20000.f, 0.f, PF_LOG);
        setRole(R_AMT, P_DEPTH); setRole(R_PITCH, P_FREQ); setRole(R_TONE, P_TONE); setRole(R_MOD, P_RATIO);
        fcur = 200.f;
    }
    static inline float blep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    void action(int id, float value) override { if (id == A_HOLD) { gateMode = true; gateT = value > 0.5f ? 1.f : 0.f; } }
    bool process(const FxCtx& c, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        float tgt = v[P_FREQ];
        if (v[P_TRACK] > 0.5f) { if (c.f0 > 20.f) lastTrack = c.f0; tgt = lastTrack * sm[P_RATIO]; }
        tgt = clampf(tgt, 1.f, 0.45f * sr);
        if (!primed) { fcur = tgt; primed = true; mixCur = sm[P_MIX]; }
        float ga = 1.f - expf(-1.f / (0.001f * v[P_GLIDE] * sr));
        int shape = (int)v[P_SHAPE];
        float depth = sm[P_DEPTH], drv = dbToLin(sm[P_DRIVE]); bool dOn = sm[P_DRIVE] > 0.05f;
        float ta = 1.f - expf(-kTwoPi * clampf(sm[P_TONE], 200.f, 0.45f * sr) / sr); bool tOn = sm[P_TONE] < 19000.f;
        float gStep = 1.f / (0.01f * sr); if (!gateMode) { gate = 1.f; gateT = 1.f; }
        float mT = sm[P_MIX], mInc = (mT - mixCur) / (float)n;
        float cr[HX_MAXBLOCK], mm[HX_MAXBLOCK];
        for (int i = 0; i < n; i++) {
            fcur += (tgt - fcur) * ga;
            float dt = fcur / sr; ph += dt; if (ph >= 1.f) ph -= 1.f;
            float y;
            if (shape == 0) y = sinPar(ph);
            else if (shape == 1) y = 4.f * fabsf(ph - 0.5f) - 1.f;
            else { y = ph < 0.5f ? 1.f : -1.f; float p2 = ph + 0.5f; if (p2 >= 1.f) p2 -= 1.f; y += blep(ph, dt) - blep(p2, dt); }
            cr[i] = 1.f - depth + depth * y;
            gate += (gateT > gate) ? fminf(gStep, gateT - gate) : -fminf(gStep, gate - gateT);
            mixCur += mInc; mm[i] = mixCur * gate;
        }
        mixCur = mT;
        int nch = stereoIn ? 2 : 1; float* io[2] = {L, R};
        for (int k = 0; k < nch; k++) {
            float* x = io[k]; float z = lpz[k];
            for (int i = 0; i < n; i++) {
                float in = x[i], u = dOn ? softclip(in * drv) : in;
                float w = u * cr[i];
                if (tOn) { z += (w - z) * ta; z = flush(z); w = z; }
                x[i] = in + (w - in) * mm[i];
            }
            lpz[k] = z;
        }
        return stereoIn;
    }
};
inline Fx* fx_create_ringmod(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxRingmod)); return m ? new (m) FxRingmod(a, sr) : nullptr; }
}
