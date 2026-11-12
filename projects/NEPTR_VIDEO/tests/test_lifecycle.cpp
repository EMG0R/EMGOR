// A particle's causes of death fail SILENTLY and slowly. A hazard that is
// secretly framerate-dependent looks like "the swarm feels different on the
// Pi"; a cause that is not exactly zero at zero looks like "the pool has
// always churned a bit". Neither throws, and neither is visible in one frame.
// This suite is where those two can actually be caught.
#include "layers/lifecycle.h"

#include <cmath>

#include "check.h"
#include "state.h"

using namespace nv;

namespace {

// Survival probability after `seconds`, simulated one frame at a time. This is
// the only thing that matters about a hazard: how long a particle lasts.
float survive(float rate, float dt, float seconds) {
    float alive = 1.0f;
    for (float t = 0.0f; t < seconds - 1e-6f; t += dt) {
        alive *= 1.0f - hazard(rate, dt);
    }
    return alive;
}

void testZeroIsExactlyFree() {
    // Not "small". A cause at zero must be bit-exactly no chance of dying, or
    // a defaulted preset slowly churns a pool that used to be stable.
    CHECK(hazard(0.0f, 1.0f / 60.0f) == 0.0f);
    CHECK(hazard(0.0f, 10.0f) == 0.0f);
    CHECK(hazard(5.0f, 0.0f) == 0.0f);
    // A stalled or reversed clock cannot resurrect or kill anything either.
    CHECK(hazard(5.0f, -0.5f) == 0.0f);
}

void testFramerateIndependence() {
    // The same expected lifetime at 30, 60 and 120fps, and in one big step.
    // A linear rate*dt model passes at 60 and fails everywhere else.
    const float rate = 2.0f;
    float ref = std::exp(-rate * 1.0f);  // one second of a 2/s hazard
    CHECK_NEAR(survive(rate, 1.0f / 120.0f, 1.0f), ref, 0.01f);
    CHECK_NEAR(survive(rate, 1.0f / 60.0f, 1.0f), ref, 0.02f);
    CHECK_NEAR(survive(rate, 1.0f / 30.0f, 1.0f), ref, 0.04f);
}

void testHitchCannotEmptyThePool() {
    // The failure this prevents: a 200ms stall at a high rate asks a linear
    // model for a probability above 1, which kills EVERY particle in one
    // frame and reads as the swarm blinking out.
    for (float rate : {1.0f, 8.0f, 64.0f, 1000.0f}) {
        float p = hazard(rate, 0.2f);
        CHECK(p > 0.0f);
        CHECK(p <= 1.0f);  // rate*dt would be 200 at the top of that list
    }
    // At the top of the knob's actual travel a 200ms stall still leaves most
    // of the pool standing, which is what "a hitch is not an event" means.
    CHECK(hazard(8.0f, 0.2f) < 0.85f);
    // Monotone in rate, or the knob would not read as "more lethal".
    CHECK(hazard(1.0f, 0.016f) < hazard(2.0f, 0.016f));
    CHECK(hazard(2.0f, 0.016f) < hazard(2.0f, 0.032f));
}

void testInteriorIsTheVoidAndNotMerelyDark() {
    // The set interior is genuinely zero. Everything with structure in it has
    // to survive, or the swarm dissolves over any dark palette.
    CHECK_NEAR(interiorWeight(0.0f), 1.0f, 1e-6f);
    CHECK_NEAR(interiorWeight(kInteriorCeil), 0.0f, 1e-6f);
    CHECK_NEAR(interiorWeight(0.5f), 0.0f, 1e-6f);
    CHECK_NEAR(interiorWeight(1.0f), 0.0f, 1e-6f);
    CHECK(interiorWeight(kInteriorCeil * 0.5f) < 1.0f);
    CHECK(interiorWeight(kInteriorCeil * 0.5f) > 0.0f);
}

void testDriftingIsNotExhaustion() {
    // "A particle that has been thrown hard should exhaust; one drifting
    // gently should persist." A still particle is the cohesion-1 case, and it
    // must be exactly zero -- that is half of why death is invisible there.
    CHECK(exhaustWeight(0.0f) == 0.0f);
    CHECK(exhaustWeight(kCalmSpeed) == 0.0f);
    CHECK(exhaustWeight(kCalmSpeed * 0.5f) == 0.0f);
    CHECK(exhaustWeight(kSpentSpeed) == 1.0f);
    CHECK(exhaustWeight(kSpentSpeed * 4.0f) == 1.0f);
    CHECK(exhaustWeight(1.0f) > 0.0f);
    CHECK(exhaustWeight(0.5f) < exhaustWeight(1.5f));
}

void testDefaultsLeaveTheSwarmAsItWas() {
    // Every cause and the deposit default to off, which is what makes a
    // preset written before they existed still describe the same picture.
    ParamBank b;
    registerGlobalParams(b);
    for (const char* id : {gid::kPtDieInterior, gid::kPtDieEdge,
                           gid::kPtDieExhaust, gid::kPtDeposit}) {
        int i = b.find(id);
        CHECK(i >= 0);
        if (i < 0) continue;
        CHECK(b.value(i) == 0.0f);
        // And reachable from a control surface at the bottom of its travel,
        // so "off" is somewhere a finger can actually land.
        b.setNorm(i, 0.0f);
        CHECK(b.value(i) == 0.0f);
    }
}

void testDefinesCarryTheSameNumbers() {
    // The shader writes the weight functions a second time; it must not write
    // the thresholds a second time. If this ever fails, the two halves of the
    // model have drifted and only the picture would tell you.
    std::string d = lifecycleDefines();
    CHECK(d.find("NV_INTERIOR_CEIL 0.060000") != std::string::npos);
    CHECK(d.find("NV_CALM_SPEED 0.250000") != std::string::npos);
    CHECK(d.find("NV_SPENT_SPEED 2.000000") != std::string::npos);
}

}  // namespace

int main() {
    testZeroIsExactlyFree();
    testFramerateIndependence();
    testHitchCannotEmptyThePool();
    testInteriorIsTheVoidAndNotMerelyDark();
    testDriftingIsNotExhaustion();
    testDefaultsLeaveTheSwarmAsItWas();
    testDefinesCarryTheSameNumbers();
    return check::finish("lifecycle");
}
