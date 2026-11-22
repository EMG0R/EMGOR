#include <Encoder.h>
#include <Bounce.h>
#include <Wire.h>
#include <MIDIUSB.h>
#include <Adafruit_NeoPixel.h>
#include <VL53L1X.h>
#include <math.h>

// Pins
#define XY_X A2 // Pin 16 (analog)
#define XY_Y A3 // Pin 17 (analog)
#define XY_BUTTON 34
#define EXPRESSION_PIN A17 // Pin 41 - analog (moved off pin 24; pin 24 is now Wire2 SCL)
#define FSR1_PIN 20 // Analog pin A6
#define FSR2_PIN 21 // Analog pin A7
#define MPU_ADDRESS 0x68
#define NEOPIXEL_PIN 7 // NeoPixel data pin
#define NUMPIXELS 126 // SEZO 144 LEDs/m strip, formed into a ring with 18 LEDs cut off (144 − 18 = 126).
// Ping-pong palette: each pixel's hue is driven by a triangle wave that
// oscillates between pure green and pure violet. The ring shows a smooth
// gradient at any instant (neighboring pixels at neighboring triangle phases)
// and the whole thing animates green→violet→green over time.
#define HUE_GREEN    21845U                          // ColorHSV: 1/3 around the wheel = green
#define HUE_VIOLET   50000U                          // ColorHSV: ~76% = violet
#define HUE_SPAN     (HUE_VIOLET - HUE_GREEN)        // 28155 — green→violet half-cycle
#define HUE_PERIOD   (HUE_SPAN * 2U)                 // 56310 — full triangle cycle (green→violet→green)
#define SPATIAL_STEP (HUE_SPAN / NUMPIXELS)          // gradient covers one green→violet across the ring
// LiDAR 1 is on Wire (pins 18/19, shared with the MPU); LiDAR 2 is on Wire2
// (pins 24/25), its own bus. Separate buses mean both VL53L1X keep the 0x29
// default — no XSHUT pin and no re-addressing needed.
VL53L1X tof1, tof2;
bool tof1ok = false, tof2ok = false;
bool mpuok = false;

// NeoPixel setup
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// MIDI Channel
#define MIDI_CHANNEL 1

// MIDI CC Numbers
#define CC_ENCODER_1 20
#define CC_ENCODER_2 21
#define CC_ENCODER_3 22
#define CC_ENCODER_4 23
#define CC_ENCODER_5 24
#define CC_ENCODER_6 25
#define CC_ENCODER_7 26
#define CC_JOY_X 27
#define CC_JOY_Y 28
#define CC_EXPRESSION 29
#define CC_ULTRASONIC 30
#define CC_ACCEL_X 31
#define CC_ACCEL_Y 32
#define CC_ACCEL_Z 33
#define CC_FSR1 34
#define CC_FSR2 35
#define CC_DISTANCE_2 36
#define CC_MIC_L 78
#define CC_MIC_R 79
#define MIC_L_PIN 38 // A14
#define MIC_R_PIN 39 // A15
#define CC_BUTTON_1 60
#define CC_BUTTON_2 61
#define CC_BUTTON_3 62
#define CC_BUTTON_4 63
#define CC_BUTTON_5 64
#define CC_BUTTON_6 65
#define CC_BUTTON_7 66
#define CC_BUTTON_8 67
#define CC_BUTTON_9 68
#define CC_ENC_BUTTON_1 70
#define CC_ENC_BUTTON_2 71
#define CC_ENC_BUTTON_3 72
#define CC_ENC_BUTTON_4 73
#define CC_ENC_BUTTON_5 74
#define CC_ENC_BUTTON_6 75
#define CC_ENC_BUTTON_7 76
#define CC_JOY_BUTTON 77

// Encoders
Encoder enc1(2, 3);
Encoder enc2(4, 5);
Encoder enc3(6, 9);
Encoder enc4(10, 11);
Encoder enc5(12, 8); // DT moved off pin 13 — that's the onboard LED, can't INPUT_PULLUP
Encoder enc6(14, 15);
Encoder enc7(22, 23);

