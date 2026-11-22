// DOT1 — per-node copy of the fcob-strip pattern, scaled down for a 7-px
// NeoPixel "dot" jewel (WS2812B, 5V, GRB) — NodeMCU ESP32 (WROOM-32).
// Edit the NODE CONFIG block below to set name / pixel count / pin.
//
// Same feature set as fcob-strip2/fcob-strip3, minus the strip-scale nebula
// blob field (7 px is too small for a wandering metaball field to read as
// anything but noise). The boot-default "generative" style here is a simple
// gentle breathing + slow hue rotation across all 7 pixels instead:
//   generative — breathe: whole dot one hue, hue rotates slowly, brightness
//     breathes smoothly (no strobe). Styles: breathe/pulse/chase/rainbow.
//   solid      — whole dot one chosen color
//   scene      — LLM-authored physics scene (background + bouncing blobs),
//     unchanged math from the strip nodes; works fine at any pixel count.
//   HTTP JSON API on :80 (/api/state, /api/set) with CORS, optional MQTT,
//   non-blocking WiFi reconnect, OTA (dot1 / "CHANGE_ME_PASSWORD").
//
// mode:"stream" — FRAME PROTOCOL v1 (UDP :21324, little-endian):
//   byte 0-1  magic 'F','C'
//   byte 2    protocol version = 1
//   byte 3    flags — bit0 = SHOW: after copying this packet's pixels,
//             latch the FULL buffer to the strip
//   byte 4-5  uint16 LE pixel offset
//   byte 6-7  uint16 LE pixel count N
//   byte 8..  3*N bytes pixel data, RGB triplets (firmware maps to strip order)
// Any offset/count with offset+count <= NUM_LEDS is accepted; invalid packets
// are dropped silently. Any valid packet auto-switches mode to "stream"; 5 s
// of silence falls back to whatever mode was active before streaming began.
// Master brightness from /api/set still applies (FastLED global brightness).
//
// FastLED power cap (5V/500mA) — plenty for 7 px, kept as a safety net.

#include <FastLED.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// =====================================================================
// NODE CONFIG — the ONLY lines you should need to touch per node
// =====================================================================
#define NODE_NAME "dot1"        // hostname / OTA name / mDNS (dot1.local)
#define NUM_LEDS  7              // 7-px NeoPixel jewel/dot module
#define DATA_PIN  13             // GPIO 13, same convention as the strip nodes
// =====================================================================
// Powered from the NodeMCU's 3V3 pin (AMS1117 regulator), NOT VIN — that
// regulator also feeds the ESP32 itself (~250-350mA with WiFi bursts, brief
// spikes higher). LED budget is capped at 450mA to leave headroom and avoid
// browning out the regulator (a brownout here drops WiFi — this exact
// failure mode has bitten this project before). 7x WS2812B at full white and
// max brightness draws ~420mA, so 450mA is right at the edge by design.
// For genuinely max 5V brightness with full safety margin, move LED power to
// VIN instead of 3V3.
#define BRIGHTNESS 255          // global default brightness (0-255), runtime-tunable — max safe for 3V3 rail w/ 450mA cap below
#define POWER_BUDGET_MA 450     // 3V3-rail budget: ~450mA for LEDs, leaves ~350-450mA for ESP32/WiFi on the AMS1117

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "CHANGE_ME_PASSWORD";
const char *OTA_HOSTNAME = NODE_NAME;
const char *OTA_PASSWORD = "CHANGE_ME_PASSWORD";

// ---- MQTT (optional / future-proofing): empty host == fully disabled ----
#define MQTT_HOST ""           // e.g. "192.168.8.10"; "" -> MQTT never runs
#define MQTT_PORT 1883
#define MQTT_TOPIC_SET   "lights/dot1/set"
#define MQTT_TOPIC_STATE "lights/dot1/state"

// ---- stream protocol v1 constants ----
#define STREAM_UDP_PORT     21324
#define STREAM_MAGIC0       'F'
#define STREAM_MAGIC1       'C'
#define STREAM_PROTO_VER    1
#define STREAM_FLAG_SHOW    0x01
#define STREAM_HDR_LEN      8
#define STREAM_TIMEOUT_MS   5000
// biggest legal packet: header + NUM_LEDS RGB triplets
#define STREAM_BUF_LEN      (STREAM_HDR_LEN + 3 * NUM_LEDS)

CRGB leds[NUM_LEDS];

// ---------------------------------------------------------------------
// runtime state driven by the API (mode / color / brightness)
// ---------------------------------------------------------------------
enum Mode : uint8_t { MODE_GENERATIVE, MODE_SOLID, MODE_SCENE, MODE_STREAM };
Mode g_mode = MODE_GENERATIVE;         // boot default = generative breathe
uint8_t g_brightness = BRIGHTNESS;     // applies in ALL modes (master scale)
CRGB g_solidColor = CRGB(0x3a, 0x0a, 0x80); // default deep violet
bool stateDirty = true;                // publish MQTT state on next tick

