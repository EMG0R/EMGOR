// TEST 08 (fw 1.1): the FULL build. Everything in test 06 (faders, pots, FSR, joystick, valves, encoders, IMU, lidar) + strip (4-bit WS encoding) + NeoTrellis +
// audio-reactive lights + mic level meter. Mic in L goes dry to out L and out R. ADC safety under audio: see main.cpp (ADC started after the audio settles,
// recalibrated once, watchdog, VDDA readback).
#pragma once
#include "test_07_lights.h"
#define TEST_MIC 1
