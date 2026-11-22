// board_test.ino — blank-slate liveness test for the ESP32-S3-Touch-AMOLED-1.43.
// Scans the I2C bus (SDA=47, SCL=48) for the onboard chips. If the IMU / RTC /
// touch answer but the display stayed black, the board is alive and the AMOLED
// panel itself is the fault (dead or unseated flex) — i.e. a hardware return,
// not a firmware problem.

#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 47
#define I2C_SCL 48

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[board_test] I2C scan on SDA=47 SCL=48 @100kHz");
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
}

void loop() {
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[board_test] FOUND 0x%02X", a);
      if (a == 0x38) Serial.print("  <- FT3168 touch");
      if (a == 0x15) Serial.print("  <- CST816 touch");
      if (a == 0x6B) Serial.print("  <- QMI8658 IMU");
      if (a == 0x6A) Serial.print("  <- QMI8658 IMU (alt)");
      if (a == 0x51) Serial.print("  <- PCF85063 RTC");
      if (a == 0x34) Serial.print("  <- AXP2101 PMIC");
      Serial.println();
      found++;
    }
  }
  Serial.printf("[board_test] scan complete: %d device(s). (0 = I2C dead too)\n", found);
  delay(2500);
}
