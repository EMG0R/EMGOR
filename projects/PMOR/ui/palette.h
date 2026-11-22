// palette.h — EMGOR's cyan / pink / yellow scheme (2026-09-14): variations of those three, white/light pink, blue-greens. No red, no orange. + Rgb helpers.
// CONTROL plane only (render_oled.cpp / render_trellis.cpp).
#pragma once
#include "render.h"   // Rgb

// ---- 1 · FX families
extern const Rgb C_UP;       // stutter/spatial   0,225,255
extern const Rgb C_RIGHT;    // modulation       255,90,200
extern const Rgb C_DOWN;     // distortion/ring  255,225,60
extern const Rgb C_LEFT;     // reverse          40,235,170
extern const Rgb C_HOME;     // blend of four    255,200,230

// ---- 2 · Engine identities
extern const Rgb C_DRUMS;    // 255,100,190
extern const Rgb C_SYNTH;    //  60,200,255
extern const Rgb C_SAMPLER;  //   0,235,180
extern const Rgb C_PATCHES;  // 255,215,65
extern const Rgb C_GLOBALS;  // 110,170,255
extern const Rgb C_DASH;     // 185,120,240

// ---- 3 · Drum quadrant tints (within ±30° of DRUMS)
extern const Rgb C_KICKS;    // 255,80,180
extern const Rgb C_SNARES;   // 255,170,215
extern const Rgb C_HATS;     // 255,230,80
extern const Rgb C_PERC;     // 60,220,255

// ---- 4 · Functional states
extern const Rgb C_FOCUS;    // 255,245,250 — focused pad (warm white)
extern const Rgb C_FILLED;   // 170,255,255 — sample-filled pad
extern const Rgb C_CURSOR;   // 255,255,255 — step cursor (cool ice-white)
extern const Rgb C_DEL3;     // 120,30,90 — delete ramp start (dim)
extern const Rgb C_DEL2;     // 230,60,170 — delete ramp middle
extern const Rgb C_DEL1;     // 255,150,220 — delete ramp end / GO
extern const Rgb C_SAVE;     // 255,215,65 — saving amber pulse

// ---- 5 · Lava-lamp base (full-scale keys; dim ~1/16 in use)
extern const Rgb C_LAVA_A;   //  70,150,255
extern const Rgb C_LAVA_B;   // 240,235,75
extern const Rgb C_LAVA_C;   // 185,65,220

// ---- helpers (header-only-cheap, CONTROL plane; no allocation, no float division inside loops)
Rgb   rgbLerp(const Rgb& a, const Rgb& b, float t);   // t clamped 0..1
Rgb   rgbScale(const Rgb& c, float brightness);       // brightness clamped 0..1
// three-stop ramp used by delete countdowns: DEL3 -> DEL2 -> DEL1, t clamped 0..1
Rgb   rgbDeleteRamp(float t);
