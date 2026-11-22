// ============================================================================
//  DUODECIMUS  --  Teensy 4.1 MIDI controller  (twelve, in Latin)
//  12 encoders . 12 NeoPixels . 1.5" SH1107 OLED . 2 TRS pedal jacks
//
//  SIMPLE BUILD: no pages, no banks. Twelve knobs, straight through,
//  everything on MIDI channel 7.
//
//  Arduino IDE:  Board = Teensy 4.1 | USB Type = MIDI | CPU = 600 MHz
//  name.c renames the USB device itself to DUODECIMUS.
//  Libraries:    Adafruit_NeoPixel, Adafruit_GFX, Adafruit_SH110X
//
//  41 of 42 top-side pins, no bottom pads, pin 41 spare.
// ============================================================================

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---------------------------------------------------------------- pin map --
#define PIN_NEOPIXEL   38
#define PIN_SDA        25          // Wire2
#define PIN_SCL        24
#define PIN_JACKA_TIP  39          // A15
#define PIN_JACKB_TIP  40          // A16
// Jack rings go to 3.3V through 270R, NOT to the Teensy. See the pedal section.

#define NUM_ENC        12
#define NUM_PIXELS     12

// Physical order, captured by the alignment sketch in v2/align/.
// Row N is the knob you want to be encoder N+1. { CLK, DT, SW }
static const uint8_t ENC_PINS[NUM_ENC][3] = {
  { 32, 33, 34 },   // encoder 1
  { 29, 30, 31 },   // encoder 2
  { 35, 36, 37 },   // encoder 3
  {  2,  3,  4 },   // encoder 4
  {  5,  6,  7 },   // encoder 5
  { 11, 12, 13 },   // encoder 6
  { 17, 18, 19 },   // encoder 7
  {  8,  9, 10 },   // encoder 8
  { 14, 15, 16 },   // encoder 9
  { 26, 27, 28 },   // encoder 10
  { 20, 21, 22 },   // encoder 11
  { 23,  0,  1 }    // encoder 12
};

// Which NeoPixel physically sits beside each encoder. Also from the alignment
// sketch: the chain was wired in whatever order reached, this undoes that.
static const uint8_t LED_FOR_ENC[NUM_ENC] =
  { 11, 10, 0, 4, 9, 1, 8, 2, 6, 7, 5, 3 };

// ------------------------------------------------------------- midi map --
// Everything on one channel. All CC numbers are in undefined MIDI space so
// nothing here collides with a standard controller a host might auto-map.
#define MIDI_CH         7
#define CC_ROT_BASE    20          // encoder rotation -> CC  20..31
#define CC_BTN_BASE   102          // encoder button   -> CC 102..113
#define CC_EXPR_A      14          // jack A as an expression pedal
#define CC_EXPR_B      15          // jack B as an expression pedal
#define CC_SW_A       114          // jack A as a switch
#define CC_SW_B       115          // jack B as a switch
#define CC_HOLD_CAPTURE 116        // one-shot at 5s held
#define CC_HOLD_DELETE  117        // one-shot at 8s held
#define HOLD_CAPTURE_MS 5000
#define HOLD_DELETE_MS  8000
// The hold one-shots carry the ENCODER NUMBER, 1..12, as their value, which is
// how NEPTR identifies which knob was held. Set this to 0 to send a flat 127
// instead; the request specified both in different sentences.
#define HOLD_VALUE_IS_ENCODER 1

// ------------------------------------------------------------- oled / i2c --
// GME128128-01 -> SH1107 monochrome. The -02 is the SSD1327; they are not
// interchangeable and the wrong driver puts garbage on the glass.
#define OLED_W          128
#define OLED_H          128
#define I2C_HZ         400000UL
#define OLED_MIN_MS     70

Adafruit_SH1107   oled(OLED_W, OLED_H, &Wire2, -1, I2C_HZ, 100000UL);
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
bool oledOK = false;

// ------------------------------------------------------------- eeprom map --
#define EE_MAGIC        0x0F007503
#define EE_ADDR_MAGIC      0
#define EE_ADDR_PEDALS     4       // one byte per jack
#define EE_ADDR_FLAGS      6       // accumulator, invert
#define EE_ADDR_VALUES     8       // 12 bytes
#define EE_ADDR_TOGGLES   20       // 12 bytes
#define EE_ADDR_CAL     1000       // 4 bytes per jack

