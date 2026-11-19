// Host replica of the FREEZE pad path (padfx fx_glue_min.h axConstruct slot + fx_controls_min.h gate / reset): create in a 96 KB hot + cold arena, gate open on a -30 dBFS tone for 1 s, gate closed + tone off (frozen hold level),
// then the 300 ms FREEZE-factor clear ramp (100 -> 0), as the firmware does it.
//   c++ -std=c++17 -O2 -I../../../HYPER_CORE/src/dsp/fx tests/freeze_host_test.cpp -o /tmp/fzh && /tmp/fzh
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fx_freeze.h"
using namespace hx;
int main() {
    static uint8_t hot[96 * 1024], cold[320 * 1024]; memset(hot, 0xA5, sizeof(hot)); memset(cold, 0xA5, sizeof(cold)); Arena a, c; a.bind(hot, sizeof(hot)); c.bind(cold, sizeof(cold)); a.cold = &c;
    FxFreeze* f = static_cast<FxFreeze*>(fx_create_freeze(a, 48000.f)); printf("freeze create ok=%d hot %zu B cold %zu B\n", f ? f->ok : -1, a.used, c.used); if(!f || !f->ok) return 2;
    f->init(48000.f, 48.f); const int iF = f->findParam("FREEZE"), iM = 0; f->setParam(iM, 0.9f); f->setParam(f->findParam("GAIN"), 2.0f); f->Fx::snap();   // the firmware default: GAIN 2.0 f->reset();
    FxCtx ctx = {}; ctx.sr = 48000.f; float L[48], R[48], ph = 0; auto blockdb = [&](bool tone, double& rms) { double e = 0; for(int b = 0; b < 48000 / 48; b++) { for(int i = 0; i < 48; i++) { L[i] = R[i] = tone ? 0.0316f * sinf(ph) : 0.f; ph += 6.2831853f * 440.f / 48000.f; } f->smoothBlock(); f->process(ctx, L, R, 48, false); for(int i = 0; i < 48; i++) e += L[i] * L[i]; } rms = 10 * log10(e / 48000.0 + 1e-20); };
    double r; f->action(A_MAIN, 1.f); blockdb(true, r); printf("gate open, tone -30 dBFS 1 s: output rms %.1f dBFS\n", r);
    f->action(A_MAIN, 0.f); double hold = 0; blockdb(false, hold); blockdb(false, hold); printf("gate closed, tone off (frozen hold): output rms %.1f dBFS -> %s (wet above -30 dB expected for a -30 dBFS tone in)\n", hold, hold > -33.f ? "AUDIBLE" : "TOO QUIET");
    for(int b = 0; b < 300 / 1; b++) { f->v[iF] = 100.f * (1.f - (float)b / 300.f); for(int i = 0; i < 48; i++) { L[i] = R[i] = 0; } f->smoothBlock(); f->process(ctx, L, R, 48, false); }
    f->v[iF] = 0.f; double e = 0; for(int b = 0; b < 100; b++) { for(int i = 0; i < 48; i++) { L[i] = R[i] = 0; } f->smoothBlock(); f->process(ctx, L, R, 48, false); for(int i = 0; i < 48; i++) e += L[i] * L[i]; } printf("after the 300 ms clear ramp: output rms %.1f dBFS\n", 10 * log10(e / 4800.0 + 1e-20));
}
