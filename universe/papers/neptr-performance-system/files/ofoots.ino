#include <MIDIUSB.h>
#include <ILI9341_t3.h>
#include <Adafruit_NeoPixel.h>

#define TFT_DC    9
#define TFT_CS    10
#define TFT_RST   8

ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);

#define NEOPIXEL_PIN 38
#define NUM_PIXELS   12

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ----------------------------------------------------------------------------------
// CONFIGURATION
// ----------------------------------------------------------------------------------

struct EncoderDef {
  int clkPin, dtPin, swPin;
  byte ccRotation, ccButton;
  int lastClkState, lastSwState;
  bool buttonState, lastButtonPress;
};

EncoderDef encoders[10] = {
  {2, 3, 4, 70, 80, HIGH, HIGH, false, HIGH},
  {5, 6, 7, 71, 81, HIGH, HIGH, false, HIGH},
  {14,15,16,72, 82, HIGH, HIGH, false, HIGH},
  {17,18,19,73, 83, HIGH, HIGH, false, HIGH},
  {20,21,22,74, 84, HIGH, HIGH, false, HIGH},
  {23,24,25,75, 85, HIGH, HIGH, false, HIGH},
  {26,27,28,76, 86, HIGH, HIGH, false, HIGH},
  {29,30,31,77, 87, HIGH, HIGH, false, HIGH},
  {32,33,34,78, 88, HIGH, HIGH, false, HIGH},
  {35,36,37,79, 89, HIGH, HIGH, false, HIGH}
};

const byte pageCCs[8][8][2] = {
  {{80,70}, {81,71}, {82,72}, {83,73}, {84,74}, {85,75}, {86,76}, {87,77}},
  {{42,32}, {43,33}, {44,34}, {45,35}, {46,36}, {47,37}, {98,88}, {99,89}},
  {{100,90}, {101,91}, {102,92}, {103,93}, {104,94}, {105,95}, {106,96}, {107,97}},
  {{108,1}, {109,2}, {110,3}, {111,4}, {112,5}, {113,6}, {114,9}, {115,14}},
  {{116,15}, {117,16}, {118,19}, {119,20}, {120,24}, {121,25}, {122,26}, {123,27}},
  {{124,28}, {125,29}, {126,30}, {127,31}, {10,0}, {17,7}, {18,8}, {21,11}},
  {{22,12}, {23,13}, {48,38}, {49,39}, {50,40}, {51,41}, {52,58}, {53,59}},
  {{54,60}, {55,65}, {56,66}, {57,67}, {68,61}, {69,62}, {78,63}, {79,64}}
};

int currentPage = 0;

// per-page states
bool pageButtonStates[8][8]; // initialized to false
int pageEncoderValues[8][8]; // initialized to 64

// smoothing globals for expression pedal
const float exprAlpha    = 0.1f;    // 0.0–1.0, lower = smoother
float       exprSmoothed = 0;

// mode flags
bool knobMode        = false;
bool reverseButtonCC = true; // HERE (initially true to reverse the toggle initiation logic)
int  encoderValues[10] = {64,64,64,64,64,64,64,64,64,64};
unsigned long modeHoldStart    = 0;
unsigned long reverseHoldStart = 0;

// timing + hue
unsigned long previousMillis = 0;
unsigned long lastExprSend   = 0;
uint16_t     baseHue         = 0;

// expression pedal
const int  exprPedalPin = A15;
const byte exprPedalCC  = 61;
int lastExprValue       = -1;

// non-blocking page indication
bool indicationMode = false;
unsigned long indicationStart = 0;
const int indicationDuration = 500; // half a second

