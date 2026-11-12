#include <Encoder.h>
#include <Bounce.h>
#include <Wire.h>
#include <MIDIUSB.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// Pins
#define BUTTON9_LED 1
#define XY_X A2 // Pin 16 (analog)
#define XY_Y A3 // Pin 17 (analog)
#define XY_BUTTON 34
#define TRIG_PIN 0
#define ECHO_PIN 8
#define EXPRESSION_PIN A10 // Pin 24 - analog
#define FSR1_PIN 20 // Analog pin A6
#define FSR2_PIN 21 // Analog pin A7
#define MPU_ADDRESS 0x68
#define NEOPIXEL_PIN 7 // NeoPixel data pin
#define NUMPIXELS 4 // Number of NeoPixels (four LEDs)

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
Encoder enc5(12, 13);
Encoder enc6(14, 15);
Encoder enc7(22, 23);

const int encoderButtonPins[7] = {35, 36, 37, 38, 39, 40, 41};
const int buttonPins[9] = {25, 26, 27, 28, 29, 30, 31, 32, 33};

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
static int lastX = 0, lastY = 0;
static int lastExpression = 0;
static int lastFsr1 = 0, lastFsr2 = 0;
static int16_t lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
static bool lastButtonStates[9] = {false};
static bool lastEncoderButtonStates[7] = {false};
static bool lastJoystickButtonState = false;
bool anyButtonPressed = false;
unsigned long lastUltrasonic = 0;
unsigned long lastMPU = 0;
unsigned long lastNeoPixelUpdate = 0;
unsigned long lastSerialPrint = 0;
uint16_t baseHue = 0;
bool pedalActive = false;
unsigned long pedalActiveStart = 0;
const unsigned long PEDAL_ACTIVE_DURATION = 60000; // 60 seconds in milliseconds
const unsigned long SERIAL_PRINT_INTERVAL = 1000; // 1 second in milliseconds

// Smoothing variables
float smoothX = 0, smoothY = 0;
float smoothFsr1 = 0, smoothFsr2 = 0;
float smoothDistance = 0;
float smoothAccelX = 0, smoothAccelY = 0, smoothAccelZ = 0;
const float ALPHA = 0.1; // Smoothing factor (0.1 for fast smoothing)

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

unsigned int ping_cm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 50000);
  if (duration == 0) return 0;
  return duration * 0.034 / 2;
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
    int brightness = 255; // Default full brightness
    if (pedalActive) {
      brightness = map(exprValue, 32, 998, 50, 255); // Map expression pedal to 50% to full brightness
    }
    for (int i = 0; i < NUMPIXELS; i++) {
      // Apply smooth random offset to hue
      uint16_t pixelHue = (baseHue + (uint16_t)pixelOffsets[i]) % 65536;
      int pixelBrightness = (i == 1) ? (int)(brightness * 0.3) : brightness; // 15% darker for third NeoPixel
      pixels.setPixelColor(i, pixels.ColorHSV(pixelHue, 255, pixelBrightness));
    }
  }
  pixels.show();
}

