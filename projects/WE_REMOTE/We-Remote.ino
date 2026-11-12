/*
  WE REMOTE — Full Firmware
  Modes: Gamepad (green), Keyboard (blue), MIDI CC (purple)
  Joystick click cycles modes. Saves to flash, reboots.

  SparkFun Thing Plus ESP32-S3
  Board: "ESP32S3 Dev Module" | USB CDC On Boot: Enabled

  Libraries:
    ESP32-BLE-Gamepad
    HijelHID_BLEKeyboard
    NimBLE-Arduino (MIDI implemented directly, no external MIDI lib)
    Adafruit_MPU6050
    Adafruit_NeoPixel
    VL53L0X (Pololu)
    SparkFun_MAX1704x
*/

#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_NeoPixel.h>
#include <VL53L0X.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <BleGamepad.h>
#include <HijelHID_BLEKeyboard.h>
// BLE MIDI implemented directly via NimBLE — no library needed
#include <NimBLEDevice.h>
#define MIDI_SERVICE_UUID      "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"
NimBLECharacteristic* midiChar  = nullptr;
NimBLECharacteristic* mouseReport = nullptr;
bool midiConnected = false;

// BLE HID mouse descriptor: 3 buttons + relative X/Y
static const uint8_t mouseHIDDesc[] = {
  0x05,0x01, 0x09,0x02, 0xA1,0x01, 0x09,0x01, 0xA1,0x00,
  0x05,0x09, 0x19,0x01, 0x29,0x03, 0x15,0x00, 0x25,0x01,
  0x95,0x03, 0x75,0x01, 0x81,0x02, 0x95,0x01, 0x75,0x05,
  0x81,0x03, 0x05,0x01, 0x09,0x30, 0x09,0x31, 0x15,0x81,
  0x25,0x7F, 0x75,0x08, 0x95,0x02, 0x81,0x06, 0xC0,0xC0
};

class MidiServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*)    { midiConnected = true; }
  void onDisconnect(NimBLEServer*) { midiConnected = false; NimBLEDevice::startAdvertising(); }
};


// --- PINS ---
#define LED_PIN      13
#define PIN_SDA      8
#define PIN_SCL      9
#define PIN_JOY_X    1
#define PIN_JOY_Y    2
#define PIN_MOTOR    11
#define PIN_NEOPIXEL 12
#define NUM_PIXELS   4

// buttons: A  B    -     PWR(physical btn 1)  +     -    2    JOY
const uint8_t btnPins[]  = {4, 15, 5, 10, 6, 7, 14, 16};
const char*   btnNames[] = {"A","B","-","PWR","+","1","2","JOY"};
const uint8_t NUM_BTNS   = 8;
#define BTN_A     0
#define BTN_B     1
#define BTN_MINUS 2
#define BTN_PWR   3
#define BTN_PLUS  4
#define BTN_1     5
#define BTN_2     6
#define BTN_JOY   7

// --- MODES ---
enum Mode { MODE_GAMEPAD=0, MODE_KEYBOARD=1, MODE_MIDI=2, MODE_MOUSE=3 };
Mode currentMode;
Preferences prefs;
RTC_DATA_ATTR bool fromModeSwitch = false;

// --- PERIPHERALS ---
Adafruit_MPU6050 mpu;
VL53L0X lidar;
SFE_MAX1704X fuel;
bool mpuOk = false;
bool lidarOk = false;
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
BleGamepad gamepad("WE Remote", "EMGOR", 100);
HijelHID_BLEKeyboard keyboard("WE Remote");

// --- SENSOR STATE ---
float smoothAccelX=0, smoothAccelY=0, smoothAccelZ=0;
float smoothGyroX=0,  smoothGyroY=0,  smoothGyroZ=0;
float smoothLidar=0;
float gyroMag=0;

// --- BUTTON STATE ---
bool btnState[8]={false};
bool btnPrev[8]={false};

// --- LED STATE ---
float pixelPhase[NUM_PIXELS];
float pixelSpeed[NUM_PIXELS];
bool  flashing=false;
unsigned long flashStart=0;
float flashBrightness=0;

