// fx_roar.h -- HYPERTRUMPET V2 "roar": saturator. Replaces v1 Ableton Roar (V1_SCRAPE 4.12: only shaper_amount_1 on pot1,
// output gain 0.3 + 0.7*(1-a)^2). Own algorithm, no third-party code.
// Chain per channel: input -> pre tilt EQ (TONE*0.5) -> DRIVE gain (1..~63x, ramped per sample) + BIAS -> pre WAVEFOLD (FOLD)
//   -> 2x oversample (15-tap Blackman halfband FIR; up = 8-tap even phase + pure 3-sample delay odd phase, down = 15-tap)
//   -> shaper TYPE {SOFT rational tanh, ASYM even-harmonic, FOLD sine-fold, CRUSH clip+quantise} at 2x -> decimate
//   -> DC block -> post tilt (TONE) -> FILTER (1-pole LP) -> auto-gain 0.3+0.7*(1-drive)^2 (v1 law) -> OUTPUT dB.
// Latency ~12.5 samples in both modes; the DRY path is delayed by 13 so MIX blends phase-aligned.
// Aliasing (2026-11-17): the 15-tap halfbands are weak in the 24..30 kHz transition and a folded shaper at high drive has harmonics far beyond 2x. So the FOLD shaper is first-order
//   antiderivative anti-aliased (ADAA), the FOLD param is applied at the oversampled rate, and above DRIVE 0.6 (FOLD type) / 0.8 (others, with hysteresis) a second 2x stage (4x total,
//   11-tap up/down halfbands around the shaper, a 31-tap decimator back to 2x) switches in. Alias level at DRIVE 1.0: FOLD -1.4 -> -34 dB, SOFT -32 -> -48 dB, CRUSH -30 -> -41 dB (host, 1.7 kHz sine).
// The CRUSH quantiser is intentionally rough and still aliases.
#pragma once
#include "fx_common.h"
#include "fx_halfband.h"
namespace hx {
struct FxRoar : Fx {
    enum { P_MIX, P_DRIVE, P_TYPE, P_BIAS, P_TONE, P_FOLD, P_OUT, P_FILT };
    static constexpr int LAT = 13;                       // wet latency is 12.5..12.7 samples in both modes (hi: 31-tap decimator + 4x stage; plain: compensating delays cd[], yd[]); dry delay 13
    struct Ch { Svf pre, post; DcBlock dc; float ux[8] = {}, zh[16] = {}; float dly[16] = {}; HbIntT<16> hu; HbDecT<16> hd; HbDecT<32> hd1; float cd[8] = {}, yd[8] = {}; int ci = 0, yi = 0; float lpf = 0.f; float xp = 0.f, Fp = -1.f / 2.042035f, xph = 0.f, Fph = -1.f / 2.042035f; int ui = 0, zi = 0, di = 0; };
    HalfBand hb4, hb31; bool hi = false; float wHi = 0.f;                          // second oversampling stage (2x -> 4x) around the shaper, on only for heavy drive
    Ch ch[2]; float h[15]; float gPrev = 1.f, aPrev = 1.f; float tmpl = 0;
    bool first = true; float wetPrev = 1.f; bool primed = false;
    FxRoar(Arena&, float s) {
        name = "ROAR"; sr = s;
        static const char* const types[] = {"SOFT", "ASYM", "FOLD", "CRUSH"};
        P(P_MIX,  "MIX",    0.f, 1.f, 1.f, 0.f, PF_PERCENT);
        P(P_DRIVE,"DRIVE",  0.f, 1.f, 0.3f, 0.f, PF_PERCENT);
        P(P_TYPE, "TYPE",   0.f, 3.f, 0.f, 1.f, PF_ENUM | PF_QUANT, types);
        P(P_BIAS, "BIAS",  -1.f, 1.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_TONE, "TONE",  -1.f, 1.f, 0.f, 0.f, PF_BIPOLAR);
        P(P_FOLD, "FOLD",   0.f, 1.f, 0.f, 0.f, PF_PERCENT);
        P(P_OUT,  "OUTPUT",-24.f, 12.f, 0.f);
        P(P_FILT, "FILTER", 500.f, 20000.f, 20000.f, 0.f, PF_LOG);
        setRole(R_AMT, P_DRIVE); setRole(R_TONE, P_TONE); setRole(R_MOD, P_FOLD); setRole(R_FB, P_BIAS);
        hb4.design(11, 4.5f); hb31.design(31, 6.0f); for (int c = 0; c < 2; c++) { ch[c].hu.hb = &hb4; ch[c].hd.hb = &hb4; ch[c].hd1.hb = &hb31; ch[c].hu.clear(); ch[c].hd.clear(); ch[c].hd1.clear(); memset(ch[c].cd, 0, sizeof ch[c].cd); memset(ch[c].yd, 0, sizeof ch[c].yd); }
        float sum = 0; for (int j = 0; j < 15; j++) { int o = j - 7; float v = 0.f;
            if (o != 0 && (o & 1)) { float x = kPi * 0.5f * o; v = 0.5f * sinf(x) / x; float w = 0.42f + 0.5f * cosf(kPi * o / 8.f) + 0.08f * cosf(2.f * kPi * o / 8.f); v *= w; sum += v; } h[j] = v; }
        float k = 0.5f / sum; for (int j = 0; j < 15; j++) h[j] *= k; h[7] = 0.5f;
    }
    static inline float preFold(float x, float f) { return f > 0.001f ? lerp(x, fold(x * (1.f + 2.f * f)), f) : x; }   // the FOLD param, now at the 2x rate (it aliased at 1x)
    static inline float fold(float x) { return sinPar(0.25f * x); }                // bounded +-1, folds past |x|=1
    // FOLD shaper with first-order antiderivative anti-aliasing (ADAA): y = (F(x) - F(xp)) / (x - xp), F = -cos(w x) / w, w = 2 pi * 0.25 * 1.3.
    // The folds per cycle at high drive are far beyond what 2x oversampling can hold; the ADAA average removes most of the folded-back energy at ~1 cosf per sample.
    static inline float foldAdaa(float& xp, float& Fp, float x) {
        const float w = 2.042035f; float F = -sinPar(0.325f * x + 0.25f) * (1.f / w), dx = x - xp, y;     // sinPar: cheap parabolic sine (0.1% error), so ADAA only above a larger step
        if (fabsf(dx) > 0.15f) y = (F - Fp) / dx; else y = sinPar(0.325f * 0.5f * (x + xp));
        xp = x; Fp = F; return y;
    }
    static inline float shape(int type, float x, float crushL) {
        switch (type) {
            default:
            case 0: return softclip(x);
            case 1: { float s = softclip(x); return s + 0.3f * s * s; }
            case 2: return fold(x * 1.3f);
            case 3: { float c = clampf(x, -1.f, 1.f); return floorf(c * crushL + 0.5f) * (1.f / crushL); }
        }
    }
    inline float one(Ch& s, int type, float x, float crushL) { return type == 2 ? foldAdaa(s.xp, s.Fp, x) : shape(type, x, crushL); }
    inline float oneH(Ch& s, int type, float x, float crushL) { return type == 2 ? foldAdaa(s.xph, s.Fph, x) : shape(type, x, crushL); }
    // shaper for one 2x-rate sample. plainZ: at 2x (delayed 5 samples to line up with the 4x path). hiZ: 4x (up, shape twice, down).
    inline float plainZ(Ch& s, float x, int type, float crushL, float fold_) {
        s.cd[s.ci] = one(s, type, preFold(x, fold_), crushL); const float z = s.cd[(s.ci - 5) & 7]; s.ci = (s.ci + 1) & 7; return z;
    }
    inline float hiZ(Ch& s, float x, int type, float crushL, float fold_) {
        float a0, a1, yv = 0.f; s.hu.push(x, &a0, &a1);
        const float z0 = oneH(s, type, preFold(a0, fold_), crushL), z1 = oneH(s, type, preFold(a1, fold_), crushL); s.hd.push(z0, &yv); s.hd.push(z1, &yv); return yv;
    }
    bool process(const FxCtx&, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) override {
        if (n > HX_MAXBLOCK) n = HX_MAXBLOCK;
        int nch = stereoIn ? 2 : 1;
        float drive = sm[P_DRIVE]; int type = (int)v[P_TYPE];
        float gT = expf(drive * 4.15f);                                       // 1 .. 63 (36 dB)
        float bias = sm[P_BIAS] * 0.5f * (1.f + drive);
        float aT = 0.3f + 0.7f * (1.f - drive) * (1.f - drive) ;              // v1 auto-gain law
        aT *= dbToLin(sm[P_OUT]);
        float fold_ = sm[P_FOLD], crushL = 3.f + 60.f * (1.f - drive) * (1.f - drive);
        float tone = sm[P_TONE];
        if (!hi && drive > (type == 2 ? 0.6f : 0.8f)) hi = true; else if (hi && drive < (type == 2 ? 0.5f : 0.7f)) hi = false;   // hysteresis
        float gl = exp2f(-tone), gh = exp2f(tone), pl = exp2f(-0.5f * tone), ph_ = exp2f(0.5f * tone);
        float fa = 1.f - expf(-kTwoPi * clampf(sm[P_FILT], 200.f, 0.45f * sr) / sr); bool fOn = sm[P_FILT] < 19000.f;
        if (first) { gPrev = gT; aPrev = aT; first = false; }
        if (!primed) { wetPrev = sm[P_MIX]; primed = true; }
        float mixT = sm[P_MIX], mInc = (mixT - wetPrev) / (float)n;
        const bool toneOn = fabsf(tone) > 1e-4f;                                // budget: TONE 0 = both tilt filters are identity, skip them
        const float h0 = h[0], h2c = h[2], h4 = h[4], h6 = h[6];                // half-band FIR: odd offsets only, symmetric -> 4 mults up, 5 mults down
        float* io[2] = {L, R};
        for (int c = 0; c < nch; c++) {
            Ch& s = ch[c]; float* x = io[c];
            if (toneOn) { s.pre.set(700.f, 0.707f, sr); s.post.set(700.f, 0.707f, sr); }
            float g = gPrev, gi = (gT - gPrev) / (float)n, a = aPrev, ai = (aT - aPrev) / (float)n, m = wetPrev;
            for (int i = 0; i < n; i++) {
                float in = x[i];
                s.dly[s.di] = in;                                                 // dry delayed by LAT for phase-aligned blend
                float dry = s.dly[(s.di - LAT) & 15]; s.di = (s.di + 1) & 15;
                float t = in; if (toneOn) { float lp, bp, hp; s.pre.tick(in, lp, bp, hp); t = in + (pl - 1.f) * lp + (ph_ - 1.f) * hp; }
                g += gi; a += ai;
                float u = t * g + bias;
                // ---- 2x up
                s.ux[s.ui] = u; float e;
                { const int q = s.ui; e = h0 * (s.ux[q] + s.ux[(q - 7) & 7]) + h2c * (s.ux[(q - 1) & 7] + s.ux[(q - 6) & 7]) + h4 * (s.ux[(q - 2) & 7] + s.ux[(q - 5) & 7]) + h6 * (s.ux[(q - 3) & 7] + s.ux[(q - 4) & 7]); }
                e *= 2.f; float o3 = s.ux[(s.ui - 3) & 7]; s.ui = (s.ui + 1) & 7;
                // ---- shape at 2x, decimate (at the even phase)
                // wHi crossfades between the plain (2x) and the 4x path over ~10 ms whenever `hi` flips, so there is no click at the switch; both run only during the fade
                if (hi) { if (wHi < 1.f) wHi = fminf(1.f, wHi + 1.f / 480.f); } else if (wHi > 0.f) wHi = fmaxf(0.f, wHi - 1.f / 480.f);
                float yP = 0.f, yH = 0.f;
                if (wHi < 1.f) {
                    const float ze = plainZ(s, e, type, crushL, fold_), zo = plainZ(s, o3, type, crushL, fold_);
                    s.zh[s.zi] = ze;
                    { const int q = s.zi; yP = 0.5f * s.zh[(q - 7) & 15] + h6 * (s.zh[(q - 6) & 15] + s.zh[(q - 8) & 15]) + h4 * (s.zh[(q - 4) & 15] + s.zh[(q - 10) & 15]) + h2c * (s.zh[(q - 2) & 15] + s.zh[(q - 12) & 15]) + h0 * (s.zh[q] + s.zh[(q - 14) & 15]); }
                    s.zi = (s.zi + 1) & 15; s.zh[s.zi] = zo; s.zi = (s.zi + 1) & 15;
                    s.yd[s.yi] = yP; yP = s.yd[(s.yi - 3) & 7]; s.yi = (s.yi + 1) & 7;            // same total latency as the 4x path
                }
                if (wHi > 0.f) { const float ze = hiZ(s, e, type, crushL, fold_), zo = hiZ(s, o3, type, crushL, fold_); float t; s.hd1.push(ze, &t); s.hd1.push(zo, &yH); }   // 31-tap half-band decimator: the 15-tap one leaves strong folds at 25..30 kHz
                float y = wHi <= 0.f ? yP : (wHi >= 1.f ? yH : yP + (yH - yP) * wHi);
                y = s.dc.tick(y);
                float p = y; if (toneOn) { float l2, b2, h2; s.post.tick(y, l2, b2, h2); p = y + (gl - 1.f) * l2 + (gh - 1.f) * h2; }
                if (fOn) { s.lpf += (p - s.lpf) * fa; s.lpf = flush(s.lpf); p = s.lpf; }
                float wet = flush(p * a);
                m += mInc; x[i] = dry + (wet - dry) * m;
            }
        }
        gPrev = gT; aPrev = aT; wetPrev = mixT;
        return stereoIn;
    }
};
inline Fx* fx_create_roar(Arena& a, float sr) { void* m = a.allocBytes(sizeof(FxRoar)); return m ? new (m) FxRoar(a, sr) : nullptr; }
}
