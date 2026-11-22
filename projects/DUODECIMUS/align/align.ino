// ============================================================================
//  DUODECIMUS  --  ALIGNMENT UTILITY   (standalone; NOT the performance sketch)
//
//  Learns two things the main firmware cannot know on its own:
//    STAGE 1  which physical knob you want to be encoder 1, 2, 3 ... 12
//    STAGE 2  which NeoPixel sits next to which knob
//
//  Then prints ready-to-paste C for the main sketch over USB serial.
//
//  Arduino IDE:  Tools > Board    = Teensy 4.1
//                Tools > USB Type = Serial        <-- NOT MIDI, for this sketch
//                Tools > CPU      = 600 MHz
//
//  Reflash the main sketch when you are done.
// ============================================================================

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define PIN_NEOPIXEL   38
#define PIN_SDA        25          // Wire2
#define PIN_SCL        24
#define NUM_ENC        12
#define NUM_PIXELS     12

// Must match foots2.ino exactly. { CLK, DT, SW }
static const uint8_t ENC_PINS[NUM_ENC][3] = {
  {  2,  3,  4 }, {  5,  6,  7 }, { 14, 15, 16 }, { 17, 18, 19 },
  { 20, 21, 22 },
  { 23,  0,  1 },
  { 26, 27, 28 }, { 29, 30, 31 }, { 32, 33, 34 }, { 35, 36, 37 },
  {  8,  9, 10 }, { 11, 12, 13 }
};

Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_SH1107   oled(128, 128, &Wire2, -1, 400000UL, 100000UL);
bool oledOK = false;

// ---------------------------------------------------------------- switches --
bool     swLevel[NUM_ENC];
bool     swRaw[NUM_ENC];
uint32_t swAt[NUM_ENC];
#define DEBOUNCE_MS 12

// ------------------------------------------------------------------ state --
enum Stage : uint8_t { STAGE_ENC = 0, STAGE_LED, STAGE_DONE };
Stage    stage      = STAGE_ENC;
int8_t   encOrder[NUM_ENC];        // encOrder[position] = firmware index
uint8_t  encCount   = 0;
int8_t   ledEnc[NUM_PIXELS];       // ledEnc[pixel]     = firmware index
uint8_t  ledCursor  = 0;
uint32_t flashUntil = 0;
uint8_t  flashKind  = 0;           // 1 = accepted, 2 = rejected

void resetAll() {
  stage = STAGE_ENC; encCount = 0; ledCursor = 0;
  for (int i = 0; i < NUM_ENC; i++)    encOrder[i] = -1;
  for (int i = 0; i < NUM_PIXELS; i++) ledEnc[i]   = -1;
  Serial.println();
  Serial.println("============== DUODECIMUS ALIGNMENT ==============");
  Serial.println("STAGE 1 of 2  --  ENCODER ORDER");
  Serial.println("Press the knobs in the order you want them numbered.");
  Serial.println("First one you press becomes encoder 1, and so on to 12.");
  Serial.println("Green flash = accepted. Red flash = already used.");
  Serial.println("Send 'r' at any time to start over.");
  Serial.println();
}

bool alreadyUsedEnc(int8_t fw) {
  for (uint8_t i = 0; i < encCount; i++) if (encOrder[i] == fw) return true;
  return false;
}
bool alreadyUsedLed(int8_t fw) {
  for (uint8_t i = 0; i < ledCursor; i++) if (ledEnc[i] == fw) return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    pinMode(ENC_PINS[i][0], INPUT_PULLUP);
    pinMode(ENC_PINS[i][1], INPUT_PULLUP);
    pinMode(ENC_PINS[i][2], INPUT_PULLUP);
    swLevel[i] = swRaw[i] = false; swAt[i] = 0;
  }
  strip.begin(); strip.setBrightness(70); strip.show();

  Wire2.setSDA(PIN_SDA); Wire2.setSCL(PIN_SCL);
  oledOK = oled.begin(0x3C, true);
  if (!oledOK) oledOK = oled.begin(0x3D, true);
  if (oledOK) { oled.setRotation(3); oled.setTextColor(SH110X_WHITE); }

  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2500) { }
  resetAll();
}

// ------------------------------------------------------------------ output --
void printResults() {
  Serial.println();
  Serial.println("================ RESULTS ================");

  // machine-readable, for the laptop to parse
  Serial.print("RESULT_ENC_ORDER ");
  for (int i = 0; i < NUM_ENC; i++) { Serial.print(encOrder[i]); if (i < NUM_ENC - 1) Serial.print(','); }
  Serial.println();
  Serial.print("RESULT_LED_ENC ");
  for (int i = 0; i < NUM_PIXELS; i++) { Serial.print(ledEnc[i]); if (i < NUM_PIXELS - 1) Serial.print(','); }
  Serial.println();
  Serial.println();

  // paste-ready: ENC_PINS reordered so position 0 == the knob pressed first
  Serial.println("// ---- paste into foots2.ino, replacing ENC_PINS ----");
  Serial.println("static const uint8_t ENC_PINS[NUM_ENC][3] = {");
  for (int pos = 0; pos < NUM_ENC; pos++) {
    int fw = encOrder[pos];
    Serial.print("  { ");
    Serial.print(ENC_PINS[fw][0]); Serial.print(", ");
    Serial.print(ENC_PINS[fw][1]); Serial.print(", ");
    Serial.print(ENC_PINS[fw][2]); Serial.print(" }");
    Serial.print(pos < NUM_ENC - 1 ? "," : " ");
    Serial.print("   // encoder "); Serial.print(pos + 1);
    Serial.print(", was firmware index "); Serial.println(fw);
  }
  Serial.println("};");
  Serial.println();

  // pixel index for each encoder POSITION (after the reorder above)
  Serial.println("// ---- paste into foots2.ino ----");
  Serial.print("static const uint8_t LED_FOR_ENC[NUM_ENC] = { ");
  for (int pos = 0; pos < NUM_ENC; pos++) {
    int fw = encOrder[pos], px = -1;
    for (int L = 0; L < NUM_PIXELS; L++) if (ledEnc[L] == fw) { px = L; break; }
    Serial.print(px);
    if (pos < NUM_ENC - 1) Serial.print(", ");
  }
  Serial.println(" };");
  Serial.println();
  Serial.println("Send 'r' to run the whole thing again.");
  Serial.println("=========================================");
}

