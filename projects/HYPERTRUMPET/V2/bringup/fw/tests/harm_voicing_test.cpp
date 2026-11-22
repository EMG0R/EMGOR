// Host: shifter quality of the NEW harmony table (padfx-4.9) on a trumpet-like input (harmonics 1..6, 1/h, light vibrato), ctx.f0 = 0 exactly as the padfx chain runs it (no external pitch tracker).
// Per interval and input pitch: pitch error of the voice alone (output with the voice minus output without it) in cents by autocorrelation, and its level re the dry in dB.
//   c++ -std=c++17 -O2 -I../../../HYPER_CORE/src/dsp/fx tests/harm_voicing_test.cpp -o /tmp/hv && /tmp/hv
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "fx_harmony.h"
using namespace hx;
static std::vector<float> run(int semi, float f0, int blocks, bool voice) {
    static uint8_t mem[40960]; memset(mem, 0xA5, sizeof(mem)); Arena a; a.bind(mem, sizeof(mem)); FxHarmony* h = static_cast<FxHarmony*>(fx_create_harmony(a, 48000.f)); h->init(48000.f, 48.f);
    h->setParam(FxHarmony::P_MIX, 1.f); h->setParam(FxHarmony::P_VOICE, 0.7f); h->Fx::snap(); if(voice) h->voiceOn(semi, 1.f);
    FxCtx c = {}; c.sr = 48000.f; c.f0 = 0.f; std::vector<float> out; float ph = 0, vib = 0; float L[48], R[48];
    for(int b = 0; b < blocks; b++) {
        for(int i = 0; i < 48; i++) { float f = f0 * (1.f + 0.004f * sinf(vib)); vib += 6.2831853f * 5.f / 48000.f; ph += 6.2831853f * f / 48000.f; float x = 0; for(int k = 1; k <= 6; k++) x += sinf(k * ph) / (float)k; L[i] = R[i] = 0.08f * x; }
        h->smoothBlock(); h->process(c, L, R, 48, false); for(int i = 0; i < 48; i++) out.push_back(L[i]);
    }
    return out;
}
int main() {
    static const int semi[16] = { -12, -10, -9, -8, -7, -5, -4, -2, 2, 3, 4, 5, 7, 9, 10, 12 }; const float f0s[3] = { 220.f, 330.f, 523.f };
    printf("interval | input Hz | voice pitch err (cents) | voice level re dry (dB)\n");
    double worstC = 0, minL = 99, maxL = -99;
    for(int fi = 0; fi < 3; fi++) for(int p = 0; p < 16; p++) {
        const int blocks = 48000 * 2 / 48; auto w = run(semi[p], f0s[fi], blocks, true), d = run(semi[p], f0s[fi], blocks, false);
        std::vector<float> v(w.size()); for(size_t i = 0; i < w.size(); i++) v[i] = w[i] - d[i];
        const size_t s0 = 48000, n = 24000; double e = 0, ed = 0; for(size_t i = 0; i < n; i++) { e += v[s0 + i] * v[s0 + i]; ed += d[s0 + i] * d[s0 + i]; }
        const float target = f0s[fi] * powf(2.f, semi[p] / 12.f); int lagLo = (int)(48000.f / (target * 1.06f)), lagHi = (int)(48000.f / (target * 0.94f)); double best = -1; int bl = lagLo;
        for(int lag = lagLo; lag <= lagHi; lag++) { double s = 0; for(size_t i = 0; i < n - 2 * lagHi; i++) s += v[s0 + i] * v[s0 + i + lag]; if(s > best) { best = s; bl = lag; } }
        const double fm = 48000.0 / bl, cents = 1200.0 * log2(fm / target), lev = 10 * log10((e + 1e-20) / (ed + 1e-20));
        printf("%+3d st | %5.0f | %+6.1f | %+5.1f\n", semi[p], f0s[fi], cents, lev); if(fabs(cents) > worstC) worstC = fabs(cents); if(lev < minL) minL = lev; if(lev > maxL) maxL = lev;
    }
    printf("worst |cents| %.1f ; level spread %.1f .. %.1f dB (%.1f dB)\n", worstC, minL, maxL, maxL - minL);
}