// ---- generative render style (extensible dispatch: one case per style) ----
enum Style : uint8_t { STYLE_BREATHE, STYLE_PULSE, STYLE_CHASE, STYLE_RAINBOW };
Style g_style = STYLE_BREATHE;         // default = gentle breathe + hue rotate

WebServer server(80);
WiFiClient mqttNet;
PubSubClient mqtt(mqttNet);
uint32_t lastMqttAttempt = 0;

// ---- stream mode state ----
WiFiUDP streamUdp;
bool streamUdpStarted = false;
Mode g_streamPrevMode = MODE_GENERATIVE; // mode to fall back to on timeout
uint32_t g_streamLastPacketMs = 0;       // millis() of last VALID packet
uint32_t g_streamPackets = 0;            // valid packets since boot
uint32_t g_streamFrames = 0;             // SHOW latches since boot
uint32_t g_streamDropped = 0;            // invalid packets dropped since boot
IPAddress g_streamLastSender;            // sender of last valid packet
bool g_streamShowPending = false;        // a SHOW packet arrived this loop
static uint8_t streamBuf[STREAM_BUF_LEN]; // static: no per-packet allocation

// =====================================================================
// TUNABLES — "breathe" generative style (boot default, 7-px scale)
// =====================================================================
// Whole dot fills with one hue; hue rotates slowly around the wheel while
// brightness breathes smoothly in and out. No strobe, no motion — simple
// and legible on a small cluster of pixels.
#define BREATHE_SAT          220   // deep, not washed out
#define BREATHE_MIN_VAL      35    // floor so it's never fully dark
#define BREATHE_MAX_VAL      255
#define BREATHE_PERIOD_MS    6000  // one full brightness breathe cycle
#define BREATHE_HUE_PERIOD_MS 42000 // one full hue-wheel rotation

// =====================================================================
// TUNABLES — other generative styles (pulse / chase / rainbow)
// no strobe/flash effects: all transitions here are smooth & continuous.
// =====================================================================
#define PULSE_PERIOD_MS    4000  // full breathe cycle, a few seconds
#define PULSE_MIN_SCALE    40    // never fully dark
#define PULSE_MAX_SCALE    255

#define CHASE_SPEED        0.35f // dot-lengths / second, moderate & smooth
#define CHASE_TRAIL_FADE   60    // per-frame fade amount -> comet trail length

#define RAINBOW_SPEED      18.0f // hue units / second (255 = one full cycle)

// =====================================================================
// mode:"scene" — LLM-authored physics scene: a background fill + up to
// MAX_SCENE_OBJECTS moving/bouncing colored blobs, rendered with the same
// soft-falloff + additive compositing math the strip nodes use.
// NOTE: scene uses LINEAR distance, NOT wrapped — the dot is treated as a
// short line of 7 pixels with hard walls.
// =====================================================================
#define MAX_SCENE_OBJECTS   10
#define SCENE_SIZE_MIN_PX   1.0f
#define SCENE_SIZE_MAX_PX   7.0f
#define SCENE_VEL_MIN       -2.0f
#define SCENE_VEL_MAX        2.0f

struct SceneObject {
  CRGB color;
  float pos;     // 0..1 normalized position
  float vel;     // dot-lengths / sec
  float radius;  // normalized (size_px / NUM_LEDS)
  bool bounce;
};

CRGB g_sceneBackground = CRGB::Black;
SceneObject g_sceneObjects[MAX_SCENE_OBJECTS];
int g_sceneObjectCount = 0;
// instrumentation: object-object impacts since boot, and wall impacts since
// boot. Exposed via GET /api/state?debug=1 so collisions can be PROVEN to
// fire rather than inferred from a velocity snapshot.
uint32_t g_collisionCount = 0;
uint32_t g_wallBounceCount = 0;

uint32_t frames = 0, lastReport = 0, lastFrameUs = 0;
uint32_t framesAtLastReport = 0;
float g_fps = 0.0f;   // instantaneous frame rate, exposed in debug state

// wifi bookkeeping (non-blocking)
uint32_t lastWifiAttempt = 0;
bool otaReady = false;

static float frand(float a, float b) {
  return a + (b - a) * (esp_random() / 4294967295.0f);
}

// =====================================================================
// control state: JSON helpers + apply logic shared by HTTP & MQTT
// =====================================================================
static String colorToHex(const CRGB &c) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
  return String(buf);
}

