// The frame governor.
//
// The premise: framerate is never the thing that gives. When the machine
// cannot hold the refresh, it gives up DETAIL instead -- iteration depth
// first, then internal resolution. Both of those are also aesthetic controls,
// so degrading reads as an artistic decision rather than as a fault, which is
// exactly the property you want on stage.
//
// The user's DETAIL and RES controls set the CEILING. The governor only ever
// scales down from there, and restores as soon as it can.
#pragma once

namespace nv {

class Governor {
public:
    // Pass the display's refresh rate. The governor defends 60 fps, or the
    // display's rate when that is lower -- never the reported rate when it is
    // higher, which cannot be trusted and is not the goal anyway.
    void configure(double displayHz);

    // Called with the measured GPU-side frame duration in seconds.
    void observe(double frameSeconds);

    // Off entirely. Only for measurement: the governor changes the iteration
    // ceiling in response to load, so two runs being COMPARED on frame time
    // end up running different amounts of work and the comparison measures the
    // governor rather than the thing under test. (Found the hard way: a
    // like-for-like pair came back with different pictures.)
    void setEnabled(bool on) { enabled_ = on; if (!on) reset(); }
    bool enabled() const { return enabled_; }

    // Multipliers to apply on top of the user's settings. Both <= 1.
    float detailScale() const { return detailScale_; }
    float resScale() const { return resScale_; }
    float resCeiling() const { return resCeiling_; }
    // Fraction of the particle pool that may be alive. The user's COUNT knob
    // is the ceiling; this is what keeps its maximum honest -- turning it to
    // full asks for as many as the machine can actually carry at 60fps, not
    // as many as the pool holds.
    float countScale() const { return countScale_; }

    double smoothedFrameMs() const { return ema_ * 1000.0; }
    double targetMs() const { return target_ * 1000.0; }
    bool throttling() const {
        return detailScale_ < 0.999f || resScale_ < 0.999f || countScale_ < 0.999f;
    }

    // Call after anything that makes the next few frames unrepresentative:
    // a kernel change, a chain recompile, a mode change.
    void reset();

    // Whether the DETAIL knob actually costs anything for what is currently
    // being rendered. Field kernels do no iterating, so cutting detail there
    // buys nothing -- and the governor would spend eight useless steps on it
    // before reaching the lever that does work.
    void setDetailEffective(bool on) { detailEffective_ = on; }

    // Whether thinning the swarm is a legitimate saving right now. It is not
    // when the particles are cohesive enough to BE the image -- removing them
    // would put holes in it rather than cost detail.
    void setCountEffective(bool on) { countEffective_ = on; }

private:
    static constexpr int kWarmupFrames = 30;
    static constexpr double kGoalHz = 60.0;

    int warmup_ = kWarmupFrames;
    double target_ = 1.0 / 60.0;  // the frame interval we are defending
    double ema_ = 0.0;
    int overRun_ = 0;
    int underRun_ = 0;
    bool enabled_ = true;
    bool detailEffective_ = true;
    bool countEffective_ = false;
    float detailScale_ = 1.0f;
    float countScale_ = 1.0f;
    float resScale_ = 1.0f;
    // The highest resolution the governor is currently willing to climb back
    // to. Without this the governor RE-PROBES a level it has already proved
    // too expensive, cuts again, and sawtooths forever -- measured on the Pi
    // as a once-per-second resolution change that never settled. See the
    // note in observe().
    float resCeiling_ = 1.0f;
    int healthyStreak_ = 0;
};

}  // namespace nv
