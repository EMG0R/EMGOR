// Host test of the toggle-on path with a steady tone: a copy of the glue's fxgRun semantics (fx_glue_tmpl.h): wake (sm[0] = 0, v[0] = mix, 15 ms blend), then the dry hold
// (out += dry x max(0, sm[0] - 0.5)) for crossfade-MIX effects; harmony and looper are additive and get no hold. L/R RMS over a sliding 20 ms window vs the pre-toggle level (a tone with vibrato).
// Build (from bringup/fw): c++ -std=c++17 -O2 -DHX_REVERB_LITE -I../../../HYPER_CORE/src/dsp/fx tests/toggle_test.cpp -o /tmp/toggle_test
#define HX_STFT_MAXN 4096
#include <stdio.h>
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
static double rms(const float* a, const float* b, int n) { double s = 0; for (int i = 0; i < n; i++) s += a[i] * a[i] + b[i] * b[i]; return sqrt(s / (2 * n)); }
int main() {
    struct E { const char* n; Mk mk; bool additive; } es[] = { {"harm", fx_create_harm, false}, {"looper", fx_create_looper, true}, {"reverb", fx_create_reverb, false}, {"grain", fx_create_grain, false}, {"delay", fx_create_delay, false}, {"flange", fx_create_flanger, false}, {"chorus", fx_create_chorus, false}, {"mood", fx_create_mood, false}, {"freeze", fx_create_freeze, false}, {"blur", fx_create_blur, false}, {"roar", fx_create_roar, false}, {"ring", fx_create_ringmod, false}, {"wah", fx_create_wah, false}, {"autopan", fx_create_autopan, false}, {"harmony", fx_create_harmony, true} };
    printf("%-8s %-22s %-22s %s\n", "effect", "dip on (dB, 2 s)", "dip on, no hold (dB)", "dip off (dB, 1 s)");
    for (int hold = 1; hold >= 0; hold--) for (auto& e : es) {
        Arena a; a.bind(mem.data(), mem.size()); memset(mem.data(), 0, 4 << 20); Fx* f = e.mk(a, 48000.f); f->init(48000.f, 48.f);
        const float user = f->v[0]; f->setParam(0, 0.f); f->Fx::snap(); float gateA = f->smA; if (e.additive == false && !strcmp(e.n, "freeze")) {}
        FxCtx c = {}; c.sr = 48000; c.f0 = 440; c.conf = 1; c.env = 0.5f; if (!strcmp(e.n, "harmony")) { static_cast<FxHarmony*>(f)->setSlot(0, 7); static_cast<FxHarmony*>(f)->setSlot(1, 4); }
        float L[48], R[48], hL[48], hR[48], dL[48], dR[48]; long blk = 0; const float amp = 0.1259f;                      // -18 dBFS
        static double ph = 0; auto fill = [&]() { for (int i = 0; i < 48; i++) { double t = (double)(blk * 48 + i) / 48000.0; ph += 6.2831853 * 440.0 * (1.0 + 0.03 * sin(6.2831853 * 4.0 * t)) / 48000.0; L[i] = amp * (float)sin(ph); R[i] = L[i]; } blk++; };   /* 440 Hz with 4 Hz +-3 % vibrato so delayed copies do not null the dry steadily */
        double base = 0; bool on = false; float fade = 1.f; int tOn = -1; double minOn = 1e9, minOff = 1e9; bool wasSt = false;
        for (int b = 0; b < 4800; b++) {                                   // 1 s baseline (effect off, dry passes), 2 s on, 1 s off
            fill(); const int t = b;
            if (t == 1000) { on = true; tOn = t; f->v[0] = (!strcmp(e.n, "looper") ? 1.f : user); f->sm[0] = 0.f; f->smA = gateA; fade = 0.f; }
            if (t == 3000) { on = false; f->smA = 0.5f; }
            if (on || (t > 3000 && f->sm[0] > 1e-4f && t < 4000)) {
                f->v[0] = on ? user : 0.f; f->smoothBlock();
                memcpy(dL, L, sizeof L); memcpy(hL, L, sizeof L); memcpy(hR, R, sizeof R);
                f->process(c, L, R, 48, false);
                if (hold && !e.additive) { float mm = f->sm[0], k = mm > 0.5f ? mm - 0.5f : 0.f; if (k > 0) for (int i = 0; i < 48; i++) { L[i] += hL[i] * k; R[i] += hR[i] * k; } }
                if (fade < 1.f) { float g0 = fade, g1 = g0 + 48.f / (0.015f * 48000.f); if (g1 > 1.f) g1 = 1.f; float x = g0, inc = (g1 - g0) / 48.f; for (int i = 0; i < 48; i++) { L[i] = dL[i] + (L[i] - dL[i]) * x; R[i] = dL[i] + (R[i] - dL[i]) * x; x += inc; } fade = g1; }
            }
            static double ring[20]; double r1 = rms(L, R, 48); ring[t % 20] = r1 * r1; double ss = 0; for (int q = 0; q < 20; q++) ss += ring[q]; double r = t >= 20 ? sqrt(ss / 20.0) : r1;   /* 20 ms RMS */
            if (t >= 500 && t < 1000) base += r / 500.0;
            if (t >= 1000 && t < 3000 && r < minOn) minOn = r; if (t >= 3000 && t < 4000 && r < minOff) minOff = r;
        }
        double dOn = 20 * log10((minOn + 1e-12) / base), dOff = 20 * log10((minOff + 1e-12) / base);
        if (hold) printf("%-8s %+7.1f%s", e.n, dOn, e.additive ? " (additive)   " : "              "); else printf("   %+7.1f   %+7.1f\n", dOn, dOff);
        (void)wasSt;
    }
    return 0;
}
