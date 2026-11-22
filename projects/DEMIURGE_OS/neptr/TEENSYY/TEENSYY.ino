#include <Encoder.h>
#include <Bounce.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <VL53L1X.h>
#include <math.h>

// ============================================================================
//  NEPTR Phase 4 — Teensy firmware (SERIAL + MIDI OUTPUT)
//
//  Dual output, built as usb=serialmidi:
//    1. One structured raw frame every 10 ms over USB CDC serial (for the
//       Pi-side bridge csound/serial_bridge.py).
//    2. The same events directly over USB-MIDI (see the MIDI mirror section),
//       so csound's midi_handler.orc works whether the bridge is running or
//       not. Both paths use identical scaling, so they agree bit-for-bit.
//
//  The previous MIDI firmware lives at TEENSYY/midi_reference/TEENSYY_midi.ino
//  as a reference. The CC mapping it used is documented at
//  TEENSYY/midi_reference/SENSOR_MAP.md and the bridge mirrors it exactly.
//
//  Frame format (one line, newline-terminated, 100 Hz):
//
//    NEPTR2 t=<ms>
//           e1=<delta> e2=<delta> e3=<delta> e4=<delta>
//           e5=<delta> e6=<delta> e7=<delta>
//           b1=<0|1> ... b9=<0|1>
//           eb1=<0|1> ... eb7=<0|1>
//           jb=<0|1>
//           jx=<0..4095> jy=<0..4095>
//           exp=<0..4095>
//           fsr1=<0..4095> fsr2=<0..4095>
//           tof1=<mm|-1> tof2=<mm|-1>
//           ax=<i16> ay=<i16> az=<i16>
//           ml=<env> mr=<env>
//
//  Field notes:
//    e1..e7  — signed click count since the last frame. CW = positive
//              (direction flipped from old firmware; physical CW now reads CW).
//    b/eb/jb — current debounced level (1 = pressed).
//    jx/jy/exp/fsr* — 12-bit ADC smoothed values; calibration + 7-bit MIDI
//                     scaling is done on the Pi-side bridge so re-tuning a
//                     curve never needs a Teensy reflash.
//    tof1/tof2 — raw distance in mm; -1 = timeout / dead sensor.
//    ax/ay/az  — raw int16 accelerometer (MPU9250, ±2g, 16384 LSB/g).
//    ml/mr     — envelope-followed piezo magnitude.
//
//  Frame rate is fixed at 100 Hz (every 10 ms). At ~200 bytes/frame that's
//  ~20 KB/s — within USB CDC bandwidth headroom and trivial Pi-side load.
// ============================================================================

// ── Pins ────────────────────────────────────────────
#define XY_X            A2      // joystick X (pin 16, analog)
#define XY_Y            A3      // joystick Y (pin 17, analog)
#define XY_BUTTON       34
#define EXPRESSION_PIN  A17     // pin 41
#define FSR1_PIN        20      // A6
#define FSR2_PIN        21      // A7
#define MIC_L_PIN       38      // A14
#define MIC_R_PIN       39      // A15
#define MPU_ADDRESS     0x68
#define NEOPIXEL_PIN    7

// ── NeoPixel ring — locked in via the joystick cursor test (last lit pixel
// landed at jx=3380 → index 122 → 123 LEDs) ───
#define NUMPIXELS     123
// RAINBOW_TEST: count-the-ring diagnostic — full hue wheel across NUMPIXELS.
#define RAINBOW_TEST  0
// CURSOR_TEST: a single lit pixel tracks joystick X across the ring; press
// the joystick button where it reaches the physical end to record the count
// (printed over serial as CURSOR_RECORD). Overrides RAINBOW_TEST when both
// are on.
#define CURSOR_TEST   0
// Lava-lamp base: each pixel slowly drifts between light pink and dark
// purple/violet, independently, using the existing per-pixel hue-offset
// smoothing below (pixelOffsets — retargeted every OFFSET_UPDATE_INTERVAL,
// eased at ALPHA per 100 Hz frame) so the blend crawls instead of snapping.
#define LAVA_PINK_R   255
#define LAVA_PINK_G   170
#define LAVA_PINK_B   210
#define LAVA_VIOLET_R  60
#define LAVA_VIOLET_G  10
#define LAVA_VIOLET_B  90
// Arcade-hit white burst: a pure-white pulse launches from the top pixel and
// races outward BOTH ways over the blue, a solid white band with a fading trail.
#define BURST_TOP_PIXEL 1        // ring origin/top (pixel 1 is the reference dot)
#define BURST_SPEED     1.1f     // wavefront travel, pixels per 100 Hz frame
#define BURST_CORE      3.0f     // solid pure-white band at the front, pixels
#define BURST_TAIL      6.0f     // trailing fade length behind the core, pixels
#define MAX_BURSTS      8        // concurrent bursts; new hits never clear old ones

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ── Sensors ─────────────────────────────────────────
VL53L1X tof1, tof2;
bool tof1ok = false, tof2ok = false;
bool mpuok = false;

