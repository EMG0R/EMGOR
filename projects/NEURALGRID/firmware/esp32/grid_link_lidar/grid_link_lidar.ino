// neuralGrid — grid_link_lidar
// FORK of grid_link.ino (kept byte-for-byte compatible on the wire) with four
// VL53L1X time-of-flight lidar sensors added on the SAME I2C bus as the
// NeoTrellis grid (Wire, SDA=GPIO21 / SCL=GPIO22).
//
// The ESP32 remains a DUMB I/O bridge between the 8x8 NeoTrellis grid,
// 4 softpots, 4 lidars and the Raspberry Pi. NO musical logic, NO touch
// detection, NO local animation lives here — this firmware only moves bytes:
// raw key edges + raw ADC + raw ranges up, LED instructions down. All
// intelligence is on the Pi. Protocol contract: ../../../docs/PROTOCOL.md
// (this fork adds upstream/downstream types — see README.md here).
//
// Board: any ESP32 dev board (e.g. "ESP32 Dev Module")
// I2C (Wire): SDA = GPIO21, SCL = GPIO22, 400kHz — shared bus:
//             NeoTrellis 0x2E..0x31  +  4x VL53L1X 0x2A..0x2D
//   NOTE: grid_link ran this bus at 800kHz. The VL53L1X I2C interface is
//   rated to 400kHz MAX (ST datasheet), so the shared bus runs at 400kHz
//   here. If grid LED refresh visibly stutters, the fallback is moving the
//   lidars to their own Wire1 bus on GPIO25/26 — see README.md for the
//   exact two-line change.
// XSHUT: N=GPIO4, S=GPIO13, E=GPIO14, W=GPIO27 (address-assignment sequencing)
// Softpots: direct ADC1 pins, GPIO32/33/34/35 (see grid_link README for wiring)
// FSRs (fw6): direct ADC pins, GPIO36 (VP), GPIO39 (VN), GPIO15, GPIO12 —
//   see README.md "FSR wiring" for the divider convention and pin rationale
// Serial: USB CDC, 115200 (framing/CRC per PROTOCOL.md; CDC ignores actual baud)
//
// Lidar design constraints honored here:
//  * XSHUT ordering: all four held LOW at boot, then brought up ONE AT A TIME;
//    each sensor is initialized at the default address 0x29 and immediately
//    moved to its unique address before the next sensor is released. A sensor
//    that fails init has its XSHUT dropped LOW again so it can never squat on
//    0x29 and poison the next sensor's bring-up.
//  * A dead/absent sensor NEVER hangs boot or the main loop: it is marked
//    dead, reported as 0xFFFF, and skipped forever after.
//  * Reads are non-blocking: a sensor is only read when dataReady() says a
//    measurement is waiting; the loop never spin-waits on ranging.
//
// ── fw5 latency work (see README.md "Latency work (fw5)" for full detail) ──
//  Two measured root causes fixed here, both in the stock Adafruit NeoTrellis
//  stack, not our own code:
//   1. Adafruit_MultiTrellis::read() calls getKeypadCount() (which ALREADY
//      sleeps 500us internally, inside Adafruit_seesaw::read(), for the
//      seesaw's response-prep time) and then adds ANOTHER, fully redundant
//      delayMicroseconds(500) on top, per board, EVERY loop — 2ms of pure
//      waste every single loop no matter what. pollTrellisKeys() below
//      replicates the library's logic without the second delay.
//   2. seesaw_NeoPixel::setPixelColor() does an I2C write PER PIXEL (not a
//      local-buffer-only write) and Adafruit_MultiTrellis::show() always
//      shows all 4 boards. LEDS_FULL therefore cost up to 64 I2C writes +
//      4 show()s + 4x ~300us NeoPixel latch waits, every frame, even for
//      pixels that didn't change. Fixed with a shadow framebuffer (only
//      changed pixels get setPixelColor'd) + per-board dirty tracking (only
//      touched boards get show()'d).
//  Plus: non-blocking key scan is now first in loop() (never behind LED/
//  lidar work), LED frame application is coalesced (only the latest queued
//  LEDS_FULL/PARTIAL per loop is applied+shown+ACKed), a new upstream ACK
//  (0x15) times receive-complete -> show()-complete for latency measurement,
//  a new upstream TIMING frame (0x16) reports per-phase loop timing, and an
//  optional Pi-gated LOCAL_ECHO (0x24, default OFF) gives immediate physical
//  key->LED feedback without waiting for the round trip.
//
// ── fw6: 4x FSR analog inputs added (channels 4-7) ──────────────────────
//  Four force-sensitive resistors added as raw ADC channels, same voltage-
//  divider convention as the softpots (leg -> 3.3V, other leg -> ADC pin +
//  10k to GND). N_ANALOG goes 4 -> 8; PROTOCOL.md already allows this
//  ("Adding FSR/piezo later only increases n; the contract does not
//  change"). Channel order is backwards compatible: [0..3] unchanged
//  softpot N/S/E/W, [4..7] = FSR 0..3. Only two ADC1 pins remained free
//  (GPIO36/39 — softpots already claimed 32-35); the other two FSRs use
//  ADC2 pins (GPIO15, GPIO12), which is safe here specifically because
//  this firmware never enables WiFi (ADC2 and the WiFi radio share
//  hardware and normally conflict). See README.md "FSR wiring" for the
//  pin table, the GPIO2-vs-GPIO12 decision, and wiring instructions.
//
// ── fw7: idle-LED flicker investigation + lidar self-heal throttling ───
//  2026-09-08: reported symptom was "the grid flickers even at idle, and a
//  pressed pad no longer flashes white". The white-flash half was a
//  Pi-side bug (app.py's fast key path wasn't consulting led_frame() at
//  all, and a separate bug crossfaded EVERY per-tick content change --
//  including the flash -- through a 150ms fade that a 120ms flash could
//  never win; fixed in septagraph/py/septagraph/pi/app.py, no firmware
//  change needed there).
//
//  CORRECTION (same day, after physical testing): S/E/W are NOT dead --
//  they range correctly when something is actually in front of them, and
//  read 0xFFFF the rest of the time because nothing is IN RANGE, which is
//  a normal state, not a fault. pollLidar() had a real (if narrow)
//  liveness-bookkeeping gap: lidarLastSeen[i] was updated in the
//  RangeValid and not-RangeValid-but-ran branches, but NOT in the
//  timeoutOccurred() (I2C-level glitch on that one read) branch -- fixed
//  by recording liveness the INSTANT dataReady() is true, unconditionally,
//  before looking at the result. tickLidarReinit() only ever touches
//  !lidarAlive[i] channels, so this alone should stop the 5s self-heal
//  round-robin from running against S/E/W at all -- they were never
//  actually gone. See pollLidar()'s own comment for the full writeup.
//
//  Reported flicker symptom is specifically a fast, subtle SHIMMER on LIT
//  pads only (dark pads look fine) -- the classic seesaw/NeoTrellis
//  symptom of show() re-latching a board's WS2812 stream with unchanged
//  data. Re-checked line by line against exactly that: applyPendingLed()
//  (below) only ever runs when pendingLedType != 0, which is ONLY ever
//  set by dispatch() on a real incoming TYPE_LEDS_FULL/TYPE_LEDS_PARTIAL
//  from the Pi; applyLedsFull()/applyLedsPartial() already diff every
//  pixel against the shadow framebuffer and only setPixelColor()+dirty-
//  mark pixels that actually changed, so showDirtyBoards() already only
//  show()s a board that had a real change -- that IS "show() only when
//  the buffer actually changed", already implemented (fw5 latency fix #2,
//  not new this session). setBrightness() is likewise only ever called
//  from applyBrightness(), itself only reachable from a TYPE_BRIGHTNESS
//  message -- never per-loop. **No unconditional/insufficiently-gated
//  show() path was found in this source** -- see README.md's fw7 section
//  for what to check next if the idle probe (pi/tools/led_idle_probe.py)
//  proves otherwise on real hardware (stale flash predating fw5's dirty
//  tracking; pi/tools/monitor.py's separate always-on lavalamp thread
//  driving the grid instead of the app).
//
//  This pass still changes tickLidarReinit()'s handling of a channel that
//  IS genuinely dead (unplugged/wedged), on the theory that repeated
//  I2C/GPIO traffic adjacent to LED work in the same loop() is worth
//  eliminating regardless: (a) skips a reinit
//  attempt for a tick where an LED frame is currently pending, so the two
//  can never land back-to-back in the same loop() iteration, (b) backs
//  off a channel's retry period exponentially (capped) the more
//  consecutive times it fails to come up, instead of retrying a
//  permanently-dead channel every single 5s tick forever, and (c) adds a
//  new downstream TYPE_LIDAR_HEAL (0x25, u8 on/off, default ON) so the
//  Pi can disable self-heal entirely during a performance if this still
//  isn't enough -- the explicit fallback this task's brief asked for.
//  NOT flashed/verified against real hardware this session (no network
//  path to the Pi was available) -- see HANDOFF.md for the exact
//  before/after measurement this still needs before being trusted.

