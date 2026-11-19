// TEST 06 (fw 0.9): faders (A3-A5) + 2 pots, 3-wire (A6, A7, longest ADC sample time) + FSR (A8, pulldown to ground) + joystick (A9, A10, SW D10)
// + 3 Hall valve sensors (A0, A1, A2) + 2 encoders (ENC1 A D1, B D2, SW D3; ENC2 A D4, B D5, SW D6) + I2C (MPU6050, 1 x VL53L1X, SHUT D7).
#pragma once
#include "channels.h"
#define TEST_CHANNELS(X) CH_FADERS(X) CH_POTS(X) CH_FSR(X) CH_JOY(X) CH_HALLS(X)
#define TEST_I2C 1
#define TEST_ENC 1
#define TEST_JOY 1   // KY-023: VRx A9/D24, VRy A10/D25, SW D10 (pull-up, active low), +5V pin to 3V3 analog, GND to DGND
#define TEST_HALL 1  // 49E x3: OUT to A0/D15, A1/D16, A2/D17