// LiDAR 1 → Wire (pins 18/19, shared with MPU). LiDAR 2 → Wire2 (pins 24/25).
// Separate buses mean both VL53L1X can keep the 0x29 default — no XSHUT pin.

// ── Encoders ────────────────────────────────────────
Encoder enc1(2, 3);
Encoder enc2(4, 5);
Encoder enc3(6, 9);
Encoder enc4(10, 11);
Encoder enc5(12, 8);   // DT off pin 13 (that's the onboard LED, no pull-up)
Encoder enc6(14, 15);
Encoder enc7(22, 23);

// Encoder buttons (pin 1 is ENC4_BTN; pads 49 & 51 = ENC5/ENC7 BTN).
const int encoderButtonPins[7] = {35, 36, 37, 1, 49, 40, 51};
// BTN1 moved off pin 25 (now Wire2 SDA) to pin 0.
const int buttonPins[9] = {0, 26, 27, 28, 29, 30, 31, 32, 33};

// b7 (index 6) got rewired during a field repair and now reads the opposite
// polarity, plus noticeably more contact jitter at the transition edge than
// the other eight — a longer debounce window on just that one channel
// swallows the inhumanly-short bounce glitches without adding any felt lag
// (25 ms is well under human press/release timing) while the rest stay at 5.
const int BUTTON_DEBOUNCE_MS[9] = {5, 5, 5, 5, 5, 5, 25, 5, 5};
const bool BUTTON_ACTIVE_HIGH[9] = {false, false, false, false, false, false, true, false, false};

Bounce buttons[9] = {
  Bounce(buttonPins[0], BUTTON_DEBOUNCE_MS[0]), Bounce(buttonPins[1], BUTTON_DEBOUNCE_MS[1]), Bounce(buttonPins[2], BUTTON_DEBOUNCE_MS[2]),
  Bounce(buttonPins[3], BUTTON_DEBOUNCE_MS[3]), Bounce(buttonPins[4], BUTTON_DEBOUNCE_MS[4]), Bounce(buttonPins[5], BUTTON_DEBOUNCE_MS[5]),
  Bounce(buttonPins[6], BUTTON_DEBOUNCE_MS[6]), Bounce(buttonPins[7], BUTTON_DEBOUNCE_MS[7]), Bounce(buttonPins[8], BUTTON_DEBOUNCE_MS[8])
};

// True if button i is currently pressed, correcting for b7's reversed wiring.
bool buttonPressed(int i) {
  bool lvl = buttons[i].read() == LOW;
  return BUTTON_ACTIVE_HIGH[i] ? !lvl : lvl;
}
Bounce encoderButtons[7] = {
  Bounce(encoderButtonPins[0], 5), Bounce(encoderButtonPins[1], 5),
  Bounce(encoderButtonPins[2], 5), Bounce(encoderButtonPins[3], 5),
  Bounce(encoderButtonPins[4], 5), Bounce(encoderButtonPins[5], 5),
  Bounce(encoderButtonPins[6], 5)
};
Bounce joystickButton(XY_BUTTON, 5);

// ── State ───────────────────────────────────────────
static long lastPos[7]  = {0};   // last position (in 4×detents) for delta calc
static int  encDelta[7] = {0};   // accumulated clicks since last frame emit

// Smoothed analog state (12-bit raw)
float smoothX = 0, smoothY = 0;
float smoothExp = 0;
float smoothFsr1 = 0, smoothFsr2 = 0;
float smoothAccelX = 0, smoothAccelY = 0, smoothAccelZ = 0;
float envMicL = 0, envMicR = 0;

