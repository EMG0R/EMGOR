// amoled_bringup.ino — hardware bring-up test for the Waveshare
// ESP32-S3-Touch-AMOLED-1.43 (466x466 round QSPI AMOLED).
//
// Purpose: prove the NEW board's display + touch light up BEFORE porting the
// whole watch app. Pins are from Waveshare's own demo (07_LVGL_Test/lcd_config.h):
//   QSPI: CS=9 CLK=10 D0=11 D1=12 D2=13 D3=14 RST=21   (no backlight; brightness=0x51)
//   Touch FT3168 @0x38 on I2C SDA=47 SCL=48            (also QMI8658 IMU + PCF85063 RTC)
//   No AXP2101 power-up needed (vendor LVGL demo lights the panel with just QSPI+RST).
//
// The panel is either SH8601 (col offset 0) or CO5300 (col offset 6), auto-detected
// on the real board via reg 0xDA. Arduino_GFX picks at COMPILE time, so we default
// to CO5300; if the image is shifted ~6px or blank, flip WATCH_PANEL_SH8601 to 1.

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

#define WATCH_PANEL_SH8601 1   // 0 = CO5300 (offset 6), 1 = SH8601 (offset 0)

#define LCD_CS 9
#define LCD_SCK 10
#define LCD_D0 11
#define LCD_D1 12
#define LCD_D2 13
#define LCD_D3 14
#define LCD_RST 21
#define TP_SDA 47
#define TP_SCL 48
#define FT3168_ADDR 0x38

static Arduino_DataBus *bus =
    new Arduino_ESP32QSPI(LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
#if WATCH_PANEL_SH8601
static Arduino_SH8601 *gfx = new Arduino_SH8601(bus, LCD_RST, 0, 466, 466, 0, 0, 0, 0);
static const char *PANEL_NAME = "SH8601";
#else
static Arduino_CO5300 *gfx = new Arduino_CO5300(bus, LCD_RST, 0, 466, 466, 6, 0, 0, 0);
static const char *PANEL_NAME = "CO5300";
#endif

// Read one touch point from the FT3168 (standard FTx capacitive register map:
// 0x02 = #points, 0x03/04 = X hi/lo, 0x05/06 = Y hi/lo; coords are 12-bit).
static bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(FT3168_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(FT3168_ADDR, 5) != 5) return false;
  uint8_t n  = Wire.read();   // 0x02 : number of touch points (low nibble)
  uint8_t xh = Wire.read();   // 0x03
  uint8_t xl = Wire.read();   // 0x04
  uint8_t yh = Wire.read();   // 0x05
  uint8_t yl = Wire.read();   // 0x06
  if ((n & 0x0F) == 0) return false;
  x = ((uint16_t)(xh & 0x0F) << 8) | xl;
  y = ((uint16_t)(yh & 0x0F) << 8) | yl;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n[bringup] AMOLED-1.43 test, panel=%s\n", PANEL_NAME);

  Wire.begin(TP_SDA, TP_SCL, 400000);

  if (!gfx->begin(40000000)) {
    Serial.println("[bringup] gfx->begin FAILED (check QSPI pins/panel)");
  } else {
    Serial.println("[bringup] gfx->begin OK");
  }
  gfx->setBrightness(255);

  // Color sweep so it's obvious the panel is alive.
  gfx->fillScreen(RGB565_RED);   delay(350);
  gfx->fillScreen(RGB565_GREEN); delay(350);
  gfx->fillScreen(RGB565_BLUE);  delay(350);
  gfx->fillScreen(RGB565_BLACK);

  // Round-face reference: outline the circular edge + centered text.
  gfx->drawCircle(233, 233, 232, RGB565_WHITE);
  gfx->drawCircle(233, 233, 231, RGB565_WHITE);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(4);
  gfx->setCursor(120, 180);
  gfx->println("IT LIVES");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(150, 240);
  gfx->printf("%s 466x466", PANEL_NAME);
  gfx->setCursor(120, 275);
  gfx->setTextColor(RGB565_MAGENTA);
  gfx->println("touch to draw");
}

void loop() {
  uint16_t x, y;
  if (readTouch(x, y)) {
    gfx->fillCircle(x, y, 6, RGB565_YELLOW);
    Serial.printf("[touch] %u,%u\n", x, y);
  }
  delay(12);
}