// ============================================================================
//  ENCODERS
// ============================================================================
// Full quadrature state machine. Four transitions per detent on an EC11, so we
// accumulate and emit at +/-4. Does not miss or double-count.
static const int8_t QDEC[16] = {
   0, -1,  1,  0,   1,  0,  0, -1,  -1,  0,  0,  1,   0,  1, -1,  0
};

struct Encoder {
  uint8_t  pinA, pinB, pinSW;
  uint8_t  quadState;
  int8_t   quadAccum;
  bool     swLevel, swRaw;
  uint32_t swChangedAt;
};
Encoder enc[NUM_ENC];
#define SW_DEBOUNCE_MS 8

// ============================================================================
//  PEDAL JACKS  --  type chosen by hand, no auto-detection
// ============================================================================
// ONE Teensy pin per jack, on the TIP.
//     SLEEVE -> GND
//     RING   -> 3.3V THROUGH A 270R RESISTOR
//     TIP    -> the analog pin above
//
// The resistor is not optional. An expression pedal needs the rail across its
// pot, but a sustain-style plug is MONO and its barrel shorts ring to sleeve.
// Wired straight to the rail that is a dead short; through 270R it is 12 mA.
//
// Both types are sensed the SAME way: weak pull-down, read as analog. The
// Hotone Ampero II Press wires its footswitch TIP-to-RING, so closing it pulls
// the tip UP. A pull-up would read high open and closed and never see it.
//
//     SHIFT + press encoder 6  ->  toggle jack A type
//     SHIFT + press encoder 7  ->  toggle jack B type

enum PedalKind : uint8_t { PEDAL_EXPR = 0, PEDAL_SUSTAIN = 1 };

#define ADC_BITS        12
#define ADC_MAX       4095
#define EXPR_ALPHA    0.15f
#define EXPR_MIN_DELTA    2
#define EXPR_MIN_MS      12
#define EXPR_INVERT       1        // Ampero II Press sweeps backwards
#define SW_THRESH      2048
#define SW_REBASE_MS   2000

struct Jack {
  uint8_t    pinTip, slot;
  PedalKind  kind;
  float      smooth;
  int        lastSent;
  uint32_t   lastSendAt;
  int        calLo, calHi;
  bool       restLevel, sustainOn, lastRaw;
  uint32_t   rawChangedAt;
  bool       calDirty;
  uint32_t   calDirtyAt;
};
Jack jackA = { PIN_JACKA_TIP, 0 };
Jack jackB = { PIN_JACKB_TIP, 1 };

// forward declarations (the Arduino preprocessor cannot make these itself,
// they take references to types declared in this file)
void jackSetKind(Jack &j, PedalKind k);
void jackService(Jack &j, uint32_t now);
void saveJackCal(Jack &j);
void loadJackCal(Jack &j);
void savePedalKinds();
void loadPedalKinds();
void savePreset();
void loadPreset();
void encRotate(uint8_t i, int8_t dir);
void encPress(uint8_t i);
void encRelease(uint8_t i);
void encService(uint32_t now);
void holdService(uint32_t now);
void ledService(uint32_t now);
void oledService(uint32_t now);
void bootShow();
void setStatus(const char *s);
static inline uint8_t ccExprFor(uint8_t s) { return s ? CC_EXPR_B : CC_EXPR_A; }
static inline uint8_t ccSwFor  (uint8_t s) { return s ? CC_SW_B   : CC_SW_A;   }

// ============================================================================
//  STATE
// ============================================================================
uint8_t encValue[NUM_ENC];
bool    encToggle[NUM_ENC];

bool accumulatorMode = false;      // false = relative (127/0), true = absolute
bool invertRange     = false;

uint32_t holdStart[NUM_ENC];       // when this button went down
uint8_t  holdStage[NUM_ENC];       // 0 none, 1 capture fired, 2 delete fired
bool     holdArmed[NUM_ENC];       // false if the press was eaten by SHIFT
bool     btnSent[NUM_ENC];         // a 'press' CC is outstanding for this key

bool     shiftHeld     = false;
bool     shiftConsumed = false;
uint32_t shiftPressedAt = 0;
#define  SHIFT_TAP_MS 350

int8_t   lastTouched = -1;
char     glyphChar   = 0;
uint32_t glyphUntil  = 0;
#define  GLYPH_MS  280

bool     oledDirty   = true;
uint32_t lastOledAt  = 0;
char     statusMsg[18] = "";
uint32_t statusUntil = 0;