const float ALPHA          = 0.1;   // joystick / FSR / expression / TOF / mics
const float ALPHA_ACCEL    = 0.3;   // accel — lighter so motion stays responsive
const float ALPHA_MIC_ATK  = 0.5;   // fast attack on transients
const float ALPHA_MIC_REL  = 0.05;  // slow release for natural decay

int lidar1mm = -1, lidar2mm = -1;   // -1 = timeout / sensor dead

// Pedal-active state — drives NeoPixel brightness when the heel is down for a
// while. In the old 10-bit firmware "very near heel" was scaledMIDI ≤ 3. In
// 12-bit, calibrated range 128..3992, the equivalent point is raw ≤ 220.
bool pedalActive = false;
unsigned long pedalActiveStart = 0;
const int PEDAL_ACTIVE_THRESHOLD = 220;
const unsigned long PEDAL_ACTIVE_DURATION = 60000;   // 60 s

// Arcade button (b1) hold → NeoPixel blackout toggle. Hold the arcade button
// for ARCADE_HOLD_MS (1 s) with the joystick button NOT pressed and the ring flips
// off (hold again to flip back on), so it's recoverable without a reflash.
// Touching the joystick button at any point during the hold voids it, keeping
// the arcade hold and the joystick button registered as separate gestures.
bool neoPixelOff = false;
bool  burstActive[MAX_BURSTS] = {false};  // each slot = one white burst in flight
float burstRadius[MAX_BURSTS] = {0.0f};   // wavefront distance from top, per burst
unsigned long b1HoldStart = 0;     // when b1 went down (0 = not currently held)
bool b1HoldFired  = false;         // already toggled for the current hold
bool b1HoldVoided = false;         // joystick button touched during hold → ignore
const unsigned long ARCADE_HOLD_MS = 1000;   // hold-to-toggle duration

// Timing
unsigned long lastTof              = 0;
unsigned long lastMPU              = 0;
unsigned long lastNeoPixelUpdate   = 0;
unsigned long lastFrameEmit        = 0;
unsigned long lastOffsetUpdate     = 0;
const unsigned long FRAME_INTERVAL_MS    = 10;     // 100 Hz serial output
const unsigned long OFFSET_UPDATE_INTERVAL = 1000; // 1 s

// NeoPixel hue jitter (smoothed random per-pixel offset)
uint16_t baseHue = 0;
float pixelOffsets[NUMPIXELS]  = {0};
float targetOffsets[NUMPIXELS] = {0};

// ── MIDI mirror (channel 1) ─────────────────────────
//  Dual output: the firmware still emits the full raw serial frame for the
//  Pi-side serial_bridge.py, AND emits the same CCs directly over USB-MIDI so
//  csound / Max / anything on the MIDI bus works even with the bridge stopped.
//  The scaling below mirrors serial_bridge.py EXACTLY (integer math identical),
//  so both paths produce the same 7-bit values. Build with usb=serialmidi.
//
//  Tradeoff: the analog calibration constants (FSR band, etc.) are baked into
//  the MIDI path here, so re-tuning the MIDI feel needs a reflash. The serial
//  path stays the no-reflash tuning route (edit serial_bridge.py).  CC table:
//    enc1-7 → CC20-26   btn1-9 → CC60-68   encBtn1-7 → CC70-76   joyBtn → CC77
//    jx27 jy28 exp29 fsr1=34 fsr2=35 tof1=30 tof2=36 ax31 ay32 az33 ml78 mr79
const uint8_t MIDI_CHANNEL = 1;            // ch1 (matches bridge CHANNEL=0)
const uint8_t ENC_CC[7]    = {20, 21, 22, 23, 24, 25, 26};
const uint8_t BTN_CC[9]    = {60, 61, 62, 63, 64, 65, 66, 67, 68};
const uint8_t ENCBTN_CC[7] = {70, 71, 72, 73, 74, 75, 76};
const uint8_t JOY_BTN_CC   = 77;
const uint8_t CC_JX  = 27, CC_JY  = 28, CC_EXP = 29;
const uint8_t CC_FSR1= 34, CC_FSR2= 35;
const uint8_t CC_TOF1= 30, CC_TOF2= 36;
const uint8_t CC_AX  = 31, CC_AY  = 32, CC_AZ  = 33;
const uint8_t CC_ML  = 78, CC_MR  = 79;

// FSR usable-band calibration — mirrors serial_bridge.py (FSR_MIN/MAX_RAW).
const int FSR_MIN_RAW = 2500;   // contact floor → MIDI 0
const int FSR_MAX_RAW = 4000;   // firm press    → MIDI 127

