// neuralGrid — grid_link
// ESP32 as a DUMB I/O bridge between the 8x8 NeoTrellis grid + 4 softpots and
// the Raspberry Pi. NO musical logic, NO touch detection, NO local animation
// lives here — this firmware only moves bytes: raw key edges + raw ADC up,
// LED instructions down. All intelligence is on the Pi. See the protocol
// contract: ../../../docs/PROTOCOL.md — do not deviate from it.
//
// Board: any ESP32 dev board (e.g. "ESP32 Dev Module")
// I2C: Wire, SDA = GPIO21, SCL = GPIO22 (ESP32 defaults)
// Softpots: direct ADC1 pins, GPIO32/33/34/35 (see README.md for wiring notes)
// Serial: USB CDC, 115200 (framing/CRC per PROTOCOL.md; CDC ignores actual baud)

#include "Adafruit_NeoTrellis.h"
#include <Adafruit_NeoPixel.h>
#include <string.h>

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

const uint8_t DEFAULT_BRIGHTNESS = 255;

// ── SoftPots — direct ADC pins, no mux (only 4 sensors) ─────────────────────
#define SOFTPOT1_PIN 32  // north
#define SOFTPOT2_PIN 33  // south
#define SOFTPOT3_PIN 34  // east
#define SOFTPOT4_PIN 35  // west

#define N_ANALOG 4
const uint8_t softpotPin[N_ANALOG] = { SOFTPOT1_PIN, SOFTPOT2_PIN, SOFTPOT3_PIN, SOFTPOT4_PIN };

// ── Protocol constants (PROTOCOL.md) ────────────────────────────────────────
#define PROTO_VER 1
#define FW_VER    1

// up (ESP32 -> Pi)
#define TYPE_HELLO  0x01
#define TYPE_KEY    0x10
#define TYPE_ANALOG 0x11
// down (Pi -> ESP32)
#define TYPE_LEDS_FULL    0x20
#define TYPE_LEDS_PARTIAL 0x21
#define TYPE_BRIGHTNESS   0x22
#define TYPE_CLEAR        0x23
#define TYPE_PING         0x30

// ── CRC16 / CCITT-FALSE (poly 0x1021, init 0xFFFF) ──────────────────────────
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

// ── COBS encode/decode (classic Jacques Fortier implementation) ────────────
// encode: output buffer must be at least length + ceil(length/254) + 1 bytes.
// Does NOT append the trailing 0x00 delimiter — caller writes that separately.
size_t cobsEncode(const uint8_t *input, size_t length, uint8_t *output) {
  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < length) {
    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  output[code_index] = code;
  return write_index;
}

// decode: input is a full COBS block WITHOUT the trailing 0x00. Returns
// decoded length, or 0 on malformed input (caller drops the frame).
size_t cobsDecode(const uint8_t *input, size_t length, uint8_t *output) {
  size_t read_index = 0;
  size_t write_index = 0;
  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code == 0 || (read_index + code > length && code != 1)) return 0;  // malformed
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      if (read_index >= length) return 0;  // malformed
      output[write_index++] = input[read_index++];
    }
    if (code != 0xFF && read_index != length) {
      output[write_index++] = 0;
    }
  }
  return write_index;
}

// ── TX: build TYPE||BODY||CRC16(LE), COBS-encode, write + 0x00 delimiter ───
#define TX_PAYLOAD_MAX 200                             // covers largest up-frame (ANALOG: ~12B) with headroom
#define TX_ENCODED_MAX (TX_PAYLOAD_MAX + TX_PAYLOAD_MAX / 254 + 2)

void sendFrame(uint8_t type, const uint8_t *body, uint8_t bodyLen) {
  uint8_t payload[TX_PAYLOAD_MAX];
  payload[0] = type;
  if (bodyLen) memcpy(payload + 1, body, bodyLen);
  uint16_t crc = crc16_ccitt_false(payload, 1 + bodyLen);
  payload[1 + bodyLen] = (uint8_t)(crc & 0xFF);
  payload[2 + bodyLen] = (uint8_t)((crc >> 8) & 0xFF);
  size_t payloadLen = (size_t)(3 + bodyLen);

  uint8_t encoded[TX_ENCODED_MAX];
  size_t encLen = cobsEncode(payload, payloadLen, encoded);
  Serial.write(encoded, encLen);
  Serial.write((uint8_t)0x00);
}

void sendHello() {
  uint8_t body[5] = { PROTO_VER, X_DIM, Y_DIM, N_ANALOG, FW_VER };
  sendFrame(TYPE_HELLO, body, sizeof(body));
}

// ── RX: accumulate bytes until 0x00, COBS-decode, verify CRC, dispatch ─────
#define RX_BUF_MAX 220  // largest down-frame is LEDS_FULL: 1+192+2 = 195B pre-COBS
uint8_t rxBuf[RX_BUF_MAX];
size_t rxLen = 0;

