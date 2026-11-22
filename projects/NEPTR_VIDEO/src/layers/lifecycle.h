// What kills a particle.
//
// Respawning on a timer is bureaucratic: everything lives exactly as long as
// everything else and nothing that happens to a particle can shorten that.
// These are the three things the world can do to one instead.
//
// There is no age channel and no second render target -- the whole point of
// the state packing is that one RGBA32F texel holds a particle -- so a cause
// cannot ACCUMULATE into a stored life budget. Each cause is a per-frame
// HAZARD instead: a constant rate of dying for as long as the condition
// holds. The dwell that comes out of that is exponential rather than fixed,
// so "interior death at 2/s" means a half-life of about a third of a second
// in the interior, not a stopwatch. Memoryless is the price of costing zero
// storage, and it reads the same on screen.
//
// The maths lives here, in C++ with no GL, for two reasons: it is the part
// that can be wrong in a way nobody sees (a hazard that quietly depends on
// framerate looks like "it feels different on the Pi"), and the thresholds
// have to be the SAME number in the shader. The shader gets them as #defines
// generated from these constants, so there is one copy of every number even
// though the two-line weight functions are written twice.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace nv {

// Luma at or above which a particle is standing on structure rather than in
// the void. The set interior is genuinely zero, so the ramp is short: this
// must not fire on merely dim colour, or the whole swarm dissolves over any
// dark palette.
constexpr float kInteriorCeil = 0.06f;

// Speed below which a particle is drifting rather than being thrown. Position
// is in 0..1 screen units per second, so 0.25 is a quarter of the frame per
// second -- visible movement, but not the ballistic flight MOTION produces.
constexpr float kCalmSpeed = 0.25f;
// Speed at which it is fully spent. Above this the exhaust rate is at full.
constexpr float kSpentSpeed = 2.0f;

// Probability that a hazard running at `rate` per second fires during `dt`.
//
// 1-exp(-r*dt) rather than the obvious r*dt, and the difference is not
// pedantry: at 8/s a linear model asks for p=0.13 per 60fps frame but p=1.6
// on a 200ms hitch, so one dropped frame empties the pool. The exponential
// form cannot exceed 1 and keeps the EXPECTED LIFETIME identical at 30, 60
// and 120fps -- the swarm has to look the same on the Pi as it does here.
inline float hazard(float rate, float dt) {
    if (rate <= 0.0f || dt <= 0.0f) return 0.0f;  // a cause at zero is exactly free
    return 1.0f - std::exp(-rate * dt);
}

inline float smoothstepf(float a, float b, float x) {
    if (b <= a) return x < a ? 0.0f : 1.0f;
    float t = (x - a) / (b - a);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// How trapped by the void a particle at this luma is. 1 in the interior,
// 0 anywhere there is structure to push it.
inline float interiorWeight(float luma) {
    return 1.0f - smoothstepf(0.0f, kInteriorCeil, luma);
}

// How thrown a particle at this speed is. 0 while it drifts.
inline float exhaustWeight(float speed) {
    return smoothstepf(kCalmSpeed, kSpentSpeed, speed);
}

// The same constants, as shader defines. Prepended to the update shader so
// the two implementations cannot drift apart on the numbers that matter.
inline std::string lifecycleDefines() {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "#define NV_INTERIOR_CEIL %.6f\n"
                  "#define NV_CALM_SPEED %.6f\n"
                  "#define NV_SPENT_SPEED %.6f\n",
                  kInteriorCeil, kCalmSpeed, kSpentSpeed);
    return buf;
}

}  // namespace nv