// --- CHARGING STATE ---
bool wasCharging = false;
unsigned long chargingBuzzEnd  = 0;
unsigned long chargingFlashEnd = 0;

// ============================================================
// HELPERS
// ============================================================
bool btnPressed(int i)  { return  btnState[i] && !btnPrev[i]; }
bool btnReleased(int i) { return !btnState[i] &&  btnPrev[i]; }
int16_t readJoyX() { return map(analogRead(PIN_JOY_X), 0, 4095, -127, 127); }
int16_t readJoyY() { return map(analogRead(PIN_JOY_Y), 0, 4095, -127, 127); }

uint8_t floatToCC(float v, float mn, float mx) {
  return (uint8_t)constrain((int)((v-mn)/(mx-mn)*127.0f), 0, 127);
}

uint32_t hsv(float h, float s, float v) {
  h = fmod(h, 360.0f);
  float c=v*s, x=c*(1.0f-fabs(fmod(h/60.0f,2.0f)-1.0f)), m=v-c;
  float r=0,g=0,b=0;
  if      (h<60)  {r=c;g=x;}
  else if (h<120) {r=x;g=c;}
  else if (h<180) {g=c;b=x;}
  else if (h<240) {g=x;b=c;}
  else if (h<300) {r=x;b=c;}
  else            {r=c;b=x;}
  return pixels.Color((uint8_t)((r+m)*255),(uint8_t)((g+m)*255),(uint8_t)((b+m)*255));
}

void triggerFlash() {
  flashing = true;
  flashStart = millis();
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIN_MOTOR, OUTPUT);
  for (int i=0;i<NUM_BTNS;i++) pinMode(btnPins[i], INPUT_PULLUP);

  for (int i=0;i<NUM_PIXELS;i++) {
    pixelPhase[i] = random(0,360);
    pixelSpeed[i] = 0.3f + random(0,100)/200.0f;
  }

  pixels.begin();
  pixels.setBrightness(30);
  pixels.clear(); pixels.show();

  Wire.begin(PIN_SDA, PIN_SCL);

  if (mpu.begin()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpuOk = true;
  }

  lidar.setTimeout(500);
  if (lidar.init(true)) {
    lidar.setMeasurementTimingBudget(33000);
    lidar.startContinuous(0);
    lidarOk = true;
  }

  fuel.begin();

  prefs.begin("weremote", false);
  currentMode = (Mode)prefs.getUChar("mode", MODE_MIDI);

  switch(currentMode) {
    case MODE_GAMEPAD:  initGamepad();  break;
    case MODE_KEYBOARD: initKeyboard(); break;
    case MODE_MIDI:     initMidi();     break;
    case MODE_MOUSE:    initMouse();    break;
  }

  if (fromModeSwitch) {
    // fast mode switch: flash the mode color with HUGE vibration
    fromModeSwitch = false;
    pixels.setBrightness(128);
    uint32_t mc;
    switch(currentMode) {
      case MODE_GAMEPAD:  mc = pixels.Color(0,255,0);   break; // green
      case MODE_KEYBOARD: mc = pixels.Color(0,120,255); break; // blue
      case MODE_MIDI:     mc = pixels.Color(255,80,0);  break; // orange
      case MODE_MOUSE:    mc = pixels.Color(255,200,0); break; // yellow
      default:            mc = pixels.Color(255,255,255); break;
    }
    pixels.fill(mc); pixels.show();
    analogWrite(PIN_MOTOR, 255);
    delay(500);
    analogWrite(PIN_MOTOR, 0);
    pixels.clear(); pixels.show();
  } else {
    // fresh boot: HUGE vibration immediately + rainbow sweep
    pixels.setBrightness(128);
    analogWrite(PIN_MOTOR, 255);  // full blast from the first millisecond
    for (int i = 0; i < 60; i++) {
      for (int p = 0; p < NUM_PIXELS; p++)
        pixels.setPixelColor(p, hsv(fmod(i * 12.0f + p * 90.0f, 360.0f), 1.0f, 1.0f));
      pixels.show();
      delay(20);
    }
    delay(2500);  // motor keeps shaking through the held rainbow frame
    analogWrite(PIN_MOTOR, 0);
    pixels.clear(); pixels.show(); delay(100);
  }

  triggerFlash();
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  digitalWrite(LED_PIN, HIGH);

  readButtons();
  readSensors();
  checkCharging();
  updateHaptic();

  // hold PWR(G10)+2(G14) for 3s → hard reset
  static unsigned long btn12HoldStart = 0;
  if (btnState[3] && btnState[6]) {
    if (btn12HoldStart == 0) btn12HoldStart = millis();
    if (millis() - btn12HoldStart >= 3000) ESP.restart();
  } else {
    btn12HoldStart = 0;
  }

  // button press → jump lava lamp to random position
  for (int i = 0; i < NUM_BTNS; i++) {
    if (btnPressed(i)) {
      for (int p = 0; p < NUM_PIXELS; p++) {
        pixelPhase[p] = random(0, 360);
        pixelSpeed[p] = 0.3f + random(0, 100) / 200.0f;
      }
      break;
    }
  }

  checkJoystick();
  updateLEDs();


  switch(currentMode) {
    case MODE_GAMEPAD:  loopGamepad();  break;
    case MODE_KEYBOARD: loopKeyboard(); break;
    case MODE_MIDI:     loopMidi();     break;
    case MODE_MOUSE:    loopMouse();    break;
  }

  digitalWrite(LED_PIN, LOW);
  delay(10);
}

