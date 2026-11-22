// fx_pitchtrack.h -- cheap monophonic f0 tracker for trumpet (80..1500 Hz). NEW 2026-11-15.
// Method: input one-pole LP (3 kHz) -> boxcar decimate to ~12 kHz -> YIN (de Cheveigne & Kawahara 2002,
// cumulative-mean-normalised difference, absolute threshold 0.15, first-dip-then-local-min, parabolic
// interpolation) on a 192-sample window every 128 decimated samples (~10.7 ms hop).
// Smoothing: median-of-3 for small moves; a >23% jump needs two consecutive consistent hops (rare octave errors);
// 2 unvoiced hops -> f0 = 0.
// Interface (FIXED, used by fx_engine): init(sr, Arena*), process(mono, n), f0 (Hz, 0 = unvoiced), conf 0..1.
// Memory: ~3 KB inline, no heap.
// Ops/sample (hand count): front end ~5 (LP mul-add, accumulate). Analysis per hop (512 input samples):
//   steady 330 Hz (lag 36): ~(36-8+2) lags x 192 x 3 ops = ~17k ->  ~34 ops/sample; worst case (unvoiced/noise: all
//   143 lags) ~ 143x192x3 = 82k -> ~160 ops/sample (early-out keeps voiced notes at 30..100).
#pragma once
#include "fx_common.h"
namespace hx {
struct PitchTrack {
    float f0 = 0.f, conf = 0.f; float sr = 48000.f;

    static const int RB = 512, WIN = 192, MAXLAG = 160, HOP = 128;
    float rb[RB];
    float lin[WIN + MAXLAG + 2];
    float dn[MAXLAG + 3];
    int   wpos = 0, hopCnt = 0, D = 4, accN = 0, tmin = 8, tmax = 150;
    float acc = 0.f, lp = 0.f, lpA = 0.f, fs = 12000.f;
    float raw[3]; int nraw = 0, unv = 0, pendN = 0; float pend = 0.f;

    void init(float sampleRate, Arena* a = nullptr) {
        (void)a; sr = sampleRate; f0 = 0.f; conf = 0.f;
        D = (int)(sr / 12000.f + 0.5f); if (D < 1) D = 1;
        fs = sr / (float)D;
        lpA = 1.f - expf(-kTwoPi * 3000.f / sr);
        tmin = (int)(fs / 1500.f); if (tmin < 2) tmin = 2;
        tmax = (int)(fs / 80.f + 1.f); if (tmax > MAXLAG - 2) tmax = MAXLAG - 2;
        memset(rb, 0, sizeof(rb)); wpos = hopCnt = accN = 0; acc = lp = 0.f; nraw = unv = pendN = 0; pend = 0.f;
    }

    void process(const float* x, int n) {
        const float invD = 1.f / (float)D;
        for (int i = 0; i < n; i++) {
            lp += (x[i] - lp) * lpA;
            acc += lp;
            if (++accN >= D) {
                rb[wpos] = acc * invD; wpos = (wpos + 1) & (RB - 1); acc = 0.f; accN = 0;
                if (++hopCnt >= HOP) { hopCnt = 0; analyze(); }
            }
        }
        lp = flush(lp);
    }

private:
    static float med3(float a, float b, float c) { float m = a < b ? (b < c ? b : (a < c ? c : a)) : (a < c ? a : (b < c ? c : b)); return m; }

    void analyze() {
        const int N = WIN + tmax + 2;
        for (int i = 0; i < N; i++) lin[i] = rb[(wpos - N + i) & (RB - 1)];
        const float* w0 = lin + (N - WIN);                 // newest window
        float e0 = 0.f; for (int j = 0; j < WIN; j++) e0 += w0[j] * w0[j];
        float rms = sqrtf(e0 * (1.f / WIN));
        float cmean = 0.f, gmin = 1e9f; int gi = 0, found = 0, last = 0;
        if (rms > 1.5e-3f) {
            for (int t = tmin; t <= tmax; t++) {
                const float* w1 = w0 - t;
                float d = 0.f; for (int j = 0; j < WIN; j++) { float q = w0[j] - w1[j]; d += q * q; }
                cmean += d;
                float v = cmean > 1e-12f ? d * (float)(t - tmin + 1) / cmean : 1.f;
                dn[t] = v; last = t;
                if (v < gmin) { gmin = v; gi = t; }
                if (!found) { if (v < 0.15f) found = t; }
                else { if (v < dn[found]) found = t; else break; }
            }
        }
        int tau = found ? found : (gmin < 0.3f ? gi : 0);
        if (!tau) { conf = 0.f; if (++unv >= 2) { f0 = 0.f; nraw = 0; pendN = 0; } else { /* hold */ } if (f0 == 0.f) conf = 0.f; return; }
        unv = 0;
        float tf = (float)tau;
        if (tau > tmin && tau < last) {                       // parabolic interpolation on the normalised difference
            float a = dn[tau - 1], b = dn[tau], c = dn[tau + 1], den = a - 2.f * b + c;
            if (den > 1e-9f) { float off = 0.5f * (a - c) / den; if (off > -1.f && off < 1.f) tf += off; }
        }
        float r = fs / tf;
        conf = clampf(1.f - dn[tau], 0.f, 1.f);
        // smoothing
        if (nraw < 3) { raw[nraw++] = r; } else { raw[0] = raw[1]; raw[1] = raw[2]; raw[2] = r; }
        if (f0 <= 0.f) { f0 = r; pendN = 0; return; }
        float ratio = r / f0;
        if (ratio > 0.8f && ratio < 1.25f) {
            pendN = 0;
            f0 = (nraw >= 3) ? med3(raw[0], raw[1], raw[2]) : r;
            if (f0 / r > 1.25f || f0 / r < 0.8f) f0 = r;
        } else {
            if (pendN > 0 && fabsf(r / pend - 1.f) < 0.05f) pendN++; else { pendN = 1; }
            pend = r;
            if (pendN >= 2) { f0 = r; pendN = 0; raw[0] = raw[1] = raw[2] = r; nraw = 3; }
        }
    }
};
} // namespace hx