void setup() {
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Hello World");

  for (int i = 0; i < 10; i++) {
    pinMode(encoders[i].clkPin, INPUT_PULLUP);
    pinMode(encoders[i].dtPin,  INPUT_PULLUP);
    pinMode(encoders[i].swPin,  INPUT_PULLUP);
    encoders[i].lastClkState = digitalRead(encoders[i].clkPin);
    encoders[i].lastSwState  = digitalRead(encoders[i].swPin);
  }

  // initialize per-page states
  for (int p = 0; p < 8; p++) {
    for (int i = 0; i < 8; i++) {
      pageButtonStates[p][i] = false;
      pageEncoderValues[p][i] = 64;
    }
  }

  pinMode(exprPedalPin, INPUT);
  exprSmoothed = analogRead(exprPedalPin);  // initialize filter

  strip.begin();
  strip.setBrightness(64);  // Further lower brightness to potentially reduce flickering due to power draw issues
  strip.show();
}

void loop() {
  unsigned long currentMillis = millis();

  // toggle knobMode (encoders 8+9)
  if (digitalRead(encoders[8].swPin)==LOW && digitalRead(encoders[9].swPin)==LOW) {
    if (!modeHoldStart) modeHoldStart = currentMillis;
    if (currentMillis - modeHoldStart >= 3000) {
      knobMode = !knobMode;
      flashConfirm();
      modeHoldStart = currentMillis + 500;
    }
  } else modeHoldStart = 0;

  // toggle reverseButtonCC (encoders 4+7)
  if (digitalRead(encoders[4].swPin)==LOW && digitalRead(encoders[7].swPin)==LOW) {
    if (!reverseHoldStart) reverseHoldStart = currentMillis;
    if (currentMillis - reverseHoldStart >= 3000) {
      reverseButtonCC = !reverseButtonCC;
      flashConfirm();
      reverseHoldStart = currentMillis + 500;
    }
  } else reverseHoldStart = 0;

  // handle all encoders
  for (int i = 0; i < 10; i++) handleEncoder(encoders[i], i);

  // hue animation
  if (currentMillis - previousMillis > 10) {
    previousMillis = currentMillis;
    baseHue = (baseHue + 42) % 65536;
  }

  // —— optimized expression pedal —— //
  int rawExpr = analogRead(exprPedalPin);
  exprSmoothed = exprAlpha * rawExpr + (1 - exprAlpha) * exprSmoothed;
  int midiRaw  = map((int)exprSmoothed, 0, 1023, 160, -28);
      midiRaw  = constrain((int)(midiRaw * 1.07f), 0, 127);

  if (abs(midiRaw - lastExprValue) > 2 && currentMillis - lastExprSend > 25) {
    sendMIDIControlChange(exprPedalCC, midiRaw);
    lastExprValue = midiRaw;
    lastExprSend  = currentMillis;
  }
  // —— end expression pedal —— //

  // handle indication if active
  if (indicationMode) {
    if (currentMillis - indicationStart >= indicationDuration) {
      indicationMode = false;
    } else {
      setIndicationLights();
      strip.show();
    }
  } else {
    // normal pixel update
    updatePixels();
    strip.show();
  }
  
  delay(5);
}