// ============================================================
// SENSORS
// ============================================================
void readSensors() {
  if (mpuOk) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    smoothAccelX += 0.3f*(a.acceleration.x-smoothAccelX);
    smoothAccelY += 0.3f*(a.acceleration.y-smoothAccelY);
    smoothAccelZ += 0.3f*(a.acceleration.z-smoothAccelZ);
    smoothGyroX  += 0.3f*(g.gyro.x-smoothGyroX);
    smoothGyroY  += 0.3f*(g.gyro.y-smoothGyroY);
    smoothGyroZ  += 0.3f*(g.gyro.z-smoothGyroZ);
    gyroMag = sqrt(smoothGyroX*smoothGyroX + smoothGyroY*smoothGyroY + smoothGyroZ*smoothGyroZ);
  }

  if (lidarOk) {
    uint16_t raw = lidar.readRangeContinuousMillimeters();
    if (!lidar.timeoutOccurred() && raw >= 30 && raw <= 1500)
      smoothLidar += 0.5f*(raw-smoothLidar);
    else
      smoothLidar *= 0.85f; // decay toward 0 when hand moves out of range
  }
}

void readButtons() {
  static bool      rawState[8]     = {false};
  static unsigned long debounceT[8] = {0};
  for (int i = 0; i < NUM_BTNS; i++) {
    bool raw = !digitalRead(btnPins[i]);
    if (raw != rawState[i]) { rawState[i] = raw; debounceT[i] = millis(); }
    btnPrev[i] = btnState[i];
    if (millis() - debounceT[i] >= 15) btnState[i] = rawState[i];
  }
}

// ============================================================
// CHARGING DETECTION
// ============================================================
void checkCharging() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 2000) return;
  lastCheck = millis();

  bool isCharging = fuel.getChangeRate() > 1.0f;
  if (isCharging && !wasCharging) {
    chargingBuzzEnd  = millis() + 600;
    chargingFlashEnd = millis() + 2000;
  }
  wasCharging = isCharging;
}

// ============================================================
// HAPTIC
// ============================================================
void updateHaptic() {
  if (millis() < chargingBuzzEnd) {
    analogWrite(PIN_MOTOR, 180);
    return;
  }
  static float motorPower = 0;
  float gyroAvg = (fabs(smoothGyroX) + fabs(smoothGyroY) + fabs(smoothGyroZ)) / 3.0f;
  float target = constrain(gyroAvg * gyroAvg * 220.0f, 0.0f, 255.0f);
  if (target > motorPower) motorPower = target;
  else motorPower *= 0.975f;
  analogWrite(PIN_MOTOR, (int)motorPower);
}

