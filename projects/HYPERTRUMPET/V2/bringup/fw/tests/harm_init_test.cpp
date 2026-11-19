// Host replica of the 12_drone boot path for the harmony effect: the real pool allocation order and sizes, a DIRTY arena (0xA5 fill, like the Seed's unzeroed SDRAM / AXI),
// construct + init every pool effect, then run the harmony with every interval, at silence (f0 = 0), noise and a tone, and check for finite output, exhaustion and run-away loops.
//   c++ -std=c++17 -O1 -g -fsanitize=address,undefined -I../../../HYPER_CORE/src/dsp/fx tests/harm_init_test.cpp -o /tmp/harm_init_test && /tmp/harm_init_test
#define HX_STFT_MAXN 4096
#define HX_REVERB_LITE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>
#include "fx_reverb.h"
#include "fx_grain.h"
#include "fx_tape.h"
#include "fx_harmony.h"
#include "fx_modblock.h"
#include "fx_looper.h"
using namespace hx;
int main() {
    const size_t POOL = 14u << 20; uint8_t* pool = (uint8_t*)malloc(POOL); memset(pool, 0xA5, POOL);      // dirty
    Arena A; A.bind(pool, POOL); int bad = 0;
    auto alloc = [&](size_t bytes, Arena& a) { void* m = A.allocBytes(bytes, 32); if(!m) { printf("POOL EXHAUSTED for %zu\n", bytes); bad++; return false; } a.bind(m, bytes); return true; };
    Arena a1, a2, a3, a4, a5, a6;
    // the real sizes / order of fxGlueInit (looper first would be 9.4 MB; kept: it is the biggest neighbour)
    if(alloc(9400000, a1)) { Fx* f = fx_create_looper(a1, 48000.f); printf("looper ok=%d used=%zu/%zu\n", f ? f->ok : -1, a1.used, a1.cap); if(f) f->init(48000.f, 48.f); }
    if(alloc(524288, a2)) { Fx* f = fx_create_reverb(a2, 48000.f); printf("reverb ok=%d used=%zu/%zu\n", f ? f->ok : -1, a2.used, a2.cap); if(f) f->init(48000.f, 48.f); }
    if(alloc(1100000, a3)) { Fx* f = fx_create_grain(a3, 48000.f); printf("grain ok=%d used=%zu/%zu\n", f ? f->ok : -1, a3.used, a3.cap); if(f) f->init(48000.f, 48.f); }
    if(alloc(300000, a4)) { Fx* f = fx_create_tape(a4, 48000.f); printf("tape ok=%d used=%zu/%zu\n", f ? f->ok : -1, a4.used, a4.cap); if(f) f->init(48000.f, 48.f); }
    if(alloc(200000, a5)) { Fx* f = fx_create_modblock(a5, 48000.f); printf("mod ok=%d used=%zu/%zu\n", f ? f->ok : -1, a5.used, a5.cap); if(f) f->init(48000.f, 48.f); }
    if(!alloc(20480, a6)) return 1;
    FxHarmony* h = static_cast<FxHarmony*>(fx_create_harmony(a6, 48000.f)); printf("harmony ok=%d used=%zu/%zu np=%d\n", h ? h->ok : -1, a6.used, a6.cap, h ? h->np : -1);
    if(!h || !h->ok) { printf("HARMONY CREATE FAILED\n"); return 2; }
    auto t0 = std::chrono::steady_clock::now(); h->init(48000.f, 48.f); h->setParam(0, 0.0f); h->Fx::snap();
    printf("init %.2f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    static const int semi[16] = { -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 1, 2, 5, 6, 8, 9 };
    FxCtx ctx = {}; ctx.sr = 48000.f; float L[48], R[48]; unsigned rng = 1; double worst = 0;
    for(int src = 0; src < 3; src++) {        // 0 silence (f0 = 0), 1 noise, 2 tone
        for(int p = 0; p < 16; p++) {
            h->reset(); h->setParam(0, 0.7f); h->Fx::snap(); h->voiceOn(semi[p], 1.0f); float ph = 0, sumsq = 0, sumdry = 0; int nonfinite = 0;
            for(int b = 0; b < 400; b++) {
                for(int i = 0; i < 48; i++) { rng = rng * 1664525u + 1013904223u; float x = src == 0 ? 0.f : src == 1 ? 0.05f * ((rng >> 8) * (1.f / 8388608.f) - 1.f) : 0.1f * sinf(ph); ph += 6.2831853f * 220.f / 48000.f; L[i] = R[i] = x; sumdry += x * x; }
                ctx.f0 = src == 2 ? 220.f : 0.f; ctx.conf = src == 2 ? 0.9f : 0.f;
                auto t = std::chrono::steady_clock::now(); h->smoothBlock(); h->process(ctx, L, R, 48, false);
                worst = fmax(worst, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count());
                for(int i = 0; i < 48; i++) { if(!(L[i] - L[i] == 0.f)) nonfinite++; sumsq += L[i] * L[i]; }
            }
            if(nonfinite) { printf("src %d pad %d semis %+d: NON-FINITE %d\n", src, p + 1, semi[p], nonfinite); bad++; }
            if(src == 2) printf("tone: pad %2d %+3d st  out rms %.1f dB (dry %.1f dB)\n", p + 1, semi[p], 10 * log10(sumsq / (400 * 48) + 1e-12), 10 * log10(sumdry / (400 * 48) + 1e-12));
        }
    }
    printf("worst block %.1f us (host)  bad=%d\n", worst, bad); return bad ? 3 : 0;
}