// ============================================================================
//  MIDI
// ============================================================================
static inline void sendRot(uint8_t i, uint8_t v) {
  usbMIDI.sendControlChange(CC_ROT_BASE + i, v, MIDI_CH);
}
static inline void sendBtn(uint8_t i, bool on) {
  usbMIDI.sendControlChange(CC_BTN_BASE + i,
                            invertRange ? (on ? 0 : 127) : (on ? 127 : 0), MIDI_CH);
}
void setStatus(const char *s) {
  strncpy(statusMsg, s, sizeof(statusMsg) - 1);
  statusMsg[sizeof(statusMsg) - 1] = 0;
  statusUntil = millis() + 1200;
  oledDirty = true;
}
// Buttons are momentary over MIDI now, so there is no latched button state to
// push to the host. Only rotations are resent.
void resendAll() {
  if (!accumulatorMode) return;
  for (uint8_t i = 0; i < NUM_ENC; i++) sendRot(i, encValue[i]);
}

// ============================================================================
//  COLOUR  --  a lava lamp flowing across the physical panel
// ============================================================================
// PHYSICAL LAYOUT: two rows of six, filled column by column.
//   encoder 1 top-left, 2 directly below it, 3 top of the next column, ...
//   encoder i (0-based):  column = i / 2   row = i % 2  (row 0 is the top)
//
// Brightness and shade drift through that grid as a sum of three sine waves
// at different speeds, so blobs of light rise, merge and separate instead of
// scanning in a line. Everything stays in the red end of the wheel: deepest
// embers at the troughs, full bright red at the peaks.
static inline uint8_t encCol(uint8_t i) { return i >> 1; }        // 0..5
static inline uint8_t encRow(uint8_t i) { return i & 1;  }        // 0 = top

static uint32_t lavaColor(uint8_t i, float tt) {
  float x = (float)encCol(i);
  float y = (float)encRow(i) * 2.6f;         // stretch the short axis so the
                                             // two rows actually differ
  float v = sinf(x * 0.85f + tt)
          + sinf(y + tt * 1.31f)
          + sinf((x * 0.45f + y * 0.60f) + tt * 0.67f);
  float n = (v + 3.0f) * (1.0f / 6.0f);      // 0..1
  // Hue pinned at 0. Any drift up the wheel reads as orange almost immediately,
  // so the whole effect is brightness only, and the ceiling stays well below
  // full so it sits dark.
  uint8_t val = (uint8_t)(25.0f + n * 165.0f);
  return strip.ColorHSV(0, 255, val);
}

// Flat bright red. Used by the boot show, which settles into this before the
// lava takes over.
static inline uint16_t encHue(uint8_t i)   { (void)i; return 0; }
static inline uint8_t  encVal(uint8_t i)   { (void)i; return 255; }
static inline uint32_t encColor(uint8_t i) { (void)i; return strip.ColorHSV(0, 255, 255); }

// ============================================================================
//  EEPROM
// ============================================================================
void savePreset() {
  uint32_t magic = EE_MAGIC;
  EEPROM.put(EE_ADDR_MAGIC, magic);
  EEPROM.update(EE_ADDR_FLAGS,     accumulatorMode ? 1 : 0);
  EEPROM.update(EE_ADDR_FLAGS + 1, invertRange ? 1 : 0);
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    EEPROM.update(EE_ADDR_VALUES  + i, encValue[i]);
    EEPROM.update(EE_ADDR_TOGGLES + i, encToggle[i] ? 1 : 0);
  }
}
void loadPreset() {
  uint32_t magic = 0;
  EEPROM.get(EE_ADDR_MAGIC, magic);
  if (magic != EE_MAGIC) return;
  invertRange = EEPROM.read(EE_ADDR_FLAGS + 1) ? true : false;
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    uint8_t v = EEPROM.read(EE_ADDR_VALUES + i);
    encValue[i]  = (v <= 127) ? v : 64;
    encToggle[i] = EEPROM.read(EE_ADDR_TOGGLES + i) ? true : false;
  }
}
void savePedalKinds() {
  EEPROM.update(EE_ADDR_PEDALS,     (uint8_t)jackA.kind);
  EEPROM.update(EE_ADDR_PEDALS + 1, (uint8_t)jackB.kind);
}
void saveJackCal(Jack &j) {
  int a = EE_ADDR_CAL + j.slot * 4;
  EEPROM.update(a + 0, (uint8_t)(j.calLo & 0xFF));
  EEPROM.update(a + 1, (uint8_t)((j.calLo >> 8) & 0xFF));
  EEPROM.update(a + 2, (uint8_t)(j.calHi & 0xFF));
  EEPROM.update(a + 3, (uint8_t)((j.calHi >> 8) & 0xFF));
}
void loadJackCal(Jack &j) {
  int a = EE_ADDR_CAL + j.slot * 4;
  int lo = EEPROM.read(a + 0) | (EEPROM.read(a + 1) << 8);
  int hi = EEPROM.read(a + 2) | (EEPROM.read(a + 3) << 8);
  if (lo >= 0 && hi <= ADC_MAX && hi - lo >= 400) { j.calLo = lo; j.calHi = hi; }
}

