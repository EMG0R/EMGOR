/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║  TRUMPET HYPER-INSTRUMENT — Teensy 4.1 MIDI Controller      ║
 * ║  All sensors → USB MIDI CC on Channel 3                     ║
 * ║  Author: EMGOR | CalArts Creative Computing                 ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 *  SETUP: In Arduino IDE → Tools → USB Type → "MIDI"
 *         Install libraries: Adafruit_NeoPixel, Adafruit_NeoTrellis,
 *         Adafruit_BME680, Adafruit_VL53L0X, Encoder
 *         (IMU uses raw I2C — no library needed)
 *
 *  I2C BUS LAYOUT:
 *    Wire  (SDA=18, SCL=19) → NeoTrellis (0x2E), BME680 (0x77), VL53L0X (0x29)
 *    Wire1 (SDA=17, SCL=16) → MPU6500/IMU (0x68)
 *
 *  NEOPIXEL STRIPS:
 *    Strip 1: 9 LEDs on pin 29 (main body)
 *    Strip 2: 6 LEDs on pin 28 (secondary, reactive)
 *
 *  TRELLIS ANIMATION:
 *    Multi-sine generative lava lamp through purple/violet palette w/ dark blue hints
 *    Reactive to: button ripples, joystick (color + arrow), accel X (color),
 *    sliders/joystick (brightness), lidar (speed), FSR (brightness + checkerboard),
 *    pots (diagonal lines). All modulations fade back when idle.
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoTrellis.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VL53L0X.h>
#include <Encoder.h>

// ============================================================
//  MIDI
// ============================================================
const int MIDI_CHANNEL = 3;

const int CC_SLIDER_1     = 20;
const int CC_SLIDER_2     = 21;
const int CC_SLIDER_3     = 22;
const int CC_TOF          = 23;
const int CC_FSR_1        = 24;
const int CC_POT_1        = 28;
const int CC_POT_2        = 29;
const int CC_JOY_X        = 30;  // vertical: 127=up, 0=down (flipped)
const int CC_JOY_Y        = 31;  // horizontal: 0=left, 127=right
const int CC_ENC1_VAL     = 36;
const int CC_ENC2_VAL     = 37;
const int CC_ACCEL_X      = 41;
const int CC_ACCEL_Y      = 42;
const int CC_ACCEL_Z      = 43;
const int CC_GYRO_X       = 44;
const int CC_GYRO_Y       = 45;
const int CC_GYRO_Z       = 46;
const int CC_BREATH       = 47;
const int CC_CONTACT_MIC  = 48;
const int CC_CONTACT_MIC2 = 49;

const int CC_JOY_BTN      = 32;
const int CC_VALVE_1      = 33;
const int CC_VALVE_2      = 34;
const int CC_VALVE_3      = 35;
const int CC_ENC1_BTN     = 38;
const int CC_ENC2_BTN     = 39;
const int CC_ARCADE_BTN   = 40;

const int CC_TRELLIS_BASE = 50;

// ============================================================
//  PINS
// ============================================================
const int PIN_SLIDER_1     = A0;   // pin 14
const int PIN_SLIDER_2     = A1;   // pin 15
const int PIN_SLIDER_3     = A6;   // pin 20
const int PIN_FSR_1        = A7;   // pin 21
const int PIN_JOY_X        = A10;  // pin 24
const int PIN_JOY_Y        = A11;  // pin 25
const int PIN_CONTACT_MIC  = A12;  // pin 26
const int PIN_POT_1        = A13;  // pin 27
const int PIN_POT_2        = A14;  // pin 40 (bottom pad)
const int PIN_CONTACT_MIC2 = A9;   // pin 23 — new contact mic

const int PIN_HALL_1      = 2;
const int PIN_HALL_2      = 3;
const int PIN_HALL_3      = 4;
const int PIN_JOY_BTN     = 5;
const int PIN_ARCADE_BTN  = 6;
const int PIN_ENC1_A      = 7;
const int PIN_ENC1_B      = 8;
const int PIN_ENC1_BTN    = 9;
const int PIN_ENC2_A      = 10;
const int PIN_ENC2_B      = 11;
const int PIN_ENC2_BTN    = 12;

const int PIN_NEOPIXEL    = 29;   // strip 1: 9 LEDs
const int NUM_PIXELS      = 9;
const int PIN_NEOPIXEL2   = 28;   // strip 2: 6 LEDs
const int NUM_PIXELS2     = 6;

// ============================================================
//  OBJECTS
// ============================================================
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_PIXELS2, PIN_NEOPIXEL2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoTrellis trellis;
const uint8_t IMU_ADDR = 0x68;
Adafruit_BME680 bme;
Adafruit_VL53L0X tof;
Encoder enc1(PIN_ENC1_A, PIN_ENC1_B);
Encoder enc2(PIN_ENC2_A, PIN_ENC2_B);

