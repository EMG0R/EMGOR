// ui/ui.h — LVGL UI (pages + swipe navigation), isolated from HAL/BLE.
//
// Pages are laid out in a horizontal lv_tileview; swipe left/right to move
// between them. Default (boot) page is the clock.
//
//   [0] Clock  ->  [1] Weather  ->  [2] Music  ->  [3] Notifications  ->  [4] Menu
//
// The UI reads the shared WatchState; it calls into the BLE layer only for
// user actions (music control, dismiss, find-phone).
#pragma once

#include <lvgl.h>

// Build all pages. Call after theme_init().
void ui_init();

// Called periodically from loop() (via an internal lv_timer, so loop() only
// needs lv_timer_handler()). Pushes WatchState changes into widgets and ticks
// the clock. Safe: runs in the LVGL/loop context.
void ui_start_timers();