// ============================================================================
//  JACKS
// ============================================================================
void jackSetKind(Jack &j, PedalKind k) {
  j.kind = k;
  j.lastSent = -1;
  pinMode(j.pinTip, INPUT_PULLDOWN);
  delayMicroseconds(200);
  if (k == PEDAL_EXPR) {
    j.smooth = analogRead(j.pinTip);
    j.calLo = ADC_MAX; j.calHi = 0;
  } else {
    j.restLevel = (analogRead(j.pinTip) > SW_THRESH);
    j.sustainOn = false;
    j.lastRaw   = j.restLevel;
    usbMIDI.sendControlChange(ccSwFor(j.slot), 0, MIDI_CH);
  }
  oledDirty = true;
}
void loadPedalKinds() {
  uint8_t a = EEPROM.read(EE_ADDR_PEDALS);
  uint8_t b = EEPROM.read(EE_ADDR_PEDALS + 1);
  // Blank EEPROM reads 0xFF, so the defaults land on A expression, B switch.
  jackSetKind(jackA, (a == PEDAL_SUSTAIN) ? PEDAL_SUSTAIN : PEDAL_EXPR);
  jackSetKind(jackB, (b == PEDAL_EXPR)    ? PEDAL_EXPR    : PEDAL_SUSTAIN);
  loadJackCal(jackA);
  loadJackCal(jackB);
}
void jackService(Jack &j, uint32_t now) {
  if (j.kind == PEDAL_EXPR) {
    int raw = analogRead(j.pinTip);
    j.smooth = EXPR_ALPHA * raw + (1.0f - EXPR_ALPHA) * j.smooth;
    int sm = (int)j.smooth;
    if (sm < j.calLo) { j.calLo = sm; j.calDirty = true; j.calDirtyAt = now; }
    if (sm > j.calHi) { j.calHi = sm; j.calDirty = true; j.calDirtyAt = now; }
    if (j.calDirty && now - j.calDirtyAt > 3000) { saveJackCal(j); j.calDirty = false; }
    int span = j.calHi - j.calLo;
    if (span < 400) return;
    // Pull the ends in 4%, otherwise the pedal bottoms out at 3 or 4 instead
    // of 0 because it never quite returns to the value that set the endpoint.
    int m = span / 25, lo = j.calLo + m, hi = j.calHi - m;
    if (hi <= lo) return;
#if EXPR_INVERT
    int v = constrain(map(sm, lo, hi, 127, 0), 0, 127);
#else
    int v = constrain(map(sm, lo, hi, 0, 127), 0, 127);
#endif
    if (abs(v - j.lastSent) >= EXPR_MIN_DELTA && now - j.lastSendAt >= EXPR_MIN_MS) {
      usbMIDI.sendControlChange(ccExprFor(j.slot), v, MIDI_CH);
      j.lastSent = v; j.lastSendAt = now; oledDirty = true;
    }
  } else {
    bool raw = (analogRead(j.pinTip) > SW_THRESH);
    if (raw != j.lastRaw) { j.lastRaw = raw; j.rawChangedAt = now; }
    if (now - j.rawChangedAt > 5) {
      // Re-learn the resting level after a quiet spell, so a pedal plugged in
      // AFTER boot is not stuck against the empty jack's baseline.
      if (!j.sustainOn && raw != j.restLevel && now - j.rawChangedAt > SW_REBASE_MS)
        j.restLevel = raw;
      bool on = (raw != j.restLevel);       // polarity-agnostic
      if (on != j.sustainOn) {
        j.sustainOn = on;
        usbMIDI.sendControlChange(ccSwFor(j.slot), on ? 127 : 0, MIDI_CH);
        oledDirty = true;
      }
    }
  }
}

