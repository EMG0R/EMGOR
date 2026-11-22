// Host check: BLOOPER slice pitch vs SPEEDMUL (joystick). 440 Hz tone in, PROB 1, DIST 0, SIZE 0.5; for SPEEDMUL = -1, 0, +1 find the slices (output deviating from the ducked dry) and measure their pitch by zero crossings.
//   c++ -std=c++17 -O2 -I../../../HYPER_CORE/src/dsp/fx tests/blooper_pitch_test.cpp -o /tmp/blp_pitch && /tmp/blp_pitch
#define HX_BLP_SECONDS 20
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "fx_blooper.h"
using namespace hx;
int main() {
    std::vector<uint8_t> mem(20u << 20, 0xA5); Arena a; a.bind(mem.data(), mem.size());
    for(int pass = 0; pass < 3; pass++) {
        const float sp = pass == 0 ? -1.f : pass == 1 ? 0.f : 1.f;
        Arena aa; aa.bind(mem.data(), mem.size()); FxBlooper* b = static_cast<FxBlooper*>(fx_create_blooper(aa, 48000.f)); b->init(48000.f, 48.f);
        b->setParam(FxBlooper::P_MIX, 1.f); b->setParam(FxBlooper::P_PROB, 1.f); b->setParam(FxBlooper::P_DIST, 0.f); b->setParam(FxBlooper::P_SIZE, 0.5f); b->Fx::snap();
        FxCtx ctx = {}; ctx.sr = 48000.f; float L[48], R[48], ph = 0; std::vector<float> out, ref;
        for(int blk = 0; blk < 48000 * 14 / 48; blk++) {
            for(int i = 0; i < 48; i++) { L[i] = R[i] = 0.1f * sinf(ph); ph += 6.2831853f * 440.f / 48000.f; }
            if(blk == 48000 * 4 / 48) b->action(A_MAIN, 1.f);            // on after 4 s of capture
            if(blk >= 48000 * 4 / 48) b->v[FxBlooper::P_SPEED] = sp;
            for(int i = 0; i < 48; i++) ref.push_back(L[i]);
            b->smoothBlock(); b->process(ctx, L, R, 48, false); for(int i = 0; i < 48; i++) out.push_back(L[i]);
        }
        // measure: zero-crossing rate in 20 ms windows after the on-point; report the distribution of window frequencies
        int bins[8] = {0}; const char* nm[8] = {"<0.4x", "0.4-0.7x(0.5x)", "0.7-0.9x", "0.9-1.1x(1x)", "1.1-1.6x", "1.6-2.4x(2x)", ">2.4x", ""};
        for(size_t s0 = 48000 * 5; s0 + 960 < out.size(); s0 += 960) {
            int zc = 0; for(size_t i = s0 + 1; i < s0 + 960; i++) if((out[i - 1] < 0) != (out[i] < 0)) zc++;
            float f = zc * 0.5f / 0.02f; float r = f / 440.f; int k = r < 0.4f ? 0 : r < 0.7f ? 1 : r < 0.9f ? 2 : r < 1.1f ? 3 : r < 1.6f ? 4 : r < 2.4f ? 5 : 6; bins[k]++;
        }
        printf("SPEEDMUL %+.0f: rate=%.3f  windows by pitch ratio:", sp, b->rate()); for(int k = 0; k < 7; k++) printf("  %s=%d", nm[k], bins[k]); printf("\n");
    }
}