// ============================================================
//  NEOPIXEL STRIP PALETTE (9 pixels — strip 1)
// ============================================================
const uint32_t PURPLE_PALETTE[] = {
  Adafruit_NeoPixel::Color(80,   0, 128),
  Adafruit_NeoPixel::Color(128,  0, 255),
  Adafruit_NeoPixel::Color(180,  0, 180),
  Adafruit_NeoPixel::Color(40,   0, 160),
  Adafruit_NeoPixel::Color(100, 50, 200),
  Adafruit_NeoPixel::Color(148,  0, 211),
  Adafruit_NeoPixel::Color(60,  10, 130),
  Adafruit_NeoPixel::Color(200,  0, 200),
  Adafruit_NeoPixel::Color(128, 30, 128),
};

// NEOPIXEL STRIP 2 PALETTE (6 pixels)
const uint32_t PURPLE_PALETTE2[] = {
  Adafruit_NeoPixel::Color(100,  0, 200),
  Adafruit_NeoPixel::Color(60,   0, 180),
  Adafruit_NeoPixel::Color(140,  0, 220),
  Adafruit_NeoPixel::Color(80,  20, 160),
  Adafruit_NeoPixel::Color(120,  0, 190),
  Adafruit_NeoPixel::Color(50,  10, 170),
};

// ============================================================
//  TRELLIS PALETTE — purples & violets w/ hints of dark blue
// ============================================================
const uint8_t TPAL_R[] = { 68,102, 85,119, 50,136,153, 34, 90,170, 60, 10,  0, 20, 55, 75};
const uint8_t TPAL_G[] = {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5, 12,  5,  0,  0};
const uint8_t TPAL_B[] = {170,255,204,221,170,204,221,136,180,204,160,140,136,130,170,190};

// ============================================================
//  MIDI STATE
// ============================================================
int prevSlider[3]     = {-1, -1, -1};
int prevFSR           = -1;
int prevPot[2]        = {-1, -1};
int prevJoyX          = -1;
int prevJoyY          = -1;
int prevMicLevel      = -1;
int prevMicLevel2     = -1;
int prevAccel[3]      = {-1, -1, -1};
int prevGyro[3]       = {-1, -1, -1};
int prevBreath        = -1;
int prevTof           = -1;

int prevHall[3]       = {-1, -1, -1};
int prevJoyBtn        = -1;
int prevArcadeBtn     = -1;
int prevEnc1Btn       = -1;
int prevEnc2Btn       = -1;

long prevEnc1Pos      = 0;
long prevEnc2Pos      = 0;

float baselinePressure = 0;
bool  bmeReady         = false;
bool  bmeReadingStarted = false;
unsigned long bmeEndTime = 0;
bool  mpuReady         = false;
bool  tofReady         = false;
bool  trellisButtonState[16];

// VL53L0X filter
const int TOF_FILTER_SIZE = 8;
int tofBuffer[TOF_FILTER_SIZE];
int tofBufIdx = 0;
bool tofBufFull = false;

// ============================================================
//  CURRENT SENSOR VALUES FOR ANIMATION
// ============================================================
int curJoyX  = 64;
int curJoyY  = 64;
int curSlider[3] = {0, 0, 0};
int curFSR   = 0;
int curAccelX = 64;
int curTof   = 0;
int curPot[2] = {0, 0};
int curMic1  = 0;
int curMic2  = 0;

// ============================================================
//  TRELLIS ANIMATION STATE
// ============================================================
float animTime          = 0;
float modBrightness     = 0;
float modFSRBright      = 0;
float modSpeedSmooth    = 1.0;
float modJoyColor       = 0;
float modAccelColor     = 0;
float modCheckerPhase   = 0;

int actPrevJoyX = 64, actPrevJoyY = 64;
int actPrevSlider[3] = {0, 0, 0};

struct Ripple {
  bool active;
  int row, col;
  unsigned long startTime;
  int paletteIdx;
};
const int MAX_RIPPLES = 8;
Ripple ripples[MAX_RIPPLES];

// Timing
unsigned long lastAnalogRead    = 0;
unsigned long lastIMURead       = 0;
unsigned long lastMicRead       = 0;
unsigned long lastTofRead       = 0;
unsigned long lastPixelUpdate   = 0;
unsigned long lastPixel2Update  = 0;
unsigned long lastTrellisUpdate = 0;

