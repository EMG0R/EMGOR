// hello_touch.ino — minimal example for the Waveshare ESP32-S3-Touch-AMOLED-1.43
// (466x466 round QSPI AMOLED, CNC metal case).
//
// All it does: lights the screen, prints "Hello World", and senses touch —
// each touch draws a dot and shows the live x,y under the greeting.
//
// Pins per docs/BOARD_AMOLED_1.43.md (verified against Waveshare's demo):
//   QSPI: CS=9 CLK=10 D0=11 D1=12 D2=13 D3=14 RST=21
//   Brightness is panel command 0x51 (setBrightness) — there is no backlight pin.
//   Touch FT3168 @0x38 on I2C SDA=47 SCL=48 (bus shared with QMI8658 + PCF85063).

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

#define WATCH_PANEL_SH8601 1  // 0 = CO5300 (col offset 6), 1 = SH8601 (offset 0)

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
#else
static Arduino_CO5300 *gfx = new Arduino_CO5300(bus, LCD_RST, 0, 466, 466, 6, 0, 0, 0);
#endif

// FT3168: reg 0x02 = touch count, 0x03..0x06 = point 1 X/Y (12-bit big-endian nibbles).
static bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(FT3168_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(FT3168_ADDR, 5) != 5) return false;
  uint8_t n  = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();
  if ((n & 0x0F) == 0) return false;
  x = ((uint16_t)(xh & 0x0F) << 8) | xl;
  y = ((uint16_t)(yh & 0x0F) << 8) | yl;
  return true;
}

static void drawFace() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->drawCircle(233, 233, 232, RGB565_WHITE);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(5);
  gfx->setCursor(80, 190);
  gfx->println("Hello");
  gfx->setCursor(80, 240);
  gfx->println("World");
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(140, 320);
  gfx->println("touch the screen");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // FT3168 into normal reporting mode (write 0x00 to reg 0x00) — required,
  // the chip won't report touches without it.
  Wire.begin(TP_SDA, TP_SCL, 400000);
  Wire.beginTransmission(FT3168_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();

  if (!gfx->begin(40000000)) {
    Serial.println("[hello] gfx->begin FAILED");
  }
  gfx->setBrightness(255);
  drawFace();
}

void loop() {
  static uint32_t lastTouchMs = 0;
  uint16_t x, y;
  if (readTouch(x, y)) {
    gfx->fillCircle(x, y, 6, RGB565_YELLOW);
    gfx->fillRect(120, 360, 230, 24, RGB565_BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_YELLOW);
    gfx->setCursor(140, 364);
    gfx->printf("touch %u,%u", x, y);
    Serial.printf("[touch] %u,%u\n", x, y);
    lastTouchMs = millis();
  } else if (lastTouchMs && millis() - lastTouchMs > 4000) {
    drawFace();  // clear doodles a few seconds after the last touch
    lastTouchMs = 0;
  }
  delay(12);
}