// ============================================================================
//  ENCODER HANDLING
// ============================================================================
void encRotate(uint8_t i, int8_t dir) {
  lastTouched = i;
  glyphChar   = (dir > 0) ? '>' : '<';
  glyphUntil  = millis() + GLYPH_MS;
  oledDirty   = true;
  if (shiftHeld) { shiftConsumed = true; return; }
  if (accumulatorMode) {
    encValue[i] = (uint8_t)constrain(encValue[i] + dir, 0, 127);
    sendRot(i, encValue[i]);
  } else {
    sendRot(i, dir > 0 ? 127 : 0);
  }
}
void encPress(uint8_t i) {
  lastTouched = i;
  glyphChar   = 'o';
  glyphUntil  = millis() + GLYPH_MS;
  oledDirty   = true;

  holdStart[i] = millis();
  holdStage[i] = 0;
  holdArmed[i] = false;
  btnSent[i]   = false;

  if (i == 0) {
    shiftHeld = true; shiftConsumed = false; shiftPressedAt = millis();
    holdArmed[0] = true;          // encoder 1 can still be held for capture
    return;
  }

  if (shiftHeld) {
    shiftConsumed = true;
    switch (i) {
      case 1: accumulatorMode = !accumulatorMode;
              setStatus(accumulatorMode ? "ACCUM ON" : "ACCUM OFF");
              if (accumulatorMode) resendAll();
              break;
      case 2: invertRange = !invertRange;
              setStatus(invertRange ? "INVERT ON" : "INVERT OFF");
              resendAll();
              break;
      case 3: savePreset(); setStatus("SAVED"); break;
      case 4: loadPreset(); resendAll(); setStatus("LOADED"); break;
      case 5: jackSetKind(jackA, jackA.kind == PEDAL_EXPR ? PEDAL_SUSTAIN : PEDAL_EXPR);
              savePedalKinds();
              setStatus(jackA.kind == PEDAL_EXPR ? "JACK A EXPR" : "JACK A SW");
              break;
      case 6: jackSetKind(jackB, jackB.kind == PEDAL_EXPR ? PEDAL_SUSTAIN : PEDAL_EXPR);
              savePedalKinds();
              setStatus(jackB.kind == PEDAL_EXPR ? "JACK B EXPR" : "JACK B SW");
              break;
      default: break;
    }
    return;
  }
  // RADIO selection: exactly one knob is lit at a time, matching how presets
  // work. Purely visual and device-local; the host is told momentary
  // press/release instead, so it can time a hold.
  for (uint8_t k = 0; k < NUM_ENC; k++) encToggle[k] = (k == i);
  sendBtn(i, true);
  btnSent[i]   = true;
  holdArmed[i] = true;
}

void encRelease(uint8_t i) {
  if (btnSent[i]) { sendBtn(i, false); btnSent[i] = false; }
  holdArmed[i] = false;
  holdStage[i] = 0;

  if (i == 0) {
    // A short tap on encoder 1 with no command issued is its normal press:
    // toggle the light and emit one momentary pulse.
    if (!shiftConsumed && millis() - shiftPressedAt < SHIFT_TAP_MS) {
      for (uint8_t k = 0; k < NUM_ENC; k++) encToggle[k] = (k == 0);
      sendBtn(0, true);
      sendBtn(0, false);
    }
    shiftHeld = false; shiftConsumed = false;
  }
  oledDirty = true;
}

// Fires the two hold one-shots. A key eaten by the SHIFT layer is not armed,
// so shift chords can never trip capture or delete.
void holdService(uint32_t now) {
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    if (!holdArmed[i] || !enc[i].swLevel) continue;
    uint32_t held = now - holdStart[i];
#if HOLD_VALUE_IS_ENCODER
    uint8_t val = i + 1;
#else
    uint8_t val = 127;
#endif
    if (holdStage[i] < 1 && held >= HOLD_CAPTURE_MS) {
      usbMIDI.sendControlChange(CC_HOLD_CAPTURE, val, MIDI_CH);
      holdStage[i] = 1;
      if (i == 0) shiftConsumed = true;   // a 5s hold is not a shift chord
      setStatus("CAPTURE");
    }
    if (holdStage[i] < 2 && held >= HOLD_DELETE_MS) {
      usbMIDI.sendControlChange(CC_HOLD_DELETE, val, MIDI_CH);
      holdStage[i] = 2;
      setStatus("DELETE");
    }
  }
}
void encService(uint32_t now) {
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    Encoder &e = enc[i];
    uint8_t st = (digitalRead(e.pinA) << 1) | digitalRead(e.pinB);
    if (st != e.quadState) {
      e.quadAccum += QDEC[(e.quadState << 2) | st];
      e.quadState = st;
      if (e.quadAccum >= 4)       { encRotate(i,  1); e.quadAccum = 0; }
      else if (e.quadAccum <= -4) { encRotate(i, -1); e.quadAccum = 0; }
    }
    bool raw = (digitalRead(e.pinSW) == LOW);
    if (raw != e.swRaw) { e.swRaw = raw; e.swChangedAt = now; }
    if (now - e.swChangedAt > SW_DEBOUNCE_MS && raw != e.swLevel) {
      e.swLevel = raw;
      if (raw) encPress(i); else encRelease(i);
    }
  }
}