// ============================================================
//  NEOTRELLIS CALLBACK — MIDI + ripple trigger
// ============================================================
TrellisCallback trellisCallback(keyEvent evt) {
  int key = evt.bit.NUM;
  if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
    trellisButtonState[key] = true;
    usbMIDI.sendControlChange(CC_TRELLIS_BASE + key, 127, MIDI_CHANNEL);
    for (int r = 0; r < MAX_RIPPLES; r++) {
      if (!ripples[r].active) {
        ripples[r].active = true;
        ripples[r].row = key / 4;
        ripples[r].col = key % 4;
        ripples[r].startTime = millis();
        ripples[r].paletteIdx = key;
        break;
      }
    }
  } else if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_FALLING) {
    trellisButtonState[key] = false;
    usbMIDI.sendControlChange(CC_TRELLIS_BASE + key, 0, MIDI_CHANNEL);
  }
  return 0;
}

// ============================================================
//  HELPERS
// ============================================================
void sendCC(int cc, int newVal, int &prevVal) {
  if (abs(newVal - prevVal) >= 2 || (prevVal == -1 && newVal != -1)) {
    usbMIDI.sendControlChange(cc, newVal, MIDI_CHANNEL);
    prevVal = newVal;
  }
}

void sendCCDigital(int cc, int newVal, int &prevVal) {
  if (newVal != prevVal) {
    usbMIDI.sendControlChange(cc, newVal, MIDI_CHANNEL);
    prevVal = newVal;
  }
}

int readAnalogSmoothed(int pin, int samples = 4) {
  long sum = 0;
  for (int i = 0; i < samples; i++) sum += analogRead(pin);
  return map(sum / samples, 0, 1023, 0, 127);
}

void imuWriteReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

int16_t imuRead16(uint8_t reg) {
  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom(IMU_ADDR, (uint8_t)2);
  int16_t val = (Wire1.read() << 8) | Wire1.read();
  return val;
}

int tofFilteredRead() {
  VL53L0X_RangingMeasurementData_t measure;
  tof.rangingTest(&measure, false);
  int rawMm;
  if (measure.RangeStatus != 4) {
    rawMm = measure.RangeMilliMeter;
  } else {
    rawMm = 333;
  }
  rawMm = constrain(rawMm, 56, 333);
  tofBuffer[tofBufIdx] = rawMm;
  tofBufIdx = (tofBufIdx + 1) % TOF_FILTER_SIZE;
  if (tofBufIdx == 0) tofBufFull = true;
  int count = tofBufFull ? TOF_FILTER_SIZE : tofBufIdx;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += tofBuffer[i];
  int avgMm = sum / count;
  return constrain(map(avgMm, 56, 333, 127, 0), 0, 127);
}