// parse "#rrggbb" (or "rrggbb", "%23rrggbb" already decoded) -> CRGB
static bool hexToColor(const String &s, CRGB &out) {
  String h = s;
  h.trim();
  if (h.startsWith("#")) h = h.substring(1);
  if (h.length() != 6) return false;
  char *end = nullptr;
  long v = strtol(h.c_str(), &end, 16);
  if (end == h.c_str() || *end != '\0') return false;
  out = CRGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
  return true;
}

static Style styleFromString(const String &s) {
  if (s == "pulse")   return STYLE_PULSE;
  if (s == "chase")   return STYLE_CHASE;
  if (s == "rainbow") return STYLE_RAINBOW;
  return STYLE_BREATHE; // "breathe" or unrecognized -> default
}

static const char *styleToString(Style s) {
  switch (s) {
    case STYLE_PULSE:   return "pulse";
    case STYLE_CHASE:   return "chase";
    case STYLE_RAINBOW: return "rainbow";
    default:             return "breathe";
  }
}

static const char *modeToString(Mode m) {
  switch (m) {
    case MODE_SOLID:  return "solid";
    case MODE_SCENE:  return "scene";
    case MODE_STREAM: return "stream";
    default:          return "generative";
  }
}

// debug=false (default) matches the documented public contract exactly
// (objectCount only, no per-object detail). debug=true adds a full
// per-object readback (pos/vel/radius/color/bounce) for instrumentation /
// bug-hunting via GET /api/state?debug=1 — never used by the LLM skill.
static String buildStateJson(bool debug = false) {
  DynamicJsonDocument doc(debug ? 2048 : 768);
  doc["name"] = NODE_NAME;
  doc["hostname"] = OTA_HOSTNAME;
  doc["mode"] = modeToString(g_mode);
  doc["style"] = styleToString(g_style);
  doc["brightness"] = g_brightness;
  doc["color"] = colorToHex(g_solidColor);
  if (g_mode == MODE_SCENE) {
    doc["objectCount"] = g_sceneObjectCount;
    doc["background"] = colorToHex(g_sceneBackground);
    if (debug) {
      doc["collisions"] = g_collisionCount;   // object-object impacts since boot
      doc["wallBounces"] = g_wallBounceCount; // wall impacts since boot
      doc["fps"] = g_fps;
      JsonArray arr = doc.createNestedArray("objects");
      for (int i = 0; i < g_sceneObjectCount; i++) {
        SceneObject &o = g_sceneObjects[i];
        JsonObject jo = arr.createNestedObject();
        jo["i"] = i;
        jo["color"] = colorToHex(o.color);
        jo["pos"] = o.pos;
        jo["vel"] = o.vel;
        jo["radius"] = o.radius;                       // normalized (contract)
        jo["radiusPx"] = o.radius * (float)NUM_LEDS;   // pixels (readable)
        jo["bounce"] = o.bounce;
      }
    }
  }
  if (g_mode == MODE_STREAM) {
    JsonObject st = doc.createNestedObject("stream");
    st["port"] = STREAM_UDP_PORT;
    st["packets"] = g_streamPackets;   // valid packets since boot
    st["frames"] = g_streamFrames;     // SHOW latches since boot
    st["dropped"] = g_streamDropped;   // invalid packets dropped since boot
    st["lastSender"] = g_streamLastSender.toString();
    st["fallbackMode"] = modeToString(g_streamPrevMode);
    st["ageMs"] = (uint32_t)(millis() - g_streamLastPacketMs);
  }
  bool wifi = (WiFi.status() == WL_CONNECTED);
  doc["wifi"] = wifi;
  doc["ip"] = wifi ? WiFi.localIP().toString() : String("");
  String out;
  serializeJson(doc, out);
  return out;
}

// apply any subset of {mode, color, brightness, style}. Rule: if color is
// present and mode is NOT explicitly given, switch to solid automatically.
// An explicit mode of generative/solid/scene force-exits stream mode.
static void applyControl(bool haveMode, const String &modeStr,
                         bool haveColor, const String &colorStr,
                         bool haveBright, int brightVal,
                         bool haveStyle, const String &styleStr) {
  bool changed = false;

  if (haveColor) {
    CRGB c;
    if (hexToColor(colorStr, c)) { g_solidColor = c; changed = true; }
  }
  if (haveBright) {
    int b = constrain(brightVal, 0, 255);
    if (b != g_brightness) { g_brightness = (uint8_t)b; changed = true; }
    FastLED.setBrightness(g_brightness); // master scale — applies in stream too
  }
  if (haveStyle) {
    Style ns = styleFromString(styleStr);
    if (ns != g_style) { g_style = ns; changed = true; }
  }
  if (haveMode) {
    if (modeStr == "solid" && g_mode != MODE_SOLID) { g_mode = MODE_SOLID; changed = true; }
    else if (modeStr == "generative" && g_mode != MODE_GENERATIVE) { g_mode = MODE_GENERATIVE; changed = true; }
    else if (modeStr == "scene" && g_mode != MODE_SCENE) { g_mode = MODE_SCENE; changed = true; }
    // force-exit stream: an explicit API mode wins until the NEXT valid
    // stream packet arrives (which by contract re-enters stream mode).
    if (g_mode != MODE_STREAM) g_streamShowPending = false;
  } else if (haveColor && g_mode != MODE_SOLID) {
    // color given without explicit mode -> auto-switch to solid
    g_mode = MODE_SOLID; changed = true;
  }

  // Bare {"mode":"generative"} with no style/color = the canonical "default
  // ambient" request: restore stock breathe style, so the original look is
  // always reachable.
  if (haveMode && modeStr == "generative" && !haveStyle && !haveColor) {
    if (g_style != STYLE_BREATHE) { g_style = STYLE_BREATHE; changed = true; }
  }

  if (changed) stateDirty = true;
}

