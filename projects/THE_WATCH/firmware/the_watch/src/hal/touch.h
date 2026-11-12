// hal/touch.h — Touch Hardware Abstraction Layer (CST816T capacitive).
//
// Registers an LVGL input device so the UI gets swipe/tap for free. The UI
// never references CST816 directly. Porting = swap the driver in touch.cpp.
#pragma once

#include <lvgl.h>
#include <Wire.h>

// Initialise the CST816 on the shared I2C bus and register the LVGL indev.
// `wire` must already be Wire.begin()'d (see hal/imu or the sketch).
void touch_init(TwoWire &wire);
