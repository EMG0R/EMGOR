// The endless zoom journey.
//
// The failures this pins are all of the "reads as feel, not as a fault" kind,
// which is why they are checked arithmetically here:
//
//   * a journey that is not monotone in travel is the ORIGINAL BUG -- the
//     picture dancing back and forth instead of advancing;
//   * a recycle that is not exactly reversible makes reversing SPEED land
//     somewhere other than where you came from, minutes into a performance;
//   * a dive boundary that is a cut rather than a dissolve is a visible jump
//     once per dive, unreproducible and indistinguishable from a dropped frame.
#include <cmath>

#include "check.h"
#include "journey.h"

using namespace nv;

namespace {

// Depth advances strictly with travel, everywhere except inside a dissolve
// (where the frame is two views and "the" depth is not one number).
void testMonotone() {
    Journey j;
    double prev = -1e9;
    int wraps = 0;
    for (int i = 0; i < 20000; ++i) {
        j.advance(1.0 / 60.0, 1.0);
        const double u = j.frame().a.logZoom;
        if (u < prev) ++wraps;   // only ever at a dive boundary
        prev = u;
    }
    // 20000 frames at 1/60 s and rate 1 is ~333 decades of travel, which is
    // 27 dives at kSpan 12. Anything wildly different means the travel is not
    // accumulating the way the caller thinks it is.
    const int want = static_cast<int>(333.3 / Journey::kSpan);
    CHECK(wraps >= want - 1 && wraps <= want + 1);
    CHECK(j.travel() > 300.0);
}

// The whole point of the pure-function design: reversing travel through a
// recycle boundary must reproduce the dive you came from, not a new one.
void testReversible() {
    Journey fwd;
    // Land deliberately PAST a recycle, so the reversal has to cross it.
    for (int i = 0; i < 900; ++i) fwd.advance(1.0 / 60.0, 1.0);
    const double deep = fwd.travel();
    const JourneyFrame at = fwd.frame();

    // Come back to the same travel from the other direction.
    for (int i = 0; i < 300; ++i) fwd.advance(1.0 / 60.0, 1.0);
    for (int i = 0; i < 300; ++i) fwd.advance(1.0 / 60.0, -1.0);

    CHECK(std::fabs(fwd.travel() - deep) < 1e-9);
    const JourneyFrame back = fwd.frame();
    CHECK(std::fabs(back.a.cx - at.a.cx) < 1e-15);
    CHECK(std::fabs(back.a.cy - at.a.cy) < 1e-15);
    CHECK(std::fabs(back.a.logZoom - at.a.logZoom) < 1e-9);
    CHECK(std::fabs(back.xfade - at.xfade) < 1e-6f);
}

// Crossing a dive boundary must be a dissolve that reaches 1, not a cut. If
// xfade never gets near 1 before the index flips, the last frame of the old
// dive is still partly visible when it vanishes -- that is the jump.
void testDissolveCompletes() {
    Journey j;
    j.setTravel(Journey::kSpan - Journey::kFade - 0.01);
    CHECK(j.frame().xfade == 0.0f);

    double maxFade = 0.0;
    const long long startDive = j.diveIndex(j.travel());
    for (int i = 0; i < 4000; ++i) {
        j.advance(1.0 / 60.0, 0.02);       // creep through the boundary
        const JourneyFrame f = j.frame();
        if (j.diveIndex(j.travel()) == startDive) {
            if (f.xfade > maxFade) maxFade = f.xfade;
        }
    }
    // Reached full dissolve before the index changed.
    CHECK(maxFade > 0.995);
    // And we did actually cross.
    CHECK(j.diveIndex(j.travel()) == startDive + 1);
}

// The incoming dive must arrive WIDE (negative depth), not as a second deep
// view. If b.logZoom were >= 0 the dissolve would be one deep field of detail
// cutting into another, which reads as a glitch rather than as emerging.
void testIncomingIsWide() {
    Journey j;
    j.setTravel(Journey::kSpan - Journey::kFade * 0.5);
    const JourneyFrame f = j.frame();
    CHECK(f.xfade > 0.0f);
    CHECK(f.b.logZoom < 0.0);
    CHECK(f.a.logZoom > Journey::kSpan - Journey::kFade);
    // Two different places, or the dissolve is invisible and pointless.
    CHECK(std::fabs(f.b.cx - f.a.cx) + std::fabs(f.b.cy - f.a.cy) > 1e-6);
}

// Depth must never exceed what a hi/lo float centre can resolve (~1e-14).
void testDepthWithinPrecision() {
    Journey j;
    for (int i = 0; i < 50000; ++i) {
        j.advance(1.0 / 60.0, 1.0);
        CHECK(j.frame().a.logZoom < Journey::kSpan);
    }
}

// Detail has to grow with depth or the deep frames go smooth, but it must
// stay bounded or the governor cannot defend the framerate.
void testDetailBoost() {
    CHECK(Journey::detailBoost(0.0) == 1.0f);
    CHECK(Journey::detailBoost(6.0) > Journey::detailBoost(2.0));
    // Bounded is the property under test, not the size of the bound. The
    // ceiling moved from 5.5 to 16 when the iteration cap went 512 -> 2048:
    // a ten-decade dive needs ~1250 iterations to keep its boundary resolved,
    // and the old clamp could only ask for 528.
    CHECK(Journey::detailBoost(100.0) <= 16.0f);
    CHECK(Journey::detailBoost(1e9) == Journey::detailBoost(100.0));
}

// A generator with nothing to descend into must HOLD NORMAL POSITION -- not
// dive slowly, not hop centres at the recycle. This is the whole point of
// per-kernel depth, and it is the half that is easy to get wrong: parking the
// zoom while leaving the dissolve running would still lurch the picture
// sideways every dive, which is indistinguishable from the frame-jump bug.
void testDepthZeroHoldsStill() {
    Journey j;
    for (int i = 0; i < 4000; ++i) {
        j.advance(1.0 / 60.0, 1.0);   // ~66 s: many dives at full depth
        const JourneyFrame f = j.frame(0.0);
        CHECK(f.a.logZoom == 0.0);
        CHECK(f.a.cx == 0.0);
        CHECK(f.a.cy == 0.0);
        CHECK(f.xfade == 0.0f);       // no dissolve, so no centre hop
    }
}

// A shallow generator reaches its own bottom and no further, while still
// recycling on the same cadence as a deep one -- JOURNEY has to keep meaning
// the same thing whatever is loaded.
void testShallowDepthIsScaled() {
    Journey j;
    double deepest = 0.0;
    bool sawFade = false;
    // Long enough to cross a whole dive, expressed in kSpan rather than as a
    // frame count: a dive is kSpan of TRAVEL, and at rate 1.0 with dt 1/60
    // that is 60*kSpan frames. Hard-coding 400 meant this test silently
    // stopped reaching the recycle the moment kSpan grew.
    const int frames = static_cast<int>(60.0 * (Journey::kSpan + 1.0));
    for (int i = 0; i < frames; ++i) {
        j.advance(1.0 / 60.0, 1.0);
        const JourneyFrame f = j.frame(2.0);
        if (f.a.logZoom > deepest) deepest = f.a.logZoom;
        if (f.xfade > 0.0f) sawFade = true;
        CHECK(f.a.logZoom <= 2.0 + 1e-9);
    }
    CHECK(deepest > 1.9);   // it does reach its bottom
    CHECK(sawFade);         // and it still recycles, on the usual cadence
}

}  // namespace

int main() {
    testMonotone();
    testReversible();
    testDissolveCompletes();
    testIncomingIsWide();
    testDepthWithinPrecision();
    testDetailBoost();
    testDepthZeroHoldsStill();
    testShallowDepthIsScaled();
    return check::finish("journey");
}
