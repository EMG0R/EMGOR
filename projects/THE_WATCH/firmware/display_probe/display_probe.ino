// display_probe.ino — ESP-side check for whether the AMOLED panel is actually
// connected. Uses Waveshare's own bit-bang QSPI read to read THREE display-
// controller ID registers (0xDA/0xDB/0xDC) plus RDID (0x04).
//
//   * A real, connected panel returns DIFFERENT, sensible bytes on those regs.
//   * A disconnected/dead panel floats the data line -> reads 0xFF on ALL of
//     them (or 0x00). A genuine controller cannot return 0xFF on all three.
//
// This is the definitive "can the ESP see the screen" test.

#include <Arduino.h>
extern "C" {
#include "read_lcd_id_bsp.h"
uint8_t SPI_ReadData_Continue(void);   // defined (non-static) in read_lcd_id_bsp.c
}
#include "lcd_config.h"

static uint8_t readReg(uint8_t reg) {
  SPI_ReadComm(reg);
  return SPI_ReadData_Continue();
}

static void probe(bool banner) {
  uint8_t da = read_lcd_id();          // does panel reset + reads 0xDA
  uint8_t db = readReg(0xDB);
  uint8_t dc = readReg(0xDC);
  uint8_t r04 = readReg(0x04);
  Serial.printf("[probe] IDs: 0xDA=0x%02X  0xDB=0x%02X  0xDC=0x%02X  0x04=0x%02X\n",
                da, db, dc, r04);
  bool allFF = (da == 0xFF && db == 0xFF && dc == 0xFF && r04 == 0xFF);
  bool all00 = (da == 0x00 && db == 0x00 && dc == 0x00 && r04 == 0x00);
  if (allFF)      Serial.println("[probe] VERDICT: all 0xFF -> PANEL NOT RESPONDING (flex disconnected / dead)");
  else if (all00) Serial.println("[probe] VERDICT: all 0x00 -> PANEL NOT RESPONDING");
  else            Serial.println("[probe] VERDICT: varied ID bytes -> PANEL IS ELECTRICALLY PRESENT (screen should be fixable in software)");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[probe] AMOLED display presence probe (reads controller ID regs over QSPI)");
  probe(true);
}

void loop() {
  probe(false);   // repeat so a serial capture always catches it
  delay(1500);
}
