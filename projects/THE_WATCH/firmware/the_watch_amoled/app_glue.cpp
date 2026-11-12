// app_glue.cpp — bridge between the vendor esp_lcd/LVGL bring-up (lcd_bsp.c)
// and THE WATCH application UI (src/ui, src/theme).
//
// lcd_bsp.c's lcd_lvgl_Init() creates the LVGL display + FT3168 touch indev,
// spawns the LVGL FreeRTOS pump task, then (inside the LVGL lock) calls
// app_ui_create(). We use that single hook to stand up the whole watch UI, so
// the known-good vendor display/touch/LVGL path is left completely untouched.
//
// NOTE: the vendor has ALREADY registered lv_disp + the touch indev by the time
// this runs, so we must NOT call the_watch's display_init()/touch_init().

#include <lvgl.h>

#include "src/theme.h"
#include "src/ui/ui.h"

extern "C" void app_ui_create(void) {
  // theme_init() refines the already-registered default display's theme.
  theme_init(lv_disp_get_default());
  // ui_init() builds the tileview/pages on lv_scr_act() (the vendor's screen).
  ui_init();
  // ui_start_timers() installs the 150 ms UI pump as an lv_timer, which the
  // vendor's LVGL task services — no extra pumping needed in loop().
  ui_start_timers();
}