// ---- HTTP handlers (CORS enabled so a browser dashboard can call cross-origin)
static void sendJson(int code, const String &body) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(code, "application/json", body);
}

static void handleState() {
  bool debug = server.hasArg("debug") && server.arg("debug") != "0" && server.arg("debug") != "";
  sendJson(200, buildStateJson(debug));
}

static void handleSet() {
  bool haveMode = false, haveColor = false, haveBright = false, haveStyle = false;
  String modeStr, colorStr, styleStr; int brightVal = 0;

  // JSON body (POST) takes priority when present. Sized generously (heap,
  // not stack) to fit a scene payload with up to MAX_SCENE_OBJECTS objects.
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(3072);
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err == DeserializationError::Ok) {
      if (doc.containsKey("mode"))       { haveMode = true;  modeStr = doc["mode"].as<String>(); }
      if (doc.containsKey("color"))      { haveColor = true; colorStr = doc["color"].as<String>(); }
      if (doc.containsKey("brightness")) { haveBright = true; brightVal = doc["brightness"].as<int>(); }
      if (doc.containsKey("style"))      { haveStyle = true; styleStr = doc["style"].as<String>(); }

      // scene: background fill + array of moving/bouncing blobs. Parsed
      // defensively — malformed/missing fields per-object are skipped or
      // defaulted, never crash the device.
      if (doc.containsKey("background")) {
        CRGB c;
        if (hexToColor(doc["background"].as<String>(), c)) {
          g_sceneBackground = c;
          stateDirty = true;
        }
      }
      if (doc.containsKey("objects") && doc["objects"].is<JsonArray>()) {
        JsonArray arr = doc["objects"].as<JsonArray>();
        int n = 0;
        for (JsonVariant v : arr) {
          if (n >= MAX_SCENE_OBJECTS) break;   // cap: truncate silently
          if (!v.is<JsonObject>()) continue;   // skip malformed entries
          JsonObject o = v.as<JsonObject>();
          SceneObject so;
          so.color = CRGB(0xff, 0xff, 0xff);   // default white if bad/missing
          if (o.containsKey("color")) {
            CRGB c;
            if (hexToColor(o["color"].as<String>(), c)) so.color = c;
          }
          so.pos = o.containsKey("pos") ? constrain(o["pos"].as<float>(), 0.0f, 1.0f) : 0.5f;
          so.vel = o.containsKey("vel") ? constrain(o["vel"].as<float>(), SCENE_VEL_MIN, SCENE_VEL_MAX) : 0.1f;
          // size in PIXELS. Accept "size" (canonical) or "radius" (common
          // caller spelling) — previously "radius" was silently dropped and
          // the object fell back to the 6px default, which made objects
          // smaller than requested and collisions harder to trigger.
          float sizePx = 2.0f; // default sized for a 7-px dot (was 6 on a strip)
          if (o.containsKey("size"))
            sizePx = constrain(o["size"].as<float>(), SCENE_SIZE_MIN_PX, SCENE_SIZE_MAX_PX);
          else if (o.containsKey("radius"))
            sizePx = constrain(o["radius"].as<float>(), SCENE_SIZE_MIN_PX, SCENE_SIZE_MAX_PX);
          so.radius = sizePx / (float)NUM_LEDS;
          so.bounce = o.containsKey("bounce") ? o["bounce"].as<bool>() : true;
          g_sceneObjects[n++] = so;
        }
        g_sceneObjectCount = n;
        stateDirty = true;
      }
    }
  }
  // query params (GET or overriding) — easy curl testing
  if (server.hasArg("mode"))       { haveMode = true;  modeStr = server.arg("mode"); }
  if (server.hasArg("color"))      { haveColor = true; colorStr = server.arg("color"); }
  if (server.hasArg("brightness")) { haveBright = true; brightVal = server.arg("brightness").toInt(); }
  if (server.hasArg("style"))      { haveStyle = true; styleStr = server.arg("style"); }

  applyControl(haveMode, modeStr, haveColor, colorStr, haveBright, brightVal, haveStyle, styleStr);
  sendJson(200, buildStateJson());
}

