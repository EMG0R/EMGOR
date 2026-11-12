// trellis.cpp — NeoTrellis seesaw driver. VERBATIM from PMOR_V2.cpp:
//   protocol notes + ntShow/ntEnableKeys/ntInit/ntSoftInit   L293–438
//   recovery / watchdog / LED push / keypad FIFO poll        L4376–4614
// reorganised behind hw.h's trellisTick / trellisSetFrame / trellisPollKeys /
// trellisSetRotation. The 90° rotation is ONE remap table applied to the LED
// index in trellisSetFrame() and to the key decode in trellisPollKeys() —
// nowhere else. The v2 `*179>>8` global scale is replaced by the runtime
// g_trellis_brightness (declared here, default 0.45).
#include <string.h>
#include "hw.h"
#include "led_push.h"
#include "hw_priv.h"
#include "bbus.h"
#include "../ui/render.h"           // g_trellis_brightness is declared there
extern "C" {
#include "tusb.h"
}
using namespace daisy;

// ============================================================
// NeoTrellis — seesaw protocol
// ============================================================
// Seesaw neopixel module 0x0E:
//   0x01 PIN   — always 3 on NeoTrellis PCB
//   0x02 SPEED — 0x01 = 800kHz (WS2812B requirement)
//   0x03 BUFLEN— 48 bytes = 16 px × 3 GRB
//   0x04 BUF   — [offset_hi, offset_lo, data...] write pixel data at offset
//   0x05 SHOW  — latch; {0x0E, 0x05} exactly 2 bytes, NO trailing byte
//
// Seesaw keypad module 0x10:
//   0x01 EVENT — {base, 0x01, key, 0x07} enable press+release on key N
//   0x04 COUNT — read 1 byte = events waiting in FIFO
//   0x10 FIFO  — read N bytes; each = (key<<2) | edge; edge 0x01=press 0x02=release
//
// CHUNK DELAY: 2ms (bumped from 1ms) — extra margin so SAMD09 finishes storing
// each 28-byte burst into its pixel RAM before the next chunk arrives.
// Crackling (random pixel glitches) was traced to the SAMD09 still processing
// chunk N when chunk N+1 arrived, causing torn pixel buffer writes.
static const uint8_t TREL = 0x2E;

// Runtime global brightness (0..1). v2 hard-coded `*179>>8` (= 0.70) for
// power headroom on the 5V rail; v3 defaults lower and GLOBALS can change it.
float g_trellis_brightness = 0.45f;

// LED push is staged in trellisTick() (LED_CHUNK data bytes per bus transaction, LED_GAP_US between them, then SHOW): the old blocking
// ntShow() was 2 x 28-byte writes + 2 x 2 ms spin = ~8 ms of bus per frame, during which a pad hit sat unread in the seesaw FIFO.
static const uint8_t  LED_CHUNK  = 4;      // data bytes per BUF write (13 wire bytes ~1.5 ms at 8, ~1.0 ms at 4; was 24 data bytes ~3 ms); 12 chunks + SHOW
static const uint32_t LED_GAP_US = 2000;   // SAMD09 must finish storing chunk N before chunk N+1 arrives (2026-08: torn pixels at 1 ms)

// Enable all 16 keys for FALLING (press) + RISING (release) events.
//
// Key mapping (from Adafruit_NeoTrellis.h NEO_TRELLIS_KEY macro):
//   Seesaw GPIO pin = (logical_key / 4) * 8 + (logical_key % 4)
//   Keys 0-3 → pins 0-3 | keys 4-7 → pins 8-11 | keys 8-11 → pins 16-19 | keys 12-15 → pins 24-27
//   Must use seesaw pin, NOT logical key number, in the enable command.
//
// keyState byte = (ACTIVE << 1) | STATE
//   STATE  bit 0 = 1 (enable)
//   ACTIVE bits 4:1 — bitmask of edges: bit2=EDGE_FALLING(press), bit3=EDGE_RISING(release)
//   ACTIVE = (1<<EDGE_FALLING)|(1<<EDGE_RISING) = (1<<2)|(1<<3) = 12
//   keystate byte = (12 << 1) | 1 = 25 = 0x19
static void ntEnableKeys() {
    for(uint8_t k=0;k<16;k++) {
        uint8_t seesaw_pin = (k/4)*8 + (k%4);
        uint8_t cmd[4]={0x10,0x01,seesaw_pin,0x19};
        tbus.send(TREL,cmd,4);
    }
}

