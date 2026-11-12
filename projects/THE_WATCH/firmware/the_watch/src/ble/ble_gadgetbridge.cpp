#include "ble_gadgetbridge.h"
#include "../config.h"
#include "../ui/watch_state.h"

#include <NimBLEDevice.h>   // NimBLE-Arduino (h2zero) — light BLE stack
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

// --- Nordic UART Service UUIDs (standard) ----------------------------------
static const char *NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_RX      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // phone->watch (write)
static const char *NUS_TX      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // watch->phone (notify)

static NimBLEServer         *s_server = nullptr;
static NimBLECharacteristic *s_tx     = nullptr;

// ---------------------------------------------------------------------------
// Multi-central connection table (PROTOCOL_V2.md).
//
// The watch is now a peripheral to TWO centrals at once (phone + Mac). We keep a
// small table keyed by NimBLE connection handle. Each entry remembers which
// `src` the central declared via `hello` (defaulting to PHONE for back-compat)
// and its live negotiated ATT MTU (a single notify carries at most MTU-3 bytes;
// NimBLE silently truncates anything longer, so TX chunking is sized off it).
//
// NimBLE's default CONFIG_BT_NIMBLE_MAX_CONNECTIONS is 3 (nimconfig.h) — already
// >= 2 — so NO config bump is required for two simultaneous centrals.
// ---------------------------------------------------------------------------
#define BLE_MAX_CONNS 3
struct BleConn {
  bool     used;
  uint16_t handle;
  int      src;      // WS_SRC_PHONE / WS_SRC_MAC (defaults to PHONE until hello)
  uint16_t mtu;
  String   rxline;   // per-connection inbound line assembly (avoids interleaving
                     // two centrals' partial writes into one shared buffer)
};
static BleConn s_conns[BLE_MAX_CONNS];

static BleConn *conn_by_handle(uint16_t h) {
  for (auto &c : s_conns) if (c.used && c.handle == h) return &c;
  return nullptr;
}
static BleConn *conn_alloc(uint16_t h) {
  for (auto &c : s_conns) if (!c.used) {
    c.used = true; c.handle = h; c.src = WS_SRC_PHONE; c.mtu = 23; c.rxline = "";
    return &c;
  }
  return nullptr;
}
static void conn_free(uint16_t h) {
  for (auto &c : s_conns) if (c.used && c.handle == h) c.used = false;
}
static int conn_count() {
  int n = 0; for (auto &c : s_conns) if (c.used) n++; return n;
}
// First connection matching src, else (if none) the first connection present, so
// notifications/camera still reach the only central when it's the Mac. Returns
// the handle or BLE_HS_CONN_HANDLE_NONE if nothing is connected.
static uint16_t handle_for_src(int src) {
  for (auto &c : s_conns) if (c.used && c.src == src) return c.handle;
  for (auto &c : s_conns) if (c.used) return c.handle;      // fallback: any
  return BLE_HS_CONN_HANDLE_NONE;
}

// ---------------------------------------------------------------------------
// Per-source media state + active-source arbitration (PROTOCOL_V2.md).
//
// Every central's media is stored separately; the arbitration below decides
// which one the play screen follows and writes that winner's snapshot into the
// shared g_ws.mu_* fields (so the existing UI just reads g_ws unchanged).
// ---------------------------------------------------------------------------
struct SrcMedia {
  bool     valid;
  char     track[WS_STR];
  char     artist[WS_STR];
  char     album[WS_STR];
  bool     ab;
  bool     playing;
  uint32_t last_update;  // millis() of the most recent musicinfo OR musicstate
  uint32_t last_info;    // millis() of the most recent musicinfo (metadata)
};
static SrcMedia s_media[WS_SRC_COUNT];

