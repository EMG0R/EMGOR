// test_core_synth.cpp -- ch2 synth notes must make sound through the SAME path pmor.core~ uses (max/pmor_hostctl.h exact-pitch note path + pmorRenderBlock).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <initializer_list>
#include "config.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "params.h"
#include "render.h"
#include "../max/pmor_hostctl.h"
static int passes = 0, fails = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void boot() {
    memset(&g, 0, sizeof(g)); clockInit(120.f); seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit(); pmorSchemaInit();
}
static float renderPeak(double seconds) {
    float in0[BLOCK] = {0}, L[BLOCK], R[BLOCK], pk = 0; int blocks = (int)(seconds * 48000 / BLOCK);
    for (int b = 0; b < blocks; b++) { pmorRenderBlock(in0, L, R, BLOCK); for (int i = 0; i < (int)BLOCK; i++) { float a = fabsf(L[i]) > fabsf(R[i]) ? fabsf(L[i]) : fabsf(R[i]); if (!(a == a)) return -1.f; if (a > pk) pk = a; } }
    return pk;
}
// FX automation loop (all steps, mask 0x0F0F) planted / recorded with and without the host flag.
static uint16_t fxLoopRun(bool notesOnly, bool plant, bool rec, bool* autoEnabled) {
    boot(); g_render_notes_only = notesOnly;
    if (plant) { for (int i = 0; i < MAXSTEPS; i++) for (int b = 0; b < FX_BANKS; b++) fx.auto_mask[i][b] = 0x0F0F; fx.auto_enabled = true; }
    if (rec) { fx.auto_rec = true; fx.held[1] = 0x0003; }
    pmorhc::State hs; pmorhc::synOn(hs, 60, 0.f, 0.8f);
    renderPeak(0.5);
    if (autoEnabled) *autoEnabled = fx.auto_enabled;
    uint16_t e = fxEffectiveBank(0);
    g_render_notes_only = false;
    return e;
}
int main() {
    for (int algo = 0; algo < 8; algo++) {
        for (int note : {36, 48, 60, 72, 84, 96, 108}) {
            boot(); pmorParamSet(pmorParamId(PD_SYNTH, 256), (float)algo);
            pmorhc::State hs; pmorhc::synOn(hs, note, 0.f, 100 / 127.f); const int pad = hs.padOf[note];
            float pk = renderPeak(0.4);
            CHECK(pk > 0.001f, "algo %d note %d (pad %d): peak %.6f (silent)", algo, note, pad, pk);
            pmorhc::synOff(hs, note); renderPeak(0.2);
        }
    }
    {   // FX loops never play or record on the Mac (host flag g_render_notes_only); firmware default (false) still plays and records them
        CHECK(fxLoopRun(true, true, false, nullptr) == 0, "notes-only: a planted FX automation loop is not replayed");
        CHECK(fxLoopRun(false, true, false, nullptr) == 0x0F0F, "control: firmware (flag false) replays it");
        bool en = true; fxLoopRun(true, false, true, &en); CHECK(!en, "notes-only: the recording gesture records no FX loop");
        bool en2 = false; fxLoopRun(false, false, true, &en2); CHECK(en2, "control: firmware still records an FX loop");
        boot(); g_render_notes_only = true; pmorParamSet(pmorParamId(PD_FX, 3), 1.f); CHECK(fxIsHeld(3), "notes-only: live engage (param / ch5) still engages the effect"); g_render_notes_only = false;
    }
    printf("test_core_synth: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
