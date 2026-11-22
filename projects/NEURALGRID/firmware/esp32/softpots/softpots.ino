// neuralGrid — softpots
// ESP32 dedicated to the 4x SoftPot ribbons (north/south/east/west), each
// wired directly to its own ADC1 pin — no mux needed at just 4 sensors.
// Sends readings to the Pi over USB-serial. The Pico is untouched and keeps
// driving the NeoTrellis grid independently (separate USB-serial link).
//
// Board: any ESP32 dev board (e.g. "ESP32 Dev Module")
// WiFi/Bluetooth: left OFF. Only ADC1 pins are used (ADC2 shares hardware
// with the WiFi radio and gets unreliable if WiFi is ever active — since
// WiFi is off here it wouldn't matter, but ADC1 keeps this true even if
// WiFi gets added later for something else, so leaving margin costs nothing.

// Pins — ADC1 channels only.
#define SOFTPOT1_PIN 32  // north, ADC1_CH4
#define SOFTPOT2_PIN 33  // south, ADC1_CH5
#define SOFTPOT3_PIN 34  // east,  ADC1_CH6
#define SOFTPOT4_PIN 35  // west,  ADC1_CH7

#define N_SOFTPOTS 4
const uint8_t softpotPin[N_SOFTPOTS] = { SOFTPOT1_PIN, SOFTPOT2_PIN, SOFTPOT3_PIN, SOFTPOT4_PIN };
int softpotValue[N_SOFTPOTS] = { 0 };  // raw ADC, 0..4095 (12-bit)

void scanSoftpots() {
  for (int i = 0; i < N_SOFTPOTS; i++) {
    softpotValue[i] = analogRead(softpotPin[i]);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);       // 0..4095 full range
  analogSetAttenuation(ADC_11db); // lets the input read the full 0..~3.3V span
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 20) return;   // ~50Hz scan
  last = now;

  scanSoftpots();

  Serial.print("softpot: ");
  for (int i = 0; i < N_SOFTPOTS; i++) {
    Serial.print(softpotValue[i]);
    Serial.print(i < N_SOFTPOTS - 1 ? "," : "\n");
  }
}