void updatePixels() {
  uint16_t startingHue = currentPage * (65536 / 8);
  for (int i = 0; i < 8; i++) {
    uint32_t c = strip.ColorHSV((startingHue + (i * (65536 / 8))) % 65536, 255, 255);
    strip.setPixelColor(i, encoders[i].buttonState ? strip.Color(180,180,180) : c);
  }

  uint32_t rainbowColor = strip.ColorHSV(baseHue,255,map(lastExprValue,0,127,30,255));
  strip.setPixelColor(8,  encoders[8].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(10, encoders[8].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(9,  encoders[9].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(11, encoders[9].buttonState ? strip.Color(180,180,180) : rainbowColor);
}

void setIndicationLights() {
  int nLights = currentPage + 1;
  for (int i = 0; i < 8; i++) {
    if (i < nLights) {
      strip.setPixelColor(i, strip.Color(255, 255, 255)); // white
    } else {
      strip.setPixelColor(i, 0); // dark
    }
  }
  // set rainbow lights as normal
  uint32_t rainbowColor = strip.ColorHSV(baseHue,255,map(lastExprValue,0,127,30,255));
  strip.setPixelColor(8,  encoders[8].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(10, encoders[8].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(9,  encoders[9].buttonState ? strip.Color(180,180,180) : rainbowColor);
  strip.setPixelColor(11, encoders[9].buttonState ? strip.Color(180,180,180) : rainbowColor);
}

void handleEncoder(EncoderDef &enc, int index) {
  int clk = digitalRead(enc.clkPin);
  if (clk != enc.lastClkState) {
    bool dir = (digitalRead(enc.dtPin) != clk);
    if (knobMode) {
      encoderValues[index] = constrain(
        encoderValues[index] + (dir ? 1 : -1), 0, 127
      );
      sendMIDIControlChange(enc.ccRotation, encoderValues[index]);
    } else {
      sendMIDIControlChange(enc.ccRotation, dir ? 127 : 0);
    }
    enc.lastClkState = clk;
  }

  int sw = digitalRead(enc.swPin);
  if (index < 8) {
    if (sw==LOW && enc.lastButtonPress==HIGH) {
      delay(50);
      if (digitalRead(enc.swPin)==LOW) {
        enc.buttonState = !enc.buttonState;
        byte v = reverseButtonCC
          ? (enc.buttonState ? 127 : 0)
          : (enc.buttonState ?   0 :127);
        sendMIDIControlChange(enc.ccButton, v);
      }
    }
    enc.lastButtonPress = sw;
  } else {
    if (sw != enc.lastSwState) {
      delay(5);
      enc.buttonState = (sw==LOW);
      if (enc.buttonState) { // on press
        int oldPage = currentPage;
        if (index == 8) { currentPage = max(0, currentPage - 1); }
        else if (index == 9) { currentPage = min(7, currentPage + 1); }
        if (oldPage != currentPage) {
          // save old page states
          for (int i = 0; i < 8; i++) {
            pageButtonStates[oldPage][i] = encoders[i].buttonState;
            pageEncoderValues[oldPage][i] = encoderValues[i];
          }
          // update CCs
          updatePageCCs();
          // load new page states
          for (int i = 0; i < 8; i++) {
            encoders[i].buttonState = pageButtonStates[currentPage][i];
            encoderValues[i] = pageEncoderValues[currentPage][i];
            // send button state for new CC
            byte v = reverseButtonCC
              ? (encoders[i].buttonState ? 127 : 0)
              : (encoders[i].buttonState ?   0 :127);
            sendMIDIControlChange(encoders[i].ccButton, v);
            // if knobMode, send rotation value for new CC
            if (knobMode) {
              sendMIDIControlChange(encoders[i].ccRotation, encoderValues[i]);
            }
          }
          // start indication
          startIndication();
        }
      }
      byte v = reverseButtonCC
        ? (enc.buttonState ? 127 : 0)
        : (enc.buttonState ?   0 :127);
      sendMIDIControlChange(enc.ccButton, v);
    }
    enc.lastSwState = sw;
  }
}

void updatePageCCs() {
  for (int i = 0; i < 8; i++) {
    encoders[i].ccButton = pageCCs[currentPage][i][0];
    encoders[i].ccRotation = pageCCs[currentPage][i][1];
  }
}

void startIndication() {
  indicationMode = true;
  indicationStart = millis();
  // immediately set lights for indication
  setIndicationLights();
  strip.show();
}

void flashConfirm() {
  for (int i = 0; i < NUM_PIXELS; i++)
    strip.setPixelColor(i, strip.Color(255,255,255));
  strip.show();
  delay(500);
}

void sendMIDIControlChange(byte control, byte value) {
  midiEventPacket_t ev = {0x0B, 0xB9, control, value};
  MidiUSB.sendMIDI(ev);
  MidiUSB.flush();
}