void paletteLerp(float idx, float &r, float &g, float &b) {
  idx = fmod(fmod(idx, 16.0f) + 16.0f, 16.0f);
  int c1 = (int)idx % 16;
  int c2 = (c1 + 1) % 16;
  float t = idx - (int)idx;
  r = TPAL_R[c1] * (1.0f - t) + TPAL_R[c2] * t;
  g = TPAL_G[c1] * (1.0f - t) + TPAL_G[c2] * t;
  b = TPAL_B[c1] * (1.0f - t) + TPAL_B[c2] * t;
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("=== TRUMPET HYPER-INSTRUMENT INITIALIZING ===");

  analogReadResolution(10);

  pinMode(PIN_HALL_1,     INPUT_PULLUP);
  pinMode(PIN_HALL_2,     INPUT_PULLUP);
  pinMode(PIN_HALL_3,     INPUT_PULLUP);
  pinMode(PIN_JOY_BTN,    INPUT_PULLUP);
  pinMode(PIN_ARCADE_BTN, INPUT_PULLUP);
  pinMode(PIN_ENC1_BTN,   INPUT_PULLUP);
  pinMode(PIN_ENC2_BTN,   INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000);
  Wire1.begin();
  Wire1.setClock(400000);

  // --- NeoPixel strip 1 (9 LEDs, pin 29) ---
  strip.begin();
  strip.setBrightness(60);
  for (int i = 0; i < NUM_PIXELS; i++) strip.setPixelColor(i, PURPLE_PALETTE[i]);
  strip.show();
  Serial.println("[OK] NeoPixel strip 1 (9 LEDs, pin 29)");

  // --- NeoPixel strip 2 (6 LEDs, pin 28) ---
  strip2.begin();
  strip2.setBrightness(60);
  for (int i = 0; i < NUM_PIXELS2; i++) strip2.setPixelColor(i, PURPLE_PALETTE2[i]);
  strip2.show();
  Serial.println("[OK] NeoPixel strip 2 (6 LEDs, pin 28)");

  if (trellis.begin(0x2E)) {
    Serial.println("[OK] NeoTrellis on Wire (0x2E)");
    for (int i = 0; i < 16; i++) {
      trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
      trellis.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
      trellis.registerCallback(i, trellisCallback);
      trellisButtonState[i] = false;
    }
  } else {
    Serial.println("[FAIL] NeoTrellis not found at 0x2E");
  }

  if (tof.begin(0x29, false, &Wire)) {
    Serial.println("[OK] VL53L0X on Wire (0x29)");
    tof.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE);
    tofReady = true;
    for (int i = 0; i < TOF_FILTER_SIZE; i++) tofBuffer[i] = 180;
  } else {
    Serial.println("[FAIL] VL53L0X not found at 0x29");
  }

  Wire1.beginTransmission(IMU_ADDR);
  Wire1.write(0x75);
  Wire1.endTransmission(false);
  Wire1.requestFrom(IMU_ADDR, (uint8_t)1);
  if (Wire1.available()) {
    uint8_t whoami = Wire1.read();
    Serial.print("[OK] IMU on Wire1, WHO_AM_I: 0x");
    Serial.println(whoami, HEX);
    imuWriteReg(0x6B, 0x00);
    delay(10);
    imuWriteReg(0x1C, 0x08);
    imuWriteReg(0x1B, 0x08);
    imuWriteReg(0x1A, 0x04);
    mpuReady = true;
  } else {
    Serial.println("[FAIL] IMU not found on Wire1 (0x68)");
  }

  // ── BME680: BREATH PRESSURE CONFIG ──
  if (bme.begin(0x77)) {
    Serial.println("[OK] BME680 on Wire (0x77)");
    bme.setTemperatureOversampling(BME680_OS_1X);
    bme.setHumidityOversampling(BME680_OS_NONE);
    bme.setPressureOversampling(BME680_OS_2X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_0);
    bme.setGasHeater(0, 0);
    delay(500);
    float sum = 0;
    int good = 0;
    for (int i = 0; i < 10; i++) {
      if (bme.performReading()) {
        sum += bme.pressure / 100.0;
        good++;
      }
      delay(50);
    }
    if (good > 0) {
      baselinePressure = sum / good;
      bmeReady = true;
      Serial.print("[OK] Baseline pressure (avg of ");
      Serial.print(good);
      Serial.print("): ");
      Serial.print(baselinePressure, 4);
      Serial.println(" hPa");
    }
  } else if (bme.begin(0x76)) {
    Serial.println("[OK] BME680 on Wire (0x76)");
    bme.setTemperatureOversampling(BME680_OS_1X);
    bme.setHumidityOversampling(BME680_OS_NONE);
    bme.setPressureOversampling(BME680_OS_2X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_0);
    bme.setGasHeater(0, 0);
    delay(500);
    float sum = 0;
    int good = 0;
    for (int i = 0; i < 10; i++) {
      if (bme.performReading()) {
        sum += bme.pressure / 100.0;
        good++;
      }
      delay(50);
    }
    if (good > 0) {
      baselinePressure = sum / good;
      bmeReady = true;
      Serial.print("[OK] Baseline pressure (avg of ");
      Serial.print(good);
      Serial.print("): ");
      Serial.print(baselinePressure, 4);
      Serial.println(" hPa");
    }
  } else {
    Serial.println("[FAIL] BME680 not found");
  }

  enc1.write(0);
  enc2.write(0);

  for (int i = 0; i < MAX_RIPPLES; i++) ripples[i].active = false;

  if (bmeReady) bme.beginReading();

  Serial.println("=== INITIALIZATION COMPLETE ===");
  Serial.println("MIDI Ch 3 | CC30 flipped (127=up) | Reactive trellis");
  Serial.println("Strip1: 9px pin29 | Strip2: 6px pin28 | Mic2: A9 CC49");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  // --- ANALOG: 50Hz ---
  if (now - lastAnalogRead >= 20) {
    lastAnalogRead = now;

    int s1 = constrain(map(readAnalogSmoothed(PIN_SLIDER_1), 2, 125, 0, 127), 0, 127);
    int s2 = constrain(map(readAnalogSmoothed(PIN_SLIDER_2), 2, 125, 0, 127), 0, 127);
    int s3 = constrain(map(readAnalogSmoothed(PIN_SLIDER_3), 2, 125, 0, 127), 0, 127);
    sendCC(CC_SLIDER_1, s1, prevSlider[0]);
    sendCC(CC_SLIDER_2, s2, prevSlider[1]);
    sendCC(CC_SLIDER_3, s3, prevSlider[2]);
    curSlider[0] = s1; curSlider[1] = s2; curSlider[2] = s3;

    int fsrVal = constrain(map(readAnalogSmoothed(PIN_FSR_1), 2, 50, 0, 127), 0, 127);
    sendCC(CC_FSR_1, fsrVal, prevFSR);
    curFSR = fsrVal;

    int p1 = constrain(map(readAnalogSmoothed(PIN_POT_1), 2, 125, 0, 127), 0, 127);
    int p2 = constrain(map(readAnalogSmoothed(PIN_POT_2), 2, 125, 0, 127), 0, 127);
    sendCC(CC_POT_1, p1, prevPot[0]);
    sendCC(CC_POT_2, p2, prevPot[1]);
    curPot[0] = p1; curPot[1] = p2;

    int jx = 127 - readAnalogSmoothed(PIN_JOY_X);
    sendCC(CC_JOY_X, jx, prevJoyX);
    curJoyX = jx;

    int jy = readAnalogSmoothed(PIN_JOY_Y);
    sendCC(CC_JOY_Y, jy, prevJoyY);
    curJoyY = jy;
  }

  // --- DIGITAL: every loop ---
  sendCCDigital(CC_VALVE_1, digitalRead(PIN_HALL_1) == LOW ? 127 : 0, prevHall[0]);
  sendCCDigital(CC_VALVE_2, digitalRead(PIN_HALL_2) == LOW ? 127 : 0, prevHall[1]);
  sendCCDigital(CC_VALVE_3, digitalRead(PIN_HALL_3) == LOW ? 127 : 0, prevHall[2]);
  sendCCDigital(CC_JOY_BTN, digitalRead(PIN_JOY_BTN) == LOW ? 127 : 0, prevJoyBtn);
  sendCCDigital(CC_ARCADE_BTN, digitalRead(PIN_ARCADE_BTN) == LOW ? 127 : 0, prevArcadeBtn);
  sendCCDigital(CC_ENC1_BTN, digitalRead(PIN_ENC1_BTN) == LOW ? 127 : 0, prevEnc1Btn);
  sendCCDigital(CC_ENC2_BTN, digitalRead(PIN_ENC2_BTN) == LOW ? 127 : 0, prevEnc2Btn);

  // --- ENCODERS ---
  long enc1Pos = enc1.read() / 4;
  if (enc1Pos != prevEnc1Pos) {
    usbMIDI.sendControlChange(CC_ENC1_VAL, (enc1Pos > prevEnc1Pos) ? 127 : 0, MIDI_CHANNEL);
    prevEnc1Pos = enc1Pos;
  }
  long enc2Pos = enc2.read() / 4;
  if (enc2Pos != prevEnc2Pos) {
    usbMIDI.sendControlChange(CC_ENC2_VAL, (enc2Pos > prevEnc2Pos) ? 127 : 0, MIDI_CHANNEL);
    prevEnc2Pos = enc2Pos;
  }

  // --- CONTACT MICS: 100Hz (both) ---
  if (now - lastMicRead >= 10) {
    lastMicRead = now;

    // Mic 1 (A12)
    int peak1 = 0;
    for (int i = 0; i < 10; i++) {
      int dev = abs(analogRead(PIN_CONTACT_MIC) - 512);
      if (dev > peak1) peak1 = dev;
    }
    int mic1Val = constrain(map(peak1, 0, 400, 0, 127), 0, 127);
    sendCC(CC_CONTACT_MIC, mic1Val, prevMicLevel);
    curMic1 = mic1Val;

    // Mic 2 (A9)
    int peak2 = 0;
    for (int i = 0; i < 10; i++) {
      int dev = abs(analogRead(PIN_CONTACT_MIC2) - 512);
      if (dev > peak2) peak2 = dev;
    }
    int mic2Val = constrain(map(peak2, 0, 400, 0, 127), 0, 127);
    sendCC(CC_CONTACT_MIC2, mic2Val, prevMicLevel2);
    curMic2 = mic2Val;
  }

  // --- VL53L0X: 30Hz ---
  if (tofReady && (now - lastTofRead >= 33)) {
    lastTofRead = now;
    int tofVal = tofFilteredRead();
    sendCC(CC_TOF, tofVal, prevTof);
    curTof = tofVal;
  }

  // --- IMU: 50Hz ---
  if (mpuReady && (now - lastIMURead >= 20)) {
    lastIMURead = now;
    int ax = constrain(map(imuRead16(0x3B), -8000, 8000, 0, 127), 0, 127);
    int ay = constrain(map(imuRead16(0x3D), -8000, 8000, 0, 127), 0, 127);
    int az = constrain(map(imuRead16(0x3F), -8000, 8000, 0, 127), 0, 127);
    sendCC(CC_ACCEL_X, ax, prevAccel[0]);
    sendCC(CC_ACCEL_Y, ay, prevAccel[1]);
    sendCC(CC_ACCEL_Z, az, prevAccel[2]);
    curAccelX = ax;

    sendCC(CC_GYRO_X, constrain(map(imuRead16(0x43), -8000, 8000, 0, 127), 0, 127), prevGyro[0]);
    sendCC(CC_GYRO_Y, constrain(map(imuRead16(0x45), -8000, 8000, 0, 127), 0, 127), prevGyro[1]);
    sendCC(CC_GYRO_Z, constrain(map(imuRead16(0x47), -8000, 8000, 0, 127), 0, 127), prevGyro[2]);
  }

  // ── BME680 BREATH: FULLY NON-BLOCKING ASYNC ──
  if (bmeReady) {
    if (!bmeReadingStarted) {
      bmeEndTime = bme.beginReading();
      if (bmeEndTime != 0) bmeReadingStarted = true;
    } else if (now >= bmeEndTime) {
      if (bme.endReading()) {
        float currentHPa = bme.pressure / 100.0;
        float absDelta = fabsf(currentHPa - baselinePressure);
        absDelta = fmaxf(absDelta - 0.02, 0.0);
        int breathVal = constrain((int)(absDelta * (127.0 / 0.3)), 0, 127);
        sendCC(CC_BREATH, breathVal, prevBreath);
      }
      bmeReadingStarted = false;
    }
  }

  // --- NEOTRELLIS POLL ---
  trellis.read();

  // --- NEOTRELLIS ANIMATION: 25Hz ---
  if (now - lastTrellisUpdate >= 40) {
    lastTrellisUpdate = now;
    updateTrellis(now);
  }

  // --- NEOPIXEL STRIP 1: 20Hz ---
  if (now - lastPixelUpdate >= 50) {
    lastPixelUpdate = now;
    updateNeoPixelStrip(now);
  }

  // --- NEOPIXEL STRIP 2: 20Hz ---
  if (now - lastPixel2Update >= 50) {
    lastPixel2Update = now;
    updateNeoPixelStrip2(now);
  }

  usbMIDI.send_now();
  while (usbMIDI.read());
}