// ============================================================
// JOYSTICK: short press = mode switch, 3s hold = sleep
// ============================================================
static unsigned long joyHoldStart = 0;
static bool joyHolding = false;
static bool joyUsed = false;

void checkModeSwitch() {} // kept for compile, logic moved below

void checkSleep() {} // kept for compile, logic moved below

void checkJoystick() {
  if (btnState[BTN_JOY]) {
    if (!joyHolding) { joyHolding = true; joyHoldStart = millis(); joyUsed = false; }
    if (!joyUsed && millis() - joyHoldStart > 3000) {
      joyUsed = true;

      // Capture each pixel's RGB at current visible level, then fade the
      // RGB values themselves — gives a smooth gradient instead of the
      // blocky steps you get when only decrementing setBrightness().
      uint8_t origR[NUM_PIXELS], origG[NUM_PIXELS], origB[NUM_PIXELS];
      uint8_t bright = pixels.getBrightness();
      for (int i = 0; i < NUM_PIXELS; i++) {
        uint32_t c = pixels.getPixelColor(i);
        origR[i] = ((c >> 16) & 0xFF) * bright / 255;
        origG[i] = ((c >>  8) & 0xFF) * bright / 255;
        origB[i] = ( c        & 0xFF) * bright / 255;
      }
      pixels.setBrightness(255);

      // Smooth gamma-corrected fade: 600 steps at ~166Hz refresh with cubic
      // curve (≈gamma 2.8) so perceptual brightness drops linearly instead
      // of snapping through the low end. HUGE vibration throughout.
      analogWrite(PIN_MOTOR, 255);
      const int steps = 600;
      for (int s = steps; s >= 0; s--) {
        float t = (float)s / (float)steps;
        float gamma = t * t * t;
        for (int i = 0; i < NUM_PIXELS; i++) {
          pixels.setPixelColor(i, pixels.Color(
            (uint8_t)(origR[i] * gamma),
            (uint8_t)(origG[i] * gamma),
            (uint8_t)(origB[i] * gamma)
          ));
        }
        pixels.show();
        delay(6);
      }
      analogWrite(PIN_MOTOR, 0);
      pixels.clear(); pixels.show();
      delay(400);  // let rails settle before arming wake (avoids spurious wake)

      // Wake ONLY on joystick click (GPIO 16 → LOW). Explicit RTC pullup
      // keeps the pin from floating and catching noise during deep sleep.
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
      rtc_gpio_init((gpio_num_t)16);
      rtc_gpio_set_direction((gpio_num_t)16, RTC_GPIO_MODE_INPUT_ONLY);
      rtc_gpio_pullup_en((gpio_num_t)16);
      rtc_gpio_pulldown_dis((gpio_num_t)16);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)16, 0);
      esp_deep_sleep_start();
    }
  } else {
    if (joyHolding && !joyUsed && millis() - joyHoldStart < 500) {
      Mode next = (Mode)((currentMode+1) % 4);
      prefs.putUChar("mode", next);
      fromModeSwitch = true;
      analogWrite(PIN_MOTOR, 255); delay(80); analogWrite(PIN_MOTOR, 0);
      ESP.restart();
    }
    joyHolding = false;
  }
}