// ntInit — boot only. SW reset puts seesaw in known state; 500ms for firmware reload.
bool ntInit() {
    const uint8_t rst[]={0x00,0x7F,0xFF}; tbus.send(TREL,rst,3);
    usbDelay(500);

    if(tbus.ssR1(TREL,0x00,0x01) != 0x55) return false;

    const uint8_t pin[] ={0x0E,0x01,3};         tbus.send(TREL,pin, 3); usbDelay(10);
    const uint8_t spd[] ={0x0E,0x02,0x01};      tbus.send(TREL,spd, 3); usbDelay(10);
    const uint8_t blen[]={0x0E,0x03,0x00,0x30}; tbus.send(TREL,blen,4); usbDelay(10);

    uint8_t blank[28]={};
    blank[0]=0x0E; blank[1]=0x04;
    tbus.send(TREL,blank,28); usbDelay(2);
    blank[3]=24;
    tbus.send(TREL,blank,28); usbDelay(2);
    const uint8_t sh[]={0x0E,0x05}; tbus.send(TREL,sh,2);
    usbDelay(10);

    ntEnableKeys();
    return true;
}

// ntSoftInit — main-loop recovery only. No SW reset, no 500ms wait.
// After seesaw watchdog fires, SAMD09 reboots cleanly. Re-send neopixel + keypad config.
// Delays cut to 2ms (register writes only need I2C settle time, not 10ms).
// ntEnableKeys has no added delay — I2C timing itself paces the commands.
// Total: ~20ms vs ~60ms old version.
static bool ntSoftInit() {
    if(tbus.ssR1(TREL,0x00,0x01) != 0x55) return false;

    const uint8_t pin[] ={0x0E,0x01,3};         tbus.send(TREL,pin, 3); System::Delay(2);
    const uint8_t spd[] ={0x0E,0x02,0x01};      tbus.send(TREL,spd, 3); System::Delay(2);
    const uint8_t blen[]={0x0E,0x03,0x00,0x30}; tbus.send(TREL,blen,4); System::Delay(2);

    uint8_t blank[28]={};
    blank[0]=0x0E; blank[1]=0x04;
    tbus.send(TREL,blank,28); System::Delay(2);
    blank[3]=24;
    tbus.send(TREL,blank,28); System::Delay(2);
    const uint8_t sh[]={0x0E,0x05}; tbus.send(TREL,sh,2);
    System::Delay(2);

    ntEnableKeys();
    return true;
}

// ============================================================
// Module state
// ============================================================
volatile uint32_t g_kp_polls, g_kp_stretch, g_kp_down;   // keypad polls this window, bus stretch errors / trellis-down drops (cumulative)
extern volatile uint32_t g_lp_max_us[12];
volatile uint8_t g_kp_skip_gap = 0;
static bool     trel_ok   = false;
static uint8_t  frame[48] = {};     // next frame to push: PHYSICAL index, brightness applied
static uint16_t held_phys = 0;      // physically held pads (LOGICAL index), for release-all on drop