// ============================================================
//  TRELLIS REACTIVE ANIMATION ENGINE
// ============================================================
void updateTrellis(unsigned long now) {
  const float dt = 40.0;

  float targetSpeed = 1.0 + (curTof / 127.0) * 3.0;
  modSpeedSmooth += (targetSpeed - modSpeedSmooth) * 0.08;
  animTime += dt * modSpeedSmooth;

  int joyDelta = abs(curJoyX - actPrevJoyX) + abs(curJoyY - actPrevJoyY);
  int sliderDelta = 0;
  for (int i = 0; i < 3; i++) {
    sliderDelta += abs(curSlider[i] - actPrevSlider[i]);
    actPrevSlider[i] = curSlider[i];
  }
  actPrevJoyX = curJoyX;
  actPrevJoyY = curJoyY;

  float activityBoost = (joyDelta + sliderDelta) / 40.0;
  modBrightness = modBrightness * 0.92 + activityBoost;
  if (modBrightness > 1.5) modBrightness = 1.5;

  float targetFSR = curFSR / 127.0;
  modFSRBright += (targetFSR - modFSRBright) * 0.12;

  modCheckerPhase += dt * 0.001 * (1.0 + modFSRBright * 10.0);

  float targetJoyColor = (curJoyY - 64) / 64.0 * 5.0;
  modJoyColor += (targetJoyColor - modJoyColor) * 0.06;

  float accelDev = (float)(curAccelX - 64);
  float targetAccelColor = 0;
  if (accelDev > 18)       targetAccelColor = (accelDev - 18.0) / 45.0 * 4.0;
  else if (accelDev < -18) targetAccelColor = (accelDev + 18.0) / 45.0 * 4.0;
  modAccelColor += (targetAccelColor - modAccelColor) * 0.04;

  float t = animTime;

  for (int i = 0; i < 16; i++) {
    int row = i / 4;
    int col = i % 4;
    float px = col / 3.0;
    float py = row / 3.0;

    float n1 = sinf(t * 0.00031f + px * 6.28f * 1.0f + py * 6.28f * 0.7f);
    float n2 = sinf(t * 0.00073f + px * 6.28f * 0.4f + py * 6.28f * 1.3f + 2.1f);
    float n3 = sinf(t * 0.00019f + px * 6.28f * 1.6f + py * 6.28f * 0.3f + 4.7f);
    float n4 = sinf(t * 0.00053f + px * 6.28f * 0.8f + py * 6.28f * 1.1f + 1.3f);
    float n5 = sinf(t * 0.00011f + px * 6.28f * 1.2f + py * 6.28f * 1.5f + 3.4f);

    float colorIdx = n1 * 3.5f + n2 * 2.5f + n3 * 4.0f + n4 * 2.0f + n5 * 3.0f;
    colorIdx += modJoyColor + modAccelColor;

    float pr, pg, pb;
    paletteLerp(colorIdx, pr, pg, pb);

    float baseBright = 0.07f + 0.05f * sinf(t * 0.00041f + i * 0.97f + 3.2f)
                             + 0.04f * sinf(t * 0.00067f + i * 1.51f);
    baseBright += modFSRBright * 0.5f;
    baseBright += modBrightness * 0.35f;

    if (modFSRBright > 0.03f) {
      float checker = ((row + col) % 2 == 0) ?
        sinf(modCheckerPhase * 6.28f) : -sinf(modCheckerPhase * 6.28f);
      baseBright += checker * modFSRBright * 0.2f;
    }

    float r = pr * baseBright;
    float g = pg * baseBright;
    float b = pb * baseBright;

    if (curPot[0] > 5 && row == col) {
      float potStr = curPot[0] / 127.0f;
      float pulse = 0.5f + 0.5f * sinf(t * 0.002f + row * 0.8f);
      r += 50.0f * potStr * pulse;
      g += 15.0f * potStr * pulse;
      b += 90.0f * potStr * pulse;
    }

    if (curPot[1] > 5 && (row + col) == 3) {
      float potStr = curPot[1] / 127.0f;
      float pulse = 0.5f + 0.5f * sinf(t * 0.0025f + row * 0.6f);
      r += 15.0f * potStr * pulse;
      g += 60.0f * potStr * pulse;
      b += 50.0f * potStr * pulse;
    }

    float arrowStr = 0;
    int vDist = curJoyX - 64;
    if (abs(vDist) > 20) {
      float vStr = (abs(vDist) - 20) / 43.0f;
      if (vDist > 0) {
        if (row == 0)                          arrowStr = fmaxf(arrowStr, vStr);
        if (row == 1 && (col == 1 || col == 2)) arrowStr = fmaxf(arrowStr, vStr * 0.7f);
      } else {
        if (row == 3)                          arrowStr = fmaxf(arrowStr, -vDist > 20 ? (-vDist - 20) / 43.0f : 0);
        if (row == 2 && (col == 1 || col == 2)) arrowStr = fmaxf(arrowStr, (-vDist - 20) / 43.0f * 0.7f);
      }
    }
    int hDist = curJoyY - 64;
    if (abs(hDist) > 20) {
      float hStr = (abs(hDist) - 20) / 43.0f;
      if (hDist > 0) {
        if (col == 3)                          arrowStr = fmaxf(arrowStr, hStr);
        if (col == 2 && (row == 1 || row == 2)) arrowStr = fmaxf(arrowStr, hStr * 0.7f);
      } else {
        if (col == 0)                          arrowStr = fmaxf(arrowStr, (-hDist - 20) / 43.0f);
        if (col == 1 && (row == 1 || row == 2)) arrowStr = fmaxf(arrowStr, (-hDist - 20) / 43.0f * 0.7f);
      }
    }
    if (arrowStr > 0) {
      if (arrowStr > 1.0f) arrowStr = 1.0f;
      float pulse = 0.6f + 0.4f * sinf(t * 0.004f);
      r += 90.0f * arrowStr * pulse;
      g += 35.0f * arrowStr * pulse;
      b += 130.0f * arrowStr * pulse;
    }

    for (int ri = 0; ri < MAX_RIPPLES; ri++) {
      if (!ripples[ri].active) continue;
      float elapsed = (now - ripples[ri].startTime) / 1000.0f;
      if (elapsed > 2.5f) { ripples[ri].active = false; continue; }
      float dr = (float)(row - ripples[ri].row);
      float dc = (float)(col - ripples[ri].col);
      float dist = sqrtf(dr * dr + dc * dc);
      float wavePos = elapsed * 2.8f;
      float waveDelta = fabsf(dist - wavePos);
      if (waveDelta < 0.9f) {
        float waveStr = (1.0f - waveDelta / 0.9f) * (1.0f - elapsed / 2.5f);
        waveStr *= waveStr;
        int pi = ripples[ri].paletteIdx;
        r += TPAL_R[pi] * waveStr * 1.2f;
        g += TPAL_G[pi] * waveStr * 1.2f;
        b += TPAL_B[pi] * waveStr * 1.2f;
      }
    }

    if (trellisButtonState[i]) {
      r = TPAL_R[i] * 1.0f;
      g = TPAL_G[i] * 1.0f;
      b = TPAL_B[i] * 1.0f;
    }

    uint8_t ro = (uint8_t)constrain((int)r, 0, 255);
    uint8_t go = (uint8_t)constrain((int)g, 0, 255);
    uint8_t bo = (uint8_t)constrain((int)b, 0, 255);
    trellis.pixels.setPixelColor(i, ((uint32_t)ro << 16) | ((uint32_t)go << 8) | bo);
  }

  trellis.pixels.show();
}