#include "Adafruit_NeoTrellis.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <VL53L1X.h>  // Pololu VL53L1X library
#include <string.h>

#define X_DIM 8
#define Y_DIM 8

// 2x2 array of boards, matching physical placement: [row][col]
//   0x2E (TL)  0x2F (TR)
//   0x30 (BL)  0x31 (BR)
Adafruit_NeoTrellis t_array[Y_DIM / 4][X_DIM / 4] = {
  { Adafruit_NeoTrellis(0x2E), Adafruit_NeoTrellis(0x2F) },
  { Adafruit_NeoTrellis(0x30), Adafruit_NeoTrellis(0x31) }
};
Adafruit_MultiTrellis trellis((Adafruit_NeoTrellis *)t_array, Y_DIM / 4, X_DIM / 4);

const uint8_t DEFAULT_BRIGHTNESS = 255;

// ── SoftPots — direct ADC pins, no mux (only 4 sensors) ─────────────────────
#define SOFTPOT1_PIN 32  // north
#define SOFTPOT2_PIN 33  // south
#define SOFTPOT3_PIN 34  // east
#define SOFTPOT4_PIN 35  // west

// ── FSRs (fw6) — direct ADC pins, same divider convention as softpots ──────
// GPIO36 (VP) / GPIO39 (VN): the only two ADC1 pins left free (ADC1 32-35
// are the softpots). GPIO15 / GPIO12: ADC2 pins, usable here ONLY because
// this firmware never calls WiFi.begin() (ADC2 is normally reserved by the
// WiFi driver on ESP32 classic). See README.md "FSR wiring" for the full
// GPIO2-vs-GPIO12 tradeoff and wiring table.
//   FSR0 = GPIO36 (VP)  — ADC1_CH0, input-only, no internal pull
//   FSR1 = GPIO39 (VN)  — ADC1_CH3, input-only, no internal pull
//   FSR2 = GPIO15       — ADC2_CH3, MTDO strap (boot-log verbosity only,
//                          never blocks boot — safe)
//   FSR3 = GPIO12       — ADC2_CH5, MTDI strap (flash-voltage select; a
//                          HIGH here at the reset instant can make the
//                          ESP32 pick the wrong flash voltage and fail to
//                          boot — our divider idles LOW via the mandatory
//                          10k pulldown, so this only matters if FSR3 is
//                          physically pressed at the exact moment of
//                          power-on/reset; see README.md)
#define FSR_PIN_0 36
#define FSR_PIN_1 39
#define FSR_PIN_2 15
#define FSR_PIN_3 12

#define N_ANALOG 8
// Channel order (protocol-visible, backwards compatible): [0..3] softpot
// N/S/E/W unchanged, [4..7] FSR 0..3.
const uint8_t analogPin[N_ANALOG] = {
  SOFTPOT1_PIN, SOFTPOT2_PIN, SOFTPOT3_PIN, SOFTPOT4_PIN,
  FSR_PIN_0, FSR_PIN_1, FSR_PIN_2, FSR_PIN_3
};

// ── Lidar — 4x VL53L1X on the shared Wire bus (GPIO21/22 with NeoTrellis) ──
// I2C_BUS_KHZ: shared-bus clock in kHz, overridable at compile time for a
// bus-integrity test (e.g. -DI2C_BUS_KHZ=100 — see README.md). Default 400
// = VL53L1X datasheet maximum.
#ifndef I2C_BUS_KHZ
#define I2C_BUS_KHZ 400
#endif

#define N_LIDAR 4
// Channel order matches the softpot/protocol convention: N, S, E, W.
const uint8_t lidarXshutPin[N_LIDAR] = { 4, 13, 14, 27 };        // N, S, E, W
const uint8_t lidarAddr[N_LIDAR]     = { 0x2A, 0x2B, 0x2C, 0x2D };  // N, S, E, W

#define LIDAR_INVALID_MM   0xFFFF   // dead / absent / no valid measurement
#define LIDAR_BUDGET_US    50000UL  // 50ms timing budget (short-distance mode)
#define LIDAR_PERIOD_MS    50UL     // inter-measurement period == budget
#define LIDAR_REPORT_MS    50UL     // upstream LIDAR frame cadence
#define LIDAR_STALE_MS     500UL    // no valid range this long -> report invalid
#define LIDAR_DEAD_MS      5000UL   // no data-ready AT ALL this long -> declare
                                    // dead + XSHUT LOW (re-init will retry it)
#define LIDAR_REINIT_MS    5000UL   // dead-channel re-init retry period
#define LIDAR_XSHUT_LOW_MS 15UL     // hold-in-reset time (>=10ms)
#define LIDAR_TBOOT_MS     10UL     // per-sensor boot wait after XSHUT high
                                    // (datasheet tBOOT max 1.2ms; generous)
#define LIDAR_DEFAULT_ADDR 0x29     // factory VL53L1X address
#define LIDAR_PARK_ADDR    0x08     // where boot-time zombies get parked

