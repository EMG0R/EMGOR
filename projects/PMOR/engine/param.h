// param.h — one descriptor type drives the universal 1-D / 2-D editor, the OLED lists,
// and patch serialisation. Every user-facing knob in the instrument is a Param.
#pragma once
#include <stdint.h>

enum ParamFlags : uint8_t {
    PF_NONE      = 0,
    PF_QUANT     = 1 << 0,   // snap to integer steps of `step`
    PF_PAIR_X    = 1 << 1,   // this param is the X of a 2-D pair; the next Param in the list is Y
    PF_PERCENT   = 1 << 2,   // display as %
    PF_ENUM      = 1 << 3,   // display names[(int)value]
    PF_BIPOLAR   = 1 << 4,   // centre-zero slider
    PF_HIDDEN    = 1 << 5,   // stored but not shown
    PF_INT       = 1 << 6,   // display as a plain integer count
};

struct Param {
    const char*  name;      // ≤ 8 chars, upper-case, fits a 128-px title
    float*       value;     // lives in the owning engine's state struct (patch-serialised there)
    float        min, max;
    float        step;      // quantum for PF_QUANT, else fine-step for encoder nudge
    uint8_t      flags;
    const char* const* names;  // PF_ENUM only
};

// A page = contiguous Params. PF_PAIR_X marks a 2-D pair (x then y).
struct ParamPage {
    const char*  title;
    const Param* items;
    uint8_t      count;
};

// helpers (inline, header-only)
static inline float paramClamp(const Param& p, float v) {
    if (v < p.min) v = p.min;
    if (v > p.max) v = p.max;
    if (p.flags & PF_QUANT) {
        float q = p.step > 0 ? p.step : 1.f;
        v = q * (float)(int)((v / q) + (v >= 0 ? 0.5f : -0.5f));
    }
    return v;
}
static inline float paramNorm(const Param& p, float v)   { return (v - p.min) / (p.max - p.min); }
static inline float paramDenorm(const Param& p, float n) { return paramClamp(p, p.min + n * (p.max - p.min)); }
