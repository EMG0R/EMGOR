// palette.cpp — colour constants from research/palette.md (exact RGB triples) + Rgb helpers.
#include "palette.h"

// ---- 1 · FX families
const Rgb C_UP      = {  0,225,255};
const Rgb C_RIGHT   = {255, 90,200};
const Rgb C_DOWN    = {255,225, 60};
const Rgb C_LEFT    = { 40,235,170};
const Rgb C_HOME    = {255,200,230};

// ---- 2 · Engine identities
const Rgb C_DRUMS   = {255,100,190};
const Rgb C_SYNTH   = { 60,200,255};
const Rgb C_SAMPLER = {  0,235,180};
const Rgb C_PATCHES = {255,215, 65};
const Rgb C_GLOBALS = {110,170,255};
const Rgb C_DASH    = {185,120,240};

// ---- 3 · Drum quadrant tints
const Rgb C_KICKS   = {255, 80,180};
const Rgb C_SNARES  = {255,170,215};
const Rgb C_HATS    = {255,230, 80};
const Rgb C_PERC    = { 60,220,255};

// ---- 4 · Functional states
const Rgb C_FOCUS   = {255,245,250};
const Rgb C_FILLED  = {170,255,255};
const Rgb C_CURSOR  = {255,255,255};
const Rgb C_DEL3    = {120, 30, 90};
const Rgb C_DEL2    = {230, 60,170};
const Rgb C_DEL1    = {255,150,220};
const Rgb C_SAVE    = {255,215, 65};

// ---- 5 · Lava-lamp base
const Rgb C_LAVA_A  = { 70,150,255};
const Rgb C_LAVA_B  = {240,235, 75};
const Rgb C_LAVA_C  = {185, 65,220};

static inline uint8_t lerpByte(uint8_t a, uint8_t b, float t) {
    float v = (float)a + ((float)b - (float)a) * t;
    if (v < 0.f) v = 0.f;
    if (v > 255.f) v = 255.f;
    return (uint8_t)(v + 0.5f);
}

Rgb rgbLerp(const Rgb& a, const Rgb& b, float t) {
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    Rgb out;
    out.r = lerpByte(a.r, b.r, t);
    out.g = lerpByte(a.g, b.g, t);
    out.b = lerpByte(a.b, b.b, t);
    return out;
}

Rgb rgbScale(const Rgb& c, float brightness) {
    if (brightness < 0.f) brightness = 0.f;
    if (brightness > 1.f) brightness = 1.f;
    Rgb out;
    out.r = (uint8_t)((float)c.r * brightness + 0.5f);
    out.g = (uint8_t)((float)c.g * brightness + 0.5f);
    out.b = (uint8_t)((float)c.b * brightness + 0.5f);
    return out;
}

Rgb rgbDeleteRamp(float t) {
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    if (t < 0.5f) return rgbLerp(C_DEL3, C_DEL2, t * 2.f);
    return rgbLerp(C_DEL2, C_DEL1, (t - 0.5f) * 2.f);
}
