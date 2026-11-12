#include "touch.h"
#include "../config.h"

#include <Arduino.h>
#include <Wire.h>

// FT3168 capacitive touch @0x38, polled over plain I2C (no INT/RST wired on the
// 1.43 AMOLED board). Register map: 0x02 = touch count (low nibble), 0x03..0x06
// = point-1 X/Y as 12-bit big-endian nibble pairs. The chip reports nothing
// until 0x00 is written to reg 0x00 ("normal mode") — vendor Touch_Init does
// exactly that.
static TwoWire *s_wire = nullptr;
static int32_t s_last_x = 0, s_last_y = 0;

static bool ft3168_read(uint16_t &x, uint16_t &y) {
  s_wire->beginTransmission(WATCH_TP_ADDR);
  s_wire->write(0x02);
  if (s_wire->endTransmission(false) != 0) return false;
  if (s_wire->requestFrom((int)WATCH_TP_ADDR, 5) != 5) return false;
  uint8_t n  = s_wire->read();
  uint8_t xh = s_wire->read();
  uint8_t xl = s_wire->read();
  uint8_t yh = s_wire->read();
  uint8_t yl = s_wire->read();
  if ((n & 0x0F) == 0) return false;
  x = ((uint16_t)(xh & 0x0F) << 8) | xl;
  y = ((uint16_t)(yh & 0x0F) << 8) | yl;
  if (x >= WATCH_LCD_WIDTH)  x = WATCH_LCD_WIDTH - 1;
  if (y >= WATCH_LCD_HEIGHT) y = WATCH_LCD_HEIGHT - 1;
  return true;
}

// LVGL polls this to learn pointer state.
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  LV_UNUSED(drv);
  uint16_t x, y;
  if (ft3168_read(x, y)) {
    s_last_x = x; s_last_y = y;
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
    return;
  }
  // Released: LVGL still wants the last known point for click/gesture logic.
  data->point.x = s_last_x;
  data->point.y = s_last_y;
  data->state   = LV_INDEV_STATE_RELEASED;
}

void touch_init(TwoWire &wire) {
  s_wire = &wire;
  // Switch the FT3168 to normal reporting mode — required or it stays silent.
  s_wire->beginTransmission(WATCH_TP_ADDR);
  s_wire->write(0x00);
  s_wire->write(0x00);
  s_wire->endTransmission();

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read_cb;
  // Snappier swiping: start scrolling after a tiny finger movement (default 10
  // px) and make swipe gestures easier to trigger.
  indev_drv.scroll_limit = 2;
  indev_drv.gesture_limit = 20;   // px for a swipe gesture (default 50) — easier
  lv_indev_t *indev = lv_indev_drv_register(&indev_drv);

  // Poll every 10 ms (~100 Hz) so finger movement is tracked finely and swipes
  // register immediately (LVGL's default 30 ms sampling felt sluggish).
  if (indev && indev->driver && indev->driver->read_timer)
    lv_timer_set_period(indev->driver->read_timer, 10);
}
