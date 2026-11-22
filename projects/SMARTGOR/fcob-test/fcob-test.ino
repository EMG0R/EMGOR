// FCOB strip — DEFAULT GENERATIVE LIGHTING ("lava lamp" metaball drift)
// Board: DOIT ESP32 DevKit V1 (30-pin clone)
// Strip: BTF FCOB WS2812B 5V, 800 px, data GPIO 13, powered from VIN (USB).
//
// This sketch is the room's baseline generative layer: slow-drifting purple
// globs with occasional exponential "shots" across the strip. Later phases
// (room LED mapping, Pi-driven reactive input) merge on top of this mode.
//
// Power safety: still on USB supply — FastLED power limiter holds total
// draw at 5V/900mA. Do not raise until the dedicated PSU is installed.
//
// OTA: enabled (standard for all smartGOR ESP32 nodes). First flash goes
// over USB; every later flash can be pushed over WiFi to the node's
// hostname/IP (ArduinoOTA, password-protected).

#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// ---------- WiFi (2.4GHz IoT network) ----------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASS     "CHANGE_ME_PASSWORD"
#define WIFI_HOSTNAME "smartgor-strip1"
#define OTA_PASS      "CHANGE_ME_PASSWORD"   // OTA flash password (change with WiFi creds)

// ---------- Strip ----------
#define DATA_PIN   13
#define NUM_LEDS   800
#define BRIGHTNESS 128        // power limiter below is the real safety cap
#define POWER_BUDGET_MA 900   // USB budget incl. margin for the ESP32

// ---------- Default generative mode: tunables ----------
#define GLOB_COUNT        6     // number of lava globs on the strip
#define GLOB_MIN_RADIUS   28.0f // px, soft falloff radius (min)
#define GLOB_MAX_RADIUS   55.0f // px, soft falloff radius (max)
#define BASE_SPEED        0.35f // px/frame, languid drift speed cap
#define SHOT_CHANCE       9     // per-glob chance in 10000 per frame to fire a shot
#define SHOT_SPEED        9.0f  // px/frame initial burst velocity
#define SHOT_DECAY        0.955f// exponential decay of shot velocity per frame
                                // travel ≈ SHOT_SPEED/(1-SHOT_DECAY) ≈ 200-400px (half strip w/ drift)
#define HUE_CENTER        182   // deep purple/violet (FastLED HSV)
#define HUE_SPREAD        9     // per-glob variation; keeps hues in ~173-191, no pink
#define SATURATION        245

struct Glob {
  float pos;      // px
  float vel;      // px/frame, slow drift component
  float shotVel;  // px/frame, decaying burst component
  float radius;   // px
  uint8_t hue;
};

CRGB leds[NUM_LEDS];
Glob globs[GLOB_COUNT];
uint32_t frames = 0;
uint32_t lastReport = 0;
uint32_t lastWifiCheck = 0;
bool wifiWasConnected = false;
WebServer server(80);

float frand(float lo, float hi) {
  return lo + (esp_random() / (float)UINT32_MAX) * (hi - lo);
}

void initGlobs() {
  for (int i = 0; i < GLOB_COUNT; i++) {
    globs[i].pos = frand(0, NUM_LEDS - 1);
    globs[i].vel = frand(-BASE_SPEED, BASE_SPEED);
    globs[i].shotVel = 0;
    globs[i].radius = frand(GLOB_MIN_RADIUS, GLOB_MAX_RADIUS);
    globs[i].hue = HUE_CENTER + (int)frand(-HUE_SPREAD, HUE_SPREAD);
  }
}

