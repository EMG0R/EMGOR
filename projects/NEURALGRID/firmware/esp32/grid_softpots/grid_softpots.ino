// neuralGrid — grid_softpots
// ESP32 driving BOTH the 8x8 NeoTrellis grid (I2C) and the 4x SoftPot ribbons
// (direct ADC, no mux needed at just 4 sensors). Single board, no Pico.
//
// Behavior: press a pad -> it lights solid warm white (like the old Pico test).
// Idle: grid split into 4 quadrants (TL/TR/BL/BR), one softpot each, each its
// own bright solid color. Each quadrant shows a single lit column that slides
// left/right across its own 4 columns as that softpot's reading moves.
//
// Board: any ESP32 dev board (e.g. "ESP32 Dev Module")
// I2C: Wire, SDA = GPIO21, SCL = GPIO22 (ESP32 defaults)
// Softpots: direct ADC1 pins, GPIO32/33/34/35 — see SOFTPOTS_WIRING.md

#include "Adafruit_NeoTrellis.h"
#include <Adafruit_NeoPixel.h>

#define X_DIM 8
#define Y_DIM 8

// 2x2 array of boards, matching physical placement: [row][col]
//   0x2E (TL)  0x2F (TR)
//   0x30 (BL)  0x31 (BR)
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {
  { Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) },
  { Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x31) }
};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

bool held[Y_DIM][X_DIM] = { { false } };
bool dirty = false;

const uint8_t BRIGHTNESS = 255;              // driver brightness; idle dimness comes from color "value" below
const uint8_t HELD_R = 255, HELD_G = 236, HELD_B = 205;  // warm near-white, held pad

// ── SoftPots — direct ADC pins, no mux (only 4 sensors) ─────────────────────
#define SOFTPOT1_PIN 32  // north
#define SOFTPOT2_PIN 33  // south
#define SOFTPOT3_PIN 34  // east
#define SOFTPOT4_PIN 35  // west

#define N_SOFTPOTS 4
const uint8_t softpotPin[N_SOFTPOTS] = { SOFTPOT1_PIN, SOFTPOT2_PIN, SOFTPOT3_PIN, SOFTPOT4_PIN };
int softpotValue[N_SOFTPOTS] = { 0 };  // raw ADC, 0..4095 (12-bit), noise-gated

// Noise gate: a SoftPot's wiper is a bare floating pin with nothing touching
// it — no contact at all until pressed — so an untouched channel reads pure
// electrical noise, not a stable low value. A real finger sliding can't move
// the reading more than ~MAX_JUMP counts in one 20ms tick; noise routinely
// jumps almost the full 0..4095 range instantly. Reject any single-sample
// jump bigger than that as noise and hold the last accepted value instead.
const int MAX_JUMP = 700;

void scanSoftpots() {
  for (int i = 0; i < N_SOFTPOTS; i++) {
    int raw = analogRead(softpotPin[i]);
    if (abs(raw - softpotValue[i]) <= MAX_JUMP) {
      softpotValue[i] = raw;
    }  // else: noise spike, keep last accepted value
  }
}

// One bright solid color per band/softpot.
const uint32_t BAND_COLOR[4] = {
  0x00FF0000,  // rows 0-1 / softpot1 (north) — red
  0x0000FF00,  // rows 2-3 / softpot2 (south) — green
  0x000060FF,  // rows 4-5 / softpot3 (east)  — blue
  0x00FFD500,  // rows 6-7 / softpot4 (west)  — yellow
};

// 8x8 split into 4 full-width horizontal bands (2 rows each) so every
// softpot's sliding indicator uses the FULL 8-column width — showing its
// whole 0..4095 range at full grid resolution instead of being cramped into
// a single 4x4 NeoTrellis quadrant.
uint8_t bandOf(uint8_t y) {
  return y / 2;
}

uint32_t bandColor(uint8_t x, uint8_t y) {
  uint8_t b = bandOf(y);
  int litCol = map(softpotValue[b], 0, 4095, 0, 7);
  litCol = constrain(litCol, 0, 7);
  return (x == litCol) ? BAND_COLOR[b] : 0x00000000;
}

// Key event callback — runs synchronously inside trellis.read().
TrellisCallback onKey(keyEvent evt) {
  uint8_t x = evt.bit.NUM % X_DIM;
  uint8_t y = evt.bit.NUM / X_DIM;
  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    held[y][x] = true;
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
    held[y][x] = false;
  }
  dirty = true;  // render this frame now, don't wait for the tick
  return 0;
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  // GPIO32/33 support an internal pulldown (helps settle the noise floor when
  // untouched). GPIO34/35 are input-only pins with NO internal pull resistors
  // at all — that's an ESP32 hardware limit, not fixable in software; if their
  // noise gate isn't enough, add an external ~100k resistor from wiper to GND.
  pinMode(SOFTPOT1_PIN, INPUT_PULLDOWN);
  pinMode(SOFTPOT2_PIN, INPUT_PULLDOWN);

  Wire.begin(21, 22);
  Wire.setClock(800000);  // 800kHz — matches the working Pico config; drop to 400000 if the bus is flaky

  if (!trellis.begin()) {
    Serial.println("NeoTrellis not found — check addresses / wiring / power.");
    while (1) delay(10);
  }

  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(BRIGHTNESS);

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, onKey);
    }
}

void loop() {
  trellis.read();  // fires onKey() for any new press/release

  static uint32_t lastSoftpot = 0;
  uint32_t nowSoftpot = millis();
  if (nowSoftpot - lastSoftpot >= 20) {
    lastSoftpot = nowSoftpot;
    scanSoftpots();
    Serial.print("softpot: ");
    for (int i = 0; i < N_SOFTPOTS; i++) {
      Serial.print(softpotValue[i]);
      Serial.print(i < N_SOFTPOTS - 1 ? "," : "\n");
    }
  }

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 20 && !dirty) return;  // ~50fps tick, or immediately on a key edge
  dirty = false;
  last = now;

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      uint32_t color;
      if (held[y][x]) {
        color = ((uint32_t)HELD_R << 16) | ((uint32_t)HELD_G << 8) | HELD_B;
      } else {
        color = bandColor(x, y);
      }
      trellis.setPixelColor(x, y, color);
    }
  trellis.show();
}
