// trumpet_synth.h -- HYPERTRUMPET V2 simulator: synthetic Bb trumpet (sim only, never part of the firmware).
// Valve logic: open series of Bb2 (116.54 Hz) partials 2..9; valve2 = -1 st, valve1 = -2, valve3 = -3 (combos add).
// Continuous Hall depth: >=0.5 acts as pressed (with glide); below that a half-valve bend (flat + airy).
#pragma once
#include <math.h>
#include <stdint.h>

namespace sim {

struct TrumpetSynth {
    static const int NH = 16;
    // controls (set from the UI)
    float valve[3] = {0, 0, 0};   // depth 0..1
    int   partial  = 4;           // 2..9
    float breath   = 0.f;         // 0..1 (already gated by the host)
    float vibRate  = 5.2f;        // Hz
    float vibDepth = 0.15f;       // 0..1 (1 = +-0.5 semitone)
    bool  quirks   = true;        // intonation quirks of real fingerings

    void init(float sampleRate);
    void tongue();                                    // retrigger the attack
    void process(float* out, int n);                  // mono, adds nothing, writes out[]
    float f0() const { return gate_ > 0.02f ? freq_ : 0.f; }   // hint a real instrument derives from valves + tracker
    float targetHz() const { return target_; }

private:
    float sr_ = 48000.f;
    float freq_ = 233.f, target_ = 233.f;
    float phase_ = 0.f, vibPh_ = 0.f;
    float amp_ = 0.f, gate_ = 0.f;
    float duck_ = 0.f;            // seconds remaining of tongue closure
    float ampH_[NH];              // current per-harmonic amplitude
    float lp1_ = 0.f, nbp1_ = 0.f, nbp2_ = 0.f, shelf_ = 0.f;
    uint32_t rng_ = 22222u;
    float eff(float d) const;
    float semis() const;
};

} // namespace sim
