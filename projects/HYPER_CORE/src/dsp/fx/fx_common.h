// fx_common.h -- HYPERTRUMPET V2 FX library core (HYPER_CORE/src/dsp/fx). NEW 2026-11-15.
// Portable C++17, float32, block processing, no heap in the audio path.
// Rules for every effect (see fx/README.md):
//  * process() is called once per block (n <= HX_MAXBLOCK). No virtual call per sample.
//  * Param 0 is ALWAYS "MIX" (0..1). Every effect honours it (dry/wet) or is documented as "inline" (mix=1 means fully engaged).
//  * Params live in Fx::v[]; Fx::sm[] is the block-smoothed copy (one-pole, ~25 ms). Use sm[] in DSP.
//  * Big buffers come from the slot Arena (SDRAM on the Seed). Arena is reset when the slot's effect changes.
//  * Effect declares which param each macro ROLE moves (Fx::role[]), so the mapper drives any effect in any slot.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <new>
#include "../../proto/param.h"

#if defined(HX_TARGET_SEED)
  #include "daisy_seed.h"
  #define HX_SDRAM DSY_SDRAM_BSS
#else
  #define HX_SDRAM
#endif

#define HX_MAXBLOCK 128
#define HX_MAXP     16
#define HX_NSLOTS   4

#ifndef HX_SLOT_ARENA_BYTES
#define HX_SLOT_ARENA_BYTES (12u * 1024u * 1024u)   // 4 x 12 MB = 48 MB of the Seed's 64 MB SDRAM
#endif

#ifdef __GNUC__
#define HX_RESTRICT __restrict__
#else
#define HX_RESTRICT
#endif

namespace hx {

static const float kPi    = 3.14159265358979f;
static const float kTwoPi = 6.28318530717959f;

// ---------------------------------------------------------------- arena (bump allocator, control-path only)
struct Arena {
    uint8_t* base = nullptr; size_t cap = 0, used = 0;
    Arena* cold = nullptr;   // optional second arena for cold/sequential buffers (Stft ring, overlap-add, windows): lets hot arrays sit in fast RAM and the rest in SDRAM
    void  bind(void* mem, size_t bytes) { base = (uint8_t*)mem; cap = bytes; used = 0; }
    void  reset() { used = 0; }
    void* allocBytes(size_t n, size_t align = 16) {
        size_t p = (used + (align - 1)) & ~(align - 1);
        if (p + n > cap) return nullptr;
        used = p + n; return base + p;
    }
    // zero-initialised float array; nullptr on exhaustion (effect must then degrade gracefully via ok())
    float* floats(size_t n) { float* f = (float*)allocBytes(n * sizeof(float)); if (f) memset(f, 0, n * sizeof(float)); return f; }
    template <class T> T* make() { void* m = allocBytes(sizeof(T), 16); return m ? new (m) T() : nullptr; }
    size_t left() const { return cap - used; }
};

// ---------------------------------------------------------------- per-block context from the engine (shared analysis)
struct FxCtx {
    float sr;          // sample rate
    float f0;          // tracked fundamental of the input (Hz), 0 if unvoiced / unknown
    float conf;        // 0..1 pitch confidence
    float env;         // input RMS-ish envelope 0..1 (breath proxy)
    float valvePitchHz;// pitch hint from valve combo (0 = none); set by host/mapper when known
    uint32_t blockIndex;
};

// ---------------------------------------------------------------- macro roles and actions
enum Role : uint8_t { R_AMT = 0, R_TIME, R_TONE, R_MOD, R_FB, R_PITCH, R_COUNT };
// Roles: AMT = intensity/depth/drive/wet amount (FSR); TIME = time/size/rate/length (pot1);
//        TONE = brightness/damping/filter/formant (pot2, lidar); MOD = motion/warp/mod depth (tilt);
//        FB = feedback/regen/decay; PITCH = interval/frequency/transposition (joystick, valves).
enum Action : uint8_t { A_MAIN = 0, A_ALT = 1, A_CLEAR = 2, A_HOLD = 3 };
// Meaning of actions per effect is declared in each effect's status json ("actions") and in fx_registry list.

// ---------------------------------------------------------------- base class
struct Fx {
    const char* name = "FX";
    uint8_t np = 0;
    float   v[HX_MAXP];      // user target values
    float   sm[HX_MAXP];     // smoothed (block-rate one-pole)
    Param   pt[HX_MAXP];     // descriptors (value -> v[i]) in the existing Param schema
    uint8_t role[R_COUNT];
    float   sr = 48000.f;
    float   smA = 0.3f;      // block smoothing coefficient
    bool    ok = true;       // false if the arena was too small (effect passes audio through)