// Change-only emission state (mirrors the bridge's sparse-CC discipline).
int prevBtn[9]    = {0};
int prevEncBtn[7] = {0};
int prevJoyBtn    = 0;
// analog idx: 0 jx 1 jy 2 exp 3 fsr1 4 fsr2 5 tof1 6 tof2 7 ax 8 ay 9 az 10 ml 11 mr
int prevScaled[12];

static inline int clip127(int v) { return v < 0 ? 0 : (v > 127 ? 127 : v); }

// Scalers — byte-for-byte equivalents of serial_bridge.py. Return -1 = skip.
int joyScale(int raw)   { return clip127((int)((long)raw * 127 / 4095)); }
int expScale(int raw)   { return raw < 128 ? 0
                                 : clip127((int)((long)(raw - 128) * 127 / (3992 - 128))); }
int fsrScale(int raw)   { if (raw <= FSR_MIN_RAW) return 0;
                          float n = (float)(raw - FSR_MIN_RAW) / (FSR_MAX_RAW - FSR_MIN_RAW);
                          if (n < 0) n = 0; if (n > 1) n = 1;
                          return clip127((int)(127.0f * n + 0.5f)); }
int tofScale(int mm)    { if (mm < 0) return -1; int cm = mm / 10;
                          if (cm < 2 || cm > 46) return -1;
                          return clip127((int)((long)(cm - 2) * 127 / (46 - 2))); }
int accelScale(int raw) { int v = raw; if (v < -17000) v = -17000; if (v > 17000) v = 17000;
                          return clip127((int)((long)(v + 17000) * 127 / 34000)); }
int micScale(int env)   { return clip127((int)((long)env * 127 / 800)); }

// Emit one analog CC only when its scaled value changed; -1 = skip (no update).
void emitAnalog(int idx, uint8_t cc, int scaled) {
  if (scaled < 0) return;
  if (prevScaled[idx] != scaled) {
    usbMIDI.sendControlChange(cc, scaled, MIDI_CHANNEL);
    prevScaled[idx] = scaled;
  }
}

// Mirror the current frame's state onto USB-MIDI. MUST run before emitFrame()
// since emitFrame() clears encDelta[].
void emitMidi() {
  // Encoders — one CC per click; positive delta = CW = 127 (matches bridge).
  for (int i = 0; i < 7; i++) {
    int d = encDelta[i];
    if (!d) continue;
    uint8_t val = d > 0 ? 127 : 0;
    int n = d > 0 ? d : -d;
    for (int k = 0; k < n; k++) usbMIDI.sendControlChange(ENC_CC[i], val, MIDI_CHANNEL);
  }
  // Buttons — emit on press/release edge only.
  for (int i = 0; i < 9; i++) {
    int cur = buttonPressed(i) ? 1 : 0;
    if (cur != prevBtn[i]) {
      usbMIDI.sendControlChange(BTN_CC[i], cur ? 127 : 0, MIDI_CHANNEL);
      prevBtn[i] = cur;
    }
  }
  for (int i = 0; i < 7; i++) {
    int cur = encoderButtons[i].read() == LOW ? 1 : 0;
    if (cur != prevEncBtn[i]) {
      usbMIDI.sendControlChange(ENCBTN_CC[i], cur ? 127 : 0, MIDI_CHANNEL);
      prevEncBtn[i] = cur;
    }
  }
  int jb = joystickButton.read() == LOW ? 1 : 0;
  if (jb != prevJoyBtn) {
    usbMIDI.sendControlChange(JOY_BTN_CC, jb ? 127 : 0, MIDI_CHANNEL);
    prevJoyBtn = jb;
  }
  // Analogs — scaled, change-only.
  emitAnalog(0,  CC_JX,   joyScale((int)smoothX));
  emitAnalog(1,  CC_JY,   joyScale((int)smoothY));
  emitAnalog(2,  CC_EXP,  expScale((int)smoothExp));
  emitAnalog(3,  CC_FSR1, fsrScale((int)smoothFsr1));
  emitAnalog(4,  CC_FSR2, fsrScale((int)smoothFsr2));
  emitAnalog(5,  CC_TOF1, tofScale(lidar1mm));
  emitAnalog(6,  CC_TOF2, tofScale(lidar2mm));
  emitAnalog(7,  CC_AX,   accelScale((int)smoothAccelX));
  emitAnalog(8,  CC_AY,   accelScale((int)smoothAccelY));
  emitAnalog(9,  CC_AZ,   accelScale((int)smoothAccelZ));
  emitAnalog(10, CC_ML,   micScale((int)envMicL));
  emitAnalog(11, CC_MR,   micScale((int)envMicR));

  usbMIDI.send_now();   // flush the queued CCs this frame
}