// ENC4_BTN on pin 1. ENC5_BTN on underside pad 49. ENC7_BTN on underside pad
// 51 — moved off pin 41 so pin 41/A17 can carry the expression pedal.
const int encoderButtonPins[7] = {35, 36, 37, 1, 49, 40, 51};
// BTN1 moved off pin 25 to pin 0; pin 25 is now Wire2 SDA for LiDAR 2.
const int buttonPins[9] = {0, 26, 27, 28, 29, 30, 31, 32, 33};

// Bounce objects
Bounce buttons[9] = {
  Bounce(buttonPins[0], 5), Bounce(buttonPins[1], 5), Bounce(buttonPins[2], 5),
  Bounce(buttonPins[3], 5), Bounce(buttonPins[4], 5), Bounce(buttonPins[5], 5),
  Bounce(buttonPins[6], 5), Bounce(buttonPins[7], 5), Bounce(buttonPins[8], 5)
};
Bounce encoderButtons[7] = {
  Bounce(encoderButtonPins[0], 5), Bounce(encoderButtonPins[1], 5),
  Bounce(encoderButtonPins[2], 5), Bounce(encoderButtonPins[3], 5),
  Bounce(encoderButtonPins[4], 5), Bounce(encoderButtonPins[5], 5),
  Bounce(encoderButtonPins[6], 5)
};
Bounce joystickButton(XY_BUTTON, 5);

// State variables
static long lastPos[7] = {0};
static int8_t encDirLatched[7] = {0}; // last turn direction since the last serial print: -1 L, 0 none, +1 R
static int lastX = 0, lastY = 0;
static int lastExpression = 0;
static int lastFsr1 = 0, lastFsr2 = 0;
static int lastMicL = 0, lastMicR = 0;
float envMicL = 0, envMicR = 0;
const float ALPHA_MIC_ATK = 0.5;  // fast attack on transients
const float ALPHA_MIC_REL = 0.05; // slow release for natural decay
static int lastUltra = -1;
static int lastUltra2 = -1;
// Latest raw VL53L1X readings for the serial debug print. -1 = timeout/no data.
int lidar1mm = -1, lidar2mm = -1;
static int16_t lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
static bool lastButtonStates[9] = {false};
static bool lastEncoderButtonStates[7] = {false};
static bool lastJoystickButtonState = false;
unsigned long lastUltrasonic = 0;
unsigned long lastMPU = 0;
unsigned long lastNeoPixelUpdate = 0;
unsigned long lastSerialPrint = 0;
uint16_t baseHue = 0;
bool pedalActive = false;
unsigned long pedalActiveStart = 0;
const unsigned long PEDAL_ACTIVE_DURATION = 60000; // 60 seconds in milliseconds
const unsigned long SERIAL_PRINT_INTERVAL = 200; // 200 ms — fast enough to watch the LiDARs live

// Smoothing variables
float smoothX = 0, smoothY = 0;
float smoothFsr1 = 0, smoothFsr2 = 0;
float smoothDistance = 0;
float smoothDist2 = 0;
float smoothAccelX = 0, smoothAccelY = 0, smoothAccelZ = 0;
const float ALPHA = 0.1;       // General smoothing (joystick, FSRs, ultrasonic, expression)
const float ALPHA_ACCEL = 0.3; // Lighter smoothing for the MPU6050 — chip's internal LPF already cleans noise; keeping the user-facing motion responsive (~200 ms settle vs 600 ms at 0.1).

// Random variation for NeoPixels
float pixelOffsets[NUMPIXELS] = {0};
float targetOffsets[NUMPIXELS] = {0};
unsigned long lastOffsetUpdate = 0;
const unsigned long OFFSET_UPDATE_INTERVAL = 1000; // Update offsets every 1s

void controlChange(uint8_t channel, uint8_t control, uint8_t value) {
  midiEventPacket_t event = {0x0B, (uint8_t)(0xB0 | channel), control, value};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}