// ---- keypad poll state machine ---------------------------------------------------------------------------------------------------
// The seesaw needs STOP + >= 500 us between a request and its reply. Spinning there (old ssR1: 2 x 0.5 ms per poll) idled the CPU for a
// millisecond per poll; now the request goes out, the main loop does other work for 500 us, and the reply is read on a later pass.
// While a request is outstanding NOTHING else may use the trellis bus (LED chunks, health checks wait for KP_IDLE).
//   KP_IDLE -> (poll due) request -> KP_CNT -> (+500 us) read count -> [count > 0: request FIFO -> KP_FIFO -> (+500 us) read events] -> KP_IDLE
enum KPh : uint8_t { KP_IDLE, KP_CNT, KP_HW, KP_FIFO };
static KPh      kph = KP_IDLE;
static uint32_t kph_us = 0;         // GetUs() when the pending request was sent
static uint32_t next_key_us = 0;    // earliest start of the next poll
static uint8_t  k_count = 0;
static uint32_t s_poll_prev_us = 0;   // (ticks)
static const uint32_t KEY_POLL_US  = 2000;   // microseconds; converted with TU() at use   // keypad poll period (was 20 ms): worst-case detection delay is this + one loop piece + the 2 x ~1 ms request/reply chain
static const uint32_t KEY_WAIT_US  = 500;    // seesaw reply preparation time (library: 500 us for COUNT, 1000 us for FIFO; 500 us is what this firmware has always used for both)
// Time base: raw timer TICKS, not System::GetUs() -- the 32-bit timer wraps every ~21 s at its tick rate and GetUs() (ticks / divisor) jumps
// backwards at the wrap (same trap as audio_callback.cpp's CPU meter; first version of this poll froze for 13 s after boot on it). Unsigned
// tick differences are wrap-safe; all values below named *_us are kept in ticks via TU().
static inline uint32_t usNow() { return System::GetTick(); }
static inline uint32_t TU(uint32_t us) { static uint32_t tpu = 0; if(!tpu) tpu = System::GetTickFreq() / 1000000u; return us * tpu; }
static inline void holdKeysMs(uint32_t ms) { uint32_t t = usNow() + TU(ms * 1000u); if((int32_t)(t - next_key_us) > 0) next_key_us = t; }

// ---- rotation: ONE remap table. LOGICAL pad L (what the UI sees, row-major,
// 0 = top-left in the current orientation) sits at PHYSICAL pad P = ROT[q][L].
// Quarter turns are clockwise. Keys use the inverse of the same table.
static uint8_t rot_q = 0;
static uint8_t log2phys[16];        // ROT[rot_q]
static uint8_t phys2log[16];        // inverse
static void buildRotation(uint8_t q) {
    for(uint8_t l = 0; l < 16; l++) {
        uint8_t r = l / 4, c = l % 4;
        for(uint8_t t = 0; t < q; t++) { uint8_t nr = c, nc = 3 - r; r = nr; c = nc; }
        log2phys[l] = (uint8_t)(r * 4 + c);
    }
    for(uint8_t l = 0; l < 16; l++) phys2log[log2phys[l]] = l;
}

void trellisSetRotation(uint8_t quarter_turns) {
    rot_q = quarter_turns & 3;
    buildRotation(rot_q);
}

bool trellisOk() { return trel_ok; }

void trellisBootInit() {
    buildRotation(rot_q);
    next_key_us = usNow();
    // NeoTrellis init LAST — prevents SDMMC slowness from resetting freshly-inited seesaw
    trel_ok = ntInit();
}

// Next frame to push: applies the rotation remap (LED index) and the global
// brightness. The diff + 80 ms cadence happen in trellisTick().
void trellisSetFrame(const uint8_t grb[48]) {
    int scale = (int)(g_trellis_brightness * 256.f + 0.5f);
    if(scale < 0) scale = 0;
    if(scale > 256) scale = 256;
    for(uint8_t l = 0; l < 16; l++) {
        uint8_t p = log2phys[l];
        // global brightness (was `*179>>8` = -30% in v2) — power headroom on the 5V rail
        frame[p*3+0] = (uint8_t)((grb[l*3+0] * scale) >> 8);
        frame[p*3+1] = (uint8_t)((grb[l*3+1] * scale) >> 8);
        frame[p*3+2] = (uint8_t)((grb[l*3+2] * scale) >> 8);
    }
}