// Decide the active source and publish its snapshot to g_ws (must be called with
// the ws lock NOT held; it takes the lock itself). Rule:
//   1. exactly one source playing -> it is active
//   2. several playing            -> the most-recently-updated one
//   3. none playing               -> the most-recently-updated musicinfo
static void media_arbitrate() {
  int active = WS_SRC_NONE;
  int playingCount = 0, onlyPlaying = WS_SRC_NONE;
  int bestPlay = WS_SRC_NONE; uint32_t bestPlayT = 0;
  for (int i = 0; i < WS_SRC_COUNT; i++) {
    if (!s_media[i].valid) continue;
    if (s_media[i].playing) {
      playingCount++; onlyPlaying = i;
      if (bestPlay < 0 || (int32_t)(s_media[i].last_update - bestPlayT) >= 0) {
        bestPlayT = s_media[i].last_update; bestPlay = i;
      }
    }
  }
  if (playingCount == 1)      active = onlyPlaying;
  else if (playingCount > 1)  active = bestPlay;
  else {                       // none playing: most-recently-updated info wins
    uint32_t bestT = 0; int bi = WS_SRC_NONE;
    for (int i = 0; i < WS_SRC_COUNT; i++) {
      if (!s_media[i].valid) continue;
      if (bi < 0 || (int32_t)(s_media[i].last_info - bestT) >= 0) {
        bestT = s_media[i].last_info; bi = i;
      }
    }
    active = bi;
  }

  ws_lock();
  g_ws.mu_active_src = active;
  if (active >= 0) {
    SrcMedia &m = s_media[active];
    strncpy(g_ws.mu_artist, m.artist, WS_STR - 1); g_ws.mu_artist[WS_STR-1]=0;
    strncpy(g_ws.mu_album,  m.album,  WS_STR - 1); g_ws.mu_album[WS_STR-1]=0;
    strncpy(g_ws.mu_track,  m.track,  WS_STR - 1); g_ws.mu_track[WS_STR-1]=0;
    g_ws.mu_ab      = m.ab;
    g_ws.mu_playing = m.playing;
    g_ws.mu_valid   = true;
  }
  ws_mark_dirty(WS_DIRTY_MUSIC);
  ws_unlock();
  Serial.printf("MEDIA arbitrate: active=%d (phone.valid=%d play=%d, mac.valid=%d play=%d)\n",
                active, s_media[WS_SRC_PHONE].valid, s_media[WS_SRC_PHONE].playing,
                s_media[WS_SRC_MAC].valid, s_media[WS_SRC_MAC].playing);
}

// ---------------------------------------------------------------------------
// Outbound helper: send one raw JSON line to a SPECIFIC central (by handle).
// Targeted delivery (PROTOCOL_V2.md): transport/commands go only to the active
// source; notifications/camera go to the phone. handle==NONE => nothing sent.
// ---------------------------------------------------------------------------
static void tx_line_to(uint16_t handle, const String &json) {
  // Wire format (verified against Gadgetbridge master, BangleJSDeviceSupport):
  //
  //   receivedLine += packetStr;
  //   while (receivedLine.contains("\n")) {
  //       int p = receivedLine.indexOf("\n");
  //       String line = receivedLine.substring(0,(p>0) ? (p-1) : 0);   // <-- !!
  //       ...
  //
  // Gadgetbridge UNCONDITIONALLY drops the character before '\n' -- it assumes
  // every line from the watch ends in "\r\n", because a real Bangle.js sends
  // lines with Espruino's Bluetooth.println(), which appends "\r\n".
  //
  // Sending "...}\n" (as this firmware previously did) therefore made
  // Gadgetbridge delete the closing '}' before JSON-parsing, producing the
  // persistent 'Malformed JSON ... character 23/24' toast on every 24-char
  // command. Lines MUST end with "\r\n".
  String out = json + "\r\n";
  const size_t n = out.length();               // includes the trailing '\n'
  const uint8_t *bytes = (const uint8_t *)out.c_str();

  // Log the EXACT outbound bytes + length so framing is verifiable over Serial
  // even when disconnected. Newline is rendered as \n so it is visible.
  Serial.print("BLE TX (len=");
  Serial.print(n);
  Serial.print("): ");
  for (size_t i = 0; i < n; i++) {
    uint8_t c = bytes[i];
    if (c == '\n')      Serial.print("\\n");
    else if (c < 0x20)  { Serial.print("\\x"); if (c < 16) Serial.print('0'); Serial.print(c, HEX); }
    else                Serial.print((char)c);
  }
  Serial.println();

  BleConn *c = conn_by_handle(handle);
  if (handle == BLE_HS_CONN_HANDLE_NONE || !c || !s_tx) {
    Serial.println("BLE TX (dropped: no target connection)"); return;
  }
  uint16_t mtu = c->mtu;

  // Chunk sized off the LIVE negotiated MTU so short commands (music/find, ~24
  // bytes) go out in exactly ONE ordered notify whenever the central granted a
  // large MTU -- the most reliable path. NimBLE truncates a notify longer than
  // (MTU-3), so never exceed it. Floor at 20 (default-MTU case). Notifications
  // on a connection are delivered in-order over L2CAP; the small delay only
  // paces multi-chunk lines so we never overrun the controller TX buffers.
  size_t CH = (mtu > 23) ? (size_t)(mtu - 3) : 20;
  size_t chunks = (n + CH - 1) / CH;
  Serial.printf("BLE TX notify -> handle %u: %u byte(s) in %u chunk(s) (MTU=%u, CH=%u)\n",
                (unsigned)handle, (unsigned)n, (unsigned)chunks, (unsigned)mtu, (unsigned)CH);
  for (size_t i = 0; i < n; i += CH) {
    size_t len = (n - i < CH) ? (n - i) : CH;         // last chunk = remainder
    bool ok = s_tx->notify(bytes + i, len, handle);   // targeted notify
    Serial.printf("  chunk @%u len=%u -> %s\n",
                  (unsigned)i, (unsigned)len, ok ? "sent" : "FAILED");
    if (n - i > CH) delay(6);                          // pace multi-chunk lines
  }
}