// ============================================================================
//  LEDS
// ============================================================================
void ledService(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 20) return;              // 50 Hz is plenty for a lava lamp,
  last = now;                               // and it frees the loop for encoders

  // ---- press-and-hold takeover, mirroring how NEPTR shows it -------------
  // Past 5s the whole ring goes green and stays green while held. Between 5s
  // and 8s a blue fill creeps across, so the distance to delete is visible
  // rather than counted. At 8s everything snaps to white.
  int8_t   hi = -1;
  uint32_t hm = 0;
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    if (!holdArmed[i] || !enc[i].swLevel) continue;
    uint32_t h = now - holdStart[i];
    if (h >= HOLD_CAPTURE_MS && h > hm) { hm = h; hi = i; }
  }
  if (hi >= 0) {
    if (hm >= HOLD_DELETE_MS) {
      for (uint8_t i = 0; i < NUM_ENC; i++)
        strip.setPixelColor(LED_FOR_ENC[i], strip.Color(255, 255, 255));
    } else {
      uint32_t span = HOLD_DELETE_MS - HOLD_CAPTURE_MS;
      uint8_t  n    = (uint8_t)(((hm - HOLD_CAPTURE_MS) * NUM_ENC + span / 2) / span);
      for (uint8_t i = 0; i < NUM_ENC; i++)
        strip.setPixelColor(LED_FOR_ENC[i],
          (i < n) ? strip.Color(0, 0, 255) : strip.Color(0, 255, 0));
    }
    strip.show();
    return;
  }

  float tt = (float)now * 0.0011f;

  for (uint8_t i = 0; i < NUM_ENC; i++) {
    uint32_t c;
    if (encToggle[i]) {
      c = strip.Color(255, 255, 255);       // selected: flat white, no lava
    }
    else if (shiftHeld) {
      c = (i <= 6) ? strip.ColorHSV(0, 255, 255) : 0;   // the shift menu
      if (i == 0) c = strip.Color(255, 255, 255);
    }
    else {
      c = lavaColor(i, tt);
    }
    strip.setPixelColor(LED_FOR_ENC[i], c);
  }
  strip.show();
}

// ============================================================================
//  BOOT SHOW  --  six seconds, chaos into order
// ============================================================================
static void bootOledGlitch() {
  oled.clearDisplay();
  for (int k = 0; k < 14; k++) {
    int w = random(8, 70), h = random(3, 16);
    oled.fillRect(random(0, 128 - w), random(0, 128 - h), w, h, SH110X_WHITE);
  }
  oled.display();
}
static void bootOledRings(uint32_t t) {
  oled.clearDisplay();
  for (int k = 0; k < 4; k++) {
    int r = ((t / 6) + k * 22) % 92;
    oled.drawCircle(64, 64, r,     SH110X_WHITE);
    oled.drawCircle(64, 64, r + 1, SH110X_WHITE);
  }
  oled.display();
}
static void bootOledTitle(int reveal) {
  oled.clearDisplay();
  oled.setTextSize(2);                       // 10 chars at 12px advance = 118
  oled.setCursor(5, 44);
  oled.print("DUODECIMUS");
  oled.setTextSize(3);
  oled.setCursor(38, 70);
  oled.print("XII");
  if (reveal < 128) oled.fillRect(reveal, 0, 128 - reveal, 128, SH110X_BLACK);
  oled.display();
}