void updateNeoPixels(int exprValue, bool anyEncoderButtonPressed) {
  if (anyEncoderButtonPressed) {
    for (int i = 0; i < NUMPIXELS; i++) {
      if (i == 1) {
        pixels.setPixelColor(i, pixels.Color(153, 153, 153)); // 15% darker white for third NeoPixel
      } else {
        pixels.setPixelColor(i, pixels.Color(180, 180, 180)); // White for others
      }
    }
  } else {
    // Pedal-driven brightness. THE BUG (2026-05-15): the previous version used
    // `map(exprValue, 32, 998, 8, 255)` with no clamp. When the pedal ADC read
    // at its physical max (~1023, past the 998 reference) the map returned
    // 256+ and the result was truncated to a tiny uint8_t when handed to
    // pixels.ColorHSV(...), making three pixels drop to near-zero exactly at
    // the pedal toe. Pixel 1 had a *0.3 multiplier on top, which kept ITS
    // value in-range, which is why it appeared to be a "different brand"
    // that survived the cut-out. Same brand. constrain() fixes it cleanly.
    int brightness = 255;
    if (pedalActive) {
      brightness = constrain(map(exprValue, 32, 998, 8, 255), 8, 255);
    }
    // Triangle wave maps phase ∈ [0, HUE_PERIOD) → tri ∈ [0, HUE_SPAN], folded so
    // it goes 0 → HUE_SPAN → 0 across one period. Adding HUE_GREEN puts the hue
    // in the green–violet band. Subtracting i*SPATIAL_STEP from phase makes the
    // gradient travel first → last as baseHue ticks. Per-pixel jitter still in
    // play; it shifts the pixel's position on the triangle a few %, keeping the
    // organic feel without pushing hue outside green↔violet.
    for (int i = 0; i < NUMPIXELS; i++) {
      int32_t raw = (int32_t)baseHue + (int32_t)pixelOffsets[i] - (int32_t)i * SPATIAL_STEP;
      uint16_t phase = (uint16_t)(((raw % (int32_t)HUE_PERIOD) + HUE_PERIOD) % HUE_PERIOD);
      uint16_t tri = (phase <= HUE_SPAN) ? phase : (uint16_t)(HUE_PERIOD - phase);
      uint16_t pixelHue = HUE_GREEN + tri;
      pixels.setPixelColor(i, pixels.ColorHSV(pixelHue, 255, brightness));
    }
  }
  pixels.show();
}

void printMidiValues() {
  // Encoder values (relative, print last sent value)
  for (int i = 0; i < 7; i++) {
    Serial.print("ENC"); Serial.print(i+1); Serial.print(":"); Serial.print(lastPos[i]); Serial.print(" ");
  }
  
  // Button states
  for (int i = 0; i < 9; i++) {
    Serial.print("BTN"); Serial.print(i+1); Serial.print(":"); Serial.print(lastButtonStates[i] ? 127 : 0); Serial.print(" ");
  }
  
  // Encoder button states
  for (int i = 0; i < 7; i++) {
    Serial.print("ENCBTN"); Serial.print(i+1); Serial.print(":"); Serial.print(lastEncoderButtonStates[i] ? 127 : 0); Serial.print(" ");
  }
  
  // Joystick
  Serial.print("JOY_X:"); Serial.print(map(smoothX, 0, 1023, 0, 127)); Serial.print(" ");
  Serial.print("JOY_Y:"); Serial.print(map(smoothY, 0, 1023, 0, 127)); Serial.print(" ");
  Serial.print("JOYBTN:"); Serial.print(lastJoystickButtonState ? 127 : 0); Serial.print(" ");
  
  // Expression pedal
  Serial.print("EXP:"); Serial.print(lastExpression); Serial.print(" ");
  
  // FSRs
  Serial.print("FSR1:"); Serial.print((int)(127 * pow(smoothFsr1 / 1023.0, 2))); Serial.print(" ");
  Serial.print("FSR2:"); Serial.print((int)(127 * pow(smoothFsr2 / 1023.0, 2))); Serial.print(" ");
  
  // Ultrasonic
  Serial.print("ULTRA:"); Serial.print(constrain(map(smoothDistance, 2, 46, 0, 127), 0, 127)); Serial.print(" ");
  
  // MPU6050
  Serial.print("ACCEL_X:"); Serial.print(constrain(map(smoothAccelX, -17000, 17000, 0, 127), 0, 127)); Serial.print(" ");
  Serial.print("ACCEL_Y:"); Serial.print(constrain(map(smoothAccelY, -17000, 17000, 0, 127), 0, 127)); Serial.print(" ");
  Serial.print("ACCEL_Z:"); Serial.print(constrain(map(smoothAccelZ, -17000, 17000, 0, 127), 0, 127));
  
  Serial.println();
}

