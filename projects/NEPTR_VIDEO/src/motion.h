// The intrinsic-motion clock: the one number every generator's own motion is
// a function of.
//
// Deliberately its own header and free of GL, so the continuity claims below
// can be checked by a test binary rather than by looking at frames and
// believing them.
#pragma once

#include <cmath>

namespace nv {

// The intrinsic-motion clock.
//
// The one job here is that the four numbers handed to the shader are CONTINUOUS
// FOREVER. Two things threaten that and both are handled in one place:
//
//   * float precision. sceneTime is a double and grows without bound; the
//     uniform is a float. Every term below is a sinusoid in `phase` at a
//     rational multiple of one base frequency -- 1, 1/2 and 1/3 -- so the
//     vector is EXACTLY periodic in phase with period 12*pi. Wrapping the
//     accumulator into [0, 12*pi) is therefore not an approximation and not a
//     discontinuity: it lands on a point where the function has the identical
//     value. The float that reaches the GPU is a small bounded number for the
//     life of the process rather than a large one whose ulp eventually exceeds
//     a frame.
//
//   * a jump on a speed change. The phase is INTEGRATED (phase += dt * rate),
//     never computed as wallClock * rate, so changing the rate -- including
//     through zero and into reverse -- changes only the derivative. The picture
//     cannot move when the knob does.
//
// It is deliberately not a member of Renderer: the phase has to survive a
// kernel change, a chain recompile and a preset load, none of which the
// renderer's own state does.
class MotionClock {
public:
    static constexpr double kTwoPi = 6.283185307179586;
    // The full period of the vector. The three rates are 1, 1/2 and 1/3, so
    // they come back into phase together after six laps of the fastest one --
    // and only exactly there, which is why the wrap has to be this and not a
    // round number of seconds.
    static constexpr double kPeriod = 6.0 * kTwoPi;
    // Base rate of the seed circle, in Hz of SCENE time. ~12.5 s per lap: fast
    // enough to be obviously alive on a still frame, slow enough that a long
    // zoom is not fighting it.
    static constexpr double kBaseHz = 0.08;

    // dtScene is already dt * SPEED, so this integrates the same clock the
    // fractal does and reverses with it.
    void advance(double dtScene) {
        phase_ += dtScene * kTwoPi * kBaseHz;
        // Into [0, kPeriod) whichever way it went. fmod alone keeps the sign.
        phase_ = phase_ - kPeriod * std::floor(phase_ / kPeriod);
    }

    double phase() const { return phase_; }
    void setPhase(double p) { phase_ = p - kPeriod * std::floor(p / kPeriod); }

    // amount is the EVOLVE knob. At exactly 0 this returns (0, 0, 0, 0), which
    // every kernel body is written to reduce to identity under.
    //
    // Four oscillators at three different rates rather than one wobble fanned
    // out: kernels pick the component that suits them, so two generators
    // side by side are not breathing in lockstep, and no single beat is
    // audible across the bank.
    void vector(float amount, float out[4]) const {
        out[0] = static_cast<float>(amount * std::cos(phase_));
        out[1] = static_cast<float>(amount * std::sin(phase_));
        out[2] = static_cast<float>(amount * std::sin(phase_ * 0.5));
        out[3] = static_cast<float>(amount * std::sin(phase_ * (1.0 / 3.0)));
    }

private:
    double phase_ = 0.0;
};

}  // namespace nv