// ── NeoPixel update ─────────────────────────────────
void updateNeoPixels(int exprValue, bool anyEncoderButtonPressed) {
#if CURSOR_TEST
  pixels.clear();
  {
    int idx = (int)((long)smoothX * (NUMPIXELS - 1) / 4095);
    if (idx < 0) idx = 0;
    if (idx > NUMPIXELS - 1) idx = NUMPIXELS - 1;
    pixels.setPixelColor(idx, pixels.Color(255, 255, 255));
  }
  pixels.show();
  return;
#endif
  if (neoPixelOff) {
    pixels.clear();
    pixels.show();
    return;
  }
  if (anyEncoderButtonPressed) {
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(i == 1 ? 153 : 180,
                                           i == 1 ? 153 : 180,
                                           i == 1 ? 153 : 180));
    }
  } else {
    int brightness = 255;
    if (pedalActive) {
      // 12-bit calibrated range 128..3992 → brightness 8..255
      brightness = constrain(map(exprValue, 128, 3992, 8, 255), 8, 255);
    }
    // Advance every in-flight burst one step (this runs at 100 Hz). Each ends
    // once its core + trail have swept past the pixel opposite the top. Bursts
    // are independent, so several can be travelling at once.
    for (int s = 0; s < MAX_BURSTS; s++) {
      if (!burstActive[s]) continue;
      burstRadius[s] += BURST_SPEED;
      if (burstRadius[s] > (NUMPIXELS / 2) + BURST_CORE + BURST_TAIL)
        burstActive[s] = false;
    }
    // Icy-blue base, scaled by pedal brightness; the white bursts are blended on
    // top at full white (brightest burst wins) so they pop regardless of pedal.
    for (int i = 0; i < NUMPIXELS; i++) {
      uint8_t r, g, b;
#if RAINBOW_TEST
      uint16_t hue = (uint32_t)i * 65535UL / NUMPIXELS;   // one full wheel, pixel 0 → pixel NUMPIXELS-1
      uint32_t c = pixels.gamma32(pixels.ColorHSV(hue, 255, brightness));
      r = (uint8_t)(c >> 16);
      g = (uint8_t)(c >> 8);
      b = (uint8_t)c;
#else
      // pixelOffsets[i] drifts in [-2000, 2000]; map to a 0..1 blend crawl.
      // Cubed so violet is the resting base most of the time and pink only
      // flares in as a highlight when a pixel's drift swings near its peak.
      float t = (pixelOffsets[i] + 2000.0f) / 4000.0f;
      if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
      float tb = t * t * t;
      r = (uint8_t)((((float)LAVA_VIOLET_R + (LAVA_PINK_R - LAVA_VIOLET_R) * tb) * brightness) / 255.0f);
      g = (uint8_t)((((float)LAVA_VIOLET_G + (LAVA_PINK_G - LAVA_VIOLET_G) * tb) * brightness) / 255.0f);
      b = (uint8_t)((((float)LAVA_VIOLET_B + (LAVA_PINK_B - LAVA_VIOLET_B) * tb) * brightness) / 255.0f);
#endif
      // Circular distance from the top — same metric both ways, so each pulse
      // expands symmetrically clockwise and counter-clockwise at once.
      int dRaw = abs(i - BURST_TOP_PIXEL);
      int d = min(dRaw, NUMPIXELS - dRaw);
      float w = 0.0f;                                  // brightest burst here
      for (int s = 0; s < MAX_BURSTS; s++) {
        if (!burstActive[s]) continue;
        float delta = burstRadius[s] - (float)d;       // ≥0 once the front passes i
        if (delta < 0.0f) continue;
        float ws = (delta <= BURST_CORE)               // solid white core...
                     ? 1.0f
                     : expf(-(delta - BURST_CORE) / BURST_TAIL);  // ...then fade
        if (ws > w) w = ws;
      }
      if (w > 0.0f) {
        r = (uint8_t)(r + (255 - r) * w);
        g = (uint8_t)(g + (255 - g) * w);
        b = (uint8_t)(b + (255 - b) * w);
      }
      pixels.setPixelColor(i, pixels.Color(r, g, b));
    }
  }
  pixels.show();
}

