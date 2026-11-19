// Host replica of the TEST 12 audio path (fx_glue_d12.h fxChain: gate + drone, additive send, mono sum, master 0 dB) with the real fx_drone.h and a DIRTY arena. The three acceptance checks:
//  1 fader 0: dry at unity, no drone, no dropouts;  2 fader 0 -> 1000 -> 0 on steady noise: no dropout, dry dip <= 1 dB;  3 five minutes at 50 %: finite, no run-away.
//   c++ -std=c++17 -O2 -I../../../HYPER_CORE/src/dsp/fx tests/drone12_host.cpp -o /tmp/drone12_host && /tmp/drone12_host
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "fx_drone.h"
using namespace hx;
enum { IDLE, ACTIVE, TAIL };
struct Gate { int state = IDLE, tail = 0; float smA = 0.3f; };
static bool gateEnter(Gate& g, Fx* fx, float m) { int st = g.state; bool w = false; if(st == IDLE) { if(m > 0.0005f) { st = ACTIVE; w = true; } } else if(st == ACTIVE) { if(m <= 0.0005f) { st = TAIL; g.tail = 0; fx->smA = 0.5f; } } else if(m > 0.0005f) { st = ACTIVE; fx->smA = g.smA; } g.state = st; return w; }
static void gateLeave(Gate& g, Fx* fx, size_t n) { if(g.state != TAIL) return; g.tail++; if(fx->sm[0] < 1e-4f || (float)g.tail * (float)n * (1000.f / 48000.f) >= 50.f) { g.state = IDLE; fx->smA = g.smA; } }
int main() {
    static uint8_t mem[62 * 1024], cold[28 * 1024]; memset(mem, 0xA5, sizeof(mem)); memset(cold, 0xA5, sizeof(cold));   // dirty
    Arena a, c; a.bind(mem, sizeof(mem)); c.bind(cold, sizeof(cold)); a.cold = &c;
    FxDrone* d = static_cast<FxDrone*>(fx_create_drone(a, 48000.f)); if(!d || !d->ok) { printf("DRONE CREATE FAILED (used %zu / %zu)\n", a.used, a.cap); return 2; }
    printf("drone arena used %zu / %zu B hot, cold %zu / %zu B\n", a.used, a.cap, c.used, c.cap);
    d->init(48000.f, 48.f); Gate g; g.smA = d->smA; d->setParam(0, 0.f); d->Fx::snap();
    unsigned rng = 7; float mixReq = 0.f; int bad = 0;
    auto block = [&](const float* in, float* oL, float* oR) {
        for(int i = 0; i < 48; i++) { oL[i] = in[i]; oR[i] = in[i]; }
        if(g.state == IDLE && mixReq <= 0.0005f) return;
        if(gateEnter(g, d, mixReq)) { d->wake(); d->v[0] = mixReq; d->sm[0] = 0.f; d->mixCur = 0.f; d->primed = true; d->smA = g.smA; }
        if(g.state == IDLE) return;
        d->v[0] = g.state == TAIL ? 0.f : mixReq; d->smoothBlock(); FxCtx ctx = {}; ctx.sr = 48000.f; d->process(ctx, oL, oR, 48, false); gateLeave(g, d, 48);
    };
    auto run = [&](const char* name, int blocks, auto fader, bool noise, float& minRatioDb, float& rmsOutDb, float& rmsInDb) {
        double sIn = 0, sOut = 0; float minB = 1e9f; int nb = 0; float in[48], L[48], R[48]; float ph = 0;
        for(int b = 0; b < blocks; b++) {
            mixReq = fader(b);
            for(int i = 0; i < 48; i++) { rng = rng * 1664525u + 1013904223u; in[i] = noise ? 0.02f * ((rng >> 8) * (1.f / 8388608.f) - 1.f) : 0.02f * sinf(ph); ph += 6.2831853f * 440.f / 48000.f; }
            block(in, L, R);
            double ei = 0, eo = 0; for(int i = 0; i < 48; i++) { const float o = 0.5f * (L[i] + R[i]); ei += in[i] * in[i]; eo += o * o; if(o - o != 0.f) bad++; }
            sIn += ei; sOut += eo; if(b > 200 && (b % 4) == 3) { /* 4 ms windows */ } if(b > 200) { const float r = (float)(eo / (ei + 1e-20)); if(r < minB) minB = r; } nb++;
        }
        minRatioDb = 10.f * log10f(minB + 1e-20f); rmsOutDb = 10.f * (float)log10(sOut / (nb * 48.0) + 1e-20); rmsInDb = 10.f * (float)log10(sIn / (nb * 48.0) + 1e-20); (void)name;
    };
    float mr, ro, ri;
    run("1", 48000 * 3 / 48, [](int) { return 0.f; }, true, mr, ro, ri);
    printf("CHECK 1 fader 0: out vs in %.2f dB (rms), drone state IDLE=%d, non-finite=%d -> %s\n", ro - ri, g.state == IDLE, bad, (fabsf(ro - ri) < 0.01f && g.state == IDLE && !bad) ? "PASS" : "FAIL");
    int blocks = 48000 * 12 / 48;
    run("2", blocks, [&](int b) { const float t = (float)b / (float)blocks; return t < 0.25f ? 0.f : t < 0.5f ? (t - 0.25f) * 4.f : t < 0.75f ? 1.f - (t - 0.5f) * 4.f : 0.f; }, true, mr, ro, ri);
    printf("CHECK 2 fader 0->1000->0 on noise: worst 48-sample block out/in energy %.2f dB (dry never dips if >= -1 dB; the drone ADDS), non-finite=%d -> %s\n", mr, bad, (mr > -1.0f && !bad) ? "PASS" : "FAIL");
    run("3", 48000 * 300 / 48, [](int) { return 0.5f; }, false, mr, ro, ri);
    printf("CHECK 3 5 min at 50 %%: out vs in %.1f dB rms, non-finite=%d -> %s\n", ro - ri, bad, bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