// Baseline generative layer: mostly-slow lava drift, occasional exponential
// shot across ~half the strip, additive blending where globs collide.
void runLavaDefault() {
  // -- update motion --
  for (int i = 0; i < GLOB_COUNT; i++) {
    Glob &g = globs[i];

    // occasionally fire a shot: fast burst that exponentially decays back to drift
    if (g.shotVel == 0 && (esp_random() % 10000) < SHOT_CHANCE) {
      float dir = (g.vel >= 0) ? 1.0f : -1.0f;
      if ((esp_random() & 3) == 0) dir = -dir; // sometimes shoot against drift
      g.shotVel = dir * SHOT_SPEED;
    }
    g.shotVel *= SHOT_DECAY;
    if (fabsf(g.shotVel) < 0.05f) g.shotVel = 0;

    // gentle drift wander
    g.vel += frand(-0.01f, 0.01f);
    g.vel = constrain(g.vel, -BASE_SPEED, BASE_SPEED);

    g.pos += g.vel + g.shotVel;

    // bounce at strip ends
    if (g.pos < 0) { g.pos = -g.pos; g.vel = fabsf(g.vel); g.shotVel = fabsf(g.shotVel); }
    if (g.pos > NUM_LEDS - 1) {
      g.pos = 2 * (NUM_LEDS - 1) - g.pos;
      g.vel = -fabsf(g.vel);
      g.shotVel = -fabsf(g.shotVel);
    }
  }

  // slow-moving globs that meet swap drift directions (soft "collision"),
  // shots pass through and just blend brighter
  for (int i = 0; i < GLOB_COUNT; i++) {
    for (int j = i + 1; j < GLOB_COUNT; j++) {
      float d = globs[j].pos - globs[i].pos;
      bool approaching = (d > 0 && globs[i].vel > globs[j].vel) ||
                         (d < 0 && globs[j].vel > globs[i].vel);
      if (fabsf(d) < 8.0f && approaching &&
          globs[i].shotVel == 0 && globs[j].shotVel == 0) {
        float t = globs[i].vel; globs[i].vel = globs[j].vel; globs[j].vel = t;
      }
    }
  }

  // -- render: metaball falloff, additive brightness, hue-weighted blend --
  for (int p = 0; p < NUM_LEDS; p++) {
    float bright = 0;
    float hueAcc = 0;
    for (int i = 0; i < GLOB_COUNT; i++) {
      float d = (p - globs[i].pos) / globs[i].radius;
      float c = expf(-d * d * 2.2f); // gaussian-ish soft edge
      bright += c;
      hueAcc += c * globs[i].hue;
    }
    if (bright > 0.012f) {
      uint8_t hue = (uint8_t)(hueAcc / bright);
      float v = bright > 1.0f ? 1.0f : bright; // overlap saturates to full
      leds[p] = CHSV(hue, SATURATION, (uint8_t)(v * 255.0f));
    } else {
      leds[p] = CRGB::Black;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== smartGOR strip1: default generative mode (lava purple) ===");
  Serial.printf("Data pin: GPIO %d, pixels: %d, brightness: %d/255, cap: %dmA\n",
                DATA_PIN, NUM_LEDS, BRIGHTNESS, POWER_BUDGET_MA);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, POWER_BUDGET_MA);
  FastLED.clear(true);

  initGlobs();

  // non-blocking WiFi join — animation never waits on this
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi: joining \"%s\" as %s (non-blocking)\n", WIFI_SSID, WIFI_HOSTNAME);

  server.on("/", []() {
    server.send(200, "text/plain",
                String("ok ") + WIFI_HOSTNAME + " " + WiFi.localIP().toString());
  });

  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);

  Serial.println("Lava drift running.");
}

void loop() {
  runLavaDefault();
  FastLED.show();
  frames++;

  uint32_t now = millis();

  // WiFi status watcher (non-blocking)
  if (now - lastWifiCheck >= 500) {
    lastWifiCheck = now;
    bool up = (WiFi.status() == WL_CONNECTED);
    if (up && !wifiWasConnected) {
      Serial.printf("WiFi connected: %s  (http://%s/)\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.localIP().toString().c_str());
      server.begin();
      ArduinoOTA.begin();
      Serial.printf("OTA ready: %s.local (password protected)\n", WIFI_HOSTNAME);
    } else if (!up && wifiWasConnected) {
      Serial.println("WiFi dropped, auto-reconnecting...");
      WiFi.reconnect();
    }
    wifiWasConnected = up;
  }
  if (wifiWasConnected) {
    server.handleClient();
    ArduinoOTA.handle();
  }

  if (now - lastReport >= 5000) {
    Serial.printf("alive: %lu frames, %.1f fps, wifi=%s ip=%s\n",
                  (unsigned long)frames, frames * 1000.0f / now,
                  wifiWasConnected ? "up" : "down",
                  WiFi.localIP().toString().c_str());
    lastReport = now;
  }
  delay(12); // ~35-40 fps with render cost
}
