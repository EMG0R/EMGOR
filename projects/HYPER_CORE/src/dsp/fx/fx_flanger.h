// fx_flanger.h -- HYPERTRUMPET V2 flanger (registry id "flange").
// Re-implementation (clean C++) of Iain McCurdy's "Flanger.csd" (Csound / Cabbage, McCurdy 2012), CC BY-NC-SA 4.0
// (https://creativecommons.org/licenses/by-nc-sa/4.0/); this derivative is CC BY-NC-SA 4.0 as well (non-commercial, share-alike,
// credit to Iain McCurdy). Csound's `flanger` opcode = feedback delay with modulated delay time: y = D(x + fb*y), which is what
// the ring below does. Chain per channel: delay = OFFSET + DEPTH*LFO(shape), feedback comb (clamped +-0.95), DC block,
// 0.5*dry + 0.5*wet, then outer mix. THRUZ=1 (v1 default): outer mix = lerp(-dry delayed 1 sample (~20 us), flange, MIX), i.e.
// at MIX=0 the output is the polarity-inverted dry (as in v1, see V1_SCRAPE 4.6); THRUZ=0: lerp(dry, flange, MIX).
// LFO shapes (v1 lfoshape 1..5): parabola (u-shape), sine, triangle (all positive-domain 0..1), randomi, randomh.
// v1 preset: rate 0.257 Hz, depth 7.8 ms, offset 20 us (clamped to 2 samples, the shortest feedback loop), fb 0.9, sine, thru-zero.
// Mono input -> mono out; stereo in -> two lines (random shapes decorrelated per side). Hermite read, per-sample 1 ms slew of delay.
#pragma once
#include "fx_common.h"
namespace hx {
struct FxFlanger : Fx {
    enum { P_MIX = 0, P_RATE, P_DEPTH, P_OFFSET, P_FB, P_SHAPE, P_THRUZ, P_LEVEL };
    DelayLine dl[2]; DcBlock dc[2];
    float ph = 0.f, prevR[2] = {0, 0}, nextR[2] = {0, 0}; uint32_t rng[2] = { 0x2468ACEu, 0x13579BDu };
    float dcur[2] = {0, 0}, x1[2] = {0, 0}, mixCur = 0.f; bool primed = false;
    volatile bool clearReq = false;
    bool satFb = false;      // opt-in (MODBLOCK): soft-clip the feedback term inside the loop (cubic clamp, no division)
    FxFlanger(Arena& a, float s) {
        name = "FLANGER"; sr = s;
        static const char* const shapes[] = { "PARAB", "SINE", "TRI", "RANDI", "RANDH" };
        static const char* const onoff[] = { "OFF", "ON" };
        P(P_MIX, "MIX", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_RATE, "RATE", 0.001f, 40.f, 0.257f, 0.f, PF_LOG);      // Hz
        P(P_DEPTH, "DEPTH", 0.f, 10.f, 7.8f, 0.f);                  // ms
        P(P_OFFSET, "OFFSET", 0.02f, 100.f, 0.02f, 0.f, PF_LOG);    // ms
        P(P_FB, "FB", -1.f, 1.f, 0.9f, 0.f, PF_BIPOLAR);
        P(P_SHAPE, "SHAPE", 0.f, 4.f, 1.f, 1.f, PF_ENUM | PF_QUANT, shapes);
        P(P_THRUZ, "THRUZ", 0.f, 1.f, 1.f, 1.f, PF_ENUM | PF_QUANT, onoff);
        P(P_LEVEL, "LEVEL", 0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        setRole(R_AMT, P_MIX); setRole(R_TIME, P_RATE); setRole(R_MOD, P_DEPTH); setRole(R_FB, P_FB); setRole(R_TONE, P_OFFSET);
        uint32_t n = (uint32_t)(0.115f * s) + 8;
        if (!dl[0].init(a, n) || !dl[1].init(a, n)) ok = false;
    }
    void reset() override { clearReq = true; }
    void action(int id, float) override { if (id == A_CLEAR) clearReq = true; }

    inline float lfo(int ch, int shape) {   // 0..1
        float x = ph;
        switch (shape) {
            case 0: return 1.f - 4.f * x * (1.f - x);
            case 1: return 0.5f + 0.5f * sinPar(x);
            case 2: return x < 0.5f ? 2.f * x : 2.f - 2.f * x;
            case 3: return prevR[ch] + (nextR[ch] - prevR[ch]) * x;
            default: return prevR[ch];
        }
    }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (!ok) return stereoIn;
        if (clearReq) { clearReq = false; for (int c = 0; c < 2; c++) { memset(dl[c].buf, 0, dl[c].size() * 4); dc[c] = DcBlock(); x1[c] = 0; } }
        const int nc = stereoIn ? 2 : 1; float* ch[2] = { L, R };
        const int shape = (int)sm[P_SHAPE]; const bool tz = sm[P_THRUZ] > 0.5f;
        const float inc = sm[P_RATE] / sr, depth = sm[P_DEPTH] * 0.001f * sr, off = sm[P_OFFSET] * 0.001f * sr;
        const float fb = clampf(sm[P_FB], -0.95f, 0.95f), lvl = sm[P_LEVEL];
        const float slew = 1.f - expf(-1.f / (0.001f * sr));
        const float mixT = sm[P_MIX], mixInc = primed ? (mixT - mixCur) / (float)n : 0.f;
        if (!primed) { mixCur = mixT; primed = true; }
        const float maxD = (float)(dl[0].size() - 8);
        float ph0 = ph, pr[2] = { prevR[0], prevR[1] }, nx[2] = { nextR[0], nextR[1] };
        for (int c = 0; c < nc; c++) {
            ph = ph0; prevR[c] = pr[c]; nextR[c] = nx[c];
            float m = mixCur; float* b = ch[c];
            for (int i = 0; i < n; i++) {
                ph += inc;
                if (ph >= 1.f) { ph -= 1.f; prevR[c] = nextR[c]; nextR[c] = rnd01(rng[c]); }
                float D = off + depth * lfo(c, shape);
                D = clampf(D, 2.f, maxD);
                if (dcur[c] == 0.f) dcur[c] = D;
                dcur[c] += (D - dcur[c]) * slew;
                float x = b[i];
                float y = dl[c].read(dcur[c] - 1.f);
                dl[c].write(flush(x + fb * (satFb ? cubicClip(y) : y)));
                float wet = 0.5f * x + 0.5f * dc[c].tick(y);
                float dry = tz ? -x1[c] : x; x1[c] = x;
                b[i] = (dry + (wet - dry) * m) * lvl;
                m += mixInc;
            }
        }
        mixCur = mixT;
        // advance shared phase only once (loop above restored ph0 per channel); after the last channel ph is at ph0 + n*inc already
        return stereoIn;
    }
};
inline Fx* fx_create_flanger(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxFlanger)); return m ? new (m) FxFlanger(a, sr) : nullptr; }
}
