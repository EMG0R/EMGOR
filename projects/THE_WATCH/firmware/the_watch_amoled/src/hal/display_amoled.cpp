// hal/display_amoled.cpp — display HAL shim for the vendor esp_lcd path.
//
// On this build the panel/LVGL bring-up is done by the vendor lcd_bsp.c
// (esp_lcd SH8601/CO5300 QSPI + LVGL), NOT by the_watch's Arduino_GFX
// display.cpp (which the user reported "didn't work" on this board). So we do
// NOT implement display_init() here — nothing calls it (app_glue drives the
// vendor lcd_lvgl_Init() instead). We only provide the small size/backlight
// accessors that ui.cpp / theme.cpp / starfield.cpp link against.
#include "display.h"
#include "../config.h"

// Logical resolution the UI lays out against: the 1.43" AMOLED is 466x466.
int display_width()  { return WATCH_LCD_WIDTH;  }   // 466
int display_height() { return WATCH_LCD_HEIGHT; }   // 466

// AMOLED brightness on this panel is controller command 0x51, driven inside the
// vendor init (lcd_bsp.c sets it to full). The vendor BSP does not expose a
// runtime brightness setter, so this is a no-op. Kept so the UI links and a
// future brightness hook has a home.
// TODO(hardware): wire this to esp_lcd_panel_io tx of cmd 0x51 if runtime
// dimming is wanted.
void display_set_backlight(uint8_t level) { (void)level; }
