// fx_phasecloud.h -- per-bin random-phase generator for the chill spectral effects (FREEZE, BLUR). Paulstretch-style, own code (2026-11-17).
// Each bin owns a unit phasor z_k. Per STFT frame (hop = N/2, sqrt-Hann synthesis) it advances by
//     z_k <- z_k * (-1)^k * (+-j) * R(drift)
// (-1)^k is the bin-centre advance for a half-frame hop (pi*k); (+-j) is a fixed per-bin random +-90 degrees: consecutive frames of one bin are in QUADRATURE, so the
// overlap-added sqrt-Hann frames are power complementary (sin^2 + cos^2 = 1) and the texture has no hop-rate amplitude ripple (a perfectly coherent sinusoid would show
// the sqrt-Hann sum's 3 dB ripple; random phase per frame is what Paulstretch uses for the same reason). The +-90 degrees detunes each bin by +-1/(4*hop) (5.8 Hz at N = 2048
// @ 24 kHz): half the bins go up, half down, so a frozen partial becomes a slow shimmer doublet, not a net detune. R(drift) is a random rotation uniform in
// +-drift*pi: DRIFT = 1 is fully random phase every frame (classic Paulstretch noise), small DRIFT is a slow random walk (smooth, chorusing). Cost per bin per frame: one LCG,
// one table lookup, ~10 flops; no trig, no sqrt.
#pragma once
#include "fx_common.h"
namespace hx {
struct PhaseCloud {
    static const int TAB = 256;
    inline static float tc[TAB], ts[TAB]; inline static bool tabOk = false;
    float *zr = nullptr, *zi = nullptr, *sg = nullptr; uint32_t seed = 0x9E3779B9u;
    static void initTab() { if (tabOk) return; for (int i = 0; i < TAB; i++) { tc[i] = (float)cos(6.283185307179586 * i / TAB); ts[i] = (float)sin(6.283185307179586 * i / TAB); } tabOk = true; }
    bool alloc(Arena& a, int nb) { initTab(); zr = a.floats(nb); zi = a.floats(nb); sg = a.floats(nb); return zr && zi && sg; }
    inline uint32_t rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
    void seedBins(int nb) {             // random starting phases and a random +-90 degree quadrature sign per bin (control path / resize)
        for (int k = 0; k < nb; k++) { const uint32_t r = rnd(); const int i = (int)(r >> 24); zr[k] = tc[i]; zi[k] = ts[i]; sg[k] = (r & 0x10000u) ? 1.f : -1.f; }
    }
    // advance bin k by one frame; dq = drift * 128 (0..128); result in zr[k], zi[k]
    inline void step(int k, int dq) {
        const uint32_t r = rnd(); const int off = (dq == 0) ? 0 : (int)((int)((r >> 20) & 255) - 128) * dq >> 7; const int idx = off & (TAB - 1);
        const float c = tc[idx], s = ts[idx], q = (k & 1) ? -sg[k] : sg[k];          // (-1)^k * (+-j) as: z * j*q = (-zi*q, zr*q)
        const float xr = -zi[k] * q, xi = zr[k] * q;
        float nr = xr * c - xi * s, ni = xr * s + xi * c; const float g = 1.5f - 0.5f * (nr * nr + ni * ni);
        zr[k] = nr * g; zi[k] = ni * g;
    }
};
}
