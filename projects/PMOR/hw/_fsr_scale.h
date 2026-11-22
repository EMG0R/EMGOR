// _fsr_scale.h — canonical FSR conditioning for PMOR.
//
// HARDWARE (locked 2026-09-12, measured by feel on the real panel):
//   Large FSR under the button panel, pulldown = 330R.
//     3V3 -> FSR -> D15 (A0) -> 330R -> DGND   (+100nF D15 to GND)
//   A big FSR spreads force over a large area, so its resistance under a
//   normal button press drops to a few hundred ohms. That is why the usual
//   1.5k / 4.7k pulldowns all pinned near full scale on the lightest touch --
//   the pulldown has to be SMALLER than the FSR at the force you care about.
//
// THE CEILING (this is the important part):
//   Full output must be reached at a firm-but-comfortable press, NOT at the
//   hardest press the sensor can register. Otherwise the top of the range is
//   only reachable by mashing it, and there is no room left for aftertouch
//   above a normal keypress.
//
//   Ceiling raised to 16/16 (full measured span) on 2026-09-13 -- 13/16 still
//   felt too sensitive on the real panel. Full output now needs the hardest
//   press the sensor has actually seen.
//
// LOCKED by feel on the real panel: 330R + CUBED curve + 16/16 ceiling.
// Do not "improve" these without testing on hardware; they were arrived at
// empirically after 4.7k / 1.5k / linear / squared all felt like hair triggers.
//
// USAGE
//   static FsrScale fsr;
//   float v = fsr.Process(raw12);   // raw12 = adc.GetFloat(ch) * 4095
//   // v is 0..1, curved, ready for velocity / aftertouch / modulation.
//   fsr.Reset();                    // after any wiring or resistor change

#pragma once
#include <stdint.h>

struct FsrScale
{
    /** Fraction of the observed span that counts as "full".
     *  1.0 = the full measured range: you must press as hard as the hardest
     *  press this sensor has seen to reach 1.0. Lower values make it reach
     *  max sooner (13/16 was the previous setting -- felt too sensitive). */
    static constexpr float kCeiling = 16.0f / 16.0f;

    /** Readings at or below this are treated as no touch (0..4095 scale). */
    static constexpr uint16_t kNoise = 120;

    /** Response curve. An FSR is roughly 1/force, so linear feels like a hair
     *  trigger at any resistor value -- the exponent is what makes it playable.
     *  0 = linear, 1 = squared, 2 = cubed, 3 = ^4.
     *  LOCKED AT 2 (cubed) -- chosen on hardware. */
    uint8_t curve = 2;

    uint16_t lo = 4095;   /**< lowest reading seen while touched */
    uint16_t hi = 0;      /**< highest reading seen while touched */

    void Reset()
    {
        lo = 4095;
        hi = 0;
    }

    /** raw12: 0..4095 (adc.GetFloat(ch) * 4095.f, matching PMOR's scale).
     *  Returns 0..1. */
    float Process(uint16_t raw12)
    {
        if(raw12 <= kNoise) return 0.f;

        if(raw12 < lo) lo = raw12;
        if(raw12 > hi) hi = raw12;

        uint16_t span = (uint16_t)(hi - lo);
        if(span < 64) span = 64;             // until the range is learned

        // Pin full scale at kCeiling of the span, not at the absolute max.
        float top = (float)span * kCeiling;
        if(top < 1.f) top = 1.f;

        float t = (float)(raw12 - lo) / top;
        if(t < 0.f) t = 0.f;
        if(t > 1.f) t = 1.f;                 // clamp -- pressing harder does nothing

        switch(curve)
        {
            case 1: t = t * t; break;
            case 2: t = t * t * t; break;
            case 3: t = t * t * t * t; break;
            default: break;
        }
        return t;
    }
};