// ── NeoTrellis — own schedule ────────────────────────────────────────────
// Recovery: probe every 250 ms when down; ntSoftInit when SAMD09 comes back after watchdog reset.
// LED push: staged (see LED_CHUNK / LED_GAP_US), one bus transaction per call, only while no keypad request is outstanding and the next keypad
// poll is not about to fall due. Frame-diff: nothing is sent for an unchanged frame.
static const uint32_t LED_FIT_US = 700;    // start an LED transaction only if the next keypad poll is further away than this
static LedPusher SDRAM_BSS s_led;                    // scheduler (hw/led_push.h): background 12-chunk frame push + priority pixels for pad press/release
static bool      s_led_init = false;
volatile uint32_t g_led_lat_us_max = 0, g_led_lat_us_last = 0;   // pad event -> its LED written + SHOWn (priority path), us; STATUS led_us=

void trellisTick(uint32_t now) {
    if(!trel_ok) {
        // (events are being lost while the trellis is down — the release-all
        //  lives in trellisPollKeys() so it reaches the UI as pad events)
        kph = KP_IDLE; s_led.reset();
        // probe every 250ms, not every pass — the ~1.3ms I2C probe per
        // loop was dragging the whole loop (and the groove) when down
        static uint32_t next_probe = 0;
        if(now >= next_probe) {
            next_probe = now + 250;
            // I2C bus-clear first: 9 SCL pulses release a slave that died
            // holding SDA low (the classic stuck-bus wedge)
            tbus.sda(1);
            for(int p9 = 0; p9 < 9; p9++){
                tbus.scl(0); System::DelayUs(5);
                tbus.scl(1); System::DelayUs(5);
            }
            tbus.stop();
            if(tbus.ssR1(TREL,0x00,0x01) == 0x55) {
                trel_ok = ntSoftInit();
                if(trel_ok) next_key_us = usNow();
            }
        }
    }

    tud_task();   // service USB before NeoTrellis LED push (bit-banged I2C)
    if(!trel_ok) return;

    if(!s_led_init) { s_led.init(TU(LED_GAP_US), TU(LED_FIT_US), TU(40000u)); s_led_init = true; }
    {
        uint32_t us = usNow();
        auto r = s_led.step(now, us, kph == KP_IDLE, (int32_t)(next_key_us - us), frame, [](const uint8_t* pkt, uint8_t n) { return tbus.send(TREL, pkt, n); });
        if(r == LedPusher::SEND_FAILED) { trel_ok = false; g_kp_down = g_kp_down + 1; }
        else if(r == LedPusher::FULL_SHOW || r == LedPusher::URGENT_SHOW) {
            // After SHOW, SAMD09 drives WS2812B for ~530µs with I2C deaf — push the next keypad poll past that window.
            holdKeysMs(1);
            if(r == LedPusher::URGENT_SHOW) { g_led_lat_us_last = s_led.lat_last / TU(1); if(g_led_lat_us_last > g_led_lat_us_max) g_led_lat_us_max = g_led_lat_us_last; }
        }
    }
}

