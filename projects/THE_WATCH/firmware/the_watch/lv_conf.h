// lv_conf.h — LVGL v8 configuration for THE WATCH.
//
// PLACEMENT: LVGL looks for lv_conf.h one directory ABOVE the lvgl library
// folder. With the Arduino IDE that means copy this file to:
//     <Arduino>/libraries/lv_conf.h
// (i.e. next to the `lvgl` library folder, NOT inside it). This copy in the
// sketch is the source of truth — keep them identical. Alternatively define
// LV_CONF_PATH / LV_CONF_INCLUDE_SIMPLE in your build.
//
// Only the settings that matter for this firmware are called out; everything
// else is left at LVGL defaults.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// 16-bit color, byte-swapped for SPI panels driven via Arduino_GFX.
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   1

// Memory for LVGL objects. 48 KB is comfortable for these pages on the S3.
#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (72U * 1024U)

// Tick is provided by our esp_timer via lv_tick_inc() (see hal/display.cpp).
#define LV_TICK_CUSTOM     0

// --- Fonts: these MUST match the pointers in src/theme.cpp ---
// Bigger type everywhere for the redesign; 48 is the largest built-in Montserrat.
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_18  1   // small / captions
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_24  1   // medium / body labels
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_34  1   // large / track title / weather text
#define LV_FONT_MONTSERRAT_40  1   // headline temp
#define LV_FONT_MONTSERRAT_48  1   // big clock digits (largest built-in)
#define LV_FONT_DEFAULT        &lv_font_montserrat_24

// Widgets used by the UI.
#define LV_USE_TILEVIEW    1
#define LV_USE_LABEL       1
#define LV_USE_BTN         1
#define LV_USE_METER       1     // analog watch face (ticks + hour/min/sec needles)
#define LV_LABEL_TEXT_SELECTION 0
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1     // start from a dark base; our theme refines it

// Enable the symbols we use (bluetooth, play/pause/prev/next, call).
#define LV_FONT_FMT_TXT_LARGE 0

#endif // LV_CONF_H
