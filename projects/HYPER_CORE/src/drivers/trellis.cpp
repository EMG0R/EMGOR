/* HYPER_CORE import
 * origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_live/hw/trellis.cpp
 * commit : PMOR_V3_live ff86e4c
 * changes: includes -> hyper_hw.h and bbus.h (dropped hw_priv.h, ui/render.h); g_trellis_brightness is defined here and declared in hyper_hw.h. Protocol, recovery, rotation table and keypad logic unchanged
 * licence: Emory (own work) */
// trellis.cpp — NeoTrellis seesaw driver. VERBATIM from PMOR_V2.cpp:
//   protocol notes + ntShow/ntEnableKeys/ntInit/ntSoftInit   L293–438
//   recovery / watchdog / LED push / keypad FIFO poll        L4376–4614
// reorganised behind hw.h's trellisTick / trellisSetFrame / trellisPollKeys /
// trellisSetRotation. The 90° rotation is ONE remap table applied to the LED
// index in trellisSetFrame() and to the key decode in trellisPollKeys() —
// nowhere else. The v2 `*179>>8` global scale is replaced by the runtime
// g_trellis_brightness (declared here, default 0.45).
#include <string.h>
#include "hyper_hw.h"
#include "bbus.h"
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

// Returns true if SAMD09 ACKed both chunks (false = device watchdog'd, caller sets trel_ok=false)
static bool ntShow(const uint8_t* grb48) {
    bool ok = true;
    for(uint8_t chunk=0;chunk<2;chunk++) {
        uint16_t off=(uint16_t)chunk*24;
        uint8_t pkt[28];
        pkt[0]=0x0E; pkt[1]=0x04;
        pkt[2]=off>>8; pkt[3]=off&0xFF;
        for(uint8_t i=0;i<24;i++) pkt[4+i]=grb48[off+i];
        if(!tbus.send(TREL,pkt,28)) { ok=false; break; }
        System::Delay(2);
    }
    if(ok) {
        const uint8_t sh[]={0x0E,0x05};
        tbus.send(TREL,sh,2);
    }
    return ok;
}

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
static bool     trel_ok   = false;
static uint32_t next_trel = 0;      // LED push schedule
static uint32_t next_keys = 0;      // keypad poll schedule
static uint8_t  frame[48] = {};     // next frame to push: PHYSICAL index, brightness applied
static uint16_t held_phys = 0;      // physically held pads (LOGICAL index), for release-all on drop

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
// Recovery: poll ssR1 every pass when down (~1.3ms overhead); ntSoftInit
// when SAMD09 comes back after watchdog reset.
void trellisTick(uint32_t now) {
    if(!trel_ok) {
        // (events are being lost while the trellis is down — the release-all
        //  lives in trellisPollKeys() so it reaches the UI as pad events)
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
            if(tbus.ssR1(TREL,0x00,0x01) == 0x55)
                trel_ok = ntSoftInit();
        }
    } else {
        // health watchdog: every 3s verify the SAMD09 still answers —
        // catches silent wedges that ntShow's ACK check misses
        static uint32_t next_chk = 0;
        if(now >= next_chk) {
            next_chk = now + 3000;
            if(tbus.ssR1(TREL,0x00,0x01) != 0x55) trel_ok = false;
        }
    }

    tud_task();   // service USB before NeoTrellis LED push (bit-banged I2C, ~28-byte writes)
    if(trel_ok && now >= next_trel) {
        next_trel = now + 80;   // gentler bus pacing — fewer SAMD09 overruns

        // only send frames that actually changed — most are identical in
        // seq/FX views, so this slashes I2C traffic (= fewer SAMD09 upsets)
        static uint8_t grb_prev[48] = {1};
        bool lchg = (memcmp(frame, grb_prev, 48) != 0);
        if(lchg) memcpy(grb_prev, frame, 48);
        bool show_ok = lchg ? ntShow(frame) : true;
        if(!show_ok) {
            trel_ok = false;
        } else if(lchg) {
            // After SHOW, SAMD09 drives WS2812B for ~530µs with I2C deaf —
            // push the next keypad poll past that window.
            System::Delay(1);
            next_keys = now + 3;
        }
    }
}