// Convenience: send to the ACTIVE media source (transport/commands).
static void tx_line_active(const String &json) {
  int src;
  ws_lock(); src = g_ws.mu_active_src; ws_unlock();
  uint16_t h = (src >= 0) ? handle_for_src(src) : handle_for_src(WS_SRC_PHONE);
  tx_line_to(h, json);
}
// Convenience: send to the PHONE (notifications/camera/find), or the only central.
static void tx_line_phone(const String &json) {
  tx_line_to(handle_for_src(WS_SRC_PHONE), json);
}

// ---------------------------------------------------------------------------
// Inbound: parse one logical line from the phone.
//
// Lines look like:  GB({"t":"notify",...})    (Gadgetbridge JSON), or raw
// Espruino REPL JS like  setTime(1600000000);E.setTimeZone(1);  for time.
// We handle both: extract JSON from GB(...) and, separately, sniff setTime().
// ---------------------------------------------------------------------------
static void handle_settime(const String &line) {
  int p = line.indexOf("setTime(");
  if (p < 0) return;
  p += 8;
  // Gadgetbridge sends: setTime(<UTC epoch seconds>);E.setTimeZone(<hours>);
  // The epoch is UTC. E.setTimeZone gives the LOCAL offset in HOURS (may be
  // negative and fractional, e.g. -7 for Los Angeles PDT, 5.5 for IST).
  long epoch = line.substring(p).toInt();  // stops at first non-digit
  if (epoch < 1000000000L) return;         // sanity (post-2001)
  int tzmin = 0;
  int tzp = line.indexOf("setTimeZone(");
  if (tzp >= 0) tzmin = (int)lroundf(line.substring(tzp + 12).toFloat() * 60.0f);

  Serial.print("BLE RX setTime: epoch="); Serial.print(epoch);
  Serial.print(" tz(min)="); Serial.print(tzmin);

  // Compute local time for a verifiable 12-hour AM/PM log.
  time_t local = (time_t)(epoch + (long)tzmin * 60);
  struct tm tmv; gmtime_r(&local, &tmv);
  int h12 = tmv.tm_hour % 12; if (h12 == 0) h12 = 12;
  Serial.printf("  -> local %02d:%02d %s\n", h12, tmv.tm_min,
                tmv.tm_hour < 12 ? "AM" : "PM");

  ws_lock();
  g_ws.epoch = (uint32_t)epoch;   // stored as UTC; UI adds tz_offset_min
  g_ws.tz_offset_min = tzmin;
  g_ws.time_valid = true;
  ws_mark_dirty(WS_DIRTY_TIME);
  ws_unlock();
}

