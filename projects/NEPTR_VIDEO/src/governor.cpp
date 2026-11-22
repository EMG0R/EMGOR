#include "governor.h"

#include <algorithm>

namespace nv {

// The measurement is wall-clock frame time, and the target is the FRAMERATE
// GOAL -- not the display's refresh interval.
//
// Under vsync lock a frame takes a whole refresh interval whether the GPU
// finished in 2 ms or 15 ms; the rest is spent waiting for the flip. So any
// threshold at or below the refresh interval declares a perfectly healthy
// engine to be over budget and throttles it forever. This file did exactly
// that twice: first with a budget of refresh x 0.85, then by trusting a
// reported 120 Hz on a panel the compositor actually swaps at 60.
//
// Thresholding above the goal interval separates "keeping up" from "missing
// frames" and is correct whether or not vsync is engaged, and whatever the
// display claims its rate to be.
static constexpr double kMissThreshold = 1.35;  // above this, frames are being missed
static constexpr double kHealthyMargin = 1.10;  // below this, we are keeping up

// Hysteresis. Reacting to a single slow frame would make the picture pump
// visibly; requiring a run of them means the governor responds to load, not
// to noise.
static constexpr int kOverFramesToCut = 4;
static constexpr int kUnderFramesToRestore = 45;

// How long the engine must run healthy before the governor will consider
// climbing back ABOVE a resolution it has already found too expensive.
//
// This is the fix for the frame-jump bug. Restoring on the same evidence that
// the cut used means a healthy reading always licenses another upward probe,
// so the governor re-climbs to the level it just proved it cannot afford,
// overshoots, cuts, and repeats -- measured on the Pi as
//
//   61 fps 326x544 -> 345x576 -> 364x608 -> 384x640 -> 403x672 -> 52 fps
//   -> 307x512 -> 326x544 -> ...   once per second, forever
//
// Every one of those steps resamples the whole image, and on a fractal --
// which is all high-frequency detail -- that resample IS the visible pop the
// jump was reported as. 900 frames is ~15 s: long enough that the instrument
// settles and holds, short enough that genuinely getting cheaper (a simpler
// kernel, a zoomed-out view) is still recovered from within a few seconds of
// the change.
static constexpr int kFramesToRaiseCeiling = 120;
// Genuine headroom: the frame costs three quarters of the budget or less.
static constexpr double kAmpleHeadroom = 0.75;

void Governor::configure(double displayHz) {
    // Defend the GOAL (60 fps), not the display's advertised rate.
    //
    // The advertised rate cannot be trusted: a ProMotion panel reports 120 Hz
    // while the compositor actually swaps at 60, and targeting the reported
    // 8.3 ms would throttle a rig that is hitting 60 perfectly. Nor is
    // targeting it desirable -- 60 is the requirement, and a 120 Hz display
    // that settles at 60 is meeting it.
    //
    // On a display that cannot reach 60 the goal becomes the display: a 50 Hz
    // output must not be told forever that it is 10 fps short.
    double goalHz = kGoalHz;
    if (displayHz > 1.0 && displayHz < kGoalHz) goalHz = displayHz;
    target_ = 1.0 / goalHz;
    reset();
}

void Governor::reset() {
    ema_ = 0.0;
    overRun_ = underRun_ = 0;
    detailScale_ = 1.0f;
    countScale_ = 1.0f;
    resScale_ = 1.0f;
    resCeiling_ = 1.0f;
    healthyStreak_ = 0;
    warmup_ = kWarmupFrames;
    // detailEffective_ is deliberately NOT reset: it describes the kernel
    // currently loaded, not the measurement history.
}

void Governor::observe(double frameSeconds) {
    if (!enabled_) return;
    if (frameSeconds <= 0.0 || frameSeconds > 1.0) return;  // a stall, not a signal

    // The first frames after a start or a kernel change include shader
    // compilation, texture uploads and a cold cache. Reacting to them would
    // throttle a machine that is in fact perfectly capable, and the slow
    // restore ramp means it would stay throttled for a long time.
    if (warmup_ > 0) {
        --warmup_;
        return;
    }

    ema_ = (ema_ == 0.0) ? frameSeconds : ema_ * 0.9 + frameSeconds * 0.1;

    // Sustained health is what earns the ceiling back -- and only sustained:
    // a single fast frame must not license a climb, or the ceiling is no
    // ceiling at all.
    if (ema_ < target_ * kHealthyMargin) {
        // How fast the ceiling comes back depends on how much HEADROOM there
        // is, not merely on being under the line. Sitting a hair under budget
        // is exactly the state the sawtooth lived in -- "healthy" every frame
        // and unable to afford one more step -- so that state must climb
        // slowly or not at all. Real headroom (the kernel got simpler, the
        // view zoomed out, the chain got shorter) is a different situation and
        // should recover in seconds.
        // AMPLE headroom, not merely being under the line. This is the whole
        // fix. At 61 fps against a 60 fps goal the engine is "healthy" every
        // frame and has about 2% to spend, while one resolution step costs
        // ~8% of the pixels -- so climbing from there is guaranteed to
        // overshoot, cut, and climb again. That is the sawtooth. Requiring
        // real headroom means the marginal state simply HOLDS, which is what
        // a performer wants: a steady picture, not a governor hunting.
        if (ema_ < target_ * kAmpleHeadroom) {
            if (++healthyStreak_ >= kFramesToRaiseCeiling) {
                healthyStreak_ = 0;
                resCeiling_ = std::min(1.0f, resCeiling_ + 0.04f);
            }
        } else {
            healthyStreak_ = 0;
        }
    } else {
        healthyStreak_ = 0;
    }

    if (ema_ > target_ * kMissThreshold) {
        underRun_ = 0;
        if (++overRun_ >= kOverFramesToCut) {
            overRun_ = 0;
            // Iteration depth goes first: it is the dominant cost, and losing
            // it softens the image rather than pixelating it.
            // Order matters: give up the cheapest-looking thing first.
            // Iteration depth softens the image, thinning the swarm sparses
            // it, and resolution is the last resort because it costs
            // everything at once.
            if (detailEffective_ && detailScale_ > 0.35f) {
                detailScale_ = std::max(0.35f, detailScale_ - 0.08f);
            } else if (countEffective_ && countScale_ > 0.2f) {
                countScale_ = std::max(0.2f, countScale_ - 0.08f);
            } else if (resScale_ > 0.4f) {
                resScale_ = std::max(0.4f, resScale_ - 0.08f);
                // Remember that this is as high as we can afford. The
                // restore path may not climb past it again until the engine
                // has been healthy for a long time, which is what turns the
                // sawtooth into converge-and-hold.
                resCeiling_ = resScale_;
                healthyStreak_ = 0;
            }
        }
    } else if (ema_ < target_ * kHealthyMargin) {
        overRun_ = 0;
        if (++underRun_ >= kUnderFramesToRestore) {
            underRun_ = 0;
            // Restore in the reverse order, and slower than we cut, so the
            // system settles instead of oscillating around the threshold.
            if (resScale_ < resCeiling_) {
                resScale_ = std::min(resCeiling_, resScale_ + 0.04f);
            } else if (countEffective_ && countScale_ < 1.0f) {
                countScale_ = std::min(1.0f, countScale_ + 0.04f);
            } else if (detailEffective_ && detailScale_ < 1.0f) {
                detailScale_ = std::min(1.0f, detailScale_ + 0.04f);
            }
        }
    } else {
        // Between the two thresholds: keeping up, but without margin. Hold
        // where we are rather than hunting.
        overRun_ = 0;
        underRun_ = 0;
    }
}

}  // namespace nv