void bootShow() {
  uint32_t t0 = millis(), lastLed = 0, lastOled = 0;
  randomSeed(analogRead(PIN_JACKA_TIP) ^ micros());

  for (;;) {
    uint32_t now = millis(), t = now - t0;
    if (t >= 6000) break;

    if (now - lastLed >= 16) {
      lastLed = now;
      if (t < 1900) {                                   // 1. static / chaos
        for (uint8_t i = 0; i < NUM_ENC; i++) {
          long r = random(100);
          uint32_t c = (r < 34) ? strip.ColorHSV(0, 255, random(70, 256))
                     : (r < 42) ? strip.Color(255, 255, 255)
                                : 0;
          strip.setPixelColor(LED_FOR_ENC[i], c);
        }
      }
      else if (t < 3900) {                              // 2. comet, accelerating
        uint32_t e = t - 1900;
        int period = 460 - (int)(e * 380 / 2000);
        int head   = (int)(((t % period) * NUM_ENC) / period);
        for (uint8_t i = 0; i < NUM_ENC; i++) {
          int d = (head - i + NUM_ENC) % NUM_ENC;
          uint8_t v = (d > 4) ? 0 : (uint8_t)(255 >> d);
          strip.setPixelColor(LED_FOR_ENC[i],
            d == 0 ? strip.Color(255, 255, 255) : strip.ColorHSV(encHue(i), 255, v));
        }
      }
      else if (t < 5300) {                              // 3. snap into the ramp
        int upto = (int)((t - 3900) * NUM_ENC / 1400);
        for (uint8_t i = 0; i < NUM_ENC; i++) {
          uint32_t c = (i < upto) ? encColor(i)
                     : (i == upto) ? strip.Color(255, 255, 255) : 0;
          strip.setPixelColor(LED_FOR_ENC[i], c);
        }
      }
      else {                                            // 4. bleed from white
        uint32_t e = t - 5300;
        uint8_t k = (e >= 700) ? 255 : (uint8_t)(e * 255 / 700);
        for (uint8_t i = 0; i < NUM_ENC; i++) {
          uint8_t v = (uint8_t)(255 - (uint32_t)(255 - encVal(i)) * k / 255);
          strip.setPixelColor(LED_FOR_ENC[i], strip.ColorHSV(encHue(i), k, v));
        }
      }
      strip.show();
    }

    if (oledOK && now - lastOled >= 80) {
      lastOled = now;
      if      (t < 1900) bootOledGlitch();
      else if (t < 3900) bootOledRings(t);
      else if (t < 5300) bootOledTitle((int)((t - 3900) * 128 / 1400));
      else               bootOledTitle(128);
    }
  }
}

// ============================================================================
//  OLED  --  readable from across the room
// ============================================================================
// Left: one huge glyph, the last-touched encoder's number. On a turn it flips
// to < or >, on a press to o, then snaps back. Right: a widget per jack, a
// filling bar for expression or a thick ring that solidifies for a switch.
// Only changed regions repaint, so a full frame is not pushed on every update.
#define G_X 0
#define G_Y 6
#define G_W 92
#define G_H 100
#define J_X 94
#define J_W 34
#define J_AY 4
#define J_BY 66
#define J_H 58

const char *kindName(PedalKind k) { return k == PEDAL_EXPR ? "EXP" : "SW"; }

static void drawJackWidget(Jack &j, int ry) {
  oled.fillRect(J_X, ry, J_W, J_H, SH110X_BLACK);
  oled.setTextSize(1);
  oled.setCursor(J_X + 1, ry);
  oled.print(j.slot == 0 ? 'A' : 'B');
  if (j.kind == PEDAL_EXPR) {
    int bx = J_X + 10, by = ry + 10, bw = 18, bh = J_H - 12;
    oled.drawRect(bx, by, bw, bh, SH110X_WHITE);
    oled.drawRect(bx + 1, by + 1, bw - 2, bh - 2, SH110X_WHITE);
    int v = (j.lastSent < 0) ? 0 : j.lastSent;
    int fh = (int)((long)(bh - 6) * v / 127);
    if (fh > 0) oled.fillRect(bx + 3, by + bh - 3 - fh, bw - 6, fh, SH110X_WHITE);
  } else {
    int cx = J_X + J_W / 2 + 2, cy = ry + J_H / 2 + 3, r = 14;
    oled.drawCircle(cx, cy, r,     SH110X_WHITE);
    oled.drawCircle(cx, cy, r - 1, SH110X_WHITE);
    oled.drawCircle(cx, cy, r - 2, SH110X_WHITE);
    if (j.sustainOn) oled.fillCircle(cx, cy, r - 5, SH110X_WHITE);
  }
}

static void drawGlyph() {
  oled.fillRect(G_X, G_Y, G_W, G_H, SH110X_BLACK);
  oled.setTextSize(8);                       // 40 x 56 per character
  char buf[4];
  if      (glyphChar)        { buf[0] = glyphChar; buf[1] = 0; }
  else if (lastTouched >= 0) snprintf(buf, sizeof(buf), "%d", lastTouched + 1);
  else                       { buf[0] = '-'; buf[1] = 0; }
  int n = strlen(buf), w = n * 48 - 8;
  oled.setCursor(G_X + (G_W - w) / 2, G_Y + (G_H - 56) / 2);
  oled.print(buf);
}

