// TEST 10 (fw 1.3): test 08 (full build) + Valhalla-style delay in the audio path: mic in L -> delay -> output gain -> out L/R.
// Time and modulation from MPU6050 accel Y, dry/wet from fader 2, all three faders remapped so the noisy bottom 30% reads 0.
#pragma once
#include "test_08_mic.h"
#define TEST_DELAY 1
