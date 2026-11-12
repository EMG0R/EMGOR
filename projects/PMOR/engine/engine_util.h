// engine_util.h — small header-only DSP helpers shared by the engines.
// Self-contained: only <stdint.h>, <math.h> and config.h. Everything here is
// AUDIO-plane safe (no allocation, no double, no divisions in the hot path;
// the few divisions live in the set*() functions that run at control rate).
//
// Reminder: any buffer > 8 KB belongs in SDRAM:
//     static float SDRAM_BSS bigbuf[N];      // and memset() it in your init()
// SDRAM is NOT zeroed at reset, so .sdram_bss objects start with garbage.
#pragma once
#include <stdint.h>
#include <math.h>
#include "../config.h"

// ---------------------------------------------------------------- randomness
static inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
static inline float    lcg01(uint32_t& s) { return (float)(lcg(s) >> 8) * (1.f / 16777216.f); }   // 0..1

// ---------------------------------------------------------------- NaN / denormal flush
static inline float flushBad(float x) { return (x * 0.f == 0.f) ? x : 0.f; }   // NaN and inf → 0
static inline float flushDenorm(float x) { return (x > -1e-15f && x < 1e-15f) ? 0.f : x; }

// ---------------------------------------------------------------- fast exp / pow2
// 2^x with ~0.1 % error; valid for x in about -126..126. Branch-light, no floor().
static inline float fastPow2(float x) {
    if (x < -126.f) x = -126.f;
    if (x >  126.f) x =  126.f;
    float fi = (float)(int)x;
    if (x < fi) fi -= 1.f;                  // floor for negatives
    float f  = x - fi;                      // 0..1
    float p  = 1.f + f * (0.6951786f + f * (0.2261698f + f * 0.0786516f));
    union { uint32_t u; float f; } bits;
    bits.u = (uint32_t)((int)fi + 127) << 23;
    return bits.f * p;
}
static inline float fastExp(float x) { return fastPow2(x * 1.44269504f); }
// semitone → ratio, dB → linear (control-rate friendly, ISR-safe)
static inline float semisToRatio(float semis) { return fastPow2(semis * (1.f / 12.f)); }
static inline float dbToLin(float db)         { return fastPow2(db * (1.f / 6.0206f)); }

// ---------------------------------------------------------------- one-pole smoother
// y += (x - y) * a. setMs(ms) picks `a` for a 63 % step response in `ms` at the
// given update rate (per-sample by default, or BLOCK_HZ for block-rate use).
struct OnePole {
    float y = 0.f, a = 1.f;
    void  setMs(float ms, float rate_hz = SR) {
        float n = ms * 0.001f * rate_hz;
        a = (n > 0.5f) ? (1.f - expf(-1.f / n)) : 1.f;
    }
    void  set(float v)     { y = v; }
    float process(float x) { y += (x - y) * a; return y; }
    float value() const    { return y; }
};

// ---------------------------------------------------------------- ADSR
// Linear attack, exponential decay/release (reach the target within ~3 time
// constants — the times you set are "audible" times, i.e. -60 dB / 95 %).
// retrigger() restarts the attack from the CURRENT level: no discontinuity.
struct ADSR {
    enum Stage : uint8_t { IDLE = 0, ATT, DEC, SUS, REL };
    float   level = 0.f;
    float   a_inc = 1.f, d_coef = 1.f, r_coef = 1.f, sus = 1.f;
    uint8_t stage = IDLE;

    // times in seconds; s is the sustain level 0..1
    void setTimes(float a, float d, float s, float r) {
        a_inc  = (a > 1e-4f) ? (1.f / (a * SR)) : 1.f;
        d_coef = (d > 1e-4f) ? (1.f - expf(-3.f / (d * SR))) : 1.f;
        r_coef = (r > 1e-4f) ? (1.f - expf(-3.f / (r * SR))) : 1.f;
        sus    = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
    }
    // Multiply every settable time (attack/decay/release) by k in place,
    // re-derived exactly from the coefficients already stored (no seconds
    // kept around). Since d_coef/r_coef = 1-exp(-3/(T*SR)), scaling T by k
    // is (1-coef)' = (1-coef)^(1/k); a_inc (= 1/(T*SR)) simply divides by k.
    // Control-rate only (e.g. drum "length ratio" applied once per hit) —
    // not for the per-sample process() loop.
    void  scaleTimes(float k) {
        if (k < 0.02f) k = 0.02f;
        a_inc  = a_inc / k;
        float ik = 1.f / k;
        d_coef = 1.f - powf(1.f - d_coef, ik);
        r_coef = 1.f - powf(1.f - r_coef, ik);
    }
    void  gate(bool on)     { if (on) retrigger(); else release(); }
    void  retrigger()       { stage = ATT; }          // continues from `level`
    void  release()         { if (stage != IDLE) stage = REL; }
    void  kill()            { stage = IDLE; level = 0.f; }
    bool  active() const    { return stage != IDLE; }
    bool  sustaining() const{ return stage == ATT || stage == DEC || stage == SUS; }
    float process() {
        switch (stage) {
            case ATT:
                level += a_inc;
                if (level >= 1.f) { level = 1.f; stage = DEC; }
                break;
            case DEC:
                level += (sus - level) * d_coef;
                if (level - sus < 0.001f) { level = sus; stage = SUS; }
                break;
            case SUS:
                level = sus;
                break;
            case REL:
                level += (0.f - level) * r_coef;
                if (level < 0.0002f) { level = 0.f; stage = IDLE; }
                break;
            default:
                level = 0.f;
                break;
        }
        return level;
    }
    // block-rate variant: advance n samples at once (cheap gate rendering)
    float processBlock(uint32_t n) {
        float v = level;
        for (uint32_t i = 0; i < n; i++) v = process();
        return v;
    }
};

// ---------------------------------------------------------------- peak follower
// Instant attack, exponential release. Call per sample with |x| or use block().
struct Env {
    float y = 0.f, rel = 0.999f;
    void  setReleaseMs(float ms, float rate_hz = SR) {
        float n = ms * 0.001f * rate_hz;
        rel = (n > 0.5f) ? expf(-1.f / n) : 0.f;
    }
    float process(float x) {
        float u = x < 0.f ? -x : x;
        y = (u > y) ? u : y * rel;
        return y;
    }
    // feed a whole block's peak (rel must then be set with rate_hz = BLOCK_HZ)
    float block(float peak) { y = (peak > y) ? peak : y * rel; return y; }
    float value() const { return y; }
};

// ---------------------------------------------------------------- misc
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t)    { return a + (b - a) * t; }
static inline int   popcount16(uint16_t m) { return __builtin_popcount((unsigned)m); }