static void handleOptions() { sendJson(204, ""); }

static void startHttpServer() {
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/set", HTTP_GET, handleSet);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/set", HTTP_OPTIONS, handleOptions);
  server.onNotFound([]() { sendJson(404, "{\"error\":\"not found\"}"); });
  server.begin();
  Serial.println("http: control API on :80 (/api/state, /api/set)");
}

// ---- MQTT (optional, non-blocking) ----
static void mqttCallback(char *topic, byte *payload, unsigned int len) {
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
  bool haveMode = doc.containsKey("mode");
  bool haveColor = doc.containsKey("color");
  bool haveBright = doc.containsKey("brightness");
  bool haveStyle = doc.containsKey("style");
  applyControl(haveMode, haveMode ? doc["mode"].as<String>() : String(),
               haveColor, haveColor ? doc["color"].as<String>() : String(),
               haveBright, haveBright ? doc["brightness"].as<int>() : 0,
               haveStyle, haveStyle ? doc["style"].as<String>() : String());
}

static void serviceMqtt(uint32_t now) {
  if (strlen(MQTT_HOST) == 0) return;          // disabled: never touch broker
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    if (now - lastMqttAttempt < 5000) return;  // gentle, non-blocking retry
    lastMqttAttempt = now;
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    if (mqtt.connect(OTA_HOSTNAME)) {
      mqtt.subscribe(MQTT_TOPIC_SET);
      stateDirty = true; // publish current state on (re)connect
    }
    return;
  }
  mqtt.loop();
  if (stateDirty) {
    mqtt.publish(MQTT_TOPIC_STATE, buildStateJson().c_str(), true);
    stateDirty = false;
  }
}