// ── VL53L1X bring-up with retry ─────────────────────
bool configureTOF(VL53L1X &sensor, TwoWire &bus) {
  sensor.setBus(&bus);
  sensor.setTimeout(50);
  for (uint8_t attempt = 1; attempt <= 3; attempt++) {
    if (sensor.init()) {
      sensor.setDistanceMode(VL53L1X::Long);
      sensor.setMeasurementTimingBudget(33000);
      sensor.startContinuous(50);
      return true;
    }
    delay(50);
  }
  return false;
}

// ── Frame emit ──────────────────────────────────────
void emitFrame() {
  Serial.print("NEPTR2");
  Serial.print(" t="); Serial.print(millis());

  for (int i = 0; i < 7; i++) {
    Serial.print(" e"); Serial.print(i + 1);
    Serial.print("=");  Serial.print(encDelta[i]);
    encDelta[i] = 0;            // per-frame accumulator
  }

  for (int i = 0; i < 9; i++) {
    Serial.print(" b"); Serial.print(i + 1);
    Serial.print("=");  Serial.print(buttonPressed(i) ? 1 : 0);
  }
  for (int i = 0; i < 7; i++) {
    Serial.print(" eb"); Serial.print(i + 1);
    Serial.print("=");   Serial.print(encoderButtons[i].read() == LOW ? 1 : 0);
  }
  Serial.print(" jb="); Serial.print(joystickButton.read() == LOW ? 1 : 0);

  Serial.print(" jx=");   Serial.print((int)smoothX);
  Serial.print(" jy=");   Serial.print((int)smoothY);
  Serial.print(" exp=");  Serial.print((int)smoothExp);
  Serial.print(" fsr1="); Serial.print((int)smoothFsr1);
  Serial.print(" fsr2="); Serial.print((int)smoothFsr2);

  Serial.print(" tof1="); Serial.print(lidar1mm);
  Serial.print(" tof2="); Serial.print(lidar2mm);

  Serial.print(" ax="); Serial.print((int)smoothAccelX);
  Serial.print(" ay="); Serial.print((int)smoothAccelY);
  Serial.print(" az="); Serial.print((int)smoothAccelZ);

  Serial.print(" ml="); Serial.print((int)envMicL);
  Serial.print(" mr="); Serial.print((int)envMicR);

  Serial.println();
}

// ── Setup ───────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // 12-bit ADC for highest-fidelity continuous inputs (FSR, expression, joy,
  // mics). Calibration ranges scale ×4 from the 10-bit defaults; per-channel
  // scaling and 7-bit MIDI conversion are done by the Pi-side bridge, so
  // re-tuning never needs a Teensy reflash.
  analogReadResolution(12);

  pixels.begin();
  pixels.setBrightness(80);
  pixels.clear();
  pixels.show();

  for (int i = 0; i < 9; i++) pinMode(buttonPins[i], INPUT_PULLUP);
  for (int i = 0; i < 7; i++) pinMode(encoderButtonPins[i], INPUT_PULLUP);
  pinMode(XY_BUTTON, INPUT_PULLUP);

  pinMode(2, INPUT_PULLUP);  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP); pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP); pinMode(8, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP); pinMode(15, INPUT_PULLUP);
  pinMode(22, INPUT_PULLUP); pinMode(23, INPUT_PULLUP);

  pinMode(XY_X, INPUT);
  pinMode(XY_Y, INPUT);
  pinMode(EXPRESSION_PIN, INPUT);
  pinMode(FSR1_PIN, INPUT);
  pinMode(FSR2_PIN, INPUT);
  pinMode(MIC_L_PIN, INPUT);
  pinMode(MIC_R_PIN, INPUT);

  Wire.begin();  Wire.setClock(400000);
  Wire2.begin(); Wire2.setClock(400000);
  delay(100);

  // MPU9250 wake (register-compatible with MPU6050)
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x6B);
  Wire.write(0x00);
  mpuok = (Wire.endTransmission(true) == 0);

  tof1ok = configureTOF(tof1, Wire);
  tof2ok = configureTOF(tof2, Wire2);

  // First MIDI frame should emit every analog once: prime to an out-of-range
  // sentinel so the initial scaled value always differs.
  for (int i = 0; i < 12; i++) prevScaled[i] = -1;

  randomSeed(analogRead(0));
  for (int i = 0; i < NUMPIXELS; i++) {
    targetOffsets[i] = random(-2000, 2000);
    pixelOffsets[i]  = targetOffsets[i];
  }
}

