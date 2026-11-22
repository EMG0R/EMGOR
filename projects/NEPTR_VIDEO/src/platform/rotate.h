// Screen rotation, as arithmetic.
//
// DEGREES CLOCKWISE, always. Every layer of this stack expresses rotation
// clockwise -- the kernel's `video=...,rotate=`, config.txt's quarter-turn
// `display_hdmi_rotate`, RetroArch's `video_rotation` -- and a reader who
// assumes counter-clockwise gets a 180-degree error that looks like a
// completely different bug. A panel mounted 90 degrees COUNTER-clockwise wants
// 270 here.
//
// Header-only and GL-free on purpose: the display half of this lives in a
// shader and the touch half lives behind a Linux-only evdev reader, so the
// only place the two can be checked against each other is a test. And they
// MUST agree -- if the picture turns and the input does not, every tap lands
// somewhere else, which reads as broken input rather than a sideways screen.
#pragma once

namespace nv {

struct Pt {
    float x, y;
};

// Where a LOGICAL point (0..1, y down, in the rotated layout everything above
// the platform seam works in) lands on the PHYSICAL surface.
//
// This is the transform the present pass and the panel's vertex shader apply.
inline Pt rotateForward(int deg, Pt p) {
    switch (((deg % 360) + 360) % 360) {
        case 90:  return {1.0f - p.y, p.x};
        case 180: return {1.0f - p.x, 1.0f - p.y};
        case 270: return {p.y, 1.0f - p.x};
        default:  return p;
    }
}

// What a PHYSICAL point -- a finger on glass that did not move when the
// picture turned -- means in logical coordinates. Exactly the inverse.
inline Pt rotateInverse(int deg, Pt p) {
    switch (((deg % 360) + 360) % 360) {
        case 90:  return {p.y, 1.0f - p.x};
        case 180: return {1.0f - p.x, 1.0f - p.y};
        case 270: return {1.0f - p.y, p.x};
        default:  return p;
    }
}

inline bool isQuarterTurn(int deg) {
    int d = ((deg % 360) + 360) % 360;
    return d == 90 || d == 270;
}

}  // namespace nv