// Resolve the src for a media/battery message: explicit "src" field wins, else
// fall back to whatever this connection declared via hello (default PHONE).
static int src_from_msg(JsonDocument &doc, uint16_t handle) {
  const char *s = doc["src"] | "";
  if (!strcmp(s, "mac"))   return WS_SRC_MAC;
  if (!strcmp(s, "phone")) return WS_SRC_PHONE;
  BleConn *c = conn_by_handle(handle);
  return c ? c->src : WS_SRC_PHONE;
}

static void handle_gb_json(const String &json, uint16_t handle) {
  // Bigger buffer: notification bodies (and weather) routinely blow past 512
  // bytes; deserialize would fail with NoMemory and the message would be
  // silently dropped — a real cause of "notifications never appear".
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.print("BLE RX JSON parse FAIL ("); Serial.print(err.c_str());
    Serial.print("): "); Serial.println(json);
    return;
  }
  const char *t = doc["t"] | "";
  Serial.print("BLE RX type='"); Serial.print(t); Serial.print("'  "); Serial.println(json);

  if (!strcmp(t, "hello")) {
    // {"t":"hello","src":"mac"|"phone","name":..} — map this connection -> src.
    const char *s = doc["src"] | "phone";
    int src = !strcmp(s, "mac") ? WS_SRC_MAC : WS_SRC_PHONE;
    BleConn *c = conn_by_handle(handle);
    if (c) c->src = src;
    Serial.printf("BLE RX hello: handle %u -> src=%s name='%s'\n",
                  (unsigned)handle, src == WS_SRC_MAC ? "mac" : "phone",
                  (const char *)(doc["name"] | ""));
  } else if (!strcmp(t, "devBattery")) {
    // {"t":"devBattery","src":..,"n":<0..100>,"chg":<bool>} — per-device battery.
    int src = src_from_msg(doc, handle);
    int pct = doc["n"] | -1;
    bool chg = doc["chg"] | false;
    ws_lock();
    if (src == WS_SRC_MAC) {
      g_ws.mac_batt_pct = pct; g_ws.mac_batt_chg = chg;
      g_ws.mac_batt_valid = (pct >= 0);
      ws_mark_dirty(WS_DIRTY_MACBAT);
    } else {
      g_ws.phone_batt_pct = pct; g_ws.phone_batt_chg = chg;
      g_ws.phone_batt_valid = (pct >= 0);
      ws_mark_dirty(WS_DIRTY_PHONEBAT);
    }
    ws_unlock();
    Serial.printf("BLE RX devBattery: src=%d %d%% chg=%d\n", src, pct, (int)chg);
  } else if (!strcmp(t, "permReq")) {
    // {"t":"permReq","id":..,"title":..,"body":..,"origin":..} — remote approval.
    // ENQUEUE it (+ the conn handle) so several parallel requests are shown one
    // at a time and each permResp routes back to the SAME central. Dedupe by
    // (handle,id) so a resend doesn't double-queue. Always shows (not gated).
    uint32_t rid = doc["id"] | 0;
    ws_lock();
    int slot = -1;
    for (uint8_t i = 0; i < g_ws.perm_count; i++)
      if (g_ws.perm_id[i] == rid && g_ws.perm_handle[i] == handle) { slot = i; break; }
    if (slot < 0 && g_ws.perm_count < WS_PERM_QMAX) slot = g_ws.perm_count++;
    if (slot >= 0) {
      g_ws.perm_id[slot]     = rid;
      g_ws.perm_handle[slot] = handle;
      strncpy(g_ws.perm_title[slot],  doc["title"]  | "Approve?", WS_STR - 1);  g_ws.perm_title[slot][WS_STR-1]=0;
      strncpy(g_ws.perm_body[slot],   doc["body"]   | "",         WS_BODY - 1); g_ws.perm_body[slot][WS_BODY-1]=0;
      strncpy(g_ws.perm_origin[slot], doc["origin"] | "",         WS_STR - 1);  g_ws.perm_origin[slot][WS_STR-1]=0;
      ws_mark_dirty(WS_DIRTY_PERM);
    }
    ws_unlock();
    Serial.printf("BLE RX permReq: id=%u handle=%u origin='%s' (queue=%d)\n",
                  (unsigned)rid, (unsigned)handle,
                  (const char *)(doc["origin"] | ""), (int)g_ws.perm_count);
  } else if (!strcmp(t, "permCancel")) {
    // {"t":"permCancel","id":..} — the requester resolved this elsewhere (e.g.
    // approved on the Mac); drop it from the queue so its card disappears.
    uint32_t rid = doc["id"] | 0;
    ws_lock();
    for (uint8_t i = 0; i < g_ws.perm_count; i++) {
      if (g_ws.perm_id[i] == rid && g_ws.perm_handle[i] == handle) {
        for (uint8_t j = i; j + 1 < g_ws.perm_count; j++) {
          g_ws.perm_id[j] = g_ws.perm_id[j+1];
          g_ws.perm_handle[j] = g_ws.perm_handle[j+1];
          memcpy(g_ws.perm_title[j],  g_ws.perm_title[j+1],  WS_STR);
          memcpy(g_ws.perm_body[j],   g_ws.perm_body[j+1],   WS_BODY);
          memcpy(g_ws.perm_origin[j], g_ws.perm_origin[j+1], WS_STR);
        }
        g_ws.perm_count--;
        ws_mark_dirty(WS_DIRTY_PERM);
        break;
      }
    }
    ws_unlock();
    Serial.printf("BLE RX permCancel: id=%u (queue=%d)\n", (unsigned)rid, (int)g_ws.perm_count);
  } else if (!strcmp(t, "notify")) {
    uint32_t id = doc["id"] | 0;
    ws_lock();
    ws_push_notification(id,
                         doc["src"]   | doc["app"] | "",
                         doc["title"] | doc["sender"] | "",
                         doc["body"]  | "");
    // NOTIF repaints the list; NOTIF_POPUP tells the UI a NEW one just arrived
    // so it can raise the full-screen overlay (only on real arrivals, never on
    // notify- removals or local dismissals).
    ws_mark_dirty(WS_DIRTY_NOTIF | WS_DIRTY_NOTIF_POPUP);
    ws_unlock();
  } else if (!strcmp(t, "notify-")) {           // dismissed on phone
    uint32_t id = doc["id"] | 0;
    ws_lock(); ws_remove_notification(id); ws_mark_dirty(WS_DIRTY_NOTIF); ws_unlock();
  } else if (!strcmp(t, "musicinfo")) {
    // Per-source metadata. Stored per src; arbitration picks the active source.
    int src = src_from_msg(doc, handle);
    SrcMedia &m = s_media[src];
    strncpy(m.artist, doc["artist"] | "", WS_STR - 1); m.artist[WS_STR-1]=0;
    strncpy(m.album,  doc["album"]  | "", WS_STR - 1); m.album[WS_STR-1]=0;
    strncpy(m.track,  doc["track"]  | "", WS_STR - 1); m.track[WS_STR-1]=0;
    // "ab": audiobook/podcast source active (Audible etc.). When true the Music
    // page swaps its side transport buttons to ~30s rewind/forward.
    m.ab = doc["ab"] | false;
    m.valid = true;
    m.last_info = m.last_update = millis();
    media_arbitrate();     // re-decide + publish the active source to g_ws
  } else if (!strcmp(t, "musicstate")) {
    int src = src_from_msg(doc, handle);
    const char *st = doc["state"] | "";
    SrcMedia &m = s_media[src];
    m.playing = !strcmp(st, "play");
    m.valid = true;
    m.last_update = millis();
    media_arbitrate();
  } else if (!strcmp(t, "weather")) {
    // Gadgetbridge sends temperature in KELVIN for the Bangle.js protocol
    // (the Bangle weather app subtracts 273.15). Some rare builds send Celsius.
    // Heuristic: anything above 100 must be Kelvin (100 C is not a weather
    // reading), so convert; otherwise assume it is already Celsius.
    float traw = doc["temp"] | 0.0f;
    float celsius = (traw > 100.0f) ? (traw - 273.15f) : traw;

    // Weather is PHONE-PREFERRED with MAC FALLBACK. Resolve which central sent
    // this (handle->src via hello, default PHONE). Phone weather always wins and
    // stamps wx_phone_ms; Mac weather is only applied when the phone hasn't sent
    // any weather within WEATHER_PHONE_TTL_MS (so we fall back instead of going
    // blank), and is otherwise ignored so it can't overwrite fresh phone data.
    static const uint32_t WEATHER_PHONE_TTL_MS = 25UL * 60UL * 1000UL;  // 25 min
    int wx_src = src_from_msg(doc, handle);

    ws_lock();
    bool phone_fresh = (g_ws.wx_phone_ms != 0) &&
                       ((uint32_t)(millis() - g_ws.wx_phone_ms) <= WEATHER_PHONE_TTL_MS);
    bool apply = (wx_src == WS_SRC_PHONE) || !phone_fresh;
    if (apply) {
      g_ws.wx_temp_c   = (int)lroundf(celsius);   // model stays in Celsius
      g_ws.wx_humidity = doc["hum"]  | 0;
      strncpy(g_ws.wx_text, doc["txt"] | "", WS_STR - 1);
      g_ws.wx_wind     = doc["wind"] | 0;
      strncpy(g_ws.wx_loc, doc["loc"] | "", WS_STR - 1);
      g_ws.wx_aqi      = doc["aqi"] | -1;   // US AQI from the app (-1 = n/a)
      g_ws.wx_valid = true;
      g_ws.wx_src   = (uint8_t)wx_src;
      if (wx_src == WS_SRC_PHONE) g_ws.wx_phone_ms = millis();
      ws_mark_dirty(WS_DIRTY_WEATHER);
    }
    ws_unlock();
    Serial.printf("BLE RX weather: src=%s raw=%.1f -> %dC (%dF) hum=%d [%s]\n",
                  wx_src == WS_SRC_MAC ? "mac" : "phone", traw, (int)lroundf(celsius),
                  (int)lroundf(celsius * 9.0f / 5.0f + 32.0f), (int)(doc["hum"] | 0),
                  apply ? "applied" : "ignored (phone fresh)");
  } else if (!strcmp(t, "calendar")) {
    // {"t":"calendar","title":<str>,"time":<utcEpochSeconds>,"loc":<str>}
    ws_lock();
    strncpy(g_ws.cal_title, doc["title"] | "", WS_STR - 1);
    g_ws.cal_title[WS_STR - 1] = 0;
    g_ws.cal_time = (uint32_t)(doc["time"] | 0);
    strncpy(g_ws.cal_loc, doc["loc"] | "", WS_STR - 1);
    g_ws.cal_loc[WS_STR - 1] = 0;
    g_ws.cal_valid = true;
    ws_mark_dirty(WS_DIRTY_CAL);
    ws_unlock();
    Serial.printf("BLE RX calendar: '%s' @%u loc='%s'\n",
                  (const char *)(doc["title"] | ""), (unsigned)(doc["time"] | 0),
                  (const char *)(doc["loc"] | ""));
  } else if (!strcmp(t, "phoneBattery")) {
    // {"t":"phoneBattery","n":<pct 0..100>,"chg":<bool>}
    ws_lock();
    g_ws.phone_batt_pct = doc["n"] | -1;
    g_ws.phone_batt_chg = doc["chg"] | false;
    g_ws.phone_batt_valid = (g_ws.phone_batt_pct >= 0);
    ws_mark_dirty(WS_DIRTY_PHONEBAT);
    ws_unlock();
    Serial.printf("BLE RX phoneBattery: %d%% chg=%d\n",
                  g_ws.phone_batt_pct, (int)g_ws.phone_batt_chg);
  } else if (!strcmp(t, "cameraState")) {
    // {"t":"cameraState","open":<bool>,"rec":<bool>}
    // Optional phone->watch status so the Camera page reflects the real app.
    // The watch has no mode toggle; "rec" just drives the small REC indicator.
    ws_lock();
    g_ws.cam_open = doc["open"] | false;
    g_ws.cam_recording = doc["rec"] | false;
    ws_mark_dirty(WS_DIRTY_CAMERA);
    ws_unlock();
    Serial.printf("BLE RX cameraState: open=%d rec=%d\n",
                  (int)g_ws.cam_open, (int)g_ws.cam_recording);
  } else if (!strcmp(t, "sleepState")) {
    // {"t":"sleepState","active":<bool>,"remaining":<seconds>}
    // Optional phone->watch status for the audiobook sleep timer. Drives the
    // full-width sleep button/countdown on the Music page.
    ws_lock();
    g_ws.sleep_active = doc["active"] | false;
    g_ws.sleep_remaining = doc["remaining"] | 0;
    g_ws.sleep_valid = true;
    ws_mark_dirty(WS_DIRTY_SLEEP);
    ws_unlock();
    Serial.printf("BLE RX sleepState: active=%d remaining=%ds\n",
                  (int)g_ws.sleep_active, g_ws.sleep_remaining);
  } else if (!strcmp(t, "find")) {
    // {"t":"find","n":true} -> phone asked watch to buzz. TODO: buzz motor.
  }
  // Other message types (alarm, call, is_gps_active, http, …) are ignored for
  // this first draft — add cases here as needed.
}