// ------------------------------------------------------------------- input --
void onPress(uint8_t fw) {
  if (stage == STAGE_ENC) {
    if (alreadyUsedEnc(fw)) { flashKind = 2; flashUntil = millis() + 200; return; }
    encOrder[encCount++] = fw;
    flashKind = 1; flashUntil = millis() + 160;
    Serial.print("  encoder "); Serial.print(encCount);
    Serial.print("  =  firmware index "); Serial.println(fw);
    if (encCount == NUM_ENC) {
      stage = STAGE_LED; ledCursor = 0;
      Serial.println();
      Serial.println("STAGE 2 of 2  --  LIGHT ORDER");
      Serial.println("One light comes on at a time.");
      Serial.println("Press the knob that sits next to the lit light.");
      Serial.println();
    }
  }
  else if (stage == STAGE_LED) {
    if (alreadyUsedLed(fw)) { flashKind = 2; flashUntil = millis() + 200; return; }
    ledEnc[ledCursor] = fw;
    Serial.print("  light "); Serial.print(ledCursor);
    Serial.print("  ->  firmware index "); Serial.println(fw);
    ledCursor++;
    flashKind = 1; flashUntil = millis() + 160;
    if (ledCursor == NUM_PIXELS) { stage = STAGE_DONE; printResults(); }
  }
}

void readSwitches(uint32_t now) {
  for (uint8_t i = 0; i < NUM_ENC; i++) {
    bool raw = (digitalRead(ENC_PINS[i][2]) == LOW);
    if (raw != swRaw[i]) { swRaw[i] = raw; swAt[i] = now; }
    if (now - swAt[i] > DEBOUNCE_MS && raw != swLevel[i]) {
      swLevel[i] = raw;
      if (raw) onPress(i);
    }
  }
}

// ------------------------------------------------------------------ render --
void renderLeds(uint32_t now) {
  if (now < flashUntil) {
    uint32_t c = (flashKind == 1) ? strip.Color(0, 255, 0) : strip.Color(255, 0, 0);
    for (int i = 0; i < NUM_PIXELS; i++) strip.setPixelColor(i, c);
  }
  else if (stage == STAGE_ENC) {
    // dim amber everywhere: we do not know which pixel is which yet
    for (int i = 0; i < NUM_PIXELS; i++) strip.setPixelColor(i, strip.Color(40, 20, 0));
  }
  else if (stage == STAGE_LED) {
    for (int i = 0; i < NUM_PIXELS; i++)
      strip.setPixelColor(i, (i == ledCursor) ? strip.Color(255, 255, 255) : 0);
  }
  else {
    for (int i = 0; i < NUM_PIXELS; i++) strip.setPixelColor(i, strip.Color(0, 90, 0));
  }
  strip.show();
}

void renderOled(uint32_t now) {
  if (!oledOK) return;
  static uint32_t last = 0;
  static int pStage = -1, pA = -1, pB = -1;
  if (now - last < 90) return;
  if (stage == pStage && encCount == pA && ledCursor == pB) return;
  last = now; pStage = stage; pA = encCount; pB = ledCursor;

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(2, 2);
  if      (stage == STAGE_ENC)  oled.print("STAGE 1  KNOB ORDER");
  else if (stage == STAGE_LED)  oled.print("STAGE 2  LIGHT ORDER");
  else                          oled.print("DONE - CHECK LAPTOP");

  oled.setTextSize(7);
  char buf[8];
  if      (stage == STAGE_ENC)  snprintf(buf, sizeof(buf), "%d", encCount + 1);
  else if (stage == STAGE_LED)  snprintf(buf, sizeof(buf), "%d", ledCursor);
  else                          snprintf(buf, sizeof(buf), "OK");
  int n = strlen(buf), w = n * 42 - 7;
  oled.setCursor((128 - w) / 2, 40);
  oled.print(buf);

  oled.setTextSize(1);
  oled.setCursor(2, 112);
  if      (stage == STAGE_ENC)  oled.print("press knobs 1..12 in order");
  else if (stage == STAGE_LED)  oled.print("press knob by the lit light");
  else                          oled.print("send r to redo");
  oled.display();
}

void loop() {
  uint32_t now = millis();
  readSwitches(now);
  renderLeds(now);
  renderOled(now);
  while (Serial.available()) { if (tolower(Serial.read()) == 'r') resetAll(); }
}
