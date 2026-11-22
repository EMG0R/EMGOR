// TEST 05: faders + 2 C500K pots (A6/D21 pin 28, A7/D22 pin 29) + FSR (A8/D23 pin 30) + I2C bus on D11/D12 with
// MPU6050 @0x68 and one VL53L1X (SHUT D7 pin 8, stays at 0x29).
#pragma once
#include "channels.h"
#define TEST_CHANNELS(X) CH_FADERS(X) CH_POTS(X) CH_FSR(X)
#define TEST_I2C 1
