// TEST 12 (fw drone12-1.0): ONE effect. The 08_mic base (sensors, audio in -> mono out, output gain, strip lava) + the drone only, on ONE fader (DRONE_FADER in fx_controls_d12.h).
// The only DSP linked is the drone (fx_drone.h + fx_fft.h): no template glue, no registry, no other effect, no pads (the Trellis LEDs stay off).
#pragma once
#include "test_08_mic.h"
#define TEST_DELAY 1
#define HX_MIN_DRONE 1
#define HX_D12 1
