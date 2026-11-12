// touch_monitor.ino — continuously polls the FT3168 capacitive touch controller
// (I2C 0x38 on SDA=47/SCL=48) and prints every touch over Serial, so we can see
// whether the screen registers a press. If the chip never ACKs, the touch layer
// (which shares the AMOLED flex) is electrically absent — same root cause as the
// black display.

#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 47
#define I2C_SCL 48
#define FT3168  0x38

void setup() {
  Serial.begin(115200);
  delay(400);
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  // REQUIRED (from Waveshare Touch_Init): write 0x00 to reg 0x00 -> normal mode.
  Wire.beginTransmission(FT3168);
  Wire.write(0x00);
  Wire.write(0x00);
  uint8_t e = Wire.endTransmission();
  Serial.printf("\n[touch] FT3168 normal-mode init (write 0x00->0x00) err=%u\n", e);
  Serial.println("[touch] monitor started — TOUCH THE SCREEN");
}

void loop() {
  // Point the FT3168 at its touch-data register (0x02).
  Wire.beginTransmission(FT3168);
  Wire.write(0x02);
  uint8_t err = Wire.endTransmission(false);
  if (err != 0) {
    Serial.printf("[touch] FT3168 @0x38 NOT responding (err=%u) -> touch chip absent/dead\n", err);
    delay(500);
    return;
  }
  if (Wire.requestFrom(FT3168, 5) < 5) {
    Serial.println("[touch] chip ACKed but short read");
    delay(200);
    return;
  }
  uint8_t r[5];
  for (int i = 0; i < 5; i++) r[i] = Wire.read();  // regs 0x02..0x06
  uint8_t fingers = r[0] & 0x0F;
  int x = ((r[1] & 0x0F) << 8) | r[2];
  int y = ((r[3] & 0x0F) << 8) | r[4];
  // Print ONLY when something non-idle appears, plus a 1s idle heartbeat, so a
  // real touch is unmistakable. Raw bytes shown so any change is visible even if
  // the register map differs.
  static uint32_t t = 0;
  if (fingers > 0 || r[0] != 0 || r[1] != 0 || r[3] != 0) {
    Serial.printf("[touch] *** ACTIVITY *** raw=%02X %02X %02X %02X %02X  fingers=%u x=%d y=%d\n",
                  r[0], r[1], r[2], r[3], r[4], fingers, x, y);
  } else if (millis() - t > 1000) {
    t = millis();
    Serial.println("[touch] idle (chip OK, no press) — TOUCH NOW");
  }
  delay(30);
}