// ============================================================
// NEOPIXELS — lava lamp blue/pink, gyro brightness, mode flash
// ============================================================
void updateLEDs() {
  // charger just plugged in: green flash for 2s
  if (millis() < chargingFlashEnd) {
    pixels.setBrightness(80);
    pixels.fill(pixels.Color(0, 255, 0));
    pixels.show();
    return;
  }

  // low battery: single red flash once per minute (suppressed while charging)
  if (!wasCharging && fuel.getSOC() < 15 && millis() % 60000 < 300) {
    pixels.setBrightness(60);
    pixels.fill(pixels.Color(255, 0, 0));
    pixels.show();
    return;
  }

  // flash state
  if (flashing) {
    float elapsed = (millis()-flashStart)/1000.0f;
    if      (elapsed < 0.3f)  flashBrightness = 1.0f;
    else if (elapsed < 2.0f)  flashBrightness = 1.0f - (elapsed-0.3f)/1.7f;
    else                      { flashBrightness=0; flashing=false; }
  }

  // brightness: base 8, gyro adds up to 50, flash peaks at 70
  float bright = 8.0f + constrain(gyroMag*15.0f, 0.0f, 50.0f);
  if (flashing) bright = flashBrightness * 70.0f;
  pixels.setBrightness((uint8_t)constrain(bright, 0, 255));

  // mode color for flash blend
  uint32_t modeColor;
  switch(currentMode) {
    case MODE_GAMEPAD:  modeColor = pixels.Color(0,255,0);   break; // green
    case MODE_KEYBOARD: modeColor = pixels.Color(0,120,255); break; // blue
    case MODE_MIDI:     modeColor = pixels.Color(255,80,0);  break; // orange
    case MODE_MOUSE:    modeColor = pixels.Color(255,200,0); break; // yellow
    default:            modeColor = pixels.Color(255,255,255); break;
  }

  for (int i=0;i<NUM_PIXELS;i++) {
    // lava lamp: drifts between light blue (200°) and pink (320°)
    float hue = 260.0f + 60.0f * sinf(pixelPhase[i] * 3.14159f / 180.0f);
    uint32_t lava = hsv(hue, 0.75f, 0.6f);

    if (flashing && flashBrightness > 0.2f) {
      float blend = constrain((flashBrightness-0.2f)/0.8f, 0.0f, 1.0f);
      uint8_t lr=(lava>>16)&0xFF, lg=(lava>>8)&0xFF, lb=lava&0xFF;
      uint8_t mr=(modeColor>>16)&0xFF, mg=(modeColor>>8)&0xFF, mb=modeColor&0xFF;
      pixels.setPixelColor(i, pixels.Color(
        (uint8_t)(lr*(1-blend)+mr*blend),
        (uint8_t)(lg*(1-blend)+mg*blend),
        (uint8_t)(lb*(1-blend)+mb*blend)
      ));
    } else {
      pixels.setPixelColor(i, lava);
    }

    pixelPhase[i] = fmod(pixelPhase[i]+pixelSpeed[i], 360.0f);
  }
  pixels.show();
}

// ============================================================
// GAMEPAD
// ============================================================
BleGamepadConfiguration gamepadCfg;

void initGamepad() {
  gamepadCfg.setAutoReport(false);
  gamepadCfg.setButtonCount(8);
  gamepadCfg.setHatSwitchCount(0);
  gamepad.begin(&gamepadCfg);
}

void loopGamepad() {
  if (!gamepad.isConnected()) return;
  gamepad.setLeftThumb(readJoyX()*256, readJoyY()*256);
  gamepad.setRightThumb(
    constrain((int)(smoothAccelX*3000),-32767,32767),
    constrain((int)(smoothAccelY*3000),-32767,32767)
  );
  btnState[BTN_A]     ? gamepad.press(1) : gamepad.release(1);
  btnState[BTN_B]     ? gamepad.press(2) : gamepad.release(2);
  btnState[BTN_1]     ? gamepad.press(3) : gamepad.release(3);
  btnState[BTN_2]     ? gamepad.press(4) : gamepad.release(4);
  btnState[BTN_MINUS] ? gamepad.press(5) : gamepad.release(5);
  btnState[BTN_PLUS]  ? gamepad.press(6) : gamepad.release(6);
  btnState[BTN_PWR]   ? gamepad.press(7) : gamepad.release(7);
  btnState[BTN_JOY]   ? gamepad.press(8) : gamepad.release(8);
  gamepad.sendReport();
}

// ============================================================
// KEYBOARD
// ============================================================
void initKeyboard() { keyboard.begin(); }

