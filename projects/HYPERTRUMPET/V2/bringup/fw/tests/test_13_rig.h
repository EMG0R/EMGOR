// TEST 13 (fw rig-2.0): THE NORMAL FULL CODE. 08_mic base + every effect (fx_glue_chain.h): the Trellis is the UTILITY MENU (fx_pads.h: v1 map, harmony page), faders 1 reverb / 2 drone / 3 grain, lava on the
// strip only, strip by DMA. With every fix of the drone12 line: usbReady() null guard + self-heal only when NOT configured, no fader guard, no USB wait before the audio, LED soft start, 1 Hz heartbeat,
// mono sum + output limiter (HX_OUTSTAGE), dry-hold / wake fixes (fx_glue_tmpl.h), FLANGE THRUZ forced to 0 (fx_glue_params.h).
#pragma once
#include "test_08_mic.h"
#define TEST_DELAY 1
#define HX_OUTSTAGE 1
