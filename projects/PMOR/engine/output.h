// output.h — master fader + always-on Fruity-style soft clipper + hard ±1 + NaN guard + meter.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../config.h"

struct OutputState {
    float fader;        // 0..1.333 ; 1.0 = MASTER_UNITY linear (old 75 %). Above 1.0 only drives the clipper.
    float clip_thresh_db;
    float input_gain;   // GLOBALS: line-in gain 0..2
    // meter (audio writes)
    float peak;
};
extern OutputState outp;

void outputInit();
// AUDIO plane: in-place on the final stereo block
void outputProcess(float* L, float* R, size_t n);
// the clipper as a reusable per-sample function (also used by DIST_FL)
struct FlClip { float t, inv4w, kneeEnd, post; };
void  flclipSet(FlClip& c, float thresh_db, float post_db);
static inline float flclipSample(const FlClip& c, float x) {
    float u = x < 0 ? -x : x;
    float y;
    if (u <= c.t) y = u;
    else if (u >= c.kneeEnd) y = 1.f;
    else { float d = u - c.t; y = c.t + d - d * d * c.inv4w; }
    y *= c.post;
    if (y > 1.f) y = 1.f;
    return x < 0 ? -y : y;
}
