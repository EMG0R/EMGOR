// hello_vendor.ino — "Hello World" + touch sensing for the Waveshare
// ESP32-S3-Touch-AMOLED-1.43 (466x466 round AMOLED, CNC metal case).
//
// Built VERBATIM on Waveshare's own 07_LVGL_Test demo (ESP32-S3-AMOLED-1.43-Demo-V3):
// esp_lcd_sh8601.c (drives SH8601 and CO5300, auto-detected via reg 0xDA),
// read_lcd_id_bsp.c, lcd_bsp.c (QSPI CS=9 CLK=10 D0-3=11-14 RST=21, LVGL glue),
// FT3168.cpp touch (I2C SDA=47 SCL=48 @0x38). The ONLY change from the vendor demo:
// lv_demo_widgets() is replaced by hello_ui_create() below.

#include "lcd_bsp.h"
#include "FT3168.h"
#include <lvgl.h>

static lv_obj_t *coords_label;

static void touch_poll_cb(lv_timer_t *t) {
  uint16_t x, y;
  if (getTouch(&x, &y)) {
    lv_label_set_text_fmt(coords_label, "touch  %u , %u", x, y);
    Serial.printf("[touch] %u,%u\n", x, y);
  }
}

extern "C" void hello_ui_create(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  lv_obj_t *hello = lv_label_create(scr);
  lv_label_set_text(hello, "Hello World");
  lv_obj_set_style_text_font(hello, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(hello, lv_color_white(), 0);
  lv_obj_align(hello, LV_ALIGN_CENTER, 0, -30);

  coords_label = lv_label_create(scr);
  lv_label_set_text(coords_label, "touch the screen");
  lv_obj_set_style_text_font(coords_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(coords_label, lv_palette_main(LV_PALETTE_CYAN), 0);
  lv_obj_align(coords_label, LV_ALIGN_CENTER, 0, 40);

  lv_timer_create(touch_poll_cb, 30, NULL);
}

extern "C" uint8_t get_lcd_id(void);
extern "C" int *get_lcd_errs(void);

void setup() {
  Serial.begin(115200);
  // LCD_PWREN (GPIO42): panel power enable — from Waveshare's 09_FactoryProgram.
  // Without this the AMOLED is completely unpowered (no init response, ID reads 0xFF).
  pinMode(42, OUTPUT);
  digitalWrite(42, HIGH);
  delay(30);
  Touch_Init();
  lcd_lvgl_Init();
  Serial.println("[hello_vendor] up");
}

void loop() {
  static const char *step[] = {"spi_bus_init", "panel_io_spi", "new_panel", "panel_reset", "panel_init", "disp_on"};
  uint8_t id = get_lcd_id();
  int *e = get_lcd_errs();
  Serial.printf("[diag] lcd_id=0x%02X (%s)  ", id,
                id == 0x86 ? "SH8601" : (id == 0xFF ? "CO5300" : "UNKNOWN!"));
  for (int i = 0; i < 6; i++) Serial.printf("%s=%d ", step[i], e[i]);
  Serial.println();
  delay(2000);
}
