// fx_tape.h -- HYPERTRUMPET V2 tape-style delay. Own work (Emory Smith / EMGOR).
// One mono delay line, Hermite read, the read position slewed by a one-pole (SMOOTH ms) PER SAMPLE, so every time change repitches like a tape head moving (swoop up when the
// time shortens, down when it lengthens). Feedback path: HP (LOWCUT) -> LP (HIGHCUT) -> soft clip (SAT), then back into the line; the output tap is taken before that path.
// `press` (0..1, set by the glue from the FSR, not a UI param): the effective time falls exponentially from TIME at press 0 to 2 ms at press >= 0.6 (flanger range, the feedback then
// makes it a comb / flange). Output is stereo-compatible mono (L = R).
#pragma once
#include "fx_common.h"
namespace hx {
struct FxTape : Fx {
    enum { P_MIX = 0, P_TIME, P_FB, P_LOWCUT, P_HIGHCUT, P_SAT, P_SMOOTH };
    DelayLine dl; float D = 0.f, hp = 0.f, lp = 0.f; bool dinit = false;
    float press = 0.f; DryWet dwL, dwR; float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK], wet[HX_MAXBLOCK];
    volatile bool clearReq = false;
    FxTape(Arena& a, float s) {
        name = "TAPE"; sr = s;
        P(P_MIX, "MIX", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_TIME, "TIME", 20.f, 1200.f, 375.f, 0.f, PF_LOG);          // ms, the BASE time (encoder 1 steps it, the FSR shortens from it)
        P(P_FB, "FEEDBACK", 0.f, 0.95f, 0.4f, 0.f, PF_PERCENT);
        P(P_LOWCUT, "LOWCUT", 20.f, 1000.f, 120.f, 0.f, PF_LOG);      // Hz, high-pass in the feedback loop
        P(P_HIGHCUT, "HIGHCUT", 1000.f, 20000.f, 4500.f, 0.f, PF_LOG); // Hz, low-pass in the feedback loop
        P(P_SAT, "SAT", 0.f, 1.f, 0.25f, 0.f, PF_PERCENT);            // soft-clip drive in the feedback loop
        P(P_SMOOTH, "SMOOTH", 5.f, 500.f, 90.f, 0.f, PF_LOG);         // ms, time-change slew (the tape-pitch swoop)
        setRole(R_TIME, P_TIME); setRole(R_FB, P_FB); setRole(R_TONE, P_HIGHCUT); setRole(R_AMT, P_SAT);
        if (!dl.init(a, (uint32_t)(1.05f * s) + 8)) ok = false;
    }
    void reset() override { clearReq = true; }
    void action(int id, float) override { if (id == A_CLEAR) clearReq = true; }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (clearReq) { clearReq = false; memset(dl.buf, 0, dl.size() * 4); hp = lp = 0.f; dinit = false; }
        if (!stereoIn) memcpy(R, L, n * 4);
        memcpy(dryL, L, n * 4); memcpy(dryR, R, n * 4);
        const float tms = sm[P_TIME], p = clampf(press / 0.6f, 0.f, 1.f);
        const float effMs = (tms > 2.f) ? tms * powf(2.f / tms, p) : tms;                 // exponential (perceptual) fall from TIME to 2 ms
        const float Dt = clampf(effMs * 0.001f * sr, 2.f, (float)(dl.size() - 8));
        if (!dinit) { D = Dt; dinit = true; }
        const float a = 1.f - expf(-1.f / (sm[P_SMOOTH] * 0.001f * sr));
        const float fb = sm[P_FB], drive = 1.f + 3.f * sm[P_SAT], inv = 1.f / drive;
        const float hpA = 1.f - expf(-kTwoPi * sm[P_LOWCUT] / sr), lpA = 1.f - expf(-kTwoPi * clampf(sm[P_HIGHCUT], 1000.f, 0.45f * sr) / sr);
        for (int i = 0; i < n; i++) {
            D += (Dt - D) * a;
            const float y = dl.read(D < 2.f ? 2.f : D);
            wet[i] = y;
            float f = y * fb; hp += (f - hp) * hpA; f -= hp; lp += (f - lp) * lpA;
            const float s = (sm[P_SAT] > 0.001f) ? softclip(lp * drive) * inv : lp;
            dl.write(flush(0.5f * (dryL[i] + dryR[i]) + s));
        }
        hp = flush(hp); lp = flush(lp);
        dwL.apply(L, dryL, wet, n, sm[P_MIX]); dwR.apply(R, dryR, wet, n, sm[P_MIX]);
        return true;
    }
};
inline Fx* fx_create_tape(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxTape)); return m ? new (m) FxTape(a, sr) : nullptr; }
}
