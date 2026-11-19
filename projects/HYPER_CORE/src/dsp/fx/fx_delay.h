// fx_delay.h -- HYPERTRUMPET V2 stereo ping-pong delay with stochastic mode.
// v1 behaviour (V1_SCRAPE 4.11): Ableton delay + tanh~ on the wet *0.62; manual time = 0.6*sqrt(FSR) s; feedback up to 0.95;
// stochastic mode: every tick, if random <= CHANCE the wet opens and a new random time 0.6*sqrt(rnd) s is chosen, else the wet closes.
// v2 implementation: two power-of-two rings (up to ~2.5 s per side) in the slot Arena, Hermite read, one-pole smoothed
// time (changes glide = tape-style repitch), feedback = cross-blend -> DC block -> one-pole LP -> softclip inside the loop.
// Own work (Emory Smith / EMGOR). Actions: A_MAIN = stochastic hold (momentary, v>0.5 on), A_ALT = toggle SMOOTH glide
// on/off, A_CLEAR = clear buffers.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxDelay : Fx {
    enum { P_MIX = 0, P_TIME, P_FB, P_TONE, P_SPREAD, P_CHANCE, P_RATE, P_STOCH, P_SMOOTH };
    DelayLine dl, dr;
    DcBlock dcL, dcR; float lpL = 0, lpR = 0;
    float curD = 0.f, wD = -1.f, hw0 = 0.f, hw1 = 1.f, hw2 = 0.f, hw3 = 0.f; uint32_t wId = 2; bool primed = false;   // wD/wId/hw*: cached Hermite weights for the current delay time
    float gate = 1.f, tgtD = 0.f; uint32_t tick = 0, rng = 0x1234567u;
    float stochGate = 1.f, stochTime = 0.35f;     // current stochastic state
    volatile bool held = false, smoothOn = true, clearReq = false;
    DryWet dwL, dwR; float dryL[HX_MAXBLOCK], dryR[HX_MAXBLOCK], wetL[HX_MAXBLOCK], wetR[HX_MAXBLOCK];
    float maxD = 1000.f;

    FxDelay(Arena& a, float s) {
        name = "DELAY"; sr = s;
        static const char* const onoff[] = { "OFF", "ON" };
        P(P_MIX, "MIX", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);
        P(P_TIME, "TIME", 1.f, 2500.f, 350.f, 0.f, PF_LOG);        // ms
        P(P_FB, "FB", 0.f, 0.95f, 0.4f, 0.f, PF_PERCENT);
        P(P_TONE, "TONE", 500.f, 20000.f, 6000.f, 0.f, PF_LOG);     // damping LP Hz
        P(P_SPREAD, "SPREAD", 0.f, 1.f, 0.5f, 0.f, PF_PERCENT);    // 0 = dual mono, 1 = full ping-pong
        P(P_CHANCE, "CHANCE", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_RATE, "RATE", 30.f, 1000.f, 125.f, 0.f, PF_LOG);        // ms per stochastic tick (16th)
        P(P_STOCH, "STOCH", 0.f, 1.f, 0.f, 1.f, PF_ENUM | PF_QUANT, onoff);
        P(P_SMOOTH, "SMOOTH", 3.f, 1000.f, 80.f, 0.f, PF_LOG);      // glide ms
        setRole(R_TIME, P_TIME); setRole(R_FB, P_FB); setRole(R_AMT, P_MIX); setRole(R_TONE, P_TONE); setRole(R_MOD, P_SPREAD);
        uint32_t n = (uint32_t)(2.5f * s) + 8;
        if (!dl.init(a, n) || !dr.init(a, n)) ok = false;
        maxD = ok ? (float)(dl.size() - 8) : 1000.f;
        if (maxD > 2.5f * s) maxD = 2.5f * s;
    }
    void reset() override { memset(dl.buf, 0, dl.size() * 4); memset(dr.buf, 0, dr.size() * 4); lpL = lpR = 0; dcL = DcBlock(); dcR = DcBlock(); }   // immediate (the firmware calls it from the main loop while the effect is idle, never from the audio thread)
    void action(int id, float value) override {
        if (id == A_MAIN) held = value > 0.5f;
        else if (id == A_ALT) smoothOn = !smoothOn;
        else if (id == A_CLEAR) clearReq = true;
    }
    static inline float rd(const DelayLine& d, float D) { return d.read(D - 1.f); }   // read-before-write: delay D samples, D >= 2

    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (clearReq) { clearReq = false; memset(dl.buf, 0, dl.size() * 4); memset(dr.buf, 0, dr.size() * 4); lpL = lpR = 0; dcL = DcBlock(); dcR = DcBlock(); }
        if (!stereoIn) memcpy(R, L, n * sizeof(float));
        memcpy(dryL, L, n * 4); memcpy(dryR, R, n * 4);
        const bool stoch = sm[P_STOCH] > 0.5f || held;
        const float fb = clampf(sm[P_FB], 0.f, 0.95f), sp = sm[P_SPREAD];
        const float fc = clampf(sm[P_TONE], 200.f, 0.45f * sr);
        const float lpA = 1.f - expf(-kTwoPi * fc / sr);
        const float glideMs = smoothOn ? sm[P_SMOOTH] : 3.f;
        const float gA = 1.f - expf(-1.f / (0.001f * glideMs * sr));
        const float manualD = sm[P_TIME] * 0.001f * sr;
        const uint32_t tickLen = (uint32_t)(sm[P_RATE] * 0.001f * sr) + 1;
        const float gateA = 1.f - expf(-1.f / (0.015f * sr));
        const float chance = sm[P_CHANCE];
        if (!primed) { curD = manualD; primed = true; }
        for (int i = 0; i < n; i++) {
            if (stoch) {
                if (++tick >= tickLen) {
                    tick = 0;
                    if (rnd01(rng) <= chance) { stochGate = 1.f; stochTime = 0.6f * sqrtf(rnd01(rng)); } else stochGate = 0.f;
                }
            } else { stochGate = 1.f; tick = 0; }
            tgtD = stoch ? fmaxf(stochTime * sr, 48.f) : manualD;
            tgtD = clampf(tgtD, 48.f, maxD);
            // budget: while the delay time is steady (the normal case) the Hermite weights are cached; they are recomputed per sample only while it glides
            if (curD != tgtD) { curD += (tgtD - curD) * gA; if (fabsf(tgtD - curD) < 1e-3f) curD = tgtD; }
            if (gate != stochGate) { gate += (stochGate - gate) * gateA; if (fabsf(stochGate - gate) < 1e-5f) gate = stochGate; }
            if (curD != wD) {
                wD = curD; const float Dm = curD - 1.f; wId = (uint32_t)Dm; const float f = Dm - (float)wId, f2 = f * f, f3 = f2 * f;
                hw0 = -0.5f * f + f2 - 0.5f * f3; hw1 = 1.f - 2.5f * f2 + 1.5f * f3; hw2 = 0.5f * f + 2.f * f2 - 1.5f * f3; hw3 = -0.5f * f2 + 0.5f * f3;
            }
            float yL, yR;
            { const uint32_t p = dl.w - 1 - wId, m = dl.mask;
              yL = hw0 * dl.buf[(p + 1) & m] + hw1 * dl.buf[p & m] + hw2 * dl.buf[(p - 1) & m] + hw3 * dl.buf[(p - 2) & m];
              yR = hw0 * dr.buf[(p + 1) & m] + hw1 * dr.buf[p & m] + hw2 * dr.buf[(p - 1) & m] + hw3 * dr.buf[(p - 2) & m]; }
            float sL = yL + (yR - yL) * sp, sR = yR + (yL - yR) * sp;      // cross-feedback blend (1 = ping-pong)
            lpL += (dcL.tick(sL) - lpL) * lpA; lpL = flush(lpL);
            lpR += (dcR.tick(sR) - lpR) * lpA; lpR = flush(lpR);
            float inL = dryL[i], inR = stereoIn ? dryR[i] : dryL[i] * (1.f - sp);
            dl.write(flush(inL + fb * softclip(lpL)));
            dr.write(flush(inR + fb * softclip(lpR)));
            wetL[i] = softclip(yL) * (0.62f * gate);     // v1: tanh~ then *0.62
            wetR[i] = softclip(yR) * (0.62f * gate);
        }
        dwL.apply(L, dryL, wetL, n, sm[P_MIX]); dwR.apply(R, dryR, wetR, n, sm[P_MIX]);
        return true;
    }
};
inline Fx* fx_create_delay(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxDelay)); return m ? new (m) FxDelay(a, sr) : nullptr; }
}
