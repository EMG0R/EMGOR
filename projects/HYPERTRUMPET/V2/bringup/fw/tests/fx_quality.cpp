// Host quality checks for the effects (no hardware): click on wake, DC offset, non-finite output under random parameter sweeps, pitch accuracy (harmonizer, MOOD), roar aliasing.
// Build (from bringup/fw):  c++ -std=c++17 -O2 -DHX_REVERB_LITE -I../../../HYPER_CORE/src/dsp/fx tests/fx_quality.cpp -o /tmp/fx_quality && /tmp/fx_quality
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "fx_harmonizer.h"
#include "fx_looper.h"
#include "fx_reverb.h"
#include "fx_grain.h"
#include "fx_delay.h"
#include "fx_flanger.h"
#include "fx_chorus.h"
#include "fx_mood.h"
#include "fx_freeze.h"
#include "fx_blur.h"
#include "fx_roar.h"
#include "fx_ringmod.h"
#include "fx_wah.h"
#include "fx_autopan.h"
#include "fx_harmony.h"
using namespace hx;
typedef Fx* (*Mk)(Arena&, float);
static std::vector<uint8_t> mem(64u << 20);
struct Rig { Arena a; Fx* f; float L[48], R[48]; long blk = 0; FxCtx c; Rig(Mk mk) { a.bind(mem.data(), mem.size()); memset(mem.data(), 0, 8 << 20); f = mk(a, 48000.f); f->init(48000.f, 48.f); c = {}; c.sr = 48000; c.f0 = 220; c.conf = 1; c.env = 0.5f; }
    void run(float hz, float amp, float* out, bool stereoIn = false) { for (int i = 0; i < 48; i++) { L[i] = amp * sinf(6.2831853f * hz * (float)((blk * 48 + i) % 480000) / 48000.f); R[i] = L[i]; } blk++; f->smoothBlock(); f->process(c, L, R, 48, stereoIn); for (int i = 0; i < 48; i++) out[i] = L[i]; } };
static double pitchHz(const std::vector<float>& x) {      // autocorrelation peak over 60..1500 Hz
    double best = 0; int bl = 0; for (int lag = 32; lag < 800; lag++) { double s = 0; for (size_t i = 0; i + lag < x.size(); i += 2) s += (double)x[i] * x[i + lag]; if (s > best) { best = s; bl = lag; } } return bl ? 48000.0 / bl : 0; }
