// hal/display.h — Display Hardware Abstraction Layer.
//
// The UI layer (LVGL) never talks to the panel directly; it only knows there
// is "a display of some size". To port to a different panel (e.g. the 1.43"
// 466x466 AMOLED) you reimplement display.cpp — the UI is untouched.
#pragma once

#include <lvgl.h>

// Bring up SPI panel + LVGL draw buffers + display driver + tick source.
// Returns the LVGL display handle (for theme_init).
lv_disp_t *display_init();

// Backlight 0..255 (this board is on/off in practice; PWM optional).
void display_set_backlight(uint8_t level);

// Logical resolution the UI should lay out against.
int display_width();
int display_height();