// ---- VL53L1X helper ---------------------------------------------------------
// Initializes a VL53L1X on the given I2C bus. Each sensor is alone on its bus,
// so it keeps the 0x29 default — no XSHUT, no re-addressing. Returns true on
// success.
bool configureTOF(VL53L1X &sensor, TwoWire &bus, const char *label) {
  sensor.setBus(&bus);
  sensor.setTimeout(50);
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    if (sensor.init()) {
      sensor.setDistanceMode(VL53L1X::Long);
      sensor.setMeasurementTimingBudget(33000);
      sensor.startContinuous(50);
      Serial.print(label);
      Serial.println(" OK");
      return true;
    }
    Serial.print(label);
    Serial.print(" init failed (attempt ");
    Serial.print(attempt);
    Serial.println("/3)");
    delay(50);
  }
  Serial.print(label);
  Serial.println(" DEAD — check SDA/SCL and 3V3 power");
  return false;
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for Serial to connect (up to 2 seconds)
  
  // Initialize NeoPixel
  pixels.begin();
  pixels.setBrightness(80); // Strip power path: Pi GPIO 5V → touchscreen
                            // power pin → Teensy/strip. Budget is the Pi's
                            // full 5V rail (27W PSU minus Pi's own ~1.5 A).
                            // 126 LEDs × ~25 mA avg × 80/255 ≈ 1 A from the
                            // strip — fits the ~3 A available for accessories
                            // with headroom. Empirically: 180 → undervoltage,
                            // 30 → EXT5V 4.86 V clean. 80 sits comfortably
                            // between. Raise only after watching EXT5V_V stay
                            // above 4.75 V live (`vcgencmd pmic_read_adc`).
  pixels.clear();
  pixels.show();

  // Pin modes
  for (int i = 0; i < 9; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  for (int i = 0; i < 7; i++) {
    pinMode(encoderButtonPins[i], INPUT_PULLUP);
  }
  pinMode(XY_BUTTON, INPUT_PULLUP);

  // Encoder pins
  pinMode(2, INPUT_PULLUP); pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP); pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP); pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP); pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP); pinMode(8, INPUT_PULLUP); // enc5 (pin 13 not usable)
  pinMode(14, INPUT_PULLUP); pinMode(15, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP); pinMode(23, INPUT_PULLUP);

  // Analog inputs
  pinMode(XY_X, INPUT);
  pinMode(XY_Y, INPUT);
  pinMode(EXPRESSION_PIN, INPUT);
  pinMode(FSR1_PIN, INPUT);
  pinMode(FSR2_PIN, INPUT);
  pinMode(MIC_L_PIN, INPUT);
  pinMode(MIC_R_PIN, INPUT);

  // Two I2C buses:
  //   Wire  — pins 18 (SDA) / 19 (SCL): MPU9250 + LiDAR 1
  //   Wire2 — pins 25 (SDA) / 24 (SCL): LiDAR 2, on its own bus
  // Each VL53L1X is the only ToF on its bus, so both keep the 0x29 default —
  // no XSHUT pin and no re-addressing needed.
  Wire.begin();
  Wire.setClock(400000);
  Wire2.begin();
  Wire2.setClock(400000);
  delay(100);

  // MPU9250 init (register-compatible with MPU6050), on Wire
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B);  // PWR_MGMT_1
  Wire.write(0x00);  // wake from sleep
  mpuok = (Wire.endTransmission(true) == 0); // 0 = device ACKed
  Serial.println(mpuok ? "MPU OK (Wire, 0x68)" : "MPU FAILED — check wiring/AD0");

  tof1ok = configureTOF(tof1, Wire,  "TOF1 (Wire, pins 18/19)");
  tof2ok = configureTOF(tof2, Wire2, "TOF2 (Wire2, pins 24/25)");

  // Initialize random seed
  randomSeed(analogRead(0));
  for (int i = 0; i < NUMPIXELS; i++) {
    targetOffsets[i] = random(-2000, 2000); // Initial random hue offset
    pixelOffsets[i] = targetOffsets[i];
  }
}

