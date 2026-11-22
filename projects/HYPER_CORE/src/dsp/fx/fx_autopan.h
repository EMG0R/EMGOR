// fx_autopan.h -- v1 `p autopan` (PERFRMNC.maxpat top level, traced 2026-11-17). In v1 its audio inlets and outlets are NOT connected (only the lidar dial reaches inlet 2),
// so it was never audible; this is the working version of exactly that subpatcher.
// Law, from the cords:  x = control 0..1 (lidar, inverted: far = more).
//   gain  G = scale(x, 0..1 -> 1..2) through line 110 ms, applied to BOTH outputs (*~ G after the pan gains)
//   lo    = 1 - x                          (scale out-low of the two random scalers)
//   tick  every metro = scale(1 - x, 0..1 -> 200..600) ms:  r = random;  gL target = lo + r (1 - lo),  gR target = lo + (1 - r)(1 - lo)
//   both pan gains glide to their targets with `line 500` (500 ms), left channel *~ gL, right channel *~ gR.
// v1 uses `random 1.` (an integer random, always 0): taken literally the pan would be a fixed lo / 1 split. The intended behaviour (r uniform 0..1) is implemented.
// Params: MIX, DEPTH (= x), PERIOD (ms, v1 200..600, normally derived from x by the firmware), GLIDE (ms, v1 500). Own work.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxAutopan : Fx {
    enum { P_MIX, P_DEPTH, P_PERIOD, P_GLIDE };
    float gl = 1.f, gr = 1.f, tl = 1.f, tr = 1.f, G = 1.f, cnt = 0.f, mixCur = 0.f; bool primed = false; uint32_t rng = 0xA17090u;
    FxAutopan(Arena&, float s) {
        name = "AUTOPAN"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_DEPTH, "DEPTH", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);        // v1: the lidar value x
        P(P_PERIOD, "PERIOD", 200.f, 600.f, 400.f, 0.f);             // ms between random pan targets (v1: 200 + 400 (1 - x))
        P(P_GLIDE, "GLIDE", 20.f, 2000.f, 500.f, 0.f, PF_LOG);       // ms (v1 line 500)
        setRole(R_AMT, P_DEPTH); setRole(R_TIME, P_PERIOD);
    }
    void reset() override { gl = gr = tl = tr = 1.f; cnt = 0.f; primed = false; }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!stereoIn) memcpy(R, L, (size_t)n * sizeof(float));
        const float x = clampf(sm[P_DEPTH], 0.f, 1.f), lo = 1.f - x, Gt = 1.f + x;
        const float period = sm[P_PERIOD] * 0.001f * sr, gstep = 1.f / (clampf(sm[P_GLIDE], 20.f, 2000.f) * 0.001f * sr);
        const float aG = 1.f - expf(-1.f / (0.11f * sr));
        const float mt = sm[P_MIX]; if (!primed) { mixCur = mt; primed = true; G = Gt; }
        const float minc = (mt - mixCur) / (float)n; float m = mixCur;
        for (int i = 0; i < n; i++) {
            if (cnt <= 0.f) { const float r = rnd01(rng); tl = lo + r * (1.f - lo); tr = lo + (1.f - r) * (1.f - lo); cnt += period; }
            cnt -= 1.f;
            gl += clampf(tl - gl, -gstep, gstep); gr += clampf(tr - gr, -gstep, gstep); G += (Gt - G) * aG;
            const float wl = L[i] * gl * G, wr = R[i] * gr * G;
            L[i] += (wl - L[i]) * m; R[i] += (wr - R[i]) * m; m += minc;
        }
        mixCur = mt; return true;
    }
};
inline Fx* fx_create_autopan(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxAutopan)); return m ? new (m) FxAutopan(a, sr) : nullptr; }
}