void loopKeyboard() {
  if (!keyboard.isConnected()) return;
  int16_t jx=readJoyX(), jy=readJoyY();
  const int DZ=30;
  static bool wD=false,aD=false,sD=false,dD=false;
  bool w=jy<-DZ, s=jy>DZ, a=jx<-DZ, d=jx>DZ;
  if(w&&!wD) keyboard.press((uint8_t)'w'); else if(!w&&wD) keyboard.release((uint8_t)'w');
  if(s&&!sD) keyboard.press((uint8_t)'s'); else if(!s&&sD) keyboard.release((uint8_t)'s');
  if(a&&!aD) keyboard.press((uint8_t)'a'); else if(!a&&aD) keyboard.release((uint8_t)'a');
  if(d&&!dD) keyboard.press((uint8_t)'d'); else if(!d&&dD) keyboard.release((uint8_t)'d');
  wD=w; aD=a; sD=s; dD=d;
  if(btnPressed(BTN_A))     keyboard.press((uint8_t)' ');
  if(btnReleased(BTN_A))    keyboard.release((uint8_t)' ');
  if(btnPressed(BTN_B))     keyboard.press(KEY_LSHIFT);
  if(btnReleased(BTN_B))    keyboard.release(KEY_LSHIFT);
  if(btnPressed(BTN_1))     keyboard.press((uint8_t)'e');
  if(btnReleased(BTN_1))    keyboard.release((uint8_t)'e');
  if(btnPressed(BTN_2))     keyboard.press((uint8_t)'q');
  if(btnReleased(BTN_2))    keyboard.release((uint8_t)'q');
  if(btnPressed(BTN_MINUS)) keyboard.press(KEY_TAB);
  if(btnReleased(BTN_MINUS)) keyboard.release(KEY_TAB);
  if(btnPressed(BTN_PLUS))  keyboard.press(KEY_RETURN);
  if(btnReleased(BTN_PLUS)) keyboard.release(KEY_RETURN);
  if(btnPressed(BTN_PWR))   keyboard.press(KEY_ESCAPE);
  if(btnReleased(BTN_PWR))  keyboard.release(KEY_ESCAPE);
  if(btnPressed(BTN_JOY))   keyboard.press((uint8_t)'r');
  if(btnReleased(BTN_JOY))  keyboard.release((uint8_t)'r');
}

// ============================================================
// MIDI CC — channel 11, CCs 20-36
// ============================================================
void initMidi() {
  NimBLEDevice::init("WE Remote");
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new MidiServerCallbacks());
  NimBLEService* svc = server->createService(MIDI_SERVICE_UUID);
  midiChar = svc->createCharacteristic(
    MIDI_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY
  );
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(MIDI_SERVICE_UUID);
  adv->start();
}