VL53L1X lidar[N_LIDAR];
bool     lidarAlive[N_LIDAR]  = { false, false, false, false };
uint16_t lidarRange[N_LIDAR]  = { LIDAR_INVALID_MM, LIDAR_INVALID_MM,
                                  LIDAR_INVALID_MM, LIDAR_INVALID_MM };
uint32_t lidarLastFresh[N_LIDAR] = { 0, 0, 0, 0 };  // millis() of last VALID range
uint32_t lidarLastSeen[N_LIDAR]  = { 0, 0, 0, 0 };  // millis() of last data-ready
                                                    // (any status — liveness proof)

// ── Lidar boot diagnostic results (reported via TYPE_LIDARDIAG 0x14) ───────
uint8_t diagAckBitmap[16];    // bus scan with ALL XSHUT low: bit a&7 of byte
                              // a>>3 set = address a (0x08..0x77) ACKed
uint8_t diagAloneMask = 0;    // bit i = channel i ACKed at 0x29 when raised alone
uint8_t diagZombie    = 0;    // 1 = something ACKed 0x29 with every XSHUT low
                              // (a sensor NOT obeying XSHUT: dangling/bad joint)

// Last bring-up failure stage per channel (live; resent with every LIDARDIAG):
#define LFAIL_OK        0  // alive and configured
#define LFAIL_BUSY29    1  // 0x29 occupied before wake (zombie/bus conflict)
#define LFAIL_INIT      2  // init() at 0x29 failed (no ACK / model-ID / osc cal)
#define LFAIL_CONFIG    3  // setDistanceMode / timing-budget write failed
#define LFAIL_NODATA    4  // started, then no data-ready for 5s (runtime death)
uint8_t lidarFailStage[N_LIDAR] = { LFAIL_INIT, LFAIL_INIT, LFAIL_INIT, LFAIL_INIT };

// ── Protocol constants (PROTOCOL.md + LIDAR + latency extensions) ──────────
#define PROTO_VER 1
#define FW_VER    7

// up (ESP32 -> Pi)
#define TYPE_HELLO     0x01
#define TYPE_KEY       0x10
#define TYPE_ANALOG    0x11
#define TYPE_LIDAR     0x13  // 4x u16(LE) range mm, 0xFFFF = invalid/dead
#define TYPE_LIDARDIAG 0x14  // boot XSHUT diagnostic: alone-mask, zombie flag,
                             // full bus-scan ACK bitmap (see sendLidarDiag)
#define TYPE_ACK       0x15  // fw5: LED-apply timing ack, see sendLedAck()
#define TYPE_TIMING    0x16  // fw5: per-phase loop timing, see tickTiming()
// down (Pi -> ESP32)
#define TYPE_LEDS_FULL    0x20
#define TYPE_LEDS_PARTIAL 0x21
#define TYPE_BRIGHTNESS   0x22
#define TYPE_CLEAR        0x23
#define TYPE_LOCAL_ECHO   0x24  // fw5: u8 0/1, default 0 — see applyLocalEcho()
#define TYPE_LIDAR_HEAL   0x25  // fw7: u8 0/1, default 1 (ON) — see tickLidarReinit()
#define TYPE_PING         0x30

// ── CRC16 / CCITT-FALSE (poly 0x1021, init 0xFFFF) ──────────────────────────
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

// ── COBS encode/decode (classic Jacques Fortier implementation) ────────────
// encode: output buffer must be at least length + ceil(length/254) + 1 bytes.
// Does NOT append the trailing 0x00 delimiter — caller writes that separately.
size_t cobsEncode(const uint8_t *input, size_t length, uint8_t *output) {
  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < length) {
    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  output[code_index] = code;
  return write_index;
}

// decode: input is a full COBS block WITHOUT the trailing 0x00. Returns
// decoded length, or 0 on malformed input (caller drops the frame).
size_t cobsDecode(const uint8_t *input, size_t length, uint8_t *output) {
  size_t read_index = 0;
  size_t write_index = 0;
  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code == 0 || (read_index + code > length && code != 1)) return 0;  // malformed
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      if (read_index >= length) return 0;  // malformed
      output[write_index++] = input[read_index++];
    }
    if (code != 0xFF && read_index != length) {
      output[write_index++] = 0;
    }
  }
  return write_index;
}

// ── TX: build TYPE||BODY||CRC16(LE), COBS-encode, write + 0x00 delimiter ───
#define TX_PAYLOAD_MAX 200                             // covers largest up-frame (ANALOG: ~12B) with headroom
#define TX_ENCODED_MAX (TX_PAYLOAD_MAX + TX_PAYLOAD_MAX / 254 + 2)

void sendFrame(uint8_t type, const uint8_t *body, uint8_t bodyLen) {
  uint8_t payload[TX_PAYLOAD_MAX];
  payload[0] = type;
  if (bodyLen) memcpy(payload + 1, body, bodyLen);
  uint16_t crc = crc16_ccitt_false(payload, 1 + bodyLen);
  payload[1 + bodyLen] = (uint8_t)(crc & 0xFF);
  payload[2 + bodyLen] = (uint8_t)((crc >> 8) & 0xFF);
  size_t payloadLen = (size_t)(3 + bodyLen);

  uint8_t encoded[TX_ENCODED_MAX];
  size_t encLen = cobsEncode(payload, payloadLen, encoded);
  Serial.write(encoded, encLen);
  Serial.write((uint8_t)0x00);
}

void sendHello() {
  uint8_t body[5] = { PROTO_VER, X_DIM, Y_DIM, N_ANALOG, FW_VER };
  sendFrame(TYPE_HELLO, body, sizeof(body));
}

// ── RX: accumulate bytes until 0x00, COBS-decode, verify CRC, dispatch ─────
#define RX_BUF_MAX 220  // largest down-frame is LEDS_FULL: 1+192+2 = 195B pre-COBS
uint8_t rxBuf[RX_BUF_MAX];
size_t rxLen = 0;
uint32_t lastFrameRecvMicros = 0;  // micros() at the moment the most recent
                                   // frame's CRC verified OK ("receive-complete")

// ── Shadow framebuffer + per-board dirty tracking (fw5 latency fix #2) ─────
// seesaw_NeoPixel::setPixelColor() does an I2C write PER PIXEL, and
// Adafruit_MultiTrellis::show() always shows all 4 boards. Both LEDS_FULL
// and LEDS_PARTIAL now diff against the last-SENT color per pixel and only
// touch (setPixelColor + mark dirty) pixels that actually changed; only
// boards with at least one changed pixel get show()'d. forceRedraw forces
// every pixel through once (boot, and after a BRIGHTNESS change, since
// brightness is baked into the value at setPixelColor() time in this
// library — an unchanged raw RGB target would otherwise never be resent at
// the new brightness).
uint8_t fbR[X_DIM * Y_DIM] = { 0 };
uint8_t fbG[X_DIM * Y_DIM] = { 0 };
uint8_t fbB[X_DIM * Y_DIM] = { 0 };
bool    forceRedraw = false;  // boot state already matches (applyClear at setup)

