// SPEED's curve and the motion clock.
//
// Both fail in ways that read as "feel" rather than as faults, which is why
// they are pinned here rather than looked at:
//
//   * a bipolar curve whose inverse is not exact makes a preset land a hair
//     off where it was saved, forever, and nobody can say why the patch is
//     slightly wrong;
//   * a motion phase that is not exactly periodic in its wrap makes the
//     picture JUMP once every wrap -- minutes apart, unreproducible, and
//     indistinguishable from a dropped frame. That is the bug this whole
//     change was reported for, so it is checked arithmetically here and again
//     against real frames in tests/motion_probe.py.
#include <cmath>
#include <cstdio>

#include "check.h"
#include "motion.h"
#include "state.h"

using namespace nv;

namespace {

ParamBank speedBank() {
    ParamBank b;
    registerGlobalParams(b);
    return b;
}

// ---- SPEED: bipolar, reversible, and stopped at the stop ------------------

void testSpeedIsSignedAndSymmetric() {
    ParamBank b = speedBank();
    int i = b.find(gid::kSpeed);
    CHECK(i >= 0);
    CHECK(b.def(i).curve == Curve::BipolarCube);

    b.setNormNow(i, 1.0f);
    float fwd = b.value(i);
    b.setNormNow(i, 0.0f);
    float rev = b.value(i);
    // The two ends are the same rate in opposite directions. Anything else
    // and reverse is a different instrument from forward.
    CHECK_NEAR(fwd, -rev, 1e-5f);
    CHECK(fwd > 0.0f);
    CHECK(rev < 0.0f);
}

void testSpeedCentreIsExactlyZero() {
    ParamBank b = speedBank();
    int i = b.find(gid::kSpeed);
    b.setNormNow(i, 0.5f);
    // EXACTLY zero, not nearly. A scene clock advancing by 1e-7 per frame is
    // not frozen, it is drifting, and it would show up as the picture creeping
    // over a long hold.
    CHECK(b.value(i) == 0.0f);

    // And the detent has width, so a finger can find it.
    b.setNormNow(i, 0.505f);
    CHECK(b.value(i) == 0.0f);
    b.setNormNow(i, 0.495f);
    CHECK(b.value(i) == 0.0f);
}

void testSpeedDefaultIsForwardOne() {
    ParamBank b = speedBank();
    int i = b.find(gid::kSpeed);
    // Unchanged from the unipolar version: an instrument whose default speed
    // moved would make every existing preset play at a different rate.
    CHECK_NEAR(b.value(i), 1.0f, 1e-5f);
    // ...and 1x sits half-way out from the centre, which is what the cubic
    // taper buys: the whole inner half of the travel is slower than realtime.
    CHECK_NEAR(b.target(i), 0.75f, 1e-4f);
}

void testSpeedRoundTripsThroughRealUnits() {
    // This is the preset contract: values are stored in REAL units, so
    // setValue(value(x)) has to be the identity or every save/load pair drifts.
    ParamBank b = speedBank();
    int i = b.find(gid::kSpeed);
    const float probes[] = {-8.0f, -4.0f, -1.0f, -0.25f, 0.0f,
                            0.25f, 1.0f,  4.0f,  8.0f};
    for (float v : probes) {
        b.setValueNow(i, v);
        CHECK_NEAR(b.value(i), v, 1e-4f);
    }
}

void testOldUnipolarPresetStillLoadsForward() {
    // A preset written when SPEED was Exp 0.01..8 stores a REAL number, e.g.
    // `p g.speed 2`. Rule 2 of the preset format is what saves it: 2 still
    // means 2 on the new curve, and the only thing that changed is where the
    // fader sits. A stored value the new range cannot represent could only be
    // one from inside the old range, and the new range strictly contains it.
    ParamBank b = speedBank();
    int i = b.find(gid::kSpeed);
    const float oldValues[] = {0.01f, 0.5f, 1.0f, 2.0f, 8.0f};
    for (float v : oldValues) {
        b.setValueNow(i, v);
        CHECK_NEAR(b.value(i), v, 1e-4f);
        // Every one of them is still FORWARD. An old preset must never come
        // back playing backwards.
        CHECK(b.value(i) > 0.0f);
    }
}

void testEvolveExistsAndDefaultsOn() {
    ParamBank b = speedBank();
    int i = b.find(gid::kEvolve);
    CHECK(i >= 0);
    // Default 1, because a preset saved before this knob existed has no key
    // for it, keeps the default, and has to come back MOVING -- that was the
    // entire complaint.
    CHECK_NEAR(b.value(i), 1.0f, 1e-5f);
    // And it does not collide with the particles' own MOTION.
    CHECK(b.find(gid::kPtMotion) >= 0);
    CHECK(b.find(gid::kEvolve) != b.find(gid::kPtMotion));
}

// ---- the motion clock -----------------------------------------------------

void testMotionIsIdentityAtZeroEvolve() {
    MotionClock c;
    for (int i = 0; i < 50; ++i) {
        c.advance(0.37);
        float m[4];
        c.vector(0.0f, m);
        // Every kernel body is written to reduce to its static self under
        // exactly this vector. If it is ever not exactly zero, "hold still"
        // becomes "wobble imperceptibly" and a morph endpoint stops being the
        // fractal it names.
        CHECK(m[0] == 0.0f);
        CHECK(m[1] == 0.0f);
        CHECK(m[2] == 0.0f);
        CHECK(m[3] == 0.0f);
    }
}

void testMotionIsBounded() {
    MotionClock c;
    for (int i = 0; i < 20000; ++i) {
        c.advance(0.01);
        float m[4];
        c.vector(2.0f, m);   // EVOLVE at its maximum
        for (int k = 0; k < 4; ++k) {
            // Bounded by the knob, always. An unbounded term would eventually
            // walk the seed somewhere the frame is empty.
            CHECK(std::fabs(m[k]) <= 2.0f + 1e-5f);
        }
    }
}

void testMotionFreezesAtSpeedZero() {
    MotionClock c;
    c.advance(3.3);
    float a[4];
    c.vector(1.0f, a);
    for (int i = 0; i < 100; ++i) c.advance(0.0);
    float b[4];
    c.vector(1.0f, b);
    // dt * 0 is 0, and the phase is INTEGRATED, so a hundred frames at speed
    // zero produce the identical vector -- bit for bit, not nearly.
    for (int k = 0; k < 4; ++k) CHECK(a[k] == b[k]);
}

void testMotionReversesExactly() {
    // Run forward to T, remember it; run further forward, then back the same
    // distance. Arriving at T again must produce the same vector. This is the
    // claim "reverse actually reverses" reduced to the one place it can be
    // checked without a GPU.
    MotionClock c;
    for (int i = 0; i < 90; ++i) c.advance(1.0 / 60.0);
    float at[4];
    c.vector(1.0f, at);

    for (int i = 0; i < 200; ++i) c.advance(1.0 / 60.0);
    for (int i = 0; i < 200; ++i) c.advance(-1.0 / 60.0);
    float back[4];
    c.vector(1.0f, back);
    for (int k = 0; k < 4; ++k) CHECK_NEAR(at[k], back[k], 1e-5f);
}

void testMotionWrapIsNotADiscontinuity() {
    // The whole reason the wrap is 12*pi and not a round number. Step across
    // the wrap point in tiny increments and demand the vector never moves
    // further in one step than it does anywhere else. A wrap at any other
    // value would show up here as one enormous step.
    MotionClock c;
    c.setPhase(MotionClock::kPeriod - 1e-4);

    float prev[4];
    c.vector(1.0f, prev);
    double worst = 0.0;
    // 0.02 rad of phase per step, well past the wrap.
    for (int i = 0; i < 200; ++i) {
        // advance() takes SCENE seconds, so convert: phase = 2*pi*f0*t.
        c.advance(0.02 / (MotionClock::kTwoPi * MotionClock::kBaseHz));
        float now[4];
        c.vector(1.0f, now);
        for (int k = 0; k < 4; ++k) {
            double d = std::fabs(now[k] - prev[k]);
            if (d > worst) worst = d;
            prev[k] = now[k];
        }
    }
    // A step of 0.02 rad can move a unit sinusoid by at most 0.02. A wrap
    // discontinuity would be O(1).
    std::printf("  wrap: worst single-step delta %.6f\n", worst);
    CHECK(worst < 0.025);
}

void testMotionPhaseStaysSmallForever() {
    // Two hours of scene time at 8x. The point of the wrap is that the number
    // handed to a float uniform never grows, so its ulp never grows either --
    // which is the "sometimes it stutters after a while" failure.
    MotionClock c;
    for (int i = 0; i < 60 * 60 * 60; ++i) c.advance(8.0 / 60.0);
    CHECK(c.phase() >= 0.0);
    CHECK(c.phase() < MotionClock::kPeriod);
    // And it stays inside the period going backwards too.
    for (int i = 0; i < 60 * 60 * 60; ++i) c.advance(-8.0 / 60.0);
    CHECK(c.phase() >= 0.0);
    CHECK(c.phase() < MotionClock::kPeriod);
}

}  // namespace

int main() {
    testSpeedIsSignedAndSymmetric();
    testSpeedCentreIsExactlyZero();
    testSpeedDefaultIsForwardOne();
    testSpeedRoundTripsThroughRealUnits();
    testOldUnipolarPresetStillLoadsForward();
    testEvolveExistsAndDefaultsOn();
    testMotionIsIdentityAtZeroEvolve();
    testMotionIsBounded();
    testMotionFreezesAtSpeedZero();
    testMotionReversesExactly();
    testMotionWrapIsNotADiscontinuity();
    testMotionPhaseStaysSmallForever();
    std::printf("motion: all checks passed\n");
    return 0;
}