int main() {
    struct E { const char* n; Mk mk; } es[] = { {"harm", fx_create_harm}, {"looper", fx_create_looper}, {"reverb", fx_create_reverb}, {"grain", fx_create_grain}, {"delay", fx_create_delay}, {"flange", fx_create_flanger}, {"chorus", fx_create_chorus}, {"mood", fx_create_mood}, {"freeze", fx_create_freeze}, {"blur", fx_create_blur}, {"roar", fx_create_roar}, {"ring", fx_create_ringmod}, {"wah", fx_create_wah}, {"autopan", fx_create_autopan} };
    printf("%-8s %-22s %-14s %-12s\n", "effect", "click (jump/natural)", "DC mean", "non-finite");
    for (auto& e : es) {
        // 1) click on wake: mix 0 for 200 ms then the glue's wake (sm[0] = 0, v[0] = 1) with the effect's own smoother; max |dy| over the next 200 ms vs the sine's natural max step
        Rig r(e.mk); float o[48]; r.f->setParam(0, 0.f); r.f->Fx::snap(); for (int b = 0; b < 200; b++) r.run(220, 0.3f, o);
        r.f->v[0] = 1.f; r.f->sm[0] = 0.f; double mj = 0, last = o[47]; const double nat = 6.2831853 * 220 * 0.3 / 48000;
        for (int b = 0; b < 200; b++) { r.run(220, 0.3f, o); for (int i = 0; i < 48; i++) { double d = fabs(o[i] - last); if (d > mj) mj = d; last = o[i]; } }
        // 2) DC over 2 s steady
        double sum = 0; int cnt = 0; for (int b = 0; b < 2000; b++) { r.run(220, 0.3f, o); if (b > 1000) for (int i = 0; i < 48; i++) { sum += o[i]; cnt++; } }
        // 3) random parameter sweeps: 30 s of random params every 100 ms, count non-finite samples
        long bad = 0; uint32_t rs = 77; auto rn = [&]() { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.f / 16777216.f); };
        for (int b = 0; b < 30000; b++) { if (b % 100 == 0) for (int i = 1; i < r.f->np; i++) { const Param& p = r.f->pt[i]; float v = p.min + (p.max - p.min) * rn(); if ((p.flags & PF_LOG) && p.min > 0) v = p.min * powf(p.max / p.min, rn()); r.f->setParam(i, v); }
            r.run(220 + 600 * rn(), 0.5f * rn(), o); for (int i = 0; i < 48; i++) if (!(o[i] - o[i] == 0.f)) bad++; }
        printf("%-8s %6.1f (%.3f/%.3f)   %+.5f       %ld\n", e.n, mj / nat, mj, nat, cnt ? sum / cnt : 0, bad);
    }
    // 4) pitch accuracy: harmonizer index 12 = (+12, none), 4 = (-5); MOOD PITCH idx 6 = +12, 2 = -12, with probability 1 (wet always)
    struct PT { const char* n; Mk mk; int pi; float val; int mi; float mv; double exp; } ps[] = { {"harm +12", fx_create_harm, 1, 12, -1, 0, 880}, {"harm -5", fx_create_harm, 1, 4, -1, 0, 440 * pow(2, -5 / 12.0)}, {"mood +12", fx_create_mood, 1, 6, 2, 1.f, 880}, {"mood -12", fx_create_mood, 1, 2, 2, 1.f, 220} };
    for (auto& p : ps) { Rig r(p.mk); r.f->setParam(0, 1.f); r.f->setParam(p.pi, p.val); if (p.mi >= 0) r.f->setParam(p.mi, p.mv); r.f->Fx::snap(); r.c.f0 = 440; std::vector<float> y; float o[48];
        for (int b = 0; b < 1400; b++) { r.run(440, 0.3f, o); if (b >= 1000) for (int i = 0; i < 48; i++) y.push_back(o[i]); }
        double hz = 0; if (p.mi < 0) { double bm = 0; for (double f = 100; f < 1800; f += 1.0) { if (fabs(f - 440) < 25) continue; double re = 0, im = 0, w = 6.2831853 * f / 48000; for (size_t i = 0; i < y.size(); i += 1) { re += y[i] * cos(w * i); im -= y[i] * sin(w * i); } double m = re * re + im * im; if (m > bm) { bm = m; hz = f; } } } else hz = pitchHz(y);   /* harmonizer keeps the dry voice: strongest peak that is not the 440 Hz dry; MOOD replaces the dry: autocorrelation */ printf("pitch %-9s expected %.1f Hz measured %.1f Hz  (%+.1f cents)\n", p.n, p.exp, hz, hz > 0 ? 1200 * log2(hz / p.exp) : 0.0); }
    // 5) roar aliasing: 5 kHz sine in; energy more than 150 Hz away from any multiple of 5 kHz (folded at 24 kHz) relative to total
    for (int cs = 0; cs < 2; cs++) { Rig r(fx_create_roar); r.f->setParam(0, 1.f); r.f->setParam(1, cs ? 1.f : 0.3f); r.f->setParam(2, cs ? 2.f : 0.f); r.f->Fx::snap(); std::vector<float> y; float o[48]; for (int b = 0; b < 600; b++) { r.run(5000, 0.5f, o); if (b >= 200) for (int i = 0; i < 48; i++) y.push_back(o[i]); }
      const int N = 4096; double tot = 0, nonh = 0; for (int k = 1; k < N / 2; k++) { double re = 0, im = 0, w = 6.2831853 * k / N; for (int i = 0; i < N; i++) { double h = 0.5 - 0.5 * cos(6.2831853 * i / N); re += y[i] * h * cos(w * i); im -= y[i] * h * sin(w * i); } double e = re * re + im * im; tot += e; double hz = k * 48000.0 / N; double m = fmod(hz, 5000.0); if (m > 5000 - 150) m = 5000 - m; if (m > 150) nonh += e; }
      printf("roar aliasing (5 kHz in, %s): non-harmonic energy %.1f dB re total\n", cs ? "FOLD, drive max" : "SOFT, drive 0.3 (default)", 10 * log10((nonh + 1e-30) / tot)); }
    // 6) harmony: three voices (+4, +7, +10) pitch accuracy of the +7 voice alone, and the latency of the wet path (click response peak, voice at full gain)
    { Rig r(fx_create_harmony); r.f->setParam(0, 1.f); r.f->setParam(1, 1.f); r.f->Fx::snap(); r.c.f0 = 440; FxHarmony* h = static_cast<FxHarmony*>(r.f); h->setSlot(0, 7); float o[48]; std::vector<float> y;
      for (int b = 0; b < 1500; b++) { r.run(440, 0.3f, o); if (b >= 1000) for (int i = 0; i < 48; i++) y.push_back(o[i]); }
      double bm = 0, hz = 0; for (double f = 100; f < 1800; f += 1.0) { if (fabs(f - 440) < 25) continue; double re = 0, im = 0, w = 6.2831853 * f / 48000; for (size_t i = 0; i < y.size(); i++) { re += y[i] * cos(w * i); im -= y[i] * sin(w * i); } double m = re * re + im * im; if (m > bm) { bm = m; hz = f; } }
      double sw = 0, sf = 0; for (double f = 580; f < 740; f += 1.0) { double re = 0, im = 0, w = 6.2831853 * f / 48000; for (size_t i = 0; i < y.size(); i++) { re += y[i] * cos(w * i); im -= y[i] * sin(w * i); } double m = re * re + im * im; sw += m; sf += m * f; }
      const double ex7 = 440 * pow(2, 7 / 12.0), cen = sf / sw;
      printf("harmony +7 voice: expected %.1f Hz, strongest line %.1f Hz, spectral centroid 580-740 Hz %.1f Hz (%+.1f cents); the line spacing is the grain-jump rate (SOLAD sidebands), the centroid is the pitch\n", ex7, hz, cen, 1200 * log2(cen / ex7));
      // latency: silence, then one click; the wet response (output minus the dry click) peaks after the shifter's read delay
      std::vector<float> w; for (int b = 0; b < 200; b++) { for (int i = 0; i < 48; i++) { r.L[i] = (b == 0 && i == 0) ? 0.5f : 0.f; r.R[i] = r.L[i]; } float dry[48]; memcpy(dry, r.L, sizeof dry); r.f->smoothBlock(); r.f->process(r.c, r.L, r.R, 48, false); for (int i = 0; i < 48; i++) w.push_back(r.L[i] - dry[i]); }
      size_t pk = 0; for (size_t i = 0; i < w.size(); i++) if (fabsf(w[i]) > fabsf(w[pk])) pk = i; size_t first = 0; for (size_t i = 0; i < w.size(); i++) if (fabsf(w[i]) > 0.1f * fabsf(w[pk])) { first = i; break; }
      printf("harmony latency: first wet energy %.1f ms after the click, peak %.1f ms (ShifterCore mean read delay 10..13 ms)\n", first / 48.0, pk / 48.0); }
    return 0;
}
