// fx_halfband.h -- streaming half-band FIR resampler pieces shared by the spectral effects (24/12 kHz STFT) and the roar oversampler.
#pragma once
#include "fx_common.h"
namespace hx {
// Streaming half-band FIR (odd length L = 4q+3, causal: centre tap at C = (L-1)/2, which is odd, so every other tap is zero). Gain 1 at DC.
struct HalfBand {
    static const int MAXL = 47;
    int L = 0, C = 0, nt = 0; float h[MAXL];            // h[0..L-1] causal taps
    void design(int len, float beta) {                  // Kaiser-windowed sinc, cut-off fs/4
        L = len; C = (L - 1) / 2; double sum = 0; double w[MAXL];
        auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        for (int n = 0; n < L; n++) { double m = n - C, a = m / C, win = i0(beta * sqrt(1.0 - a * a)) / i0(beta);
            double x = 3.141592653589793 * m / 2.0, sinc = m == 0 ? 1.0 : sin(x) / x; double v = ((n - C) == 0 || ((n - C) & 1)) ? 0.5 * sinc * win : 0.0; w[n] = v; sum += v; }
        for (int n = 0; n < L; n++) h[n] = (float)(w[n] / sum);
    }
};
// 2:1 decimator and 1:2 interpolator built on it. History is a double-written ring (the last 64 samples are always contiguous); symmetric taps are folded.
template <int W = 64> struct HbDecT {      // W: history window (power of two >= filter length)
    float buf[2 * W]; uint32_t wp = 0; int par = 0; const HalfBand* hb = nullptr;
    void clear() { memset(buf, 0, sizeof buf); wp = 0; par = 0; }
    inline bool push(float x, float* y) {               // one input sample; true and *y on every second sample
        const uint32_t w = wp & (W - 1); buf[w] = x; buf[w + W] = x; wp++; par ^= 1; if (par) return false;
        const float* p = buf + W + w;                  // p[-n] = x[t - n]
        const int C = hb->C, L1 = hb->L - 1; float acc = hb->h[C] * p[-C];
        for (int n = (C & 1) ? 0 : 1; n < C; n += 2) acc += hb->h[n] * (p[-n] + p[-(L1 - n)]);
        *y = acc; return true;
    }
};
template <int W = 64> struct HbIntT {
    float buf[2 * W]; uint32_t wp = 0; const HalfBand* hb = nullptr;
    void clear() { memset(buf, 0, sizeof buf); wp = 0; }
    inline void push(float x, float* y0, float* y1) {   // one input sample -> two output samples (gain 2 compensated)
        const uint32_t w = wp & (W - 1); buf[w] = x; buf[w + W] = x; wp++;
        const float* p = buf + W + w; const int C = hb->C, L1 = hb->L - 1;
        float acc = 0.f; for (int n = 0; n < C; n += 2) acc += hb->h[n] * (p[-(n >> 1)] + p[-((L1 - n) >> 1)]);   // even outputs: taps at even n, folded
        *y0 = 2.f * acc; *y1 = p[-((C - 1) >> 1)];                                                                  // odd outputs: the centre tap alone (2 * 0.5)
    }
};

using HbDec = HbDecT<64>;
using HbInt = HbIntT<64>;
}
