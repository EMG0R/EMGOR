#include "display.h"
#include "../config.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_timer.h>

// --- Panel driver (466x466 round AMOLED over QSPI) --------------------------
// The board ships with either SH8601 or CO5300 (runtime ID reg 0xDA; Arduino_GFX
// picks at COMPILE time). CO5300 is what this unit reports; if a future panel is
// SH8601 flip this to 1 (and the col offset drops to 0 via the classes below).
#define WATCH_PANEL_SH8601 0

static Arduino_DataBus *s_bus = nullptr;
#if WATCH_PANEL_SH8601
static Arduino_SH8601 *s_gfx = nullptr;
#else
static Arduino_CO5300 *s_gfx = nullptr;
#endif

// --- LVGL plumbing ---------------------------------------------------------
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
// SINGLE partial buffer, 1/10 of the frame; synchronous QSPI push in the flush
// keeps it tear-free (same rationale as the 1.69 build).
//   466*466/10 * 2 bytes = ~43 KB internal RAM.
#define LVGL_BUF_PX (WATCH_LCD_WIDTH * WATCH_LCD_HEIGHT / 10)
static lv_color_t s_buf[LVGL_BUF_PX];

static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
  s_gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px, w, h);
#else
  s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px, w, h);
#endif
  lv_disp_flush_ready(drv);
}

// LVGL needs a millisecond tick. Drive it from a hardware timer so animation
// timing is independent of loop() jitter.
static void lv_tick_timer_cb(void *arg) {
  LV_UNUSED(arg);
  lv_tick_inc(5);
}

lv_disp_t *display_init() {
  // Panel power FIRST — without LCD_PWREN high the AMOLED is entirely dead
  // (no init response, floating ID line). From Waveshare's 09_FactoryProgram.
  pinMode(WATCH_LCD_PWREN, OUTPUT);
  digitalWrite(WATCH_LCD_PWREN, HIGH);
  delay(30);

  s_bus = new Arduino_ESP32QSPI(WATCH_LCD_CS, WATCH_LCD_SCK,
                                WATCH_LCD_D0, WATCH_LCD_D1,
                                WATCH_LCD_D2, WATCH_LCD_D3);
#if WATCH_PANEL_SH8601
  s_gfx = new Arduino_SH8601(s_bus, WATCH_LCD_RST, WATCH_LCD_ROTATION,
                             WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT, 0, 0, 0, 0);
#else
  s_gfx = new Arduino_CO5300(s_bus, WATCH_LCD_RST, WATCH_LCD_ROTATION,
                             WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT,
                             WATCH_LCD_COL_OFFSET, WATCH_LCD_ROW_OFFSET, 0, 0);
#endif
  s_gfx->begin(40000000);
  s_gfx->fillScreen(RGB565_BLACK);
  display_set_backlight(WATCH_BL_DEFAULT);

  lv_init();
  lv_disp_draw_buf_init(&s_draw_buf, s_buf, NULL, LVGL_BUF_PX);

  lv_disp_drv_init(&s_disp_drv);
  s_disp_drv.hor_res  = WATCH_LCD_WIDTH;
  s_disp_drv.ver_res  = WATCH_LCD_HEIGHT;
  s_disp_drv.flush_cb = disp_flush_cb;
  s_disp_drv.draw_buf = &s_draw_buf;
  lv_disp_t *disp = lv_disp_drv_register(&s_disp_drv);

  const esp_timer_create_args_t targs = {
      .callback = &lv_tick_timer_cb,
      .name = "lv_tick",
  };
  esp_timer_handle_t th;
  esp_timer_create(&targs, &th);
  esp_timer_start_periodic(th, 5 * 1000);  // 5 ms tick (fewer wakeups = cooler)

  return disp;
}

void display_set_backlight(uint8_t level) {
  // AMOLED has no backlight — brightness is controller command 0x51.
  if (s_gfx) s_gfx->setBrightness(level);
}

int display_width()  { return WATCH_LCD_WIDTH; }
int display_height() { return WATCH_LCD_HEIGHT; }