    Fx() { for (int i = 0; i < HX_MAXP; i++) { v[i] = sm[i] = 0.f; } for (int r = 0; r < R_COUNT; r++) role[r] = 0xFF; }
    virtual ~Fx() {}

    // define param i. Call in ctor, in order. First must be MIX.
    void P(int i, const char* nm, float mn, float mx, float def, float step = 0.f, uint8_t fl = 0, const char* const* names = nullptr) {
        pt[i].name = nm; pt[i].value = &v[i]; pt[i].min = mn; pt[i].max = mx; pt[i].step = step; pt[i].flags = fl; pt[i].names = names;
        v[i] = sm[i] = def; if (i >= np) np = (uint8_t)(i + 1);
        noSm[i] = (fl & (PF_QUANT | PF_ENUM | PF_INT)) != 0;
    }
    void setRole(Role r, int paramIdx) { role[r] = (uint8_t)paramIdx; }
    void init(float sampleRate, float blockSize = 48.f) {
        sr = sampleRate; smA = 1.f - expf(-blockSize / (0.025f * sampleRate));   // 25 ms
    }
    void setParam(int i, float x) { if (i >= 0 && i < np) v[i] = paramClamp(pt[i], x); }
    float getParam(int i) const { return (i >= 0 && i < np) ? v[i] : 0.f; }
    // normalised role control 0..1 -> mapped onto the param range (log flag respected)
    void setRoleNorm(Role r, float n) {
        int i = role[r]; if (i == 0xFF) return;
        n = n < 0.f ? 0.f : (n > 1.f ? 1.f : n);
        const Param& p = pt[i];
        if ((p.flags & PF_LOG) && p.min > 0.f) v[i] = paramClamp(p, p.min * powf(p.max / p.min, n));
        else v[i] = paramDenorm(p, n);
    }
    void smoothBlock() {
        for (int i = 0; i < np; i++) { if (noSm[i]) sm[i] = v[i]; else sm[i] += (v[i] - sm[i]) * smA; }
    }
    void snap() { for (int i = 0; i < np; i++) sm[i] = v[i]; }
    int  findParam(const char* nm) const { for (int i = 0; i < np; i++) if (!strcmp(pt[i].name, nm)) return i; return -1; }
    ParamPage page() const { ParamPage pg; pg.title = name; pg.items = pt; pg.count = np; return pg; }