// ── Loop ────────────────────────────────────────────
void loop() {
  unsigned long timestamp = millis();
  bool anyEncoderButtonPressed = false;

  // Advance debouncers; current state is read inside emitFrame()
  for (int i = 0; i < 9; i++) buttons[i].update();

  for (int i = 0; i < 7; i++) {
    encoderButtons[i].update();
    if (encoderButtons[i].read() == LOW) anyEncoderButtonPressed = true;
  }
  joystickButton.update();
  bool jbPressed = (joystickButton.read() == LOW);

  // Arcade button (b1): hold for ARCADE_HOLD_MS (INPUT_PULLUP → LOW = pressed)
  // with the joystick button NOT pressed to toggle the NeoPixel blackout. If
  // the joystick button is touched at any point during the hold, the hold is
  // voided so the two gestures stay registered as separate.
  if (buttons[0].fallingEdge()) {
    b1HoldStart  = timestamp;
    b1HoldFired  = false;
    b1HoldVoided = jbPressed;   // joystick already down → not a clean arcade hold
    // Arcade hit → launch a new white burst from the top WITHOUT clearing those
    // already travelling. Prefer a free slot; if all are busy, recycle the one
    // that's gone furthest (closest to finishing) so the freshest survive.
    int slot = 0;
    float furthest = -1.0f;
    for (int s = 0; s < MAX_BURSTS; s++) {
      if (!burstActive[s]) { slot = s; furthest = -1.0f; break; }
      if (burstRadius[s] > furthest) { furthest = burstRadius[s]; slot = s; }
    }
    burstActive[slot] = true;
    burstRadius[slot] = 0.0f;
  }
  if (buttons[0].read() == LOW) {            // still held
    if (jbPressed) b1HoldVoided = true;      // joystick touched → cancel this hold
    if (!b1HoldFired && !b1HoldVoided &&
        timestamp - b1HoldStart >= ARCADE_HOLD_MS) {
      neoPixelOff = !neoPixelOff;
      b1HoldFired = true;
    }
  } else {                                   // released → reset for next hold
    b1HoldStart  = 0;
    b1HoldVoided = false;
  }

  // Encoders. Direction flipped from old firmware (lastPos - pos instead of
  // pos - lastPos) so physical CW reads as a positive delta.
  Encoder encoders[7] = {enc1, enc2, enc3, enc4, enc5, enc6, enc7};
  for (int i = 0; i < 7; i++) {
    long pos = encoders[i].read() / 4;
    if (pos != lastPos[i]) {
      encDelta[i] += (int)(lastPos[i] - pos);
      lastPos[i]  = pos;
    }
  }

  // Joystick — 12-bit raw, smoothed
  int xVal = analogRead(XY_X);
  int yVal = analogRead(XY_Y);
  smoothX = ALPHA * xVal + (1 - ALPHA) * smoothX;
  smoothY = ALPHA * yVal + (1 - ALPHA) * smoothY;

#if CURSOR_TEST
  // LED-count test: joystick X scans a single lit pixel across the ring.
  // Press the joystick button where it visually reaches the physical end —
  // this prints the count to record.
  if (joystickButton.fallingEdge()) {
    int idx = (int)((long)smoothX * (NUMPIXELS - 1) / 4095);
    if (idx < 0) idx = 0;
    if (idx > NUMPIXELS - 1) idx = NUMPIXELS - 1;
    Serial.print("CURSOR_RECORD idx="); Serial.print(idx);
    Serial.print(" count="); Serial.println(idx + 1);
  }
#endif

  // Expression pedal — 12-bit raw, smoothed. Pedal-active engages at heel.
  int expVal = analogRead(EXPRESSION_PIN);
  smoothExp = ALPHA * expVal + (1 - ALPHA) * smoothExp;
  if (!pedalActive && (int)smoothExp <= PEDAL_ACTIVE_THRESHOLD) {
    pedalActive = true;
    pedalActiveStart = timestamp;
  }
  if (pedalActive && (timestamp - pedalActiveStart > PEDAL_ACTIVE_DURATION)) {
    pedalActive = false;
  }

  // FSRs — 12-bit raw smoothed. Bridge linearizes (conductance) → 0..127.
  smoothFsr1 = ALPHA * analogRead(FSR1_PIN) + (1 - ALPHA) * smoothFsr1;
  smoothFsr2 = ALPHA * analogRead(FSR2_PIN) + (1 - ALPHA) * smoothFsr2;

  // Contact mics — piezo at mid-rail (~2048 in 12-bit); |reading - 2048| is
  // the AC magnitude; asymmetric attack/release envelope follower.
  int micLVal = abs(analogRead(MIC_L_PIN) - 2048);
  int micRVal = abs(analogRead(MIC_R_PIN) - 2048);
  envMicL = (micLVal > envMicL)
      ? ALPHA_MIC_ATK * micLVal + (1 - ALPHA_MIC_ATK) * envMicL
      : ALPHA_MIC_REL * micLVal + (1 - ALPHA_MIC_REL) * envMicL;
  envMicR = (micRVal > envMicR)
      ? ALPHA_MIC_ATK * micRVal + (1 - ALPHA_MIC_ATK) * envMicR
      : ALPHA_MIC_REL * micRVal + (1 - ALPHA_MIC_REL) * envMicR;

  // LiDAR @ 20 Hz. Guards on init flags so a dead sensor doesn't block on
  // timeout every loop and stall everything else.
  if (timestamp - lastTof > 50) {
    if (tof1ok) {
      uint16_t mm = tof1.readRangeContinuousMillimeters();
      lidar1mm = tof1.timeoutOccurred() ? -1 : (int)mm;
    }
    if (tof2ok) {
      uint16_t mm = tof2.readRangeContinuousMillimeters();
      lidar2mm = tof2.timeoutOccurred() ? -1 : (int)mm;
    }
    lastTof = timestamp;
  }

  // MPU accel @ 50 Hz
  if (mpuok && timestamp - lastMPU > 20) {
    Wire.beginTransmission(MPU_ADDRESS);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDRESS, (uint8_t)6, true);
    // Read each byte into its own variable first. Combining two Wire.read()
    // calls in one expression — (Wire.read()<<8)|Wire.read() — is undefined:
    // the call order is unspecified, so the compiler may swap high/low bytes
    // and the accel reads as scrambled garbage. Sequence points fix it.
    uint8_t xh = Wire.read(), xl = Wire.read();
    uint8_t yh = Wire.read(), yl = Wire.read();
    uint8_t zh = Wire.read(), zl = Wire.read();
    int16_t accelX = (int16_t)((xh << 8) | xl);
    int16_t accelY = (int16_t)((yh << 8) | yl);
    int16_t accelZ = (int16_t)((zh << 8) | zl);
    smoothAccelX = ALPHA_ACCEL * accelX + (1 - ALPHA_ACCEL) * smoothAccelX;
    smoothAccelY = ALPHA_ACCEL * accelY + (1 - ALPHA_ACCEL) * smoothAccelY;
    smoothAccelZ = ALPHA_ACCEL * accelZ + (1 - ALPHA_ACCEL) * smoothAccelZ;
    lastMPU = timestamp;
  }

  // NeoPixel hue-offset retarget once per second
  if (timestamp - lastOffsetUpdate > OFFSET_UPDATE_INTERVAL) {
    for (int i = 0; i < NUMPIXELS; i++) targetOffsets[i] = random(-2000, 2000);
    lastOffsetUpdate = timestamp;
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixelOffsets[i] = ALPHA * targetOffsets[i] + (1 - ALPHA) * pixelOffsets[i];
  }

  // NeoPixel update @ 100 Hz — drives the neon-blue base and advances any
  // in-flight white arcade burst one step per frame.
  if (timestamp - lastNeoPixelUpdate > 10) {
    updateNeoPixels((int)smoothExp, anyEncoderButtonPressed);
    lastNeoPixelUpdate = timestamp;
  }

  // Serial + MIDI @ 100 Hz. emitMidi() first — it reads encDelta[], which
  // emitFrame() then prints and clears.
  if (timestamp - lastFrameEmit >= FRAME_INTERVAL_MS) {
    emitMidi();
    emitFrame();
    lastFrameEmit = timestamp;
  }

  // Drain any inbound USB-MIDI so the receive buffer never backs up.
  while (usbMIDI.read()) { }
}