// Bundled packet at 40Hz. Sensors (incl. Z axes) + ALL 7 buttons every cycle.
// Buttons send continuous 0/127 state so Max always shows the true current
// value — snaps cleanly between 0 and 127, no stuck 57.
void loopMidi() {
  if (!midiChar || !NimBLEDevice::getServer() || NimBLEDevice::getServer()->getConnectedCount() == 0) return;

  static unsigned long lastSend = 0;
  if (millis() - lastSend < 25) return;
  lastSend = millis();

  const uint8_t ch       = 11;
  const uint8_t ccStatus = 0xB0 | (ch - 1);

  uint32_t ts    = millis() & 0x1FFF;
  uint8_t tsHigh = 0x80 | ((ts >> 7) & 0x3F);
  uint8_t tsLow  = 0x80 | (ts & 0x7F);

  // 9 sensor CCs: joyX, joyY, accelX, accelY, accelZ, gyroX, gyroY, gyroZ, lidar
  const uint8_t sCC[9]  = {20, 21, 22, 23, 24, 25, 26, 27, 28};
  uint8_t       sVal[9] = {
    (uint8_t)constrain(analogRead(PIN_JOY_X) >> 5, 0, 127),
    (uint8_t)constrain(analogRead(PIN_JOY_Y) >> 5, 0, 127),
    floatToCC(smoothAccelX, -9.6f,  9.6f),
    floatToCC(smoothAccelY, -9.6f,  9.6f),
    floatToCC(smoothAccelZ, -9.6f,  9.6f),
    floatToCC(smoothGyroX,  -5.52f, 5.52f),
    floatToCC(smoothGyroY,  -5.52f, 5.52f),
    floatToCC(smoothGyroZ,  -5.52f, 5.52f),
    floatToCC(smoothLidar,   0,     280)
  };

  // A=CC35 B=CC32 MINUS=CC34 PWR=CC29 PLUS=CC33 1=CC31 2=CC36
  static const uint8_t btnCC[7]   = {35,32,34,29,33,31,36};
  static       bool    prevBtn[7] = {false};

  uint8_t pkt[65];  // 1 + 9 sensors*4 + 7 buttons*4
  int n = 0;
  pkt[n++] = tsHigh;
  for (int i = 0; i < 9; i++) {
    pkt[n++] = tsLow; pkt[n++] = ccStatus; pkt[n++] = sCC[i]; pkt[n++] = sVal[i];
  }
  for (int i = 0; i < 7; i++) {
    uint8_t val = btnState[i] ? 127 : 0;
    pkt[n++] = tsLow; pkt[n++] = ccStatus; pkt[n++] = btnCC[i]; pkt[n++] = val;
    if (btnState[i] != prevBtn[i]) prevBtn[i] = btnState[i];
  }

  midiChar->setValue(pkt, n);
  midiChar->notify();
}

// ============================================================
// MOUSE — BLE HID relative mouse, joystick=cursor, B=left, A=right
// ============================================================
class MouseServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*)    { }
  void onDisconnect(NimBLEServer*) { NimBLEDevice::startAdvertising(); }
};

void initMouse() {
  NimBLEDevice::init("WE Remote");
  NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new MouseServerCallbacks());

  NimBLEService* hid = server->createService("1812");

  NimBLECharacteristic* info = hid->createCharacteristic("2A4A", NIMBLE_PROPERTY::READ);
  uint8_t infoVal[] = {0x11,0x01,0x00,0x03};
  info->setValue(infoVal, 4);

  NimBLECharacteristic* rmap = hid->createCharacteristic("2A4B", NIMBLE_PROPERTY::READ);
  rmap->setValue(mouseHIDDesc, sizeof(mouseHIDDesc));

  hid->createCharacteristic("2A4C", NIMBLE_PROPERTY::WRITE_NR);

  NimBLECharacteristic* proto = hid->createCharacteristic("2A4E", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
  uint8_t protoVal = 0x01;
  proto->setValue(&protoVal, 1);

  mouseReport = hid->createCharacteristic("2A4D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLEDescriptor* ref = mouseReport->createDescriptor("2908", NIMBLE_PROPERTY::READ, 2);
  uint8_t refVal[] = {0x00, 0x01};
  ref->setValue(refVal, 2);

  hid->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID("1812");
  adv->setAppearance(0x03C2);
  adv->start();
}

void loopMouse() {
  if (!mouseReport || !NimBLEDevice::getServer() || NimBLEDevice::getServer()->getConnectedCount() == 0) return;
  static unsigned long lastMouse = 0;
  if (millis() - lastMouse < 16) return; // 60Hz
  lastMouse = millis();

  int16_t jx = readJoyX(); // -127..127
  int16_t jy = readJoyY();
  int8_t dx = (abs(jx) > 12) ? (int8_t)constrain(jx / 3, -127, 127) : 0;
  int8_t dy = (abs(jy) > 12) ? (int8_t)constrain(jy / 3, -127, 127) : 0;

  uint8_t btns = (btnState[BTN_B] ? 1 : 0) | (btnState[BTN_A] ? 2 : 0);
  uint8_t report[3] = {btns, (uint8_t)dx, (uint8_t)dy};
  mouseReport->setValue(report, 3);
  mouseReport->notify();
}