// =====================================================================
// mode:"stream" — UDP frame receiver (FRAME PROTOCOL v1, port 21324)
// =====================================================================
// Drains EVERY pending datagram each call (tight loop, no allocation:
// packets land in the one static streamBuf). Any valid packet:
//   * copies N RGB pixels into leds[] at the given offset
//   * auto-enters stream mode (remembering the mode to fall back to)
//   * if flags bit0 (SHOW) is set, marks the buffer ready to latch —
//     loop() calls FastLED.show() once per loop when a SHOW arrived,
//     so a multi-packet frame costs exactly one strip write.
// Invalid packets (bad magic/version/bounds/short payload) are dropped
// silently (counted in g_streamDropped for /api/state visibility).
static void serviceStream(uint32_t now) {
  if (!streamUdpStarted) return;
  int packetLen;
  while ((packetLen = streamUdp.parsePacket()) > 0) {
    int len = streamUdp.read(streamBuf, sizeof(streamBuf));
    // oversized datagrams: read() capped at buffer size; header bounds
    // check below rejects anything that claims more pixels than fit.
    if (len < STREAM_HDR_LEN) { g_streamDropped++; continue; }
    if (streamBuf[0] != STREAM_MAGIC0 || streamBuf[1] != STREAM_MAGIC1) { g_streamDropped++; continue; }
    if (streamBuf[2] != STREAM_PROTO_VER) { g_streamDropped++; continue; }
    uint8_t flags = streamBuf[3];
    uint16_t offset = (uint16_t)streamBuf[4] | ((uint16_t)streamBuf[5] << 8); // LE
    uint16_t count  = (uint16_t)streamBuf[6] | ((uint16_t)streamBuf[7] << 8); // LE
    if ((uint32_t)offset + (uint32_t)count > NUM_LEDS) { g_streamDropped++; continue; }
    if (len < STREAM_HDR_LEN + 3 * (int)count) { g_streamDropped++; continue; }

    // valid packet: enter stream mode (remember what to fall back to)
    if (g_mode != MODE_STREAM) {
      g_streamPrevMode = g_mode;
      g_mode = MODE_STREAM;
      stateDirty = true;
      Serial.printf("stream: entered (fallback %s) from %s\n",
                    modeToString(g_streamPrevMode),
                    streamUdp.remoteIP().toString().c_str());
    }
    g_streamLastPacketMs = now;
    g_streamLastSender = streamUdp.remoteIP();
    g_streamPackets++;

    // copy RGB triplets into the frame buffer. Renderer sends RGB; CRGB is
    // RGB in memory, and FastLED's controller (addLeds<WS2812B,...,GRB>)
    // performs the strip's GRB reordering on output.
    const uint8_t *px = streamBuf + STREAM_HDR_LEN;
    CRGB *dst = leds + offset;
    for (uint16_t i = 0; i < count; i++) {
      dst[i].r = px[0];
      dst[i].g = px[1];
      dst[i].b = px[2];
      px += 3;
    }
    if (flags & STREAM_FLAG_SHOW) {
      g_streamShowPending = true;
      g_streamFrames++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("=== %s: breathe + API + UDP frame streaming v1 ===\n", NODE_NAME);
  Serial.printf("pin GPIO %d, %d px, brightness %d, cap %dmA\n",
                DATA_PIN, NUM_LEDS, BRIGHTNESS, POWER_BUDGET_MA);
  Serial.printf("stream: UDP :%d, protocol v1, timeout %d ms\n",
                STREAM_UDP_PORT, STREAM_TIMEOUT_MS);

  // 7-px NeoPixel jewel — standard WS2812B, GRB channel order.
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(g_brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, POWER_BUDGET_MA);
  FastLED.clear(true);

  // WiFi: fire and forget — never block the render loop
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);      // avoid flash wear from frequent reconnect attempts
  WiFi.setAutoReconnect(true); // let the IDF's own reconnect state machine help too
  WiFi.setSleep(false);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiAttempt = millis();
  Serial.printf("wifi: joining \"%s\" (non-blocking)\n", WIFI_SSID);

  lastFrameUs = micros();
}

void serviceWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaReady) {
      Serial.printf("wifi: connected, IP %s\n", WiFi.localIP().toString().c_str());
      ArduinoOTA.setHostname(OTA_HOSTNAME);
      ArduinoOTA.setPassword(OTA_PASSWORD);
      ArduinoOTA.begin(); // registers mDNS NODE_NAME.local
      startHttpServer();  // control API up once we have an IP
      if (!streamUdpStarted) {
        streamUdp.begin(STREAM_UDP_PORT);
        streamUdpStarted = true;
        Serial.printf("stream: UDP listener on :%d\n", STREAM_UDP_PORT);
      }
      otaReady = true;
      Serial.printf("ota: ready (%s.local, auth on)\n", OTA_HOSTNAME);
    }
    ArduinoOTA.handle();
    server.handleClient();
  } else {
    if (otaReady) {
      otaReady = false;
      Serial.println("wifi: lost, retrying");
      if (streamUdpStarted) { streamUdp.stop(); streamUdpStarted = false; }
      if (g_mode == MODE_STREAM) { g_mode = g_streamPrevMode; stateDirty = true; }
    }
    // Retry forever, every 10s, until connected -- covers boards that boot
    // before the router/AP is up: WiFi.setAutoReconnect() alone doesn't
    // reliably re-trigger a connect attempt from a never-connected state on
    // all core versions, so we also force an explicit disconnect+begin here.
    if (now - lastWifiAttempt > 10000) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastWifiAttempt = now;
    }
  }
}

// =====================================================================
// mode:"scene" — step every object's physics, then render: fill
// `background`, then composite each blob additively with a soft metaball
// falloff (1/(1+d*d*2.4)), over LINEAR distance so blobs never wrap past
// the ends of the dot's 7-px line.
// =====================================================================
void updateScene(float dt) {
  // 1) integrate position + wall bounce. Positions BEFORE this step are kept
  // so the collision pass below can do a swept (continuous) test instead of
  // a point-in-time overlap test.
  static float prevPos[MAX_SCENE_OBJECTS];
  for (int i = 0; i < g_sceneObjectCount; i++) {
    SceneObject &o = g_sceneObjects[i];
    prevPos[i] = o.pos;
    o.pos += o.vel * dt;
    if (o.bounce) {
      if (o.pos < 0.0f)      { o.pos = 0.0f; o.vel = -o.vel; g_wallBounceCount++; }
      else if (o.pos > 1.0f) { o.pos = 1.0f; o.vel = -o.vel; g_wallBounceCount++; }
    } else {
      o.pos = constrain(o.pos, 0.0f, 1.0f);
    }
  }

  // 2) object-to-object collisions: one pass over every pair per frame.
  // Swept (continuous) detection: a collision is registered when the pair
  // either (a) currently overlaps, or (b) CROSSED (sign of posA-posB
  // flipped since last frame), so no closing-speed can tunnel through the
  // contact window.
  for (int i = 0; i < g_sceneObjectCount; i++) {
    for (int j = i + 1; j < g_sceneObjectCount; j++) {
      SceneObject &a = g_sceneObjects[i];
      SceneObject &b = g_sceneObjects[j];

      float posDiff  = a.pos - b.pos;          // signed, this frame
      float prevDiff = prevPos[i] - prevPos[j]; // signed, previous frame
      float pxDist = fabsf(posDiff) * (float)(NUM_LEDS - 1);
      // radius is normalized (size_px / NUM_LEDS); convert back to pixels
      float minDistPx = (a.radius + b.radius) * (float)NUM_LEDS;

      bool crossed     = (prevDiff * posDiff) < 0.0f; // passed through
      bool overlapping = pxDist < minDistPx;
      if (!crossed && !overlapping) continue;

      // canonical ordering direction: which side each object was on BEFORE
      // this frame's motion. Objects are restored to that ordering.
      float dir;
      if (prevDiff > 0.0f)      dir =  1.0f;   // a was right of b
      else if (prevDiff < 0.0f) dir = -1.0f;   // a was left of b
      else                      dir = (posDiff >= 0.0f) ? 1.0f : -1.0f;

      // (1) elastic velocity swap (equal masses) — only on a genuine impact:
      // a crossing always is one; an overlap counts only while CLOSING, so a
      // lingering overlap cannot re-trigger every frame and stall the motion.
      float relVel = a.vel - b.vel;
      bool approaching = crossed || (posDiff == 0.0f) || (relVel * posDiff < 0.0f);
      if (approaching) {
        float tmp = a.vel;
        a.vel = b.vel;
        b.vel = tmp;
        g_collisionCount++;
      }

      // (2) positional separation along `dir`: signed gap must reach
      // minDistPx. When the pair crossed, the signed gap is NEGATIVE, so this
      // both un-crosses them and separates them in one step. Split half/half.
      float signedGapPx = dir * posDiff * (float)(NUM_LEDS - 1);
      float correctionPx = minDistPx - signedGapPx;
      if (correctionPx > 0.0f) {
        float halfNorm = (correctionPx * 0.5f) / (float)(NUM_LEDS - 1);
        a.pos = constrain(a.pos + dir * halfNorm, 0.0f, 1.0f);
        b.pos = constrain(b.pos - dir * halfNorm, 0.0f, 1.0f);
      }
    }
  }
}