void printMidiValues() {
  // Encoder values (relative, print last sent value)
  const uint8_t encoderCCs[7] = {CC_ENCODER_1, CC_ENCODER_2, CC_ENCODER_3, CC_ENCODER_4, CC_ENCODER_5, CC_ENCODER_6, CC_ENCODER_7};
  for (int i = 0; i < 7; i++) {
    Serial.print("ENC"); Serial.print(i+1); Serial.print(":"); Serial.print(lastPos[i]); Serial.print(" ");
  }
  
  // Button states
  const uint8_t buttonCCs[9] = {CC_BUTTON_1, CC_BUTTON_2, CC_BUTTON_3, CC_BUTTON_4, CC_BUTTON_5, CC_BUTTON_6, CC_BUTTON_7, CC_BUTTON_8, CC_BUTTON_9};
  for (int i = 0; i < 9; i++) {
    Serial.print("BTN"); Serial.print(i+1); Serial.print(":"); Serial.print(lastButtonStates[i] ? 127 : 0); Serial.print(" ");
  }
  
  // Encoder button states
  const uint8_t encButtonCCs[7] = {CC_ENC_BUTTON_1, CC_ENC_BUTTON_2, CC_ENC_BUTTON_3, CC_ENC_BUTTON_4, CC_ENC_BUTTON_5, CC_ENC_BUTTON_6, CC_ENC_BUTTON_7};
  for (int i = 0; i < 7; i++) {
    Serial.print("ENCBTN"); Serial.print(i+1); Serial.print(":"); Serial.print(lastEncoderButtonStates[i] ? 127 : 0); Serial.print(" ");
  }
  
  // Joystick
  Serial.print("JOY_X:"); Serial.print(map(smoothX, 0, 1023, 0, 127)); Serial.print(" ");
  Serial.print("JOY_Y:"); Serial.print(map(smoothY, 0, 1023, 0, 127)); Serial.print(" ");
  Serial.print("JOYBTN:"); Serial.print(lastJoystickButtonState ? 127 : 0); Serial.print(" ");
  
  // Expression pedal
  Serial.print("EXP:"); Serial.print(constrain(map(lastExpression, 32, 998, 0, 127), 0, 127)); Serial.print(" ");
  
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

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for Serial to connect (up to 2 seconds)
  
  // Initialize NeoPixel
  pixels.begin();
  pixels.setBrightness(50); // Initial brightness cap
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
  pinMode(BUTTON9_LED, OUTPUT);
  digitalWrite(BUTTON9_LED, LOW);

  // Encoder pins
  pinMode(2, INPUT_PULLUP); pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP); pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP); pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP); pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP); pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP); pinMode(15, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP); pinMode(23, INPUT_PULLUP);

  // Analog inputs
  pinMode(XY_X, INPUT);
  pinMode(XY_Y, INPUT);
  pinMode(EXPRESSION_PIN, INPUT);
  pinMode(FSR1_PIN, INPUT);
  pinMode(FSR2_PIN, INPUT);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // MPU6050 I2C
  Wire.begin();
  Wire.setClock(400000);
  delay(100);
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Initialize random seed
  randomSeed(analogRead(0));
  for (int i = 0; i < NUMPIXELS; i++) {
    targetOffsets[i] = random(-2000, 2000); // Initial random hue offset
    pixelOffsets[i] = targetOffsets[i];
  }
}

