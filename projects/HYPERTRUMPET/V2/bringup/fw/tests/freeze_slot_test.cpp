// Host: how much HOT (AXI slot) and COLD (SDRAM) arena do freeze / trace need now (phase-cloud freeze)? Builds each with a dirty hot arena of 40 KB / 64 KB / 96 KB and a 320 KB cold arena.
//   c++ -std=c++17 -O1 -I../../../HYPER_CORE/src/dsp/fx tests/freeze_slot_test.cpp -o /tmp/fz && /tmp/fz
#define HX_STFT_MAXN 4096
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fx_freeze.h"
#include "fx_trace.h"
using namespace hx;
int main() {
    const size_t hots[3] = { 40 * 1024, 64 * 1024, 96 * 1024 }; static uint8_t hot[96 * 1024], cold[320 * 1024];
    for(int w = 0; w < 2; w++) for(int h = 0; h < 3; h++) {
        memset(hot, 0xA5, sizeof(hot)); memset(cold, 0xA5, sizeof(cold)); Arena a, c; a.bind(hot, hots[h]); c.bind(cold, sizeof(cold)); a.cold = &c;
        Fx* f = w == 0 ? fx_create_freeze(a, 48000.f) : fx_create_trace(a, 48000.f);
        printf("%s hot %3zu KB: created=%d ok=%d  hot used %zu B  cold used %zu B\n", w == 0 ? "freeze" : "trace ", hots[h] / 1024, f != nullptr, f ? f->ok : -1, a.used, c.used);
    }
}