void renderScene() {
  fill_solid(leds, NUM_LEDS, g_sceneBackground);
  for (int i = 0; i < g_sceneObjectCount; i++) {
    SceneObject &o = g_sceneObjects[i];
    for (int p = 0; p < NUM_LEDS; p++) {
      float x = (float)p / NUM_LEDS;
      // LINEAR distance, not wrapped: scene objects live on a linear line
      // with hard walls — pixel 0 and pixel NUM_LEDS-1 are NOT adjacent.
      float d = fabsf(x - o.pos) / o.radius;
      if (d > 1.6f) continue;                  // outside influence
      float f = 1.0f / (1.0f + d * d * 2.4f);  // soft metaball falloff
      CRGB add = o.color;
      add.nscale8_video((uint8_t)(constrain(f, 0.0f, 1.0f) * 255.0f));
      leds[p] += add;                          // additive compositing
    }
  }
}

// ---- style: breathe (boot default) — whole dot filled with one hue, hue
// rotates slowly around the wheel, brightness breathes smoothly in and out.
// No motion needed at 7 px — this reads clean and calm. No strobe. ----
void renderBreathe(uint32_t now) {
  float huePhase = fmodf((float)now / BREATHE_HUE_PERIOD_MS, 1.0f);   // 0..1
  uint8_t hue = (uint8_t)(huePhase * 255.0f);

  float briPhase = (now % BREATHE_PERIOD_MS) / (float)BREATHE_PERIOD_MS; // 0..1
  float s = 0.5f - 0.5f * cosf(briPhase * 2.0f * PI);                     // 0..1 smooth
  uint8_t val = BREATHE_MIN_VAL + (uint8_t)(s * (BREATHE_MAX_VAL - BREATHE_MIN_VAL));

  fill_solid(leds, NUM_LEDS, CHSV(hue, BREATHE_SAT, val));
}

// ---- style: pulse — whole dot filled with `color`, slow smooth breathing
// brightness oscillation. No motion, no strobe. ----
void renderPulse(uint32_t now) {
  float phase = (now % PULSE_PERIOD_MS) / (float)PULSE_PERIOD_MS; // 0..1
  float s = 0.5f - 0.5f * cosf(phase * 2.0f * PI);                // 0..1 smooth
  uint8_t scale = PULSE_MIN_SCALE + (uint8_t)(s * (PULSE_MAX_SCALE - PULSE_MIN_SCALE));
  CRGB c = g_solidColor;
  c.nscale8_video(scale);
  fill_solid(leds, NUM_LEDS, c);
}