// ── Keypad poll — DECOUPLED from LED frames, every 20ms ─────────────────
// Pad→sound latency was tied to the 80ms LED tick (up to ~80ms felt
// lag on drums); polling on its own fast schedule cuts that ~4x.
static uint32_t s_last_key_activity = 0;
void trellisPollKeys(uint32_t now, PadCb cb) {
    // safety net (3): a "held" pad with no keypad activity at all for 8 s is a lost release
    if(held_phys && (now - s_last_key_activity) > 8000u) {
        uint16_t m = held_phys; held_phys = 0;
        for(uint8_t k = 0; k < 16; k++) if((m >> k) & 1u) { if(cb) cb(k, false); }
    }
    if(!trel_ok) {
        // events are being lost while the trellis is down — release
        // everything so nothing can drone or stay punched in
        if(held_phys) {
            uint16_t m = held_phys; held_phys = 0;
            for(uint8_t k = 0; k < 16; k++) if((m >> k) & 1u) { if(cb) cb(k, false); }
        }
        return;
    }
    tud_task();   // service USB before NeoTrellis key FIFO read (bit-banged I2C + 500us waits)
    if(now >= next_keys) {
        next_keys = now + 20;
        {
            // Mask the OTG interrupt across the whole seesaw read
            // transaction (2026-08-16): keys-on kills USB in 40-120s
            // (rounds A/B: LED writes 300s clean, key reads dead at
            // 73s). Whatever the wedge mechanism is, the OTG ISR can't
            // interleave with a seesaw read while masked. ~1.5ms per
            // poll at 50Hz; the 16-packet audio FIFO rides it.
            // STUCK-PAD FIXES (EMGOR 2026-09-15): releases were being lost three ways —
            //  (1) only 8 FIFO events per 20 ms poll (fast playing overflowed; a count >= 32 was
            //      thrown away entirely) -> now up to 32 per poll, and if more are waiting the
            //      next poll runs on the very next loop pass instead of 20 ms later;
            //  (2) a bus stretch error dropped the poll's events (a release among them = stuck
            //      forever) -> now every held pad is RELEASED on a bus error (v2 "trellis-down
            //      release"), a cut beats a drone;
            //  (3) no safety net -> pads "held" > 8 s with zero keypad activity are force-released.
            uint8_t kcount = tbus.ssR1(TREL,0x10,0x04);
            uint8_t toread = 0;
            uint8_t kbuf[32];
            if(!tbus.stretch_err && kcount > 0) {
                toread = (kcount > 32) ? 32 : kcount;
                tbus.ssRN(TREL,0x10,0x10,kbuf,toread);
                if(kcount > toread) next_keys = now;     // more waiting: drain again immediately
                s_last_key_activity = now;
            }
            if(tbus.stretch_err) {
                tbus.stretch_err = false;
                tbus.recover();
                toread = 0;
                next_keys = now + 250;
                if(held_phys) {                           // never leave a pad stuck on a lost release
                    uint16_t m = held_phys; held_phys = 0;
                    for(uint8_t k = 0; k < 16; k++) if((m >> k) & 1u) { if(cb) cb(k, false); }
                }
            }
            if(toread) {
                for(uint8_t i=0;i<toread;i++){
                    // NUM field is seesaw GPIO pin — convert to logical key 0-15
                    // NEO_TRELLIS_SEESAW_KEY: (pin/8)*4 + (pin%8)
                    uint8_t seesaw_pin = kbuf[i] >> 2;
                    uint8_t key  = (seesaw_pin/8)*4 + (seesaw_pin%8);
                    uint8_t edge = kbuf[i] & 0x03;
                    if(key < 16) {
                        key = phys2log[key];                 // rotation remap (keys)
                        if(edge == 3) {                          // EDGE_RISING = pressed
                            held_phys |=  (uint16_t)(1u << key);
                            if(cb) cb(key, true);
                        } else if(edge == 2) {                   // EDGE_FALLING = released
                            held_phys &= (uint16_t)~(1u << key);
                            if(cb) cb(key, false);
                        }
                    }
                }
            }
        }
    }
}
