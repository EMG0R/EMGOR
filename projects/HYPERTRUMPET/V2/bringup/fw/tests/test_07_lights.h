// TEST 07 (fw 1.0): everything in test 06 (unchanged) + FCOB strip (DIN on D29 / PB14 = SPI2 MISO with IOSWP, through a 74AHCT125 and 330 ohm)
// + NeoTrellis 4x4 (seesaw 0x2E on the existing I2C bus) + audio-reactive lava-lamp lights (line in L drives them, audio passes through).
#pragma once
#include "test_06_pots2w_enc.h"
#define TEST_LIGHTS 1
#define TEST_L_STRIP 1
#define TEST_L_AUDIO 1
#define TEST_L_TRELLIS 1