// ---- style: chase — a single comet of `color` drifting smoothly around the
// 7-px dot with a fading trail, wrapping at the ends. No strobe. ----
void renderChase(float dt) {
  static float chasePos = 0.0f; // 0..1 normalized head position
  chasePos += CHASE_SPEED * dt;
  if (chasePos >= 1.0f) chasePos -= 1.0f;

  fadeToBlackBy(leds, NUM_LEDS, CHASE_TRAIL_FADE); // trail persistence

  float posF = chasePos * NUM_LEDS;
  int idx0 = ((int)posF) % NUM_LEDS;
  int idx1 = (idx0 + 1) % NUM_LEDS;
  float frac = posF - floorf(posF);

  CRGB head0 = g_solidColor; head0.nscale8_video((uint8_t)((1.0f - frac) * 255));
  CRGB head1 = g_solidColor; head1.nscale8_video((uint8_t)(frac * 255));
  leds[idx0] += head0; // additive: comet head anti-aliased across 2 pixels
  leds[idx1] += head1;
}

// ---- style: rainbow — continuous smooth hue rotation across all 7 pixels.
// `color` ignored. No strobe. ----
void renderRainbow(float dt) {
  static float rainbowOffset = 0.0f; // 0..255 rotating hue offset
  rainbowOffset += RAINBOW_SPEED * dt;
  if (rainbowOffset >= 256.0f) rainbowOffset -= 256.0f;

  for (int p = 0; p < NUM_LEDS; p++) {
    uint8_t hue = (uint8_t)((int)(rainbowOffset + (p * 255.0f / NUM_LEDS)) % 256);
    leds[p] = CHSV(hue, 255, 255);
  }
}

// ---- dispatch: one case per generative style. Adding a new style later is
// one new Style enum value + one new render function + one new case here. ----
void renderGenerative(float dt, uint32_t now) {
  switch (g_style) {
    case STYLE_PULSE:   renderPulse(now); break;
    case STYLE_CHASE:   renderChase(dt);  break;
    case STYLE_RAINBOW: renderRainbow(dt); break;
    case STYLE_BREATHE:
    default:
      renderBreathe(now);
      break;
  }
}

void loop() {
  uint32_t now = millis();
  uint32_t nowUs = micros();
  float dt = fminf(0.05f, (nowUs - lastFrameUs) / 1e6f);
  lastFrameUs = nowUs;

  // drain ALL pending stream packets first, every iteration — a valid
  // packet may switch g_mode to MODE_STREAM before rendering below.
  serviceStream(now);

  if (g_mode == MODE_STREAM) {
    // stream timeout: no valid packet for STREAM_TIMEOUT_MS -> fall back
    // to whatever mode was active before streaming began.
    if (now - g_streamLastPacketMs > STREAM_TIMEOUT_MS) {
      g_mode = g_streamPrevMode;
      g_streamShowPending = false;
      stateDirty = true;
      Serial.printf("stream: timeout, falling back to %s\n", modeToString(g_mode));
    } else {
      // latch only when a SHOW-flagged packet arrived: one strip write per
      // completed frame, never per packet.
      if (g_streamShowPending) {
        g_streamShowPending = false;
        FastLED.show();
        frames++;
      }
      serviceWifi(now);   // OTA + HTTP stay responsive while streaming
      serviceMqtt(now);
      if (now - lastReport >= 2000) {
        g_fps = (frames - framesAtLastReport) * 1000.0f / (float)(now - lastReport);
        framesAtLastReport = frames;
        Serial.printf("alive: %lu frames, mode stream, pkts %lu, drops %lu, from %s\n",
                      (unsigned long)frames, (unsigned long)g_streamPackets,
                      (unsigned long)g_streamDropped,
                      g_streamLastSender.toString().c_str());
        lastReport = now;
      }
      delay(1); // tight loop: keep UDP drain latency ~1ms for 30fps target
      return;
    }
  }

  if (g_mode == MODE_SOLID) {
    // whole dot one chosen color; global brightness handled by FastLED
    fill_solid(leds, NUM_LEDS, g_solidColor);
  } else if (g_mode == MODE_SCENE) {
    updateScene(dt);
    renderScene();
  } else {
    renderGenerative(dt, now);
  }
  FastLED.show();
  frames++;

  serviceWifi(now);   // also pumps HTTP server.handleClient() when connected
  serviceMqtt(now);   // optional, non-blocking; no-op when MQTT_HOST empty

  if (now - lastReport >= 2000) {
    g_fps = (frames - framesAtLastReport) * 1000.0f / (float)(now - lastReport);
    framesAtLastReport = frames;
    const char *ws = (WiFi.status() == WL_CONNECTED)
                       ? WiFi.localIP().toString().c_str() : "connecting";
    Serial.printf("alive: %lu frames, %.1f fps, mode %s, bri %d, wifi %s\n",
                  (unsigned long)frames, frames * 1000.0f / now,
                  modeToString(g_mode),
                  g_brightness, ws);
    lastReport = now;
  }
  delay(8); // ~40-60 fps ceiling, leaves airtime for WiFi/OTA
}