void loop() {
  unsigned long timestamp = millis();
  anyButtonPressed = false;
  bool anyEncoderButtonPressed = false;

  // 9 Buttons (CC 60-68)
  const uint8_t buttonCCs[9] = {CC_BUTTON_1, CC_BUTTON_2, CC_BUTTON_3, CC_BUTTON_4, CC_BUTTON_5, CC_BUTTON_6, CC_BUTTON_7, CC_BUTTON_8, CC_BUTTON_9};
  for (int i = 0; i < 9; i++) {
    if (buttons[i].update()) {
      bool currentState = (buttons[i].read() == LOW);
      if (currentState != lastButtonStates[i]) {
        controlChange(MIDI_CHANNEL, buttonCCs[i], currentState ? 127 : 0);
        lastButtonStates[i] = currentState;
        anyButtonPressed = currentState;
      }
    }
    if (buttons[i].read() == LOW) anyButtonPressed = true;
  }
  digitalWrite(BUTTON9_LED, anyButtonPressed ? HIGH : LOW);

  // Encoders (CC 20-26)
  Encoder encoders[7] = {enc1, enc2, enc3, enc4, enc5, enc6, enc7};
  const uint8_t encoderCCs[7] = {CC_ENCODER_1, CC_ENCODER_2, CC_ENCODER_3, CC_ENCODER_4, CC_ENCODER_5, CC_ENCODER_6, CC_ENCODER_7};
  for (int i = 0; i < 7; i++) {
    long pos = encoders[i].read() / 4;
    if (pos != lastPos[i]) {
      int delta = pos - lastPos[i];
      if (delta > 0) {
        controlChange(MIDI_CHANNEL, encoderCCs[i], 127);
      } else if (delta < 0) {
        controlChange(MIDI_CHANNEL, encoderCCs[i], 0);
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

  // Joystick (CC 27-28)
  int xVal = analogRead(XY_X);
  int yVal = analogRead(XY_Y);
  smoothX = ALPHA * xVal + (1 - ALPHA) * smoothX;
  smoothY = ALPHA * yVal + (1 - ALPHA) * smoothY;
  if (abs(smoothX - lastX) > 5) {
    int scaledX = map(smoothX, 0, 1023, 0, 127);
    controlChange(MIDI_CHANNEL, CC_JOY_X, scaledX);
    lastX = smoothX;
  }
  if (abs(smoothY - lastY) > 5) {
    int scaledY = map(smoothY, 0, 1023, 0, 127);
    controlChange(MIDI_CHANNEL, CC_JOY_Y, scaledY);
    lastY = smoothY;
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
  if (abs(expVal - lastExpression) > 1) {
    controlChange(MIDI_CHANNEL, CC_EXPRESSION, scaledExp);
    lastExpression = expVal;
  }
  if (pedalActive && (timestamp - pedalActiveStart > PEDAL_ACTIVE_DURATION)) {
    pedalActive = false;
  }

  // FSRs (CC 34-35)
  int fsr1Val = analogRead(FSR1_PIN);
  smoothFsr1 = ALPHA * fsr1Val + (1 - ALPHA) * smoothFsr1;
  if (abs(smoothFsr1 - lastFsr1) > 3) {
    int scaledFsr1 = (int)(127 * pow(smoothFsr1 / 1023.0, 2));
    controlChange(MIDI_CHANNEL, CC_FSR1, scaledFsr1);
    lastFsr1 = smoothFsr1;
  }
  int fsr2Val = analogRead(FSR2_PIN);
  smoothFsr2 = ALPHA * fsr2Val + (1 - ALPHA) * smoothFsr2;
  if (abs(smoothFsr2 - lastFsr2) > 3) {
    int scaledFsr2 = (int)(127 * pow(smoothFsr2 / 1023.0, 2));
    controlChange(MIDI_CHANNEL, CC_FSR2, scaledFsr2);
    lastFsr2 = smoothFsr2;
  }

  // Ultrasonic (CC 30)
  if (timestamp - lastUltrasonic > 50) {
    int distance = ping_cm();
    smoothDistance = ALPHA * distance + (1 - ALPHA) * smoothDistance;
    int scaledDist = constrain(map(smoothDistance, 2, 46, 0, 127), 0, 127);
    controlChange(MIDI_CHANNEL, CC_ULTRASONIC, scaledDist);
    lastUltrasonic = timestamp;
  }

  // MPU6050 (CC 31-33)
  if (timestamp - lastMPU > 20) {
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDRESS, (uint8_t)6, true);
   
    int16_t accelX = Wire.read() << 8 | Wire.read();
    int16_t accelY = Wire.read() << 8 | Wire.read();
    int16_t accelZ = Wire.read() << 8 | Wire.read();
    smoothAccelX = ALPHA * accelX + (1 - ALPHA) * smoothAccelX;
    smoothAccelY = ALPHA * accelY + (1 - ALPHA) * smoothAccelY;
    smoothAccelZ = ALPHA * accelZ + (1 - ALPHA) * smoothAccelZ;
    if (abs(smoothAccelX - lastAccelX) > 50) {
      int scaledX = constrain(map(smoothAccelX, -17000, 17000, 0, 127), 0, 127);
      controlChange(MIDI_CHANNEL, CC_ACCEL_X, scaledX);
      lastAccelX = smoothAccelX;
    }
    if (abs(smoothAccelY - lastAccelY) > 50) {
      int scaledY = constrain(map(smoothAccelY, -17000, 17000, 0, 127), 0, 127);
      controlChange(MIDI_CHANNEL, CC_ACCEL_Y, scaledY);
      lastAccelY = smoothAccelY;
    }
    if (abs(smoothAccelZ - lastAccelZ) > 50) {
      int scaledZ = constrain(map(smoothAccelZ, -17000, 17000, 0, 127), 0, 127);
      controlChange(MIDI_CHANNEL, CC_ACCEL_Z, scaledZ);
      lastAccelZ = smoothAccelZ;
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

  // NeoPixel Rainbow Effect (updated every 10ms)
  if (timestamp - lastNeoPixelUpdate > 10) {
    baseHue = (baseHue + 42) % 65536; // Increment hue for rainbow effect
    updateNeoPixels(expVal, anyEncoderButtonPressed);
    lastNeoPixelUpdate = timestamp;
  }

  // Print MIDI values every second
  if (timestamp - lastSerialPrint > SERIAL_PRINT_INTERVAL) {
    printMidiValues();
    lastSerialPrint = timestamp;
  }

  delay(10);
}