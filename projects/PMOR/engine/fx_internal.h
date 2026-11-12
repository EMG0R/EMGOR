// fx_internal.h — helpers shared by fx.cpp (host + perf throws) and the distortion set.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "../config.h"
#include "fx.h"
#include "clock.h"

static inline float fxLerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float fxTanh(float z) {                     // rational tanh, clamped
    if (z > 3.f) return 1.f;
    if (z < -3.f) return -1.f;
    float z2 = z * z; return z * (27.f + z2) / (27.f + 9.f * z2);
}
static inline float fxRingRead(const float* ring, uint32_t n, uint32_t w, float back) {  // linear-interp read `back` samples behind w
    float p = (float)w - back; while (p < 0) p += n;
    uint32_t i = (uint32_t)p; float f = p - i; uint32_t j = i + 1; if (j >= n) j -= n;
    return ring[i] + (ring[j] - ring[i]) * f;
}
static inline float fxCoefMs(float ms) { return 1.f - expf(-1.f / (0.001f * ms * SR)); }
float fxDistSample(DistAlgo a, float x, float drive);
float fxDistXfade(float x, float drive);