// ── Keypad poll — DECOUPLED from LED frames, every KEY_POLL_US (2 ms) ───
// Pad→sound latency was tied to the 80ms LED tick (up to ~80ms felt lag on drums), then to a 20 ms poll with ~8-10 ms of other
// main-loop work (OLED push, lidar, IMU) in front of it; now a 2 ms poll as a state machine (see KPh) with all of that work cut into
// pieces of <= ~1 ms.
static uint32_t s_last_key_activity = 0;
static void releaseHeld(PadCb cb) {
    if(!held_phys) return;
    uint16_t m = held_phys; held_phys = 0;
    for(uint8_t k = 0; k < 16; k++) if((m >> k) & 1u) { if(cb) cb(k, false); }
}
// STUCK-PAD FIXES (EMGOR 2026-09-15): releases were being lost three ways —
//  (1) only 8 FIFO events per poll (fast playing overflowed; a count >= 32 was thrown away entirely) -> up to 32 per poll, and if more are
//      waiting the next poll runs immediately;
//  (2) a bus stretch error dropped the poll's events (a release among them = stuck forever) -> every held pad is RELEASED on a bus error
//      (v2 "trellis-down release"), a cut beats a drone;
//  (3) no safety net -> pads "held" > 8 s with zero keypad activity are force-released.
static void keyBusFault(PadCb cb) {
    g_kp_stretch = g_kp_stretch + 1;
    tbus.stretch_err = false;
    tbus.recover();
    kph = KP_IDLE;
    next_key_us = usNow() + TU(250000u);
    releaseHeld(cb);
}
void trellisPollKeys(uint32_t now, PadCb cb) {
    // safety net (3): a "held" pad with no keypad activity at all for 8 s is a lost release
    if(held_phys && (now - s_last_key_activity) > 8000u) releaseHeld(cb);
    if(!trel_ok) {
        // events are being lost while the trellis is down — release
        // everything so nothing can drone or stay punched in
        kph = KP_IDLE;
        releaseHeld(cb);
        return;
    }
    uint32_t us = usNow();
    switch(kph) {
    case KP_IDLE: {
        if((int32_t)(us - next_key_us) < 0) return;
        tud_task();   // service USB before the NeoTrellis key request
        if(g_kp_skip_gap) { g_kp_skip_gap = 0; s_poll_prev_us = 0; }   // STATUS handling pass: host request work, not loop work
        if(s_poll_prev_us) { uint32_t d = us - s_poll_prev_us; uint32_t du = d / TU(1); if(du < 1000000u && du > g_lp_max_us[11]) g_lp_max_us[11] = du; }
        s_poll_prev_us = us;
        g_kp_polls = g_kp_polls + 1;
        next_key_us = us + TU(KEY_POLL_US);
        // every 3 s the request is the seesaw hardware-ID read instead of the key count: catches silent wedges that LED-write ACKs miss
        static uint32_t next_hw = 0;
        if(now >= next_hw) { next_hw = now + 3000; tbus.ssCmd(TREL, 0x00, 0x01); kph = KP_HW; }
        else               { tbus.ssCmd(TREL, 0x10, 0x04);                       kph = KP_CNT; }
        kph_us = usNow();
        return;
    }
    case KP_CNT:
    case KP_HW: {
        if(us - kph_us < TU(KEY_WAIT_US)) return;
        uint8_t v = tbus.ssGet1(TREL);
        if(kph == KP_HW) {
            kph = KP_IDLE;
            if(v != 0x55) { trel_ok = false; g_kp_down = g_kp_down + 1; }
            return;
        }
        if(tbus.stretch_err) { keyBusFault(cb); return; }
        if(v > 0) { k_count = v; tbus.ssCmd(TREL, 0x10, 0x10); kph = KP_FIFO; kph_us = usNow(); }
        else      kph = KP_IDLE;
        return;
    }
    case KP_FIFO: {
        if(us - kph_us < TU(KEY_WAIT_US)) return;
        uint8_t toread = (k_count > 32) ? 32 : k_count;
        uint8_t kbuf[32];
        tbus.ssGetN(TREL, kbuf, toread);
        kph = KP_IDLE;
        if(tbus.stretch_err) { keyBusFault(cb); return; }
        if(k_count > toread) next_key_us = usNow();     // more waiting: drain again immediately
        s_last_key_activity = now;
        for(uint8_t i=0;i<toread;i++){
            // NUM field is seesaw GPIO pin — convert to logical key 0-15
            // NEO_TRELLIS_SEESAW_KEY: (pin/8)*4 + (pin%8)
            uint8_t seesaw_pin = kbuf[i] >> 2;
            uint8_t key  = (seesaw_pin/8)*4 + (seesaw_pin%8);
            uint8_t edge = kbuf[i] & 0x03;
            if(key < 16) {
                key = phys2log[key];                 // rotation remap (keys)
                s_led.markPad(log2phys[key], usNow());    // the pad's LED change goes out ahead of the background frame push
                if(edge == 3) {                          // EDGE_RISING = pressed
                    held_phys |=  (uint16_t)(1u << key);
                    if(cb) cb(key, true);
                } else if(edge == 2) {                   // EDGE_FALLING = released
                    held_phys &= (uint16_t)~(1u << key);
                    if(cb) cb(key, false);
                }
            }
        }
        return;
    }
    }
}