    // L/R processed in place. stereoIn=false: only L valid. Return true if output is stereo (R valid).
    virtual bool process(const FxCtx& c, float* HX_RESTRICT L, float* HX_RESTRICT R, int n, bool stereoIn) = 0;
    virtual void action(int id, float value) { (void)id; (void)value; }
    virtual void reset() {}
    bool noSm[HX_MAXP] = {};
};

// ---------------------------------------------------------------- small DSP helpers (all header-only, per-sample inline OK, no virtuals)
static inline float flush(float x) { return (x > -1e-20f && x < 1e-20f) ? 0.f : x; }          // denormal guard
static inline float finite(float x) { return (x * 0.f == 0.f) ? x : 0.f; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float softclip(float x) {   // rational tanh-like, cheap, bounded +-1
    x = clampf(x, -3.f, 3.f); float x2 = x * x; return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float cubicClip(float x) {   // no division: clamp to +-1.5, x - 4/27 x^3 (monotone, reaches +-1 at the clamp, linear for small x); for feedback loops
    x = x > 1.5f ? 1.5f : (x < -1.5f ? -1.5f : x); return x - 0.148148f * x * x * x;
}
static inline float fastTanh(float x) { return softclip(x); }
static inline uint32_t nextPow2(uint32_t v) { v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; return v + 1; }
static inline float dbToLin(float db) { return powf(10.f, db * 0.05f); }
static inline float semiToRatio(float s) { return exp2f(s * (1.f / 12.f)); }
static inline float rnd01(uint32_t& s) { s = s * 1664525u + 1013904223u; return (float)(s >> 8) * (1.f / 16777216.f); }

// parabolic sine approx, phase 0..1 -> -1..1 (error < 1e-3), cheap LFO
static inline float sinPar(float ph) {
    float x = ph - (float)(int)ph; if (x < 0.f) x += 1.f;
    x = x * 2.f - 1.f;                       // -1..1
    float y = 4.f * x * (1.f - (x < 0.f ? -x : x));   // parabola
    // refine (Bhaskara-ish extra precision)
    float a = y < 0.f ? -y : y; y = 0.225f * (y * a - y) + y;
    return -y;                                // sign so that sinPar(0.25)=+1
}

struct Phasor { float ph = 0.f; float inc = 0.f; void set(float hz, float sr) { inc = hz / sr; }
    inline float tick() { ph += inc; if (ph >= 1.f) ph -= 1.f; return ph; } };

// DC blocker
struct DcBlock { float x1 = 0, y1 = 0; float R = 0.995f;
    inline float tick(float x) { float y = x - x1 + R * y1; x1 = x; y1 = flush(y); return y; } };

// per-block linear ramp helper for dry/wet etc: value goes from `prev` to `target` across n samples


// one-pole lowpass / highpass (per-sample)
struct OnePoleLP { float y = 0, a = 1;
    void setHz(float hz, float sr) { a = 1.f - expf(-kTwoPi * hz / sr); }
    inline float tick(float x) { y += (x - y) * a; return y; } };

// TPT state-variable filter (Zavalishin), stable under fast modulation
struct Svf { float ic1 = 0, ic2 = 0, g = 0.1f, k = 1.f, a1 = 0, a2 = 0, a3 = 0;
    void set(float hz, float q, float sr) {
        if (hz > sr * 0.45f) hz = sr * 0.45f; if (hz < 10.f) hz = 10.f;
        g = tanf(kPi * hz / sr); k = 1.f / (q < 0.3f ? 0.3f : q);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    // returns lowpass; band and high via out params
    inline void tick(float x, float& lp, float& bp, float& hp) {
        float v3 = x - ic2; float v1 = a1 * ic1 + a2 * v3; float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2; lp = v2; bp = v1; hp = x - k * v1 - v2; } };

// ---------------------------------------------------------------- delay line: power-of-two ring in arena, Hermite 4-point read
struct DelayLine {
    float* buf = nullptr; uint32_t mask = 0, w = 0;
    bool init(Arena& a, uint32_t maxSamples) {
        uint32_t sz = nextPow2(maxSamples + 4); buf = a.floats(sz); mask = sz - 1; w = 0; return buf != nullptr; }
    inline void write(float x) { buf[w] = x; w = (w + 1) & mask; }
    // delay in samples (>= 1 for after-write reads), fractional, Hermite interpolation
    inline float read(float d) const {
        float fi = floorf(d); float f = d - fi; uint32_t i = (uint32_t)fi;
        uint32_t p = w - 1 - i;
        float y0 = buf[(p + 1) & mask], y1 = buf[p & mask], y2 = buf[(p - 1) & mask], y3 = buf[(p - 2) & mask];
        float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0; }
    inline float readInt(uint32_t d) const { return buf[(w - 1 - d) & mask]; }
    uint32_t size() const { return mask + 1; }
};

// ---------------------------------------------------------------- dry/wet block crossfade (equal-ish, linear over the block, no zipper)
struct DryWet {
    float cur = 1.f; bool primed = false;
    // out = dry*(1-m) + wet*m, m ramped from previous block's value
    inline void apply(float* HX_RESTRICT out, const float* HX_RESTRICT dry, const float* HX_RESTRICT wet, int n, float target) {
        if (!primed) { cur = target; primed = true; }
        float inc = (target - cur) / (float)n, m = cur;
        for (int i = 0; i < n; i++) { out[i] = dry[i] + (wet[i] - dry[i]) * m; m += inc; }
        cur = target; }
};

} // namespace hx
