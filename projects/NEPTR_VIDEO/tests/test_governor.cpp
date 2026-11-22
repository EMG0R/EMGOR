// The governor decides when the picture gives up detail. Its failure mode is
// silent and permanent -- a wrong threshold degrades every frame forever and
// still "works" -- so its behaviour is pinned here.
#include "governor.h"

#include <string>

#include "check.h"

using namespace nv;

namespace {

void run(Governor& g, double frameSeconds, int frames) {
    for (int i = 0; i < frames; ++i) g.observe(frameSeconds);
}

void testVsyncLockedIsNotThrottled() {
    // THE regression test. Under vsync lock a frame takes exactly one refresh
    // interval no matter how little GPU work was done -- the remainder is the
    // wait for the flip. A budget below the refresh interval therefore reads a
    // perfectly healthy engine as over budget and throttles it forever.
    // This is what shipped at 60 Hz until it was caught on hardware.
    for (double hz : {60.0, 50.0, 120.0, 144.0}) {
        Governor g;
        g.configure(hz);
        run(g, 1.0 / hz, 600);
        CHECK(!g.throttling());
        CHECK_NEAR(g.detailScale(), 1.0f, 1e-6f);
        CHECK_NEAR(g.resScale(), 1.0f, 1e-6f);
    }
}

void testHighRefreshDisplayRunningAtSixty() {
    // Caught on hardware: GLFW reports a ProMotion panel as 120 Hz while the
    // compositor actually swaps at 60. Targeting the REPORTED rate makes a rig
    // that is hitting 60 perfectly look 8 ms late on every frame, and it
    // throttles to the floor and stays there.
    Governor g;
    g.configure(120.0);          // what the display claims
    run(g, 1.0 / 60.0, 900);     // what it actually delivers
    CHECK(!g.throttling());
    CHECK_NEAR(g.detailScale(), 1.0f, 1e-6f);
    CHECK_NEAR(g.resScale(), 1.0f, 1e-6f);
}

void testSlowDisplayIsNotHeldToSixty() {
    // A 50 Hz output cannot reach 60, and must not be told forever that it is
    // 10 fps short.
    Governor g;
    g.configure(50.0);
    run(g, 1.0 / 50.0, 900);
    CHECK(!g.throttling());
}

void testSlightJitterIsToleratedAtVsync() {
    // Real vsync timings wobble either side of the interval. That must not be
    // mistaken for missed flips.
    Governor g;
    g.configure(60.0);
    for (int i = 0; i < 600; ++i) {
        double jitter = (i % 2 == 0) ? 1.03 : 0.97;
        g.observe((1.0 / 60.0) * jitter);
    }
    CHECK(!g.throttling());
}

void testMissedFlipsThrottle() {
    // Missing a flip costs a whole interval, so a struggling frame lands near
    // 2x refresh. That must be caught.
    Governor g;
    g.configure(60.0);
    run(g, 2.0 / 60.0, 300);
    CHECK(g.throttling());
    // Detail is given up before resolution: it is the dominant cost, and
    // losing it softens the image rather than pixelating it.
    CHECK(g.detailScale() < 1.0f);
}

void testResolutionOnlyAfterDetailIsExhausted() {
    Governor g;
    g.configure(60.0);
    run(g, 4.0 / 60.0, 60);
    CHECK(g.detailScale() < 1.0f);
    CHECK_NEAR(g.resScale(), 1.0f, 1e-6f);  // still untouched

    run(g, 4.0 / 60.0, 2000);
    CHECK_NEAR(g.detailScale(), 0.35f, 1e-5f);  // floored
    CHECK(g.resScale() < 1.0f);                 // only now does resolution give
}

void testNeverCutsBelowTheFloor() {
    // However bad it gets, the picture must remain a picture.
    Governor g;
    g.configure(60.0);
    run(g, 0.5, 5000);
    CHECK(g.detailScale() >= 0.35f);
    CHECK(g.resScale() >= 0.4f);
}

void testRecoveryRestores() {
    Governor g;
    g.configure(60.0);
    run(g, 4.0 / 60.0, 400);
    CHECK(g.throttling());

    // Load lifts: the engine is keeping up again and should climb back.
    //
    // Deliberately WELL under budget rather than exactly at it. A frame that
    // costs precisely the whole 1/60 budget is "healthy" with zero headroom,
    // which is the state the resolution sawtooth lived in, and the governor
    // is now supposed to sit still there rather than climb. Feeding exactly
    // 1/60 here and calling it "load lifted" tested the opposite of what the
    // name says.
    run(g, 0.5 / 60.0, 6000);
    CHECK(!g.throttling());
}

// The resolution sawtooth: the frame-jump bug.
//
// Measured on the Pi, the governor changed internal resolution once per
// SECOND forever -- 326x544 -> 345x576 -> 364x608 -> 384x640 -> 403x672 ->
// drop -> 307x512 -> climb again. Each change resamples the whole image, and
// on a fractal that resample is the visible pop the bug was reported as.
//
// The mechanism is that a healthy reading licensed another upward probe, so
// the governor kept re-climbing to a level it had already proved it could not
// afford. What is asserted here is therefore the CEILING invariant directly:
// once a level has caused a cut, sustained-but-marginal health must not climb
// back above it. Marginal is the operative word -- the sawtooth lived just
// under the healthy line, never with real headroom.
void testDoesNotReprobeAfterACut() {
    Governor g;
    g.configure(60.0);
    const double target = 1.0 / 60.0;

    // Overload until resolution itself is cut (detail and count go first).
    for (int i = 0; i < 4000 && g.resScale() >= 1.0f; ++i) {
        g.observe(target * 2.0);
    }
    CHECK(g.resScale() < 1.0f);
    const float afterCut = g.resScale();

    // Now sit just under the line: healthy every single frame, but with no
    // headroom to spend. This is exactly where the Pi sat at 61 fps.
    for (int i = 0; i < 20000; ++i) g.observe(target * 1.05);

    // It may hold, it may not climb -- but it must NOT climb back above the
    // level that already proved unaffordable.
    CHECK(g.resScale() <= afterCut + 1e-6f);

    // And real headroom must still recover, or the governor would be stuck at
    // reduced resolution forever after one transient.
    for (int i = 0; i < 20000; ++i) g.observe(target * 0.4);
    CHECK(g.resScale() > afterCut);
}

void testWarmupIsIgnored() {
    // The first frames after a start or a kernel change include shader
    // compilation and a cold cache. Reacting to those would throttle a
    // machine that is in fact perfectly capable.
    Governor g;
    g.configure(60.0);
    run(g, 0.4, 20);
    CHECK(!g.throttling());
}

void testResetClearsThrottling() {
    Governor g;
    g.configure(60.0);
    run(g, 4.0 / 60.0, 400);
    CHECK(g.throttling());
    g.reset();
    CHECK(!g.throttling());
}

void testAbsurdFramesAreIgnored() {
    // A multi-second frame is a stall -- a breakpoint, a suspend, a mode set --
    // not a measurement of how fast the machine renders.
    Governor g;
    g.configure(60.0);
    run(g, 1.0 / 60.0, 100);
    for (int i = 0; i < 50; ++i) g.observe(30.0);
    CHECK(!g.throttling());
}

void testUnsyncedSlowFramesStillThrottle() {
    // With vsync off there is no cadence to lean on; a slow frame is simply
    // slow, and must still be caught.
    Governor g;
    g.configure(60.0);
    run(g, 0.05, 300);  // 20 fps
    CHECK(g.throttling());
}

}  // namespace

int main() {
    testVsyncLockedIsNotThrottled();
    testHighRefreshDisplayRunningAtSixty();
    testSlowDisplayIsNotHeldToSixty();
    testSlightJitterIsToleratedAtVsync();
    testMissedFlipsThrottle();
    testResolutionOnlyAfterDetailIsExhausted();
    testNeverCutsBelowTheFloor();
    testRecoveryRestores();
    testDoesNotReprobeAfterACut();
    testWarmupIsIgnored();
    testResetClearsThrottling();
    testAbsurdFramesAreIgnored();
    testUnsyncedSlowFramesStillThrottle();
    return check::finish("governor");
}