// ============================================================
//  NEOPIXEL STRIP 1 ANIMATION (9 pixels, pin 29)
// ============================================================
void updateNeoPixelStrip(unsigned long now) {
  for (int i = 0; i < NUM_PIXELS; i++) {
    float phase = (float)(now % 3000) / 3000.0 * 2.0 * PI;
    float offset = (float)i / NUM_PIXELS * 2.0 * PI;
    float brightness = 0.3 + 0.7 * (0.5 + 0.5 * sin(phase + offset));

    uint8_t r = (PURPLE_PALETTE[i] >> 16) & 0xFF;
    uint8_t g = (PURPLE_PALETTE[i] >> 8) & 0xFF;
    uint8_t b = PURPLE_PALETTE[i] & 0xFF;
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);

    if (i < 3 && prevHall[i] == 127) {
      r = min(255, r + 100); g = min(255, g + 20); b = min(255, b + 150);
    }
    if (i >= 3 && i <= 4 && prevBreath > 0) {
      float s = (float)prevBreath / 127.0;
      r = (uint8_t)min(255.0f, r + 80.0f * s);
      b = (uint8_t)min(255.0f, b + 120.0f * s);
    }
    if (i >= 5 && i <= 6) {
      int tilt = abs(prevAccel[0] - 64) + abs(prevAccel[1] - 64);
      float s = constrain((float)tilt / 64.0, 0.0, 1.0);
      r = (uint8_t)min(255.0f, r + 60.0f * s);
      g = (uint8_t)min(255.0f, g + 30.0f * s);
      b = (uint8_t)min(255.0f, b + 100.0f * s);
    }
    if (i >= 7 && i <= 8 && prevMicLevel > 30) {
      float s = (float)prevMicLevel / 127.0;
      r = (uint8_t)min(255.0f, r + 150.0f * s);
      b = (uint8_t)min(255.0f, b + 200.0f * s);
    }
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

// ============================================================
//  NEOPIXEL STRIP 2 ANIMATION (6 pixels, pin 28)
//  Reactive breathing with sensor modulation
// ============================================================
void updateNeoPixelStrip2(unsigned long now) {
  float t = (float)now;

  // Global sensor-driven brightness boost
  float sensorBright = modBrightness * 0.3f + modFSRBright * 0.4f;

  for (int i = 0; i < NUM_PIXELS2; i++) {
    // Base breathing: each pixel offset in phase, lidar modulates speed
    float phase = t * 0.001f * modSpeedSmooth;
    float offset = (float)i / NUM_PIXELS2 * 2.0f * PI;
    float breath = 0.25f + 0.75f * (0.5f + 0.5f * sinf(phase + offset));

    uint8_t pr = (PURPLE_PALETTE2[i] >> 16) & 0xFF;
    uint8_t pg = (PURPLE_PALETTE2[i] >> 8) & 0xFF;
    uint8_t pb = PURPLE_PALETTE2[i] & 0xFF;

    float r = pr * breath;
    float g = pg * breath;
    float b = pb * breath;

    // Sensor brightness boost
    r += 40.0f * sensorBright;
    g += 10.0f * sensorBright;
    b += 60.0f * sensorBright;

    // Contact mic 1 → pixels 0–1
    if (i <= 1 && curMic1 > 20) {
      float s = curMic1 / 127.0f;
      r += 120.0f * s;
      b += 180.0f * s;
    }

    // Contact mic 2 → pixels 4–5
    if (i >= 4 && curMic2 > 20) {
      float s = curMic2 / 127.0f;
      r += 100.0f * s;
      g += 30.0f * s;
      b += 160.0f * s;
    }

    // Breath → pixels 2–3
    if (i >= 2 && i <= 3 && prevBreath > 0) {
      float s = (float)prevBreath / 127.0f;
      r += 70.0f * s;
      b += 130.0f * s;
    }

    // Joystick tilt adds color across all pixels
    float joyShift = (curJoyY - 64) / 128.0f;
    r += 40.0f * fabsf(joyShift);
    b += 60.0f * fabsf(joyShift);

    // Accel shimmer
    float accelShift = fabsf(curAccelX - 64) / 64.0f;
    float shimmer = 0.5f + 0.5f * sinf(t * 0.003f + i * 1.2f);
    r += 30.0f * accelShift * shimmer;
    b += 50.0f * accelShift * shimmer;

    // Valve presses → flash nearest pixels
    if (i < 3 && prevHall[i] == 127) {
      r += 80.0f;
      g += 15.0f;
      b += 120.0f;
    }

    uint8_t ro = (uint8_t)constrain((int)r, 0, 255);
    uint8_t go = (uint8_t)constrain((int)g, 0, 255);
    uint8_t bo = (uint8_t)constrain((int)b, 0, 255);
    strip2.setPixelColor(i, strip2.Color(ro, go, bo));
  }
  strip2.show();
}