static inline void markBoardDirty(bool dirty[Y_DIM / 4][X_DIM / 4], uint8_t x, uint8_t y) {
  dirty[y / 4][x / 4] = true;
}

static void showDirtyBoards(bool dirty[Y_DIM / 4][X_DIM / 4]) {
  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      if (dirty[j][i]) t_array[j][i].pixels.show();
}

void applyLedsFull(const uint8_t *body, size_t len) {
  if (len != 192) return;  // malformed body for this type, drop
  bool dirty[Y_DIM / 4][X_DIM / 4] = { { false, false }, { false, false } };
  for (int i = 0; i < 64; i++) {
    uint8_t x = i % X_DIM;
    uint8_t y = i / X_DIM;
    uint8_t r = body[i * 3 + 0];
    uint8_t g = body[i * 3 + 1];
    uint8_t b = body[i * 3 + 2];
    if (!forceRedraw && fbR[i] == r && fbG[i] == g && fbB[i] == b) continue;
    trellis.setPixelColor(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
    fbR[i] = r; fbG[i] = g; fbB[i] = b;
    markBoardDirty(dirty, x, y);
  }
  forceRedraw = false;
  showDirtyBoards(dirty);
}

void applyLedsPartial(const uint8_t *body, size_t len) {
  if (len < 1) return;
  uint8_t count = body[0];
  if (len != (size_t)(1 + count * 5)) return;  // malformed, drop
  bool dirty[Y_DIM / 4][X_DIM / 4] = { { false, false }, { false, false } };
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t *e = body + 1 + i * 5;
    uint8_t x = e[0], y = e[1], r = e[2], g = e[3], b = e[4];
    if (x >= X_DIM || y >= Y_DIM) continue;
    int idx = y * X_DIM + x;
    if (!forceRedraw && fbR[idx] == r && fbG[idx] == g && fbB[idx] == b) continue;
    trellis.setPixelColor(x, y, ((uint32_t)r << 16) | ((uint32_t)g << 8) | b);
    fbR[idx] = r; fbG[idx] = g; fbB[idx] = b;
    markBoardDirty(dirty, x, y);
  }
  forceRedraw = false;
  showDirtyBoards(dirty);
}

void applyBrightness(const uint8_t *body, size_t len) {
  if (len < 1) return;
  uint8_t level = body[0];
  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(level);
  trellis.show();
  // Brightness scaling is applied inside setPixelColor() at call time in
  // this library, not at show() time — an already-buffered pixel's physical
  // output will NOT reflect the new brightness until it is setPixelColor'd
  // again. Force every pixel through the diff on the next LED frame so the
  // grid actually reaches the new brightness rather than silently staying
  // stale until something changes.
  forceRedraw = true;
}

void applyClear() {
  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++)
      trellis.setPixelColor(x, y, 0);
  trellis.show();
  memset(fbR, 0, sizeof(fbR));
  memset(fbG, 0, sizeof(fbG));
  memset(fbB, 0, sizeof(fbB));
  forceRedraw = false;  // shadow now matches actual (all-off) state
}

// ── Local echo (fw5, optional, default OFF) ─────────────────────────────
// Pi-gated (TYPE_LOCAL_ECHO 0x24) immediate key->LED feedback: on press,
// snap that pad straight to white without waiting for the Pi's next frame;
// on release, restore whatever the Pi last actually commanded for that
// pixel (the shadow framebuffer — untouched by local echo). Preserves the
// "dumb bridge" law: the Pi remains the sole authority over color, this is
// only a latency-hiding preview that the Pi's own next frame always
// eventually overrides.
bool localEchoEnabled = false;

// ── Lidar self-heal enable (fw7) ────────────────────────────────────────
// Pi-gated (TYPE_LIDAR_HEAL 0x25), default ON: tickLidarReinit()'s 5s
// round-robin re-init of dead channels does real I2C/GPIO work on the bus
// the NeoTrellis shares — see the fw7 header comment above for why that's
// the prime remaining suspect for the reported idle flicker. Lets the Pi
// turn it off entirely (e.g. for the duration of a performance) if
// throttling it (see lidarReinitBackoffTicks below) isn't enough on its
// own. Turning this off only pauses recovery attempts; a channel that's
// already alive keeps running untouched either way.
bool lidarHealEnabled = true;

void applyLocalEcho(uint8_t x, uint8_t y, uint8_t pressed) {
  uint32_t color;
  if (pressed) {
    color = 0xFFFFFFUL;
  } else {
    int idx = y * X_DIM + x;
    color = ((uint32_t)fbR[idx] << 16) | ((uint32_t)fbG[idx] << 8) | fbB[idx];
  }
  trellis.setPixelColor(x, y, color);
  t_array[y / 4][x / 4].pixels.show();
}

// ── LED-apply ACK (fw5) — Pi-side latency measurement ───────────────────
// Sent once per APPLIED (not coalesced-away) LEDS_FULL/LEDS_PARTIAL frame:
// how long between "frame fully received+CRC-verified" and "show() for
// every touched board complete". BODY (9 bytes): recv_us:u32 LE,
// done_us:u32 LE (both raw micros() snapshots — the Pi computes the delta
// itself, and can also relate recv_us to when it wrote the frame if it
// keeps its own clock in sync via the PING/HELLO round trip), acked_type:u8
// (0x20 or 0x21).
void sendLedAck(uint8_t ackedType, uint32_t recvUs, uint32_t doneUs) {
  uint8_t body[9];
  memcpy(body + 0, &recvUs, 4);
  memcpy(body + 4, &doneUs, 4);
  body[8] = ackedType;
  sendFrame(TYPE_ACK, body, sizeof(body));
}

// ── Pending LED frame coalescing (fw5) ──────────────────────────────────
// Multiple LEDS_FULL/LEDS_PARTIAL frames can arrive back-to-back inside one
// pollSerial() drain (e.g. the Pi burst-sending while the ESP32 was mid-
// cycle). Applying every one would waste I2C/show() time on frames that are
// already stale by the time we get to them. dispatch() only ever STAGES the
// most recent LED command into pendingLed*; applyPendingLed() (called once
// per loop(), after the key scan) applies+shows+ACKs it. "Keep latest".
#define PENDING_LED_MAX 200
uint8_t  pendingLedType = 0;  // 0 = none pending
uint8_t  pendingLedBody[PENDING_LED_MAX];
size_t   pendingLedLen = 0;
uint32_t pendingLedRecvUs = 0;

void applyPendingLed() {
  if (pendingLedType == 0) return;
  uint8_t ackedType = pendingLedType;
  uint32_t recvUs = pendingLedRecvUs;
  pendingLedType = 0;  // clear before applying: a re-entrant stage during a
                       // slow show() (there isn't one — single-threaded —
                       // but keeps the intent obvious) never gets lost.
  if (ackedType == TYPE_LEDS_FULL) applyLedsFull(pendingLedBody, pendingLedLen);
  else applyLedsPartial(pendingLedBody, pendingLedLen);
  sendLedAck(ackedType, recvUs, micros());
}

void dispatch(uint8_t type, const uint8_t *body, size_t len) {
  switch (type) {
    case TYPE_LEDS_FULL:
    case TYPE_LEDS_PARTIAL:
      if (len <= PENDING_LED_MAX) {
        pendingLedType = type;
        pendingLedLen = len;
        memcpy(pendingLedBody, body, len);
        pendingLedRecvUs = lastFrameRecvMicros;
      }
      break;
    case TYPE_BRIGHTNESS:   applyBrightness(body, len);  break;
    case TYPE_CLEAR:        applyClear();                break;
    case TYPE_LOCAL_ECHO:
      if (len >= 1) localEchoEnabled = (body[0] != 0);
      break;
    case TYPE_LIDAR_HEAL:
      if (len >= 1) lidarHealEnabled = (body[0] != 0);
      break;
    case TYPE_PING:         sendHello(); sendLidarDiag(); break;
    default: break;  // unknown type, ignore
  }
}

void handleFrame(const uint8_t *payload, size_t len) {
  if (len < 3) return;  // too short to hold TYPE + CRC16
  uint16_t crcRecv = (uint16_t)payload[len - 2] | ((uint16_t)payload[len - 1] << 8);
  uint16_t crcCalc = crc16_ccitt_false(payload, len - 2);
  if (crcRecv != crcCalc) return;  // drop malformed frame, do not crash
  lastFrameRecvMicros = micros();  // "receive-complete" timestamp for TYPE_ACK
  dispatch(payload[0], payload + 1, len - 3);
}

void pollSerial() {
  while (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    if (b == 0x00) {
      if (rxLen > 0) {
        uint8_t decoded[RX_BUF_MAX];
        size_t decLen = cobsDecode(rxBuf, rxLen, decoded);
        if (decLen > 0) handleFrame(decoded, decLen);
        // decLen == 0 -> malformed, drop silently and resync
      }
      rxLen = 0;
    } else if (rxLen < RX_BUF_MAX) {
      rxBuf[rxLen++] = b;
    } else {
      rxLen = 0;  // overflow: drop the runaway frame, resync at next 0x00
    }
  }
}

// ── Key events: build+send KEY (0x10) on every press/release edge ──────────
void sendKeyEvent(uint8_t x, uint8_t y, uint8_t edge) {
  uint8_t body[3] = { x, y, edge };
  sendFrame(TYPE_KEY, body, sizeof(body));
  if (localEchoEnabled) applyLocalEcho(x, y, edge);
}

// ── Fast key scan (fw5 latency fix #1) ──────────────────────────────────
// Replaces trellis.read() (Adafruit_MultiTrellis::read()). Same logic, same
// per-board getKeypadCount()+readKeypad() calls, same seesaw<->grid index
// math — MINUS the library's extra delayMicroseconds(500) per board.
// Adafruit_seesaw::getKeypadCount() already sleeps 500us internally (inside
// Adafruit_seesaw::read(): write register address -> delayMicroseconds(500)
// -> I2C read, for the seesaw's own response-prep time); the stock
// MultiTrellis::read() adds a SECOND, fully redundant 500us delay on top of
// that, per board — 4 boards x 500us = 2ms of pure waste every loop, key
// pressed or not. This was the single largest measured fixed-latency cost
// on this hardware (see README.md "Latency work (fw5)"). Also defensively
// clamps a garbage/failed-read count to NEO_TRELLIS_NUM_KEYS instead of
// trusting it for a stack VLA size (the stock library does not).
void pollTrellisKeys() {
  for (int n = 0; n < Y_DIM / 4; n++) {
    for (int m = 0; m < X_DIM / 4; m++) {
      Adafruit_NeoTrellis &t = t_array[n][m];
      uint8_t count = t.getKeypadCount();
      if (count == 0) continue;
      if (count > NEO_TRELLIS_NUM_KEYS) count = NEO_TRELLIS_NUM_KEYS;  // defensive clamp
      count = (uint8_t)(count + 2);
      keyEventRaw e[count];
      if (!t.readKeypad(e, count)) continue;
      for (int i = 0; i < count; i++) {
        uint8_t k = NEO_TRELLIS_SEESAW_KEY(e[i].bit.NUM);
        if (k >= NEO_TRELLIS_NUM_KEYS) continue;
        uint8_t edge = e[i].bit.EDGE;
        if (edge != SEESAW_KEYPAD_EDGE_RISING && edge != SEESAW_KEYPAD_EDGE_FALLING) continue;
        uint8_t gx = (uint8_t)(NEO_TRELLIS_X(k) + m * NEO_TRELLIS_NUM_COLS);
        uint8_t gy = (uint8_t)(NEO_TRELLIS_Y(k) + n * NEO_TRELLIS_NUM_ROWS);
        sendKeyEvent(gx, gy, edge == SEESAW_KEYPAD_EDGE_RISING ? 1 : 0);
      }
    }
  }
}

// ── Analog: raw, ungated ADC, every 20ms (~50Hz) ────────────────────────────
// fw6: 8 channels — [0..3] softpot N/S/E/W (unchanged), [4..7] FSR 0..3.
// Same cadence as fw5 (still one tickAnalog() call per loop, gated at 20ms);
// reading 4 extra ADC pins costs a handful of extra analogRead() calls, not
// a new phase — see the "analog" slot in TYPE_TIMING for the measured cost.
void tickAnalog() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 20) return;
  last = now;

  uint16_t vals[N_ANALOG];
  for (int i = 0; i < N_ANALOG; i++) vals[i] = (uint16_t)analogRead(analogPin[i]);

  uint8_t body[1 + N_ANALOG * 2];
  body[0] = N_ANALOG;
  for (int i = 0; i < N_ANALOG; i++) {
    body[1 + i * 2] = (uint8_t)(vals[i] & 0xFF);
    body[2 + i * 2] = (uint8_t)((vals[i] >> 8) & 0xFF);
  }
  sendFrame(TYPE_ANALOG, body, sizeof(body));
}

// ── I2C probe: zero-length write, ACK check ────────────────────────────────
bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// ── LIDARDIAG (0x14) upstream frame ────────────────────────────────────────
// BODY (18 bytes):
//   [0]     alone_mask : bit i (0=N 1=S 2=E 3=W) = channel i ACKed at 0x29
//           when its XSHUT alone was raised. Meaningful ONLY if zombie == 0
//           (a zombie ACKs 0x29 permanently, polluting every channel's probe).
//   [1]     zombie     : 1 = a device ACKed 0x29 while ALL four XSHUT were
//           held LOW — i.e. a sensor NOT obeying its XSHUT line (dangling
//           wire / cold joint). This is what shuffles working positions.
//   [2..17] ack_bitmap : bus scan (all XSHUT low) of addresses 0x08..0x77;
//           address a -> byte 2 + (a >> 3), bit (a & 7).
//           Expected content: NeoTrellis 0x2E..0x31 only.
//   [18..21] fail_stage: per channel N/S/E/W, LIVE last bring-up failure
//           stage (LFAIL_*): 0 ok, 1 0x29-busy, 2 init, 3 config, 4 no-data.
void sendLidarDiag() {
  uint8_t body[2 + sizeof(diagAckBitmap) + N_LIDAR];
  body[0] = diagAloneMask;
  body[1] = diagZombie;
  memcpy(body + 2, diagAckBitmap, sizeof(diagAckBitmap));
  for (int i = 0; i < N_LIDAR; i++) {
    body[2 + sizeof(diagAckBitmap) + i] = lidarFailStage[i];
  }
  sendFrame(TYPE_LIDARDIAG, body, sizeof(body));
}

// ── Boot-time XSHUT diagnostic (runs BEFORE normal bring-up) ───────────────
// a) all XSHUT LOW 50ms, full bus scan -> ACK bitmap; anything at 0x29 now
//    is a zombie (its XSHUT is not actually resetting it).
// b) raise each XSHUT alone, probe 0x29, drop it LOW again -> alone_mask.
// Results are stored and reported via TYPE_LIDARDIAG (once after boot, and
// again with HELLO on every PING).
void lidarBootDiag() {
  for (int i = 0; i < N_LIDAR; i++) {
    pinMode(lidarXshutPin[i], OUTPUT);
    digitalWrite(lidarXshutPin[i], LOW);
  }
  delay(50);

  memset(diagAckBitmap, 0, sizeof(diagAckBitmap));
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    if (i2cProbe(a)) diagAckBitmap[a >> 3] |= (uint8_t)(1 << (a & 7));
  }
  diagZombie = (diagAckBitmap[LIDAR_DEFAULT_ADDR >> 3] >>
                (LIDAR_DEFAULT_ADDR & 7)) & 1;

  diagAloneMask = 0;
  for (int i = 0; i < N_LIDAR; i++) {
    digitalWrite(lidarXshutPin[i], HIGH);
    delay(LIDAR_TBOOT_MS);
    if (i2cProbe(LIDAR_DEFAULT_ADDR)) diagAloneMask |= (uint8_t)(1 << i);
    digitalWrite(lidarXshutPin[i], LOW);
    delay(5);  // let it drop off the bus before the next channel's probe
  }
}

// ── Zombie parking ─────────────────────────────────────────────────────────
// A zombie (stuck-awake sensor at 0x29) would poison every subsequent init:
// setAddress() at 0x29 would readdress the zombie together with — or instead
// of — the intended sensor. Move it out of the way to LIDAR_PARK_ADDR so
// 0x29 is free for the sequenced bring-up. The zombie's own channel will
// then simply fail init (nothing wakes on its XSHUT) and show DEAD — which
// correctly names the channel whose XSHUT wire needs the soldering iron.
// (Several zombies at once all land on the same park address — pathological
//  double-fault; best effort, still keeps 0x29 clear.)
void lidarParkZombies() {
  if (!diagZombie) return;
  for (int tries = 0; tries < N_LIDAR && i2cProbe(LIDAR_DEFAULT_ADDR); tries++) {
    VL53L1X z;
    z.setBus(&Wire);
    z.setTimeout(100);
    if (!z.init()) break;          // won't init -> can't move it; give up
    z.setAddress(LIDAR_PARK_ADDR); // off 0x29; left idle (never started)
  }
}

// ── Per-channel bring-up: full XSHUT low/high + readdress + start ──────────
// Used at boot (sequenced, all others held LOW or already readdressed) and
// by the periodic re-init of dead channels. Any failure pulls XSHUT LOW
// again so a half-alive part can never squat on 0x29. Never blocks > ~30ms.
bool lidarTryInit(int i) {
  digitalWrite(lidarXshutPin[i], LOW);
  delay(LIDAR_XSHUT_LOW_MS);

  // Defensive gate: 0x29 must be vacant BEFORE we wake this sensor. If it
  // is not (unparked zombie, mid-toggle neighbor), initializing now would
  // configure/readdress the wrong part — skip; retry next period.
  if (i2cProbe(LIDAR_DEFAULT_ADDR)) {
    lidarFailStage[i] = LFAIL_BUSY29;
    return false;
  }

  digitalWrite(lidarXshutPin[i], HIGH);
  delay(LIDAR_TBOOT_MS);  // sensor firmware boot (datasheet tBOOT <= 1.2ms)

  lidar[i].setBus(&Wire);    // explicit (library default is Wire; if the
                             // lidars ever move to Wire1, change only this
                             // line and the Wire1.begin() in setup())
  lidar[i].setTimeout(100);  // ms; bounds the library's internal waits

  // init() talks to the sensor at the default address 0x29.
  if (!lidar[i].init()) {
    digitalWrite(lidarXshutPin[i], LOW);
    lidarFailStage[i] = LFAIL_INIT;
    return false;
  }

  lidar[i].setAddress(lidarAddr[i]);

  // Short-distance mode + 50ms budget/period to match the 50Hz cadence.
  // These can fail on a flaky bus — treat any failure as a dead sensor
  // rather than trusting a half-configured part.
  if (!lidar[i].setDistanceMode(VL53L1X::Short) ||
      !lidar[i].setMeasurementTimingBudget(LIDAR_BUDGET_US)) {
    digitalWrite(lidarXshutPin[i], LOW);
    lidarFailStage[i] = LFAIL_CONFIG;
    return false;
  }

  lidar[i].startContinuous(LIDAR_PERIOD_MS);
  lidarFailStage[i] = LFAIL_OK;
  lidarAlive[i] = true;
  lidarRange[i] = LIDAR_INVALID_MM;  // until the first valid measurement
  lidarLastFresh[i] = millis();
  lidarLastSeen[i]  = millis();
  return true;
}

// ── Lidar boot: XSHUT-sequenced address assignment on the shared Wire bus ──
// (diag has already left every XSHUT LOW). One channel at a time; a failed
// channel is marked dead and revisited by tickLidarReinit() every 5s.
// Caller must have run Wire.begin() (and set the clock) already.
void lidarSetupAll() {
  for (int i = 0; i < N_LIDAR; i++) {
    lidarAlive[i] = false;
    lidarTryInit(i);
  }
}

// ── Periodic re-init: one dead channel per 5s tick (round-robin) ───────────
// Flaky joints recover without a replug. Bounded work per tick (~30ms worst
// case) so the main loop never stalls noticeably. Alive channels are never
// touched: their XSHUTs stay HIGH and they answer at 0x2A..0x2D, not 0x29.
//
// fw7 additions (see the fw7 header comment for the flicker-investigation
// context this responds to):
//  - lidarHealEnabled gate (TYPE_LIDAR_HEAL): Pi can pause this entirely.
//  - Skip this tick's attempt (do NOT advance the round-robin pointer or
//    the retry clock) if an LED frame is currently pending: this
//    guarantees a lidar re-init's I2C/GPIO work and an LED show() can
//    never land in the same loop() iteration, which was the one
//    remaining way they could interact even though loop() is already
//    strictly sequential.
//  - Per-channel exponential backoff: a channel that keeps failing to
//    come up is retried less and less often (5s, 10s, 20s, ... capped at
//    LIDAR_REINIT_MAX_MS) instead of forever at a flat 5s. With 3 of 4
//    channels currently dead, the flat cadence meant a real re-init cycle
//    (XSHUT toggle + several I2C transactions) was running on the shared
//    bus on almost every tick, indefinitely — this cuts that traffic down
//    over time for a channel that's genuinely not coming back, while
//    still retrying promptly (5s) right after a channel FIRST goes dead,
//    when "flaky joint reseated itself" recovery is most likely. A
//    channel's backoff resets to the fast 5s cadence the moment it comes
//    back alive and (later) goes dead again — lidarTryInit() success
//    already resets lidarAlive[i], and failure count only accumulates
//    while a channel stays dead across consecutive attempts.
#define LIDAR_REINIT_MAX_MS   (LIDAR_REINIT_MS * 8)  // cap: 40s at the 5s base
uint8_t  lidarFailStreak[N_LIDAR] = { 0, 0, 0, 0 };       // consecutive failed re-inits
uint32_t lidarLastReinitAttempt[N_LIDAR] = { 0, 0, 0, 0 };  // millis() of last retry, NOT
                                                             // lidarLastSeen (that's about
                                                             // data-ready, already stale by
                                                             // definition once a channel is
                                                             // dead, so it can't drive backoff)

void tickLidarReinit() {
  static uint32_t last = 0;
  static uint8_t  next = 0;
  if (!lidarHealEnabled) return;
  if (pendingLedType != 0) return;  // never overlap a re-init with a pending LED apply
  uint32_t now = millis();
  if (now - last < LIDAR_REINIT_MS) return;

  for (int k = 0; k < N_LIDAR; k++) {
    int i = (next + k) % N_LIDAR;
    if (!lidarAlive[i]) {
      uint32_t backoff = LIDAR_REINIT_MS << (lidarFailStreak[i] < 4 ? lidarFailStreak[i] : 4);
      if (backoff > LIDAR_REINIT_MAX_MS) backoff = LIDAR_REINIT_MAX_MS;
      if (now - lidarLastReinitAttempt[i] < backoff) continue;  // this channel isn't due yet
      last = now;
      next = (uint8_t)((i + 1) % N_LIDAR);
      lidarLastReinitAttempt[i] = now;
      if (lidarTryInit(i)) {
        lidarFailStreak[i] = 0;
      } else if (lidarFailStreak[i] < 0xFF) {
        lidarFailStreak[i]++;
      }
      return;  // at most one attempt per tick
    }
  }
  last = now;  // nothing dead to retry; still throttle the scan itself
}

// ── Lidar poll: non-blocking; harvest only sensors with data ready ─────────
// fw7 correction: "no target in range" is NOT a fault. A VL53L1X in
// continuous mode keeps asserting data-ready on schedule regardless of
// whether anything is within its configured range — the ONLY thing that
// legitimately means "dead" is data-ready NOT firing at all for
// LIDAR_DEAD_MS. Earlier field notes (HANDOFF.md §10-12) misread S/E/W
// reporting 0xFFFF (out-of-range/no-target) as "dead" because the two
// concepts look similar upstream (both surface as 0xFFFF in TYPE_LIDAR);
// they are not the same thing, and conflating them is what made the 5s
// self-heal round-robin re-init channels that were never actually gone —
// real, repeated XSHUT/I2C traffic on the shared bus for no reason. Fixed
// by recording liveness (lidarLastSeen) the INSTANT dataReady() is true,
// unconditionally, before looking at the result at all — so even the
// timeoutOccurred() (I2C-level glitch on this one read) branch below,
// which previously fell through without updating lidarLastSeen, now still
// counts as "the sensor is there and cycling" rather than silently
// eroding toward a false dead declaration.
void pollLidar() {
  uint32_t now = millis();
  for (int i = 0; i < N_LIDAR; i++) {
    if (!lidarAlive[i]) continue;

    if (lidar[i].dataReady()) {
      lidarLastSeen[i] = now;  // liveness proof, regardless of what follows
      // read(false) = non-blocking result harvest (measurement is complete).
      uint16_t mm = lidar[i].read(false);
      if (lidar[i].timeoutOccurred()) {
        lidarRange[i] = LIDAR_INVALID_MM;  // I2C-level failure on this read
      } else if (lidar[i].ranging_data.range_status == VL53L1X::RangeValid) {
        // Raw valid range; clamp so 0xFFFF stays reserved for "invalid".
        lidarRange[i] = (mm >= LIDAR_INVALID_MM) ? (LIDAR_INVALID_MM - 1) : mm;
        lidarLastFresh[i] = now;
      } else {
        // Sensor ran but measurement is untrustworthy (wraparound, sigma
        // fail, no target...): report invalid, Pi decides what to do.
        // This is a NORMAL, expected state -- "nothing within range" is
        // not a fault -- and must never count toward "dead" (see the fw7
        // comment above this function).
        lidarRange[i] = LIDAR_INVALID_MM;
      }
    } else if (now - lidarLastSeen[i] > LIDAR_DEAD_MS) {
      // Not even a data-ready flag for 5s: the part is gone (wire fell off,
      // wedged silicon). Declare it dead + hold XSHUT LOW so it can't haunt
      // 0x29; tickLidarReinit() will retry it every 5s.
      lidarAlive[i] = false;
      lidarRange[i] = LIDAR_INVALID_MM;
      lidarFailStage[i] = LFAIL_NODATA;
      digitalWrite(lidarXshutPin[i], LOW);
    } else if (now - lidarLastFresh[i] > LIDAR_STALE_MS) {
      // No VALID range for too long: stop trusting the last value while
      // the sensor keeps running (it may just be staring past 4m).
      lidarRange[i] = LIDAR_INVALID_MM;
    }
  }
}

// ── Lidar report: 4x u16(LE) mm every 50ms, alongside ANALOG ───────────────
void tickLidar() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < LIDAR_REPORT_MS) return;
  last = now;

  uint8_t body[N_LIDAR * 2];
  for (int i = 0; i < N_LIDAR; i++) {
    body[i * 2 + 0] = (uint8_t)(lidarRange[i] & 0xFF);
    body[i * 2 + 1] = (uint8_t)((lidarRange[i] >> 8) & 0xFF);
  }
  sendFrame(TYPE_LIDAR, body, sizeof(body));
}

// ── Debug telemetry: per-phase loop timing (fw5) ────────────────────────
// Tracks the WORST (max) microseconds spent in each named phase since the
// last report, reported every TIMING_REPORT_MS as TYPE_TIMING (0x16), then
// reset. Cheap enough (a handful of micros() calls per loop) to leave on
// permanently rather than gating behind a separate debug build — this is
// what let us find the two root causes documented above and is left in so
// regressions show up immediately in the monitor UI instead of needing a
// special firmware. Body (14 bytes): 7 x u16 LE max-microseconds, order
// [serial, keys, analog, lidar_poll, lidar_report, led_apply, loop_total].
// A phase's slot stays 0 when it never ran while dirty this window (e.g.
// led_apply when no LED frame arrived) — that IS the useful signal.
#define TIMING_REPORT_MS 500UL
uint16_t dbgMaxSerial = 0, dbgMaxKeys = 0, dbgMaxAnalog = 0, dbgMaxLidarPoll = 0,
         dbgMaxLidarReport = 0, dbgMaxLedApply = 0, dbgMaxLoop = 0;