void loop() {
  unsigned long timestamp = millis();
  bool anyEncoderButtonPressed = false;

  // 9 Buttons (CC 60-68)
  const uint8_t buttonCCs[9] = {CC_BUTTON_1, CC_BUTTON_2, CC_BUTTON_3, CC_BUTTON_4, CC_BUTTON_5, CC_BUTTON_6, CC_BUTTON_7, CC_BUTTON_8, CC_BUTTON_9};
  for (int i = 0; i < 9; i++) {
    if (buttons[i].update()) {
      bool currentState = (buttons[i].read() == LOW);
      if (currentState != lastButtonStates[i]) {
        controlChange(MIDI_CHANNEL, buttonCCs[i], currentState ? 127 : 0);
        lastButtonStates[i] = currentState;
      }
    }
  }

  // Encoders (CC 20-26)
  Encoder encoders[7] = {enc1, enc2, enc3, enc4, enc5, enc6, enc7};
  const uint8_t encoderCCs[7] = {CC_ENCODER_1, CC_ENCODER_2, CC_ENCODER_3, CC_ENCODER_4, CC_ENCODER_5, CC_ENCODER_6, CC_ENCODER_7};
  for (int i = 0; i < 7; i++) {
    long pos = encoders[i].read() / 4;
    if (pos != lastPos[i]) {
      int delta = pos - lastPos[i];
      if (delta > 0) {
        controlChange(MIDI_CHANNEL, encoderCCs[i], 127);
        encDirLatched[i] = 1;
      } else if (delta < 0) {
        controlChange(MIDI_CHANNEL, encoderCCs[i], 0);
        encDirLatched[i] = -1;
      }
      lastPos[i] = pos;
    }
  }

  // Encoder buttons (CC 70-76)
  const uint8_t encButtonCCs[7] = {CC_ENC_BUTTON_1, CC_ENC_BUTTON_2, CC_ENC_BUTTON_3, CC_ENC_BUTTON_4, CC_ENC_BUTTON_5, CC_ENC_BUTTON_6, CC_ENC_BUTTON_7};
  for (int i = 0; i < 7; i++) {
    if (encoderButtons[i].update()) {
      bool currentState = (encoderButtons[i].read() == LOW);
      if (currentState != lastEncoderButtonStates[i]) {
        controlChange(MIDI_CHANNEL, encButtonCCs[i], currentState ? 127 : 0);
        lastEncoderButtonStates[i] = currentState;
      }
    }
    if (encoderButtons[i].read() == LOW) anyEncoderButtonPressed = true;
  }

  // Joystick (CC 27-28) — emit only when the scaled MIDI value actually changes.
  int xVal = analogRead(XY_X);
  int yVal = analogRead(XY_Y);
  smoothX = ALPHA * xVal + (1 - ALPHA) * smoothX;
  smoothY = ALPHA * yVal + (1 - ALPHA) * smoothY;
  int scaledJoyX = map((int)smoothX, 0, 1023, 0, 127);
  int scaledJoyY = map((int)smoothY, 0, 1023, 0, 127);
  if (scaledJoyX != lastX) {
    controlChange(MIDI_CHANNEL, CC_JOY_X, scaledJoyX);
    lastX = scaledJoyX;
  }
  if (scaledJoyY != lastY) {
    controlChange(MIDI_CHANNEL, CC_JOY_Y, scaledJoyY);
    lastY = scaledJoyY;
  }

  // Joystick button (CC 77)
  if (joystickButton.update()) {
    bool currentState = (joystickButton.read() == LOW);
    if (currentState != lastJoystickButtonState) {
      controlChange(MIDI_CHANNEL, CC_JOY_BUTTON, currentState ? 127 : 0);
      lastJoystickButtonState = currentState;
    }
  }

  // Expression Pedal (CC 29)
  int expVal = analogRead(EXPRESSION_PIN);
  int scaledExp = constrain(map(expVal, 32, 998, 0, 127), 0, 127);
  if (!pedalActive && scaledExp <= 3) {
    pedalActive = true;
    pedalActiveStart = timestamp;
  }
  // Emit only on scaled MIDI change. lastExpression now holds the LAST SENT
  // 0..127 value (was the raw analog reading); printMidiValues() is updated
  // to print it directly. This kills sub-MIDI-unit chatter from analog noise.
  if (scaledExp != lastExpression) {
    controlChange(MIDI_CHANNEL, CC_EXPRESSION, scaledExp);
    lastExpression = scaledExp;
  }
  if (pedalActive && (timestamp - pedalActiveStart > PEDAL_ACTIVE_DURATION)) {
    pedalActive = false;
  }

  // FSRs (CC 34-35) — scaled-change-only. lastFsr1/2 now hold last sent 0..127.
  int fsr1Val = analogRead(FSR1_PIN);
  smoothFsr1 = ALPHA * fsr1Val + (1 - ALPHA) * smoothFsr1;
  int scaledFsr1 = (int)(127 * pow(smoothFsr1 / 1023.0, 2));
  if (scaledFsr1 != lastFsr1) {
    controlChange(MIDI_CHANNEL, CC_FSR1, scaledFsr1);
    lastFsr1 = scaledFsr1;
  }
  int fsr2Val = analogRead(FSR2_PIN);
  smoothFsr2 = ALPHA * fsr2Val + (1 - ALPHA) * smoothFsr2;
  int scaledFsr2 = (int)(127 * pow(smoothFsr2 / 1023.0, 2));
  if (scaledFsr2 != lastFsr2) {
    controlChange(MIDI_CHANNEL, CC_FSR2, scaledFsr2);
    lastFsr2 = scaledFsr2;
  }

  // Contact mics (CC 78-79) — piezo discs biased to mid-rail (~1.65V → ~512 ADC).
  // abs(reading - 512) is the instantaneous AC magnitude; asymmetric attack/release
  // envelope follower gives a natural tap-decay shape.
  int micLVal = abs(analogRead(MIC_L_PIN) - 512);
  int micRVal = abs(analogRead(MIC_R_PIN) - 512);
  envMicL = (micLVal > envMicL)
    ? ALPHA_MIC_ATK * micLVal + (1 - ALPHA_MIC_ATK) * envMicL
    : ALPHA_MIC_REL * micLVal + (1 - ALPHA_MIC_REL) * envMicL;
  envMicR = (micRVal > envMicR)
    ? ALPHA_MIC_ATK * micRVal + (1 - ALPHA_MIC_ATK) * envMicR
    : ALPHA_MIC_REL * micRVal + (1 - ALPHA_MIC_REL) * envMicR;
  int scaledMicL = constrain(map((int)envMicL, 0, 200, 0, 127), 0, 127);
  int scaledMicR = constrain(map((int)envMicR, 0, 200, 0, 127), 0, 127);
  if (scaledMicL != lastMicL) {
    controlChange(MIDI_CHANNEL, CC_MIC_L, scaledMicL);
    lastMicL = scaledMicL;
  }
  if (scaledMicR != lastMicR) {
    controlChange(MIDI_CHANNEL, CC_MIC_R, scaledMicR);
    lastMicR = scaledMicR;
  }

  // VL53L1X #1 (CC 30) + #2 (CC 36) — poll at 20 Hz, emit on change only
  if (timestamp - lastUltrasonic > 50) {
    // Guard on the init flags: a sensor that failed bring-up would otherwise
    // block here for the full read timeout every cycle and stall the loop.
    if (tof1ok) {
      uint16_t mm1 = tof1.readRangeContinuousMillimeters();
      lidar1mm = tof1.timeoutOccurred() ? -1 : (int)mm1;
      if (!tof1.timeoutOccurred() && mm1 > 0 && mm1 <= 2000) {
        smoothDistance = ALPHA * (mm1 / 10) + (1 - ALPHA) * smoothDistance;
        int scaledDist = constrain(map((int)smoothDistance, 2, 46, 0, 127), 0, 127);
        if (scaledDist != lastUltra) { controlChange(MIDI_CHANNEL, CC_ULTRASONIC, scaledDist); lastUltra = scaledDist; }
      }
    }
    if (tof2ok) {
      uint16_t mm2 = tof2.readRangeContinuousMillimeters();
      lidar2mm = tof2.timeoutOccurred() ? -1 : (int)mm2;
      if (!tof2.timeoutOccurred() && mm2 > 0 && mm2 <= 2000) {
        smoothDist2 = ALPHA * (mm2 / 10) + (1 - ALPHA) * smoothDist2;
        int scaledDist2 = constrain(map((int)smoothDist2, 2, 46, 0, 127), 0, 127);
        if (scaledDist2 != lastUltra2) { controlChange(MIDI_CHANNEL, CC_DISTANCE_2, scaledDist2); lastUltra2 = scaledDist2; }
      }
    }
    lastUltrasonic = timestamp;
  }

  // MPU9250 (CC 31-33) — register-compatible with MPU6050; default ±2g = 16384 LSB/g
  if (mpuok && timestamp - lastMPU > 20) {
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDRESS, (uint8_t)6, true);
    int16_t accelX = Wire.read() << 8 | Wire.read();
    int16_t accelY = Wire.read() << 8 | Wire.read();
    int16_t accelZ = Wire.read() << 8 | Wire.read();
    // Lighter smoothing so motion feels live (~200 ms settle, was ~600 ms).
    smoothAccelX = ALPHA_ACCEL * accelX + (1 - ALPHA_ACCEL) * smoothAccelX;
    smoothAccelY = ALPHA_ACCEL * accelY + (1 - ALPHA_ACCEL) * smoothAccelY;
    smoothAccelZ = ALPHA_ACCEL * accelZ + (1 - ALPHA_ACCEL) * smoothAccelZ;
    // Emit only when the SCALED MIDI value changes — prevents the rail-noise
    // spam the old raw-50-unit threshold caused when an axis sat at ±17000.
    int scaledAX = constrain(map((int)smoothAccelX, -17000, 17000, 0, 127), 0, 127);
    int scaledAY = constrain(map((int)smoothAccelY, -17000, 17000, 0, 127), 0, 127);
    int scaledAZ = constrain(map((int)smoothAccelZ, -17000, 17000, 0, 127), 0, 127);
    if (scaledAX != lastAccelX) {
      controlChange(MIDI_CHANNEL, CC_ACCEL_X, scaledAX);
      lastAccelX = scaledAX;
    }
    if (scaledAY != lastAccelY) {
      controlChange(MIDI_CHANNEL, CC_ACCEL_Y, scaledAY);
      lastAccelY = scaledAY;
    }
    if (scaledAZ != lastAccelZ) {
      controlChange(MIDI_CHANNEL, CC_ACCEL_Z, scaledAZ);
      lastAccelZ = scaledAZ;
    }
    lastMPU = timestamp;
  }

  // Update random offsets for NeoPixels
  if (timestamp - lastOffsetUpdate > OFFSET_UPDATE_INTERVAL) {
    for (int i = 0; i < NUMPIXELS; i++) {
      targetOffsets[i] = random(-2000, 2000); // New random hue offset
    }
    lastOffsetUpdate = timestamp;
  }

  // Smoothly interpolate pixel offsets
  for (int i = 0; i < NUMPIXELS; i++) {
    pixelOffsets[i] = ALPHA * targetOffsets[i] + (1 - ALPHA) * pixelOffsets[i];
  }

  // NeoPixel Rainbow Effect (updated every 10ms). Pedal drives BRIGHTNESS in
  // updateNeoPixels(); hue cycles at the original fixed step.
  if (timestamp - lastNeoPixelUpdate > 10) {
    // baseHue ticks within HUE_PERIOD (not 65536) so the triangle-wave folding
    // in updateNeoPixels() is continuous across the wrap. At +42/tick and a
    // 10 ms cadence, one full green→violet→green sweep takes ~13.4 s.
    baseHue = (baseHue + 42) % HUE_PERIOD;
    updateNeoPixels(expVal, anyEncoderButtonPressed);
    lastNeoPixelUpdate = timestamp;
  }

  // Debug: print the two LiDAR distances and the MPU accelerometer. Swap
  // printMidiValues() back in here to restore the full MIDI value dump.
  if (timestamp - lastSerialPrint > SERIAL_PRINT_INTERVAL) {
    Serial.print("TOF1: ");
    if (!tof1ok)           Serial.print("not initialized");
    else if (lidar1mm < 0) Serial.print("timeout");
    else { Serial.print(lidar1mm); Serial.print(" mm"); }
    Serial.print("    TOF2: ");
    if (!tof2ok)           Serial.print("not initialized");
    else if (lidar2mm < 0) Serial.print("timeout");
    else { Serial.print(lidar2mm); Serial.print(" mm"); }
    Serial.print("    ACCEL X/Y/Z: ");
    if (!mpuok) Serial.print("not initialized");
    else {
      Serial.print((int)smoothAccelX); Serial.print(" ");
      Serial.print((int)smoothAccelY); Serial.print(" ");
      Serial.print((int)smoothAccelZ);
    }
    Serial.print("    JOY X/Y: ");
    Serial.print((int)smoothX); Serial.print(" ");
    Serial.print((int)smoothY);
    Serial.print(" BTN: ");
    Serial.print(lastJoystickButtonState ? 1 : 0);
    // 9 main buttons: prints the button's number when pressed, 0 otherwise.
    // e.g. "1 0 3 0 0 0 0 0 0" means buttons 1 and 3 are pressed.
    Serial.print("    BTNS: ");
    for (int i = 0; i < 9; i++) {
      Serial.print(lastButtonStates[i] ? (i + 1) : 0);
      if (i < 8) Serial.print(" ");
    }
    Serial.print("    EXP(p41): "); Serial.print(lastExpression);
    Serial.print("    FSR: ");
    Serial.print((int)smoothFsr1); Serial.print(" ");
    Serial.print((int)smoothFsr2);
    Serial.print("    MICS L/R: ");
    Serial.print((int)envMicL); Serial.print(" ");
    Serial.print((int)envMicR);
    // 7 encoders: L = turned left since last print, R = right, B = button
    // currently held, . = idle. Direction latch clears each print cycle.
    Serial.print("    ENCS: ");
    for (int i = 0; i < 7; i++) {
      char c = '.';
      if (lastEncoderButtonStates[i])   c = 'B';
      else if (encDirLatched[i] > 0)    c = 'R';
      else if (encDirLatched[i] < 0)    c = 'L';
      Serial.print(c);
      if (i < 6) Serial.print(' ');
      encDirLatched[i] = 0;
    }
    Serial.println();
    lastSerialPrint = timestamp;
  }

  delay(10);
}