static void handle_line(String line, uint16_t handle) {
  // Strip Espruino REPL control byte(s) like 0x10 (DLE) that prefix GB(...).
  while (line.length() && (uint8_t)line[0] <= 0x20 && line[0] != '{') line.remove(0, 1);
  line.trim();
  if (line.length() == 0) return;

  Serial.print("BLE RX raw <- "); Serial.println(line);

  handle_settime(line);  // harmless if no setTime present

  int gb = line.indexOf("GB(");
  if (gb >= 0) {
    int open = line.indexOf('{', gb);
    int close = line.lastIndexOf('}');
    if (open >= 0 && close > open) handle_gb_json(line.substring(open, close + 1), handle);
  } else if (line[0] == '{') {
    handle_gb_json(line, handle);
  }
}

// ---------------------------------------------------------------------------
// GATT / connection callbacks
// ---------------------------------------------------------------------------
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &connInfo) override {
    uint16_t handle = connInfo.getConnHandle();
    BleConn *bc = conn_by_handle(handle);
    if (!bc) bc = conn_alloc(handle);   // write before onConnect table entry: adopt
    if (!bc) return;                    // table full — drop
    std::string v = c->getValue();
    for (char ch : v) {
      if (ch == '\n') { handle_line(bc->rxline, handle); bc->rxline = ""; }
      // Cap matches the 2048-byte parse buffer. 800 was too small: a single
      // notification with a long body (which Gadgetbridge sends as ONE line)
      // could exceed it and get wiped mid-line, so it never appeared.
      else if (ch != '\r') { bc->rxline += ch; if (bc->rxline.length() > 2000) bc->rxline = ""; }
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &connInfo) override {
    uint16_t handle = connInfo.getConnHandle();
    if (!conn_by_handle(handle)) conn_alloc(handle);   // register (unless adopted)
    // CRITICAL for multi-central: keep advertising after the FIRST central
    // connects so the SECOND (phone + Mac) can still find and connect to us.
    // NimBLE stops advertising on connect by default.
    NimBLEDevice::startAdvertising();
    // On connect a central replays its existing notifications (backfill burst).
    // Suppress full-screen popups for a few seconds so they only land in the list,
    // not as a wall of overlays. Live notifications after this window pop normally.
    ws_lock();
    g_ws.ble_connected = true;
    g_ws.notif_suppress_until = millis() + 8000;
    ws_mark_dirty(WS_DIRTY_CONN);
    ws_unlock();
    Serial.printf("BLE connect: handle %u (%d total)\n", (unsigned)handle, conn_count());
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &connInfo, int) override {
    uint16_t handle = connInfo.getConnHandle();
    conn_free(handle);
    bool any = (conn_count() > 0);
    ws_lock();
    g_ws.ble_connected = any;
    ws_mark_dirty(WS_DIRTY_CONN);
    ws_unlock();
    NimBLEDevice::startAdvertising();  // stay reconnectable
    Serial.printf("BLE disconnect: handle %u (%d remain)\n", (unsigned)handle, conn_count());
  }
  void onMTUChange(uint16_t mtu, NimBLEConnInfo &connInfo) override {
    BleConn *c = conn_by_handle(connInfo.getConnHandle());
    if (c) c->mtu = mtu;               // per-connection MTU for TX chunk sizing
    Serial.printf("BLE MTU negotiated: handle %u -> %u\n",
                  (unsigned)connInfo.getConnHandle(), (unsigned)mtu);
  }
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool ble_init() {
  NimBLEDevice::init(WATCH_BLE_NAME);
  NimBLEDevice::setMTU(247);
  // Link RELIABILITY beats the tiny heat saving from lowering radio power: -9
  // dBm caused flaky/dropped notifies (music/find-phone intermittently failed).
  // +3 dBm is a solid, reliable level for a phone in a pocket.
  NimBLEDevice::setPower(3);

  s_server = NimBLEDevice::createServer();
  s_server->setCallbacks(new ServerCallbacks());

  NimBLEService *svc = s_server->createService(NUS_SERVICE);
  s_tx = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic *rx = svc->createCharacteristic(
      NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());
  svc->start();

  // Advertising layout (NimBLE 2.x, 31-byte payload limit):
  //   - The 128-bit Nordic UART Service UUID (18 bytes) + flags fills the main
  //     advertisement packet; there is no room left for the device name there.
  //   - So the name goes into the SCAN RESPONSE packet. enableScanResponse(true)
  //     MUST be called BEFORE setName() — setName() only routes the name to the
  //     scan-response data if scan response is already enabled, otherwise it
  //     tries (and fails, silently) to append it to the overflowing adv packet.
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->enableScanResponse(true);
  adv->addServiceUUID(NUS_SERVICE);
  adv->setName(WATCH_BLE_NAME);   // name MUST start "Bangle.js" for Gadgetbridge
  adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);  // connectable peripheral

  bool ok = adv->start();         // 0 = advertise forever
  if (!ok) {
    Serial.println("BLE ERROR: advertising failed to start");
  }
  return ok;
}