void oledService(uint32_t now) {
  if (!oledOK) return;
  if (!oledDirty || now - lastOledAt < OLED_MIN_MS) return;
  lastOledAt = now;
  oledDirty = false;

  static bool      first    = true;
  static char      pGlyph   = 1;
  static int8_t    pTouched = -2;
  static bool      pAcc = true, pInv = true, pShift = true;
  static PedalKind pKind[2] = { PEDAL_SUSTAIN, PEDAL_EXPR };
  static int       pVal[2]  = { -2, -2 };
  static bool      pOn[2]   = { true, true };
  static uint32_t  pStatus  = 1;

  if (first) { oled.clearDisplay(); first = false; }

  if (glyphChar != pGlyph || lastTouched != pTouched) {
    pGlyph = glyphChar; pTouched = lastTouched;
    drawGlyph();
  }

  bool showStatus = (now < statusUntil);
  if (accumulatorMode != pAcc || invertRange != pInv || shiftHeld != pShift
      || statusUntil != pStatus || !showStatus) {
    pAcc = accumulatorMode; pInv = invertRange; pShift = shiftHeld; pStatus = statusUntil;
    oled.fillRect(0, 110, 92, 18, SH110X_BLACK);
    oled.setTextSize(1);
    oled.setCursor(2, 112);
    if (showStatus)      oled.print(statusMsg);
    else if (shiftHeld)  oled.print("SHIFT");
    else {
      oled.print("CH7 ");
      oled.print(accumulatorMode ? "ABS" : "REL");
      oled.print(invertRange ? " INV" : "");
    }
  }

  Jack *jk[2] = { &jackA, &jackB };
  int   ry[2] = { J_AY, J_BY };
  for (int i = 0; i < 2; i++) {
    Jack &j = *jk[i];
    if (j.kind != pKind[i] || j.lastSent != pVal[i] || j.sustainOn != pOn[i]) {
      pKind[i] = j.kind; pVal[i] = j.lastSent; pOn[i] = j.sustainOn;
      drawJackWidget(j, ry[i]);
    }
  }
  oled.display();
}

// ============================================================================
//  SETUP / LOOP
// ============================================================================
void setup() {
  analogReadResolution(ADC_BITS);
  analogReadAveraging(8);

  for (uint8_t i = 0; i < NUM_ENC; i++) {
    enc[i].pinA = ENC_PINS[i][0];
    enc[i].pinB = ENC_PINS[i][1];
    enc[i].pinSW = ENC_PINS[i][2];
    pinMode(enc[i].pinA,  INPUT_PULLUP);
    pinMode(enc[i].pinB,  INPUT_PULLUP);
    pinMode(enc[i].pinSW, INPUT_PULLUP);
    enc[i].quadState = (digitalRead(enc[i].pinA) << 1) | digitalRead(enc[i].pinB);
    enc[i].quadAccum = 0;
    enc[i].swLevel = enc[i].swRaw = false;
    enc[i].swChangedAt = 0;
    encValue[i] = 64;
    encToggle[i] = false;
  }

  strip.begin();
  strip.setBrightness(70);
  strip.show();

  // The panel is optional: if it never answers, every display call is skipped
  // and the encoders and MIDI carry on regardless.
  Wire2.setSDA(PIN_SDA);
  Wire2.setSCL(PIN_SCL);
  oledOK = oled.begin(0x3C, true);
  if (!oledOK) oledOK = oled.begin(0x3D, true);
  if (oledOK) {
    oled.setRotation(2);
    oled.clearDisplay();
    oled.setTextColor(SH110X_WHITE);
    oled.display();
  }

  bootShow();

  loadPreset();
  accumulatorMode = false;      // always power on in relative mode
  loadPedalKinds();             // also configures both tip pins
  oledDirty = true;
}

void loop() {
  uint32_t now = millis();
  encService(now);              // highest rate, never blocked by the rest
  holdService(now);
  jackService(jackA, now);
  jackService(jackB, now);
  ledService(now);
  oledService(now);
  if (glyphUntil && now > glyphUntil) { glyphUntil = 0; glyphChar = 0; oledDirty = true; }
  while (usbMIDI.read()) { }
}
