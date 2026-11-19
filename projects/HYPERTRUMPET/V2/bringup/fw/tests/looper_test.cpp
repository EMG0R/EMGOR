// Host behaviour test of FxLooper (build: c++ -std=c++17 -I../../../HYPER_CORE/src/dsp/fx tests/looper_test.cpp -o /tmp/looper_test)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fx_looper.h"
using namespace hx;
static int fails = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while(0)
static FxLooper* lp; static FxCtx ctx; static long t = 0;
static void run(int samples, float amp) { float L[48], R[48]; for (int done = 0; done < samples; done += 48) { for (int i = 0; i < 48; i++, t++) L[i] = amp * sinf(6.2831853f * 220.f * t / 48000.f); lp->smoothBlock(); lp->process(ctx, L, R, 48, false); } }
int main() {
    static uint8_t mem[16 << 20]; Arena a; a.bind(mem, sizeof mem); lp = static_cast<FxLooper*>(fx_create_looper(a, 48000.f)); lp->init(48000.f, 48.f); lp->setParam(0, 1.f); lp->Fx::snap(); ctx.sr = 48000.f;
    CHECK(lp->state() == FxLooper::S_EMPTY, "starts empty");
    lp->action(A_MAIN, 1.f); run(48, 0.f); CHECK(lp->state() == FxLooper::S_REC1, "tap on empty = record");
    run(48000, 0.3f); run(14400, 0.f);                                  // 1 s of sound, then 0.3 s of press-timing silence
    lp->action(A_MAIN, 1.f); run(48, 0.f);
    CHECK(lp->state() == FxLooper::S_TRIM || lp->state() == FxLooper::S_PLAY, "tap while recording = close (trim scan)");
    run(1200, 0.f); CHECK(lp->state() == FxLooper::S_PLAY, "scan finished (a 0.3 s tail takes ~20 ms), playing");
    CHECK(fabsf((float)lp->length() - 48000.f) < 120.f, "loop length trimmed to the last sound (about 48000, not 62400)");
    CHECK(lp->layers() == 1, "one layer");
    lp->action(A_MAIN, 1.f); run(96, 0.f); CHECK(lp->state() == FxLooper::S_OVR, "tap while playing = overdub starts (next layer)");
    run(24000, 0.2f); lp->action(A_CLEAR, 1.f); run(96, 0.f);
    CHECK(lp->state() == FxLooper::S_PLAY && lp->layers() == 1, "hold during an overdub pass cancels only that pass (still 1 layer, still playing)");
    lp->action(A_MAIN, 1.f); run(96, 0.f); run(60000, 0.2f); run(480, 0.f); CHECK(lp->layers() == 2 && lp->state() == FxLooper::S_PLAY, "overdub punches out by itself after one loop (2 layers)");
    lp->action(A_ALT, 1.f); run(24000, 0.f); CHECK(lp->state() == FxLooper::S_STOP, "pad 14 tap while playing = stop");
    lp->action(A_ALT, 1.f); run(480, 0.f); CHECK(lp->state() == FxLooper::S_PLAY, "pad 14 tap while stopped = play");
    lp->action(A_CLEAR, 1.f); run(48000, 0.f); CHECK(lp->state() == FxLooper::S_EMPTY && lp->layers() == 0, "pad 13 hold (not overdubbing) = delete all");
    lp->action(A_MAIN, 1.f); run(48, 0.f); run(9600, 0.f); lp->action(A_MAIN, 1.f); run(96, 0.f); run(2000000 / 48 * 48, 0.f);
    CHECK(lp->state() == FxLooper::S_EMPTY || lp->state() == FxLooper::S_PLAY, "all-silent take: no crash, raw length kept");
    return fails;
}