static inline void dbgTrack(uint16_t &slot, uint32_t startUs) {
  uint32_t d = micros() - startUs;
  if (d > 0xFFFF) d = 0xFFFF;
  if ((uint16_t)d > slot) slot = (uint16_t)d;
}

void tickTiming() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < TIMING_REPORT_MS) return;
  last = now;

  uint16_t vals[7] = { dbgMaxSerial, dbgMaxKeys, dbgMaxAnalog, dbgMaxLidarPoll,
                        dbgMaxLidarReport, dbgMaxLedApply, dbgMaxLoop };
  uint8_t body[14];
  for (int i = 0; i < 7; i++) {
    body[i * 2 + 0] = (uint8_t)(vals[i] & 0xFF);
    body[i * 2 + 1] = (uint8_t)((vals[i] >> 8) & 0xFF);
  }
  sendFrame(TYPE_TIMING, body, sizeof(body));
  dbgMaxSerial = dbgMaxKeys = dbgMaxAnalog = dbgMaxLidarPoll = 0;
  dbgMaxLidarReport = dbgMaxLedApply = dbgMaxLoop = 0;
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  // GPIO32/33 support an internal pulldown. GPIO34/35 are input-only pins
  // with NO internal pull resistors at all — that's an ESP32 hardware limit;
  // use an external ~100k pulldown to GND on those (see README.md).
  pinMode(SOFTPOT1_PIN, INPUT_PULLDOWN);
  pinMode(SOFTPOT2_PIN, INPUT_PULLDOWN);

  // fw6: FSR ADC pins. None of the four have a usable internal pulldown for
  // this purpose (36/39 are input-only with no pulls at all; 15/12 are left
  // plain INPUT since the required external 10k to GND — see README.md "FSR
  // wiring" — is mandatory regardless and an internal pull would only fight
  // a lighter finger touch). analogSetPinAttenuation is called explicitly
  // per-pin (not just relying on the global analogSetAttenuation above) so
  // each pin's attenuation is set before its first analogRead() regardless
  // of ADC unit; analogRead() itself auto-attaches a pin to the ADC on
  // first use on this core (no separate attach call exists in core 3.3.8).
  // analogRead() works on the two ADC2 pins (15, 12) here specifically
  // because this firmware never calls WiFi.begin() — ADC2 is otherwise
  // reserved by the WiFi driver and analogRead() on it would read 0/garbage
  // whenever WiFi is active.
  pinMode(FSR_PIN_0, INPUT);
  pinMode(FSR_PIN_1, INPUT);
  pinMode(FSR_PIN_2, INPUT);
  pinMode(FSR_PIN_3, INPUT);
  analogSetPinAttenuation(FSR_PIN_0, ADC_11db);
  analogSetPinAttenuation(FSR_PIN_1, ADC_11db);
  analogSetPinAttenuation(FSR_PIN_2, ADC_11db);
  analogSetPinAttenuation(FSR_PIN_3, ADC_11db);

  Wire.begin(21, 22);
  // 400kHz default, NOT grid_link's 800kHz: the VL53L1X I2C interface is
  // rated to 400kHz max and now shares this bus with the NeoTrellis (which
  // is fine at 400kHz). Overridable via -DI2C_BUS_KHZ (e.g. 100 for a
  // bus-integrity test). If the grid ever needs 800kHz back, move the
  // lidars to Wire1 on GPIO25/26 — see README.md "Bus fallback".
  Wire.setClock((uint32_t)I2C_BUS_KHZ * 1000UL);
  Wire.setTimeOut(25);  // ms; a wedged device must never stall the loop long

  // Lidars before the trellis: their XSHUT lines must be driven LOW as early
  // as possible so no sensor is half-booted at 0x29 with an undefined XSHUT
  // level, and the one-at-a-time address assignment needs a quiet bus.
  // A missing/dead lidar (or all four absent) never blocks boot; the grid
  // below still comes up exactly as in grid_link.
  lidarBootDiag();     // XSHUT obedience test + bus scan (results -> 0x14)
  lidarParkZombies();  // clear 0x29 of any stuck-awake sensor
  lidarSetupAll();     // sequenced address assignment + start ranging

  if (!trellis.begin()) {
    // Can't reach the Pi meaningfully without a grid; spin with a visible
    // serial breadcrumb rather than silently hanging.
    Serial.println("NeoTrellis not found — check addresses / wiring / power.");
    while (1) delay(10);
  }

  for (int j = 0; j < Y_DIM / 4; j++)
    for (int i = 0; i < X_DIM / 4; i++)
      t_array[j][i].pixels.setBrightness(DEFAULT_BRIGHTNESS);

  for (int y = 0; y < Y_DIM; y++)
    for (int x = 0; x < X_DIM; x++) {
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_RISING, true);
      trellis.activateKey(x, y, SEESAW_KEYPAD_EDGE_FALLING, true);
      // NOTE: no registerCallback here (fw4 registered onKey()) — fw5 scans
      // keys itself in pollTrellisKeys()/sendKeyEvent() below, bypassing
      // Adafruit_MultiTrellis::read()'s callback dispatch entirely (that is
      // exactly the path with the redundant 500us/board delay). activateKey
      // still runs on the seesaw side to enable RISING/FALLING FIFO events.
    }

  applyClear();     // all pixels off until the Pi sends real frames
  sendHello();      // boot handshake
  sendLidarDiag();  // boot XSHUT diagnostic report (also resent on PING)
}

void loop() {
  uint32_t loopStart = micros();
  uint32_t t0;

  // Key scan FIRST, every loop, unconditionally — never behind LED/lidar
  // work (see pollTrellisKeys() for the redundant-delay fix that makes this
  // cheap: ~0-2ms depending on I2C traffic, down from a fixed 2ms+ floor).
  t0 = micros(); pollSerial();       dbgTrack(dbgMaxSerial, t0);
  t0 = micros(); pollTrellisKeys();  dbgTrack(dbgMaxKeys, t0);
  t0 = micros(); tickAnalog();       dbgTrack(dbgMaxAnalog, t0);
  t0 = micros(); pollLidar();        dbgTrack(dbgMaxLidarPoll, t0);
  t0 = micros(); tickLidar();        dbgTrack(dbgMaxLidarReport, t0);
  // LED apply is the only remaining I2C-heavy write path (show()); it runs
  // LAST so it can never delay the key scan or lidar polling above it, and
  // only touches boards that actually changed (dirty tracking) — see
  // applyLedsFull/applyLedsPartial.
  t0 = micros(); applyPendingLed();  dbgTrack(dbgMaxLedApply, t0);
  tickLidarReinit();  // every 5s at most; bounded ~30ms; only on dead channels
  tickTiming();        // periodic phase-timing report; cheap when not due
  dbgTrack(dbgMaxLoop, loopStart);
}