void applyLedsFull(const uint8_t *body, size_t len) {
  if (len != 192) return;  // malformed body for this type, drop
  for (int i = 0; i < 64; i++) {
    uint8_t x = i % X_DIM;
    uint8_t y = i / X_DIM;
    uint8_t r = body[i * 3 + 0];
    uint8_t g = body[i * 3 + 1];
    uint8_t b = body[i * 3 + 2];
    trellis.setPixelColor(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
  }
  trellis.show();
}

void applyLedsPartial(const uint8_t *body, size_t len) {
  if (len < 1) return;
  uint8_t count = body[0];
  if (len != (size_t)(1 + count * 5)) return;  // malformed, drop
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t *e = body + 1 + i * 5;
    uint8_t x = e[0], y = e[1], r = e[2], g = e[3], b = e[4];
    if (x >= X_DIM || y >= Y_DIM) continue;
    trellis.setPixelColor(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
  }
  trellis.show();
}

void applyBrightness(const uint8_t *body, size_t len) {
  if (len < 1) return;
  uint8_t level = body[0];
  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(level);
  trellis.show();
}

void applyClear() {
  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++)
      trellis.setPixelColor(x, y, 0);
  trellis.show();
}

void dispatch(uint8_t type, const uint8_t *body, size_t len) {
  switch (type) {
    case TYPE_LEDS_FULL:    applyLedsFull(body, len);    break;
    case TYPE_LEDS_PARTIAL: applyLedsPartial(body, len); break;
    case TYPE_BRIGHTNESS:   applyBrightness(body, len);  break;
    case TYPE_CLEAR:        applyClear();                break;
    case TYPE_PING:         sendHello();                 break;
    default: break;  // unknown type, ignore
  }
}

void handleFrame(const uint8_t *payload, size_t len) {
  if (len < 3) return;  // too short to hold TYPE + CRC16
  uint16_t crcRecv = (uint16_t)payload[len - 2] | ((uint16_t)payload[len - 1] << 8);
  uint16_t crcCalc = crc16_ccitt_false(payload, len - 2);
  if (crcRecv != crcCalc) return;  // drop malformed frame, do not crash
  dispatch(payload[0], payload + 1, len - 3);
}

void pollSerial() {
  while (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    if (b == 0x00) {
      if (rxLen > 0) {
        uint8_t decoded[RX_BUF_MAX];
        size_t decLen = cobsDecode(rxBuf, rxLen, decoded);
        if (decLen > 0) handleFrame(decoded, decLen);
        // decLen == 0 -> malformed, drop silently and resync
      }
      rxLen = 0;
    } else if (rxLen < RX_BUF_MAX) {
      rxBuf[rxLen++] = b;
    } else {
      rxLen = 0;  // overflow: drop the runaway frame, resync at next 0x00
    }
  }
}

// ── Key events: emit KEY (0x10) on every press/release edge ────────────────
TrellisCallback onKey(keyEvent evt) {
  uint8_t x = evt.bit.NUM % X_DIM;
  uint8_t y = evt.bit.NUM / X_DIM;
  uint8_t edge = (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) ? 1 : 0;
  uint8_t body[3] = { x, y, edge };
  sendFrame(TYPE_KEY, body, sizeof(body));
  return 0;
}

// ── Analog: raw, ungated ADC, every 20ms (~50Hz) ────────────────────────────
void tickAnalog() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 20) return;
  last = now;

  uint16_t vals[N_ANALOG];
  for (int i = 0; i < N_ANALOG; i++) vals[i] = (uint16_t)analogRead(softpotPin[i]);

  uint8_t body[1 + N_ANALOG * 2];
  body[0] = N_ANALOG;
  for (int i = 0; i < N_ANALOG; i++) {
    body[1 + i * 2] = (uint8_t)(vals[i] & 0xFF);
    body[2 + i * 2] = (uint8_t)((vals[i] >> 8) & 0xFF);
  }
  sendFrame(TYPE_ANALOG, body, sizeof(body));
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  // GPIO32/33 support an internal pulldown. GPIO34/35 are input-only pins
  // with NO internal pull resistors at all — that's an ESP32 hardware limit;
  // use an external ~100k pulldown to GND on those (see README.md).
  pinMode(SOFTPOT1_PIN, INPUT_PULLDOWN);
  pinMode(SOFTPOT2_PIN, INPUT_PULLDOWN);

  Wire.begin(21, 22);
  Wire.setClock(800000);  // 800kHz — matches the working reference config

  if (!trellis.begin()) {
    // Can't reach the Pi meaningfully without a grid; spin with a visible
    // serial breadcrumb rather than silently hanging.
    Serial.println("NeoTrellis not found — check addresses / wiring / power.");
    while (1) delay(10);
  }

  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(DEFAULT_BRIGHTNESS);

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      trellis.registerCallback(x, y, onKey);
    }

  applyClear();  // all pixels off until the Pi sends real frames
  sendHello();   // boot handshake
}

void loop() {
  pollSerial();   // drain + dispatch any complete inbound frames
  trellis.read(); // fires onKey() for any new press/release edge
  tickAnalog();   // ~50Hz raw ADC report
}
