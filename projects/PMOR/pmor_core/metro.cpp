// metro.cpp — see metro.h. AUDIO plane except metroInit().
#include <math.h>
#include "metro.h"
#include "core_config.h"

MetroState metro;

// Click design (measured, not guessed):
//   body   = sine, 1.20 kHz on a beat, 1.80 kHz on the bar's downbeat (a pitch step reads as "one" even at low level)
//   decay  = x *= 0.9955/sample  -> tau = 222 samples = 4.6 ms @48k; < -60 dB after ~31 ms, so it can never smear a 16th at 300 BPM (50 ms)
//   peak   = level * 0.20 (beat); downbeat is 1.5x that. Default level 0.25 => 0.050 (-26.0 dBFS) / 0.075 (-22.5 dBFS).
//            The engines' own peaks run ~0.3..1.0 before the master stage, i.e. the click sits >= 20 dB under any voice.
static const float CLICK_DECAY   = 0.9955f;
static const float CLICK_BEAT_HZ = 1200.f;
static const float CLICK_DOWN_HZ = 1800.f;
static const float CLICK_PEAK    = 0.20f;
static const float CLICK_ACCENT  = 1.5f;

void metroInit() {
    metro.level = 0.25f;
    metro.armed = false;
    metro.env = 0.f; metro.phase = 0.f; metro.inc = 0.f; metro.amp = 0.f;
}

// armed mirrors record mode exactly (rec_on of any lane), refreshed every audio block: instant on, and on
// record-off it only stops NEW clicks -- metroProcess() keeps rendering the click already in flight, so no cut.
void metroBlock(bool any_lane_recording) {
    metro.armed = any_lane_recording;
}

void metroStep(uint32_t gstep, uint32_t bar_steps) {
    if(!metro.armed || metro.level <= 0.f) return;
    if((gstep & 3u) != 0u) return;                  // a beat = 4 sixteenths
    if(bar_steps < 4u) bar_steps = 16u;
    bool down = (gstep % bar_steps) == 0u;
    metro.amp   = metro.level * CLICK_PEAK * (down ? CLICK_ACCENT : 1.f);
    metro.inc   = (down ? CLICK_DOWN_HZ : CLICK_BEAT_HZ) / SR;
    metro.phase = 0.f;
    metro.env   = 1.f;
}

void metroProcess(float* L, float* R, size_t n) {
    if(metro.env < 1.0e-4f) { metro.env = 0.f; return; }
    for(size_t i = 0; i < n; i++) {
        float s = sinf(6.2831853f * metro.phase) * metro.env * metro.amp;
        metro.phase += metro.inc; if(metro.phase >= 1.f) metro.phase -= 1.f;
        metro.env *= CLICK_DECAY;
        L[i] += s; R[i] += s;                        // centre: no pan, never fights the (mirrored) engine panning
    }
}