void ble_loop() { /* NimBLE handles its own task; nothing needed here yet */ }

bool ble_is_connected() { return conn_count() > 0; }

void ble_music_control(const char *n) {
  // Transport -> the ACTIVE media source (targeted, PROTOCOL_V2.md). Existing
  // callers (incl. the watch-local sleep-timer auto-pause) need no change: this
  // resolves the active source internally.
  StaticJsonDocument<64> d;
  d["t"] = "music";
  d["n"] = n;
  String s; serializeJson(d, s);
  tx_line_active(s);
}

void ble_notify_action(uint32_t id, const char *action, const char *msg) {
  // Notifications -> the PHONE (or the only central present).
  StaticJsonDocument<192> d;
  d["t"] = "notify";
  d["id"] = id;
  d["n"] = action;             // "DISMISS" or "REPLY"
  if (msg && *msg) d["msg"] = msg;
  String s; serializeJson(d, s);
  tx_line_phone(s);
}

void ble_find_phone(bool on) {
  StaticJsonDocument<48> d;
  d["t"] = "findPhone";
  d["n"] = on;
  String s; serializeJson(d, s);
  tx_line_phone(s);
}

void ble_camera(const char *n) {
  // {"t":"camera","n":"open"|"capture"} -> the PHONE (camera lives on the phone).
  StaticJsonDocument<64> d;
  d["t"] = "camera";
  d["n"] = n;
  String s; serializeJson(d, s);
  tx_line_phone(s);
}

void ble_sleep_timer(int minutes) {
  // {"t":"sleeptimer","n":<minutes>} -> the ACTIVE source (audiobook-related).
  StaticJsonDocument<64> d;
  d["t"] = "sleeptimer";
  d["n"] = minutes;
  String s; serializeJson(d, s);
  tx_line_active(s);
}

void ble_perm_response(uint16_t handle, uint32_t id, bool approve, const char *scope) {
  // {"t":"permResp","id":..,"approve":..,"scope":"once"|"session"|"always"} back
  // to the SPECIFIC central that sent the permReq (PROTOCOL_V2.md).
  StaticJsonDocument<96> d;
  d["t"] = "permResp";
  d["id"] = id;
  d["approve"] = approve;
  d["scope"] = (scope && *scope) ? scope : "once";
  String s; serializeJson(d, s);
  tx_line_to(handle, s);
}
