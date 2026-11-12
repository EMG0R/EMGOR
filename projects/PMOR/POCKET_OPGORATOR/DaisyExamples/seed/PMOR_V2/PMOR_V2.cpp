// POCKETGORATOR_CODE.cpp — Pocket OpGorator main firmware (was HelloOled.cpp)
//
// All I2C is bit-bang (open-drain GPIO) — libDaisy HAL I2C has an infinite busy-wait
// bug in TransmitBlocking that permanently hangs the CPU. Bit-bang is bounded: worst
// case is a NACK, never a hang.
//
// Two independent I2C buses:
//   tbus — NeoTrellis only          — D11(SCL=PB5), D12(SDA=PB8)
//   sbus — MPU9250/AK8963/VL53L1X  — D13(SCL=PB9), D14(SDA=PB6)
//
// Seesaw protocol (from Adafruit_seesaw.h source):
//   STATUS_HW_ID     = reg 0x01 in base 0x00 → returns 0x55 for SAMD09
//   NEOPIXEL_SHOW    = {0x0E, 0x05} exactly 2 bytes (3rd byte corrupts protocol)
//   KEYPAD_EVENT     = {0x10, 0x01, key, 0x07} to enable press+release on each key
//   KEYPAD_COUNT     = ssR1(0x10, 0x04) → events waiting in FIFO
//   KEYPAD_FIFO      = ssRN(0x10, 0x10, buf, n) → pop n events; bits[7:2]=key, bits[1:0]=edge
//   Chunk delay      = 2ms (was 500µs/1ms — extra margin prevents SAMD09 buffer overrun glitches)
//   Brightness       = 1/4 — 16 WS2812B at half-brightness risk brownout on 5V rail
//
// Recovery design — two distinct init paths:
//   ntInit()      — boot only. SW reset + 500ms wait + configure + keypad enable.
//   ntSoftInit()  — main loop only. No SW reset. Re-configures registers after seesaw
//                   watchdog reset. Takes ~50ms instead of 500ms → no visible freeze.
//
// ════════ ARCHITECTURE — two planes ════════
// REAL-TIME PLANE (audio interrupt, 48kHz / 64-sample blocks, preempts all):
//   gridTick()    — the WHOLE sequencer: master 16th clock (absolute deadlines,
//                   constant step_ms, NO effect may bend it), step content +
//                   firing, grid-locked stutters, BZZ roll, varispeed shadow
//                   position, FX automation playback, stuck-note safeties.
//                   Jitter ≤ 1.3ms regardless of what the UI is doing.
//   AudioCallback — voices → master SVF (accel cutoff) → drive → chorus →
//                   drums/sampler → drum SVF → master vol tanh → punch-in FX
//                   (REV/CHO/PAN/MUT/DST/CRUSH) → hard ±1 limiter.
// CONTROL PLANE (main loop, can block for ms — never affects timing):
//   trellis I2C (events + LEDs, frame-diff skipped), sensors, OLED, encoders.
//
// MENUS (left enc rotates; L-click = back out / FX-page = wipe automation):
//   SYNTH / DRUMS / SAMPLER — MAN/MCH/EUC radio + TAP. Drum+sampler MCH = the
//     PO step sequencer submenu (R-turn = write target, R-click = place mode).
//   FX   — pads = hold-to-engage punch-ins, every one scales with FSR squeeze:
//          S8 S8T S16 S32 / REV CHO PAN CRS / HLF DBL BZZ MUT / FLT WSH DST STP
//          Hold R-enc = record automation per step (empty hands scrub clean).
//   DASH — full sensor readout. FRACTAL = screensaver after 10s encoder idle.
//   Hold BOTH encoder buttons = master volume mode (pads = fader, >100% = tan).
//
// SYNTH VOICE (Mid-Air Thief vibe): ONE pitch per note (pure detune, no
// octave doubling, no intervals), saw/tri stack or harmonic 2-op FM, per-note
// VCF envelope (stable ranges + hard state clamps — cannot self-oscillate),
// krell drift, 10% glide notes, pluck/bloom/swell attacks. Low octave fixed,
// lidar currently unhooked from pitch. Fixed drive 2.5x + chorus 0.55.
// ⚠ HARDWARE PAN: this unit's L/R is mirrored vs the pad layout — synth pan
//   is SWAPPED at the mix (mixL gets .pan). Keep it that way.
//
// SAFETY INVARIANTS (never remove):
//   master clock untouchable by FX; trellis-down = all-notes-off + FX clear;
//   orphan-voice sweep every tick; 8s absolute gate cap; rt_div==0 means
//   stutter OFF (a min-clamp once promoted 0→6ms = infinite retrig bug);
//   NaN flush on all filter states; hard ±1 output limiter.
//
// OLED UI (128×64), 5 menus + screensaver. Trellis: lava idle, step grid in
// seq views, FX colors by row, sampler slot map, volume fader.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "VL53L1X_mini.h"
#include "_fsr_scale.h"
#include "fatfs.h"
#include "util/PersistentStorage.h"
#include "ff_gen_drv.h"

// ── USB composite (UAC2 capture + USB-MIDI + CDC-ACM) + MIDI-clock/role
// engine — ported from firmware-tinyusb/ (proven on hardware there), see
// usb/*.c. C linkage; TinyUSB's own headers are plain C. board_usb_init()
// MUST run before the OLED (see main(), "USB before OLED" below) — that
// ordering (settle delay, then tusb_init()) is the one thing that made USB
// enumeration reliable on the source project's hardware; skipping it there
// silently killed enumeration.
extern "C" {
#include "usb/board_stm32h7.h"
#include "tusb.h"
#include "usb/clock_engine.h"
#include "usb/usb_audio_stream.h"
}

using namespace daisy;
// SOFT SPI, not the SPI1 peripheral. Evidence: the instrument + USB audio +
// USB MIDI + CDC all run together fine, but the moment the OLED refreshes
// over hardware SPI the device drops off the bus instantly (reproduced live:
// "TOled 1" killed CDC mid-reply). Not tud_task starvation (per-page yields
// verified compiled in, no effect) and not panel current (min contrast, no
// effect) -- which points at the SPI1 peripheral itself.
// The NeoTrellis bus is bit-banged on adjacent pins and coexists with USB
// happily, so drive the display the same way. Same physical pins:
// SCK=D9(PG11), MOSI=D11(PB5), DC=D3, RST=D4.
#include "oled_pmor.h"
// PMOR's own bit-banged SSD1309 driver — see oled_pmor.h for why. The SPI1
// hardware peripheral kills USB the moment it transmits; bit-banging the same
// pins does not. This keeps libDaisy's OledDisplay<> graphics wrapper, so every
// existing Fill/SetCursor/WriteString/DrawRect/Update call is unchanged.
using MyOled = OledDisplay<pmor::BitBangSSD130x128x64>;

// Override libDaisy's weak per-page OLED yield so a screen refresh cannot
// starve TinyUSB. Update() blits the framebuffer as eight back-to-back
// BLOCKING SPI transfers; without this the host never finishes enumerating
// (measured: USB irq climbing past 20000 with tud_mounted() stuck at 0,
// while every other instrument subsystem coexisted with USB fine).
// Set at startup (see main): keeps a screen refresh from starving TinyUSB.

DaisySeed    hw;
MyOled       display;
VL53L1X_Mini lidar;
static FsrScale g_fsr;
// Full IMU state for the monitor page (ax alone drives the tilt filter).
volatile float g_ax=0,g_ay=0,g_az=0,g_gx=0,g_gy=0,g_gz=0;
// Boot timing marks (ms since reset) -- printed once over CDC so a slow boot
// can be attributed instead of guessed at.
volatile uint32_t g_t_usb=0,g_t_oled=0,g_t_sens=0,g_t_lidar=0,g_t_sd=0,g_t_boot=0;   // 330R + cubed + full-span ceiling

// Gates the USB-audio push made from inside the audio ISR (see the comment at
// the call site). Lives up here with the other globals because it is read
// from code that appears well before the probe-gate block.
// Restored to true. It was set false only for a one-off experiment that
// proved the USB-audio push is NOT what kills USB when the display runs.
// v2: USB AUDIO OFF. The UAC2 capture attempt is abandoned -- USB carries
// MIDI + CDC only now. Left as a runtime flag rather than ripped out of the
// descriptors, so this is a one-word revert and nothing else in the USB
// stack shifts. (Clock over USB MIDI is the proven path: 120.046 BPM.)
volatile bool g_en_usbaudio = false;
// USB capture test tone (see usbCapturePush). Toggled over CDC: SINE1/SINE0.
volatile bool g_usb_sine = false;
extern "C" void pmor_sine_set(int on) { g_usb_sine = (on != 0); }

// ============================================================
// Bit-bang I2C  (open-drain GPIO, 100 kHz)
// ============================================================
// Every public transaction below pumps the USB plane once on entry: a
// trellis LED push is ~50 bytes (~5-10ms) and a sensor pass strings several
// multi-byte reads together — unpumped, those bursts overflowed the USB
// audio ring (~4% capture dropouts measured 2026-08-16 once the lidar rail
// was powered). One pump per transaction bounds the feed gap to a single
// transfer. All callers are main-loop context; pmor_usb_service never
// touches these buses, so it cannot re-enter.
extern "C" void pmor_usb_service(void);
extern "C" void dcd_int_disable(uint8_t rhport);
extern "C" void dcd_int_enable(uint8_t rhport);
struct BBus {
    dsy_gpio scl_g, sda_g;
    uint8_t  us = 5;   // half-bit delay — sensors run faster than the trellis

    void init(dsy_gpio_pin sp, dsy_gpio_pin dp) {
        scl_g = {sp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        sda_g = {dp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        dsy_gpio_init(&scl_g); dsy_gpio_init(&sda_g);
        scl(1); sda(1); System::Delay(2);
    }
    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { System::DelayUs(us); }

    // After releasing SCL (open-drain high), the slave can hold it low (clock stretch)
    // while its ISR processes the previous byte. Poll IDR until the pin is actually high
    // before starting the SCL-high hold period. Without this, we charge ahead during
    // the stretch and the slave samples wrong bit values → corrupted pixel data.
    // Sticky stretch-timeout flag: set when a slave held SCL past the
    // patient wait below. The keypad poll checks it, runs recover(), and
    // backs off — instead of clocking into a desynced bus, which turns
    // every later byte into a full-bound spin (the 2026-08-16 keys-on
    // USB killer: unpumped multi-ms spin storms every 20ms poll).
    bool stretch_err = false;
    void waitScl() {
        for(uint32_t i=10000; i && !dsy_gpio_read(&scl_g); --i) {}
        if(!dsy_gpio_read(&scl_g)) {
            // Long stretch: wait patiently (up to 5ms) and keep USB fed —
            // the master is idle here, so pumping is safe.
            uint32_t t0 = System::GetNow();
            while(!dsy_gpio_read(&scl_g) && System::GetNow() - t0 < 5)
                pmor_usb_service();
            if(!dsy_gpio_read(&scl_g)) stretch_err = true;
        }
    }
    // I2C bus recovery: 9 clock pulses lets a slave stuck mid-byte shift
    // out whatever it thinks it owes, then a STOP resets every state
    // machine on the bus.
    void recover() {
        sda(1);
        for(int i=0;i<9;i++){ scl(0); hd(); scl(1); hd(); }
        stop(); hd();
    }

    void start() { sda(1);hd();scl(1);hd();sda(0);hd();scl(0);hd(); }
    void stop()  { sda(0);hd();scl(1);hd();sda(1);hd(); }

    bool byteW(uint8_t b) {
        pmor_usb_service();   // per-BYTE pump: bounds any USB feed gap to
                              // ~one I2C byte (~0.1ms) even inside long
                              // trellis/sensor transactions
        for(int i=7;i>=0;i--){
            sda((b>>i)&1); hd();
            scl(1); waitScl(); hd(); // wait for stretch, then proper hold time
            scl(0);
        }
        sda(1); hd();
        scl(1); waitScl(); hd();
        bool ack=(dsy_gpio_read(&sda_g)==0);
        scl(0); hd(); return ack;
    }
    uint8_t byteR(bool nack) {
        // NO pump here (removed 2026-08-16): pausing the master mid-READ,
        // while the seesaw slave is actively driving SDA, is the one bus
        // state writes never enter — and reads are the empirically fatal
        // transaction class. byteW keeps its pump.
        uint8_t v=0; sda(1);
        for(int i=7;i>=0;i--){
            hd(); scl(1); waitScl(); hd();
            if(dsy_gpio_read(&sda_g)) v|=(1<<i);
            scl(0);
        }
        sda(nack?1:0); hd(); scl(1); waitScl(); hd(); scl(0); hd(); sda(1);
        return v;
    }

    // Returns true if device ACKed the address byte (false = device gone)
    bool send(uint8_t a7, const uint8_t* d, uint8_t n) {
        pmor_usb_service();
        start(); bool ok=byteW((a7<<1)|0);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd(); return ok;
    }
    void w8(uint8_t a7, uint8_t reg, uint8_t val) {
        uint8_t b[2]={reg,val}; send(a7,b,2);
    }
    uint8_t r8(uint8_t a7, uint8_t reg) {
        pmor_usb_service();
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    int16_t r16be(uint8_t a7, uint8_t reg) {
        pmor_usb_service();
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t hi=byteR(false),lo=byteR(true);
        stop();hd(); return (int16_t)((hi<<8)|lo);
    }
    void w16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        pmor_usb_service();
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }
    void r16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        pmor_usb_service();
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) d[i]=byteR(i==n-1);
        stop();hd();
    }

    // Seesaw read: write cmd → full STOP → 500µs gap → new START → read bytes.
    // SAMD09 requires full STOP (not repeated-START) + ~500µs to prepare response.
    uint8_t ssR1(uint8_t a7, uint8_t base, uint8_t reg) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500);
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    void ssRN(uint8_t a7, uint8_t base, uint8_t reg, uint8_t* out, uint8_t n) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) out[i]=byteR(i==n-1);
        stop();hd();
    }
};

static BBus tbus; // NeoTrellis — D11=SCL, D12=SDA
static BBus sbus; // Sensors    — D13=SCL, D14=SDA

extern "C" void pmor_usb_service(void);   // defined near the CDC test hooks

// Pump the USB plane around every lidar register op. If the sensor is
// unresponsive, each bit-banged transaction burns ~100ms in bus timeouts;
// a full re-init is ~40 ops ≈ 4-5 SECONDS — measured as 4.8s MIDI-clock
// holes every watchdog retry (2026-08-16). With the pump, even that
// pathological path can't gap the clock or starve USB audio.
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg (uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.r16a(0x29,reg,d,n); }

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

// Lava lamp palette: light blue (dominant), green, yellow, touch of purple.
// 8 key colors interpolated across the 0-255 phase cycle.
static void lavaColor(uint8_t phase, uint8_t& r, uint8_t& g, uint8_t& b) {
    static const uint8_t P[8][3] = {
        { 70, 150, 255},  // light blue
        { 75, 175, 240},  // soft blue
        {240, 235,  75},  // light yellow
        {255, 215,  65},  // warm yellow
        {245, 240,  80},  // cool yellow
        { 80, 150, 255},  // light blue
        {155,  65, 245},  // blue-purple
        {185,  65, 220},  // light purple
    };
    uint8_t i = phase >> 5, f = phase & 0x1F, j = (i + 1) & 7;
    r = (uint8_t)((int)P[i][0] + (((int)P[j][0] - (int)P[i][0]) * f >> 5));
    g = (uint8_t)((int)P[i][1] + (((int)P[j][1] - (int)P[i][1]) * f >> 5));
    b = (uint8_t)((int)P[i][2] + (((int)P[j][2] - (int)P[i][2]) * f >> 5));
}

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
// ── USB-safe delay ───────────────────────────────────────────────────────
// System::Delay() is a hard spin: nothing else runs. That is fatal next to
// USB, because TinyUSB does its control-transfer work in tud_task(), not in
// the ISR. Block for ~500ms and the device stops answering the host, macOS
// gives up, and it vanishes from the bus.
//
// MEASURED 2026-08-14 by staged bring-up: chunk 6 (OLED + sensors + SD +
// encoders + patches + audio) enumerated fine; adding ntInit() -- whose
// first act is a seesaw reset followed by System::Delay(500) -- killed USB
// completely. Same code, one extra call.
//
// Use this anywhere a startup delay runs after USB is up.
extern "C" void pmor_usb_service(void);   // defined below main()'s helpers
static inline void usbDelay(uint32_t ms) {
    uint32_t t0 = System::GetNow();
    // Full USB service, not just tud_task(): the capture ring must keep
    // draining and MIDI clock must keep ticking through any wait, or the
    // host sees audio dropouts / BPM flicker during long delays.
    while(System::GetNow() - t0 < ms) { pmor_usb_service(); }
}

static bool ntInit() {
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
// Encoders
// ============================================================
struct Enc {
    dsy_gpio a, b, sw;
    uint8_t  lab;
    volatile uint8_t psw;   // current switch level (0 = held) — read by main loop
    volatile int8_t delta;
    volatile bool   clicked;
};
static Enc  enc1, enc2;
static bool enc_ready = false;

// ── JOYSTICK REPLACES ENC2 (v2, Seed 3) ──────────────────────────────────
// The analog stick drives the SAME Enc struct enc2 always used, so every
// existing UI call site (menu nav, FX param, tap tempo, record-hold...)
// works unchanged. Only the source of enc2.delta/.clicked/.psw changed.
//
//   X axis  -> D16 (A1), adc ch 1  -> synthesised rotation into enc2.delta
//   Y axis  -> D17 (A2), adc ch 2  -> spare
//   button  -> D24, active low     -> enc2.psw / enc2.clicked
//
// joyPoll() is called from AudioCallback at 48000/64 = 750Hz, same as
// ePoll(enc2) was. Deflection is rate-based: the further you push, the faster
// detents come. Full deflection ~= 12 detents/sec, which feels like a scroll
// wheel rather than a machine gun.
static dsy_gpio joy_sw_g;

// Exported for the DASH page — real axis values, not synthesised detents.
volatile int16_t g_joy_x  = 0;    // -1000..+1000, centred and deadzoned
volatile int16_t g_joy_y  = 0;
volatile bool    g_joy_sw = false;
volatile int8_t  g_joy_nav = 0;   // Y detents, consume where submenu nav wants them

// Rest position, MEASURED. Calibration is deferred and averaged: joyPoll()
// runs from AudioCallback, which can start before the ADC has produced a real
// conversion. Sampling the very first call risks locking in garbage as
// "centre", after which an axis reads pinned or dead forever. So: skip the
// first 200 polls (~0.27s at 750Hz), then average 64 samples.
static uint16_t joy_cx = 32768, joy_cy = 32768;
static uint16_t joy_cal_n = 0;
static uint32_t joy_cal_ax = 0, joy_cal_ay = 0;
static bool     joy_cal = false;

// Centre, deadzone, then normalise to +/-1000. Centre is MEASURED at first
// poll rather than assumed to be mid-scale: these sticks never rest exactly
// at 32768, and assuming they do gives a permanent slow drift in one
// direction.
static inline int32_t joyAxis(uint16_t raw, uint16_t centre)
{
    const int32_t DZ = 3000;
    int32_t v = (int32_t)raw - (int32_t)centre;
    if(v > DZ)       v -= DZ;
    else if(v < -DZ) v += DZ;
    else             v  = 0;
    int32_t n = v * 1000 / (32768 - DZ);
    if(n >  1000) n =  1000;
    if(n < -1000) n = -1000;
    return n;
}

// THE JOYSTICK REPLACES ENC2 ONLY. enc1 is the physical encoder and is never
// written from here -- it keeps doing exactly what it always did.
//
//   X  -> enc2.delta   (the old right-encoder rotation role)
//   SW -> enc2.psw / enc2.clicked
//   Y  -> g_joy_y + g_joy_nav, a SEPARATE axis enc2 never had. Exposed for
//         the DASH page and available for submenu up/down. It does NOT feed
//         enc1 or change pages.
//
// Both axes are rate-based: hold it further, steps come faster. Full
// deflection is ~12 steps/sec at the 750Hz poll rate. Releasing to centre
// zeroes the accumulator so a scroll stops dead instead of coasting.
static void joyPoll()
{
    if(!joy_cal) {
        if(++joy_cal_n > 200) {                 // let the ADC settle first
            joy_cal_ax += hw.adc.Get(1);
            joy_cal_ay += hw.adc.Get(2);
            if(joy_cal_n >= 264) {              // 64 averaged samples
                joy_cx = (uint16_t)(joy_cal_ax >> 6);
                joy_cy = (uint16_t)(joy_cal_ay >> 6);
                joy_cal = true;
            }
        }
        g_joy_x = 0; g_joy_y = 0;
        g_joy_sw = (dsy_gpio_read(&joy_sw_g) == 0);
        return;                                  // no output until calibrated
    }

    int32_t nx = joyAxis(hw.adc.Get(1), joy_cx);
    int32_t ny = joyAxis(hw.adc.Get(2), joy_cy);
    g_joy_x = (int16_t)nx;
    g_joy_y = (int16_t)ny;

    static int32_t accx = 0, accy = 0;
    if(nx) accx += nx; else accx = 0;
    if(ny) accy += ny; else accy = 0;

    while(accx >=  64000) { enc2.delta++; accx -= 64000; }
    while(accx <= -64000) { enc2.delta--; accx += 64000; }
    // Y is its OWN axis. It does not touch enc1.
    while(accy >=  64000) { g_joy_nav--; accy -= 64000; }   // push up = previous
    while(accy <= -64000) { g_joy_nav++; accy += 64000; }

    uint8_t sw = dsy_gpio_read(&joy_sw_g);
    g_joy_sw = (sw == 0);
    if(enc2.psw && !sw) enc2.clicked = true;
    enc2.psw = sw;
}

static void ePoll(Enc& e) {
    static const int8_t lut[4][4]={{0,-1,+1,0},{+1,0,0,-1},{-1,0,0,+1},{0,+1,-1,0}};
    uint8_t ab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    if(ab!=e.lab){e.delta+=lut[e.lab][ab];e.lab=ab;}
    uint8_t sw=dsy_gpio_read(&e.sw);
    if(e.psw&&!sw) e.clicked=true;
    e.psw=sw;
}
static void eInit(Enc& e, dsy_gpio_pin pa, dsy_gpio_pin pb, dsy_gpio_pin ps) {
    e.a ={pa,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.b ={pb,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.sw={ps,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    dsy_gpio_init(&e.a); dsy_gpio_init(&e.b); dsy_gpio_init(&e.sw);
    e.lab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    e.psw=1; e.delta=0; e.clicked=false;
}

// ============================================================
// Generative supersaw synth — 4 voices, F Lydian 4 octaves
// F3-E7, 5-oscillator supersaw per voice, plucky envelope.
//
// 16th-note arp @ 120 BPM, polyphonic per slot:
//   STEP_MS = 125ms — one grid slot every 125ms.
//   Per slot: 10% silence, 57% 1-note, 18% 2-note, 8% 3-note, 7% 4-note.
//   Hold = 100ms, slow release (0.0005) → overlapping polyphonic tails.
//   Supersaw detune: ±12 cents spread across 5 oscillators per voice.
//   Stereo: random pan (15–85%) assigned per note trigger.
//   LPF: ax (accel X) controls cutoff — wide tilt sweep, slewed smooth.
//   Lidar: hand distance picks octave zone (base = 1 oct down, up to +2).
//   Output: tanh drive → stereo chorus → soft-clip makeup gain (loud).
//
// Polling rates (I2C noise budget):
//   Sensors    — 50ms — MPU9250/AK8963/VL53L1X
//   NeoTrellis — 60ms — seesaw pixel + keypad
// ============================================================
// Scale is GENERATED at runtime from the globals (root key + mode), 28 notes
// across 4 octaves. FLYD[] is the live table the synth reads; rebuilt by
// rebuildScale() whenever key or mode changes. Defaults to F Lydian (the
// prime vibe). 9 modes selectable from the Globals page.
static float FLYD[28];
static const uint8_t MODES[9][7] = {
    {0,2,4,5,7,9,11},   // Ionian (major)
    {0,2,3,5,7,9,10},   // Dorian
    {0,1,3,5,7,8,10},   // Phrygian
    {0,2,4,6,7,9,11},   // Lydian
    {0,2,4,5,7,9,10},   // Mixolydian
    {0,2,3,5,7,8,10},   // Aeolian (minor)
    {0,1,3,5,6,8,10},   // Locrian
    {0,2,3,5,7,8,11},   // Harmonic minor
    {0,2,4,7,9,9,9},    // Major pentatonic (5-note, padded)
};
static const char* MODE_NAMES[9] = {
    "IONIAN","DORIAN","PHRYGIAN","LYDIAN","MIXOLYD","AEOLIAN","LOCRIAN","HARM-MIN","PENTA"};
static int g_key  = 0;   // root transpose, semitones, -12..+12 (0 = F)
static int g_mode = 3;   // default Lydian
static void rebuildScale(){
    // base root = F3 (174.61Hz) shifted by g_key semitones
    float root = 174.61f * powf(2.f, (float)g_key / 12.f);
    const uint8_t* iv = MODES[g_mode];
    for(int i=0;i<28;i++){
        int deg = i % 7, oct = i / 7;
        int semis = (int)iv[deg] + 12*oct;
        FLYD[i] = root * powf(2.f, (float)semis / 12.f);
    }
}
static const float DETUNE[5] = {-0.012f, -0.006f, 0.f, +0.006f, +0.012f};

// ── Global musical params (Globals page) ─────────────────────
static int g_swing = 0;    // 0..9 → up to ~⅔ swing on the off-16ths
static int g_csyn  = 6;    // synth note-density chance 0..9 (6≈ busy)  [SYNTH page]
static int g_cpoly = 5;    // synth chord/poly chance 0..9 (5 = 50%)    [SYNTH page]
static int g_cdrum = 9;    // drum sequenced-hit chance 0..9 (9 = always) [DRUM page]
static int g_csmp  = 9;    // sampler sequenced-hit chance 0..9          [SAMPLER page]
// Synth waveform: 0 SAW 1 SQR 2 TRI 3 SIN 4 PULSE 5 THIEF 6 FM 7 WT 8 FOLD
//                 9 PLUCK (Karplus) 10 SUPER (supersaw) 11 CZ (phase distortion)
static int g_wave  = 5;
#define WAVE_COUNT 13
static const char* WAVE_NAMES[WAVE_COUNT] = {"SAW","SQUARE","TRI","SINE","PULSE","THIEF",
                                             "FM","WT","FOLD","PLUCK","SUPER","CZ","ARCADE"};
static int g_prise = 0;   // chance (0..9) a note glides up from -12 semis then falls back
// Karplus-Strong delay lines (one per voice) — SDRAM, low note ≈ 550 samples
static float DSY_SDRAM_BSS kpbuf[12][672];
// ── Wavetable engine ─────────────────────────────────────────────────────────
// A morphable bank of band-limited single-cycle waves (additive, harmonic-capped
// so they stay clean across our range — like Plaits / Eurorack wavetable VCOs).
// SCAN sweeps a continuous position through the bank (interpolating between
// neighbours); the note envelope also nudges it for built-in timbral movement.
#define WT_LEN 256
#define WT_N   8
static float DSY_SDRAM_BSS wtbl[WT_N][WT_LEN];
static int   g_wtscan = 0;            // 0..15 → morph position across the bank
static void buildWavetables(){
    const float TWO_PI = 6.2831853072f;
    for(int t=0;t<WT_N;t++){
        for(int i=0;i<WT_LEN;i++){
            float ph = (float)i/(float)WT_LEN, s=0.f;
            switch(t){
            case 0: s = sinf(TWO_PI*ph); break;                                   // pure sine
            case 1: for(int h=1;h<=15;h+=2){ float sg=((h>>1)&1)?-1.f:1.f;        // triangle
                        s += sg*sinf(TWO_PI*h*ph)/(float)(h*h); } break;
            case 2: for(int h=1;h<=8;h++)   s += sinf(TWO_PI*h*ph)/(float)h; break;  // soft saw
            case 3: for(int h=1;h<=24;h++)  s += sinf(TWO_PI*h*ph)/(float)h; break;  // bright saw
            case 4: for(int h=1;h<=23;h+=2) s += sinf(TWO_PI*h*ph)/(float)h; break;  // square
            case 5: { const float a[4]={1.f,.5f,.32f,.22f}; const int hh[4]={1,2,4,8}; // organ
                        for(int j=0;j<4;j++) s += a[j]*sinf(TWO_PI*hh[j]*ph); } break;
            case 6: for(int h=1;h<=12;h++){ float d=(float)(h-4);                  // formant (vowel)
                        s += expf(-0.35f*d*d)*sinf(TWO_PI*h*ph); } break;
            case 7: for(int h=1;h<=16;h++)  s += sinf(TWO_PI*h*ph)*(0.55f+0.45f/(float)h); break; // bright/harsh
            }
            wtbl[t][i]=s;
        }
        float pk=0.0001f; for(int i=0;i<WT_LEN;i++){ float a=fabsf(wtbl[t][i]); if(a>pk)pk=a; }
        float g=1.f/pk; for(int i=0;i<WT_LEN;i++) wtbl[t][i]*=g;
    }
}
// Drum kit selector: 0 = classic 808/phonk, 1 = electronic (alt timbres)
static int kit_sel = 0;
// Per-engine mix levels 0..12 = 0%..120% in 10% steps (10 = 100% unity).
// VOL param on each engine page (SYNTH/DRUM/SAMPLER).
static int g_vsyn = 10, g_vdrum = 10, g_vsmp = 10;
static inline float engVol(int v){ return (float)v * 0.10f; }   // 0.0 .. 1.2
// Mid-Air Thief voice: detuned saw/tri stack or 2-op FM pluck, each note
// through its own VCF envelope. STABLE BY CONSTRUCTION: cutoff/damping ranges
// sit inside the filter's stability region AND states are hard-clamped every
// sample — ringing cannot grow. The 20s absolute gate cap (release loop)
// guarantees nothing sustains forever even if a release event is lost.
struct Voice {
    float phase[5] = {};
    float ratio[5] = {1.f, 1.f, 1.f, 1.f, 1.f};
    float phaseInc = 0, env = 0, envTarget = 0, pan = 0.5f, relCoef = 0.001f;
    float atk = 0.08f;
    float drift = 0.f, driftT = 0.f;
    int   type = 0;                    // 0 saw stack, 1 tri stack, 2 FM pair, 20 WT, 23 fold, 24 pluck, 25 super, 26 CZ
    float fc = 0.2f, fcT = 0.1f, fcR = 0.0001f;
    float q  = 0.9f, lp = 0.f, bp = 0.f;
    float fmi = 0.f, fmiT = 0.f;
    int   kpos = 0, klen = 64; bool kinit = false;   // Karplus-Strong delay-line state
    int   nosc = 5;                    // oscillators in the classic stack (1 = pure mono)
    bool  prise = false;               // pitch-rise note: glides up from -12 then falls
    float pmul = 1.f, pmulT = 1.f;     // freq multiplier (0.5 = octave down) + its target
};
static inline float psin(float ph){
    float x = ph + ph - 1.f;
    return 4.f * x * (1.f - fabsf(x));
}
// ── Band-limited oscillator helpers (PolyBLEP / PolyBLAMP) ───────────────────
// The naive saw/square/pulse have hard steps that alias horribly up high (the
// "weird" sound). PolyBLEP rounds each step over one sample using a 2nd-order
// polynomial; PolyBLAMP does the same for the slope corners of a triangle. This
// is how DaisySP's BlOsc and most Eurorack digital VCOs stay clean. `t` is the
// phase 0..1, `dt` the phase increment (cycles/sample) of that oscillator.
static inline float polyBlep(float t, float dt){
    if(dt <= 0.f) return 0.f;
    if(t < dt){ t /= dt; return t + t - t*t - 1.f; }
    if(t > 1.f - dt){ t = (t - 1.f) / dt; return t*t + t + t + 1.f; }
    return 0.f;
}
static inline float polyBlamp(float t, float dt){
    if(dt <= 0.f) return 0.f;
    if(t < dt){ t = t/dt - 1.f; return -0.33333333f * t*t*t; }
    if(t > 1.f - dt){ t = (t - 1.f)/dt + 1.f; return 0.33333333f * t*t*t; }
    return 0.f;
}
static uint32_t vgate_t[12] = {};      // gate-open times — absolute sustain cap
static Voice     voices[12];
static uint32_t  lcg_st = 0xBEEF1234u;
static uint32_t  lcg()  { lcg_st=lcg_st*1664525u+1013904223u; return lcg_st; }
// Lidar octave zone, written by sensor poll, read at note-trigger time.
// 0 = base (1 octave below old tuning), 1/2 = hand raising pitch.
static int       oct_shift = 0;

// ── UI / mode state ──────────────────────────────────────────
// Primary menus (left encoder rotates): FRACTAL / SYNTH / DRUMS / DASH.
// SYNTH and DRUMS pages each own one engine: right encoder picks that engine's
// seq mode (MAN/MACH/EUC/SEQ) + TAP. Selecting SEQ dives into the PO-style
// sequencer submenu; LEFT CLICK always backs out of a submenu to the primary.
enum { MENU_SYNTH = 0, MENU_DRUM = 1, MENU_SMP = 2, MENU_FX = 3,
       MENU_SESS = 4, MENU_DASH = 5, MENU_GLOB = 6, MENU_COUNT = 7 };
static int      menu       = MENU_SYNTH;
// Fractal is no longer a menu — it is a SCREENSAVER that fades in after 10s
// of no encoder movement; any encoder action returns to the current page.
static uint32_t last_enc   = 0;
static uint32_t step_ms    = 125;    // shared 16th-note grid; set by tap tempo

// Both engines run in parallel; pads control the focused one (= last
// SYNTH/DRUMS page visited), so you can play while watching the fractal.
// Drum MACHINE *is* the PO step sequencer (merged — no random groove).
// Synth MACHINE stays the generative arp.
enum { ENG_SYNTH = 0, ENG_DRUM = 1, ENG_SMP = 2 };
enum { SEQ_MANUAL = 0, SEQ_MACHINE = 1, SEQ_EUCLID = 2 };
// Synth modes (stored in seq_mode[ENG_SYNTH]): 0 = step SEQUENCER (play live +
// records into the grid), 1 = ARP (continuous arpeggio over the same recorded
// sequence), 2 = GEN (the generative engine). Drums & sampler now have a SINGLE
// unified mode — play live AND lay steps in the same mode (no Euclid).
enum { SYN_SEQ = 0, SYN_ARP = 1, SYN_GEN = 2 };
// Sequencer can run up to 8 bars × 16 beats = 128 steps. Pads show one 16-step
// PAGE at a time; the view pages through the song as it plays (PO-style).
#define MAXSTEPS 128
// Polyphonic synth step sequencer: syn_seq[step] is a 16-bit mask over the 16
// playable pads (bit p = pad p's note sounds on that step) → chords per step.
static uint16_t syn_seq[MAXSTEPS] = {};
static uint16_t syn_arm  = 0;        // notes armed for manual placement (pad mask)
static int  syn_arp_i   = 0;         // arp scan position over syn_seq
static int  eng_focus   = ENG_SYNTH;
static int  seq_mode[3] = { SYN_SEQ, SYN_SEQ, SYN_SEQ };          // boot: sequencer/unified
static int  mode_cur    = 0;         // engine-page cursor
static bool seq_view    = false;     // inside the drum sequencer submenu
static bool seq_place   = false;     // place mode: pads = 16 steps
static int  seq_edit    = 0;         // manual edit-step cursor (0..songLen-1)
static int  view_page   = 0;         // which 16-step page the pads show (auto-follows play)
// 128-step bit helpers for the per-voice / per-slot drum & sampler patterns
static inline bool seqBit(const uint8_t* p,int s){ return (p[s>>3]>>(s&7))&1u; }
static inline void seqTog(uint8_t* p,int s){ p[s>>3]^=(uint8_t)(1u<<(s&7)); }
static inline void seqSet(uint8_t* p,int s){ p[s>>3]|=(uint8_t)(1u<<(s&7)); }
static inline int  seqCount(const uint8_t* p){ int c=0; for(int j=0;j<16;j++) c+=__builtin_popcount(p[j]); return c; }
static bool param_edit  = false;     // editing a synth param (R-turn = value)
static int  gstep       = 0;         // global grid position (wraps at song_len)
static uint32_t gstep_samp = 0;      // samples elapsed since the current 16th started
static int  gstep_blk   = -1;        // gstep seen at the last audio block (resync edge)

// ── Song form / structure (PO-style) ─────────────────────────
// In a drum/sampler sequencer view, LEFT-CLICK enters FORM mode: right encoder
// sets beats-per-bar (1-16, odd meters), pads pick bar count (contiguous fill).
// song_len = beats × bars, clamped to the 16-step pattern length; the master
// clock wraps there, so the whole rig (synth/drums/sampler) follows the meter.
static int  form_blen = 16;          // beats (16th-steps) per bar, 1..16
static int  form_bars = 1;           // bars in the song, 1..16
static bool form_mode = false;       // structure-editing sub-state of seq_view
static inline int songLen(){
    int n = form_blen * form_bars;
    if(n < 1) n = 1; if(n > MAXSTEPS) n = MAXSTEPS;
    return n;
}

// Globals param editor (idx: 0 BPM 1 KEY 2 MODE 3 SWING)
static void globAdj(int idx, int d){
    switch(idx){
    case 0: { int bpm = (int)(15000u/step_ms) + d;       // BPM ±1
              if(bpm < 30) bpm = 30; if(bpm > 250) bpm = 250;
              step_ms = 15000u / (uint32_t)bpm; } break;
    case 1: g_key  += d; if(g_key < -24) g_key = -24; if(g_key > 24) g_key = 24;
            rebuildScale(); break;   // ±2 octaves — global offset sets the synth range
    case 2: g_mode = (g_mode + 9 + d) % 9; rebuildScale(); break;
    default:g_swing += d; if(g_swing < 0) g_swing = 0; if(g_swing > 9) g_swing = 9; break;
    }
}
// Engine-page param editor. SYNTH p: 0 WAVE 1 DENS 2 POLY. DRUM p: 0 KIT 1 CHANCE.
// SMP p: 0 CHANCE.
// BARS (1..8) and BEATS-per-bar (1..16) define the song length, shared globally
// but editable from every engine tab. q: 0 = BARS, 1 = BEATS.
static void songAdj(int q, int d){
    if(q == 0){ form_bars += d; if(form_bars < 1) form_bars = 1; if(form_bars > 8) form_bars = 8; }
    else      { form_blen += d; if(form_blen < 1) form_blen = 1; if(form_blen > 16) form_blen = 16; }
    int sl = songLen(); if(gstep >= sl) gstep = 0; if(seq_edit >= sl) seq_edit = 0;
}
static void engAdj(int m, int p, int d){
    if(m == MENU_SYNTH){
        if(p == 0)      g_wave  = (g_wave + WAVE_COUNT + d) % WAVE_COUNT;
        else if(p == 1){ g_csyn  += d; if(g_csyn  < 0) g_csyn  = 0; if(g_csyn  > 9) g_csyn  = 9; }
        else if(p == 2){ g_cpoly += d; if(g_cpoly < 0) g_cpoly = 0; if(g_cpoly > 9) g_cpoly = 9; }
        else if(p == 3){ g_vsyn  += d; if(g_vsyn  < 0) g_vsyn  = 0; if(g_vsyn  > 12) g_vsyn  = 12; }
        else if(p == 4){ g_wtscan += d; if(g_wtscan < 0) g_wtscan = 0; if(g_wtscan > 15) g_wtscan = 15; }
        else if(p == 5){ g_prise  += d; if(g_prise  < 0) g_prise  = 0; if(g_prise  > 9)  g_prise  = 9;  }
        else           songAdj(p - 6, d);                  // 6 BARS, 7 BEATS
    } else if(m == MENU_DRUM){
        if(p == 0)      kit_sel = (kit_sel + 2 + d) & 1;
        else if(p == 1){ g_cdrum += d; if(g_cdrum < 0) g_cdrum = 0; if(g_cdrum > 9) g_cdrum = 9; }
        else if(p == 2){ g_vdrum += d; if(g_vdrum < 0) g_vdrum = 0; if(g_vdrum > 12) g_vdrum = 12; }
        else           songAdj(p - 3, d);                  // 3 BARS, 4 BEATS
    } else { // MENU_SMP
        if(p == 0){ g_csmp += d; if(g_csmp < 0) g_csmp = 0; if(g_csmp > 9) g_csmp = 9; }
        else if(p == 1){ g_vsmp += d; if(g_vsmp < 0) g_vsmp = 0; if(g_vsmp > 12) g_vsmp = 12; }
        else           songAdj(p - 2, d);                  // 2 BARS, 3 BEATS
    }
}

// ── Punch-in FX (MENU_FX) — PO-style hold-to-engage pad effects ──────
// While the FX page is current, the 16 pads set/clear bits here (press = on,
// release = off). Bits are PHYSICAL pad positions (columns mirrored at the
// key handler) so the layout below reads left-to-right on the unit.
// EVERY effect scales with FSR squeeze (fx_fsr 0..1) — lean in for more.
// Physical layout (row 0 = top):
enum { FX_ST4 = 0,  FX_ST8,   FX_ST16,  FX_ST32,   // grid stutters: 1/4 1/8 1/16 1/32
       FX_REV,      FX_CHOR,  FX_PAN,   FX_CRUSH,  // buffer/space
       FX_HALF,     FX_DBL,   FX_BZZ,   FX_MUTE,   // varispeed / gen-buzz / drop
       FX_FLT,      FX_WASH,  FX_DIST,  FX_STOP }; // bp-sweep / wash / dist / tape-stop
static volatile uint16_t fx_held = 0;
static volatile float    fx_fsr  = 0.f;   // FSR squeeze 0..1, updated @ FSR rate
static volatile float    fx_lid  = 0.f;   // lidar 0..1 (near..far) for FX2 macros

// FX automation: hold the RIGHT encoder on the FX page to write the live pad
// mask into the current step as the loop passes — pads held get recorded,
// empty hands scrub that step clean. Playback: live pads override temporarily,
// otherwise the recorded automation drives the FX. fx_eff is what everything
// (audio callback, grid clock, filter) actually obeys.
static uint16_t          fx_auto[8][16] = {};    // [bank][step] recorded masks (all 8 banks)
static uint8_t           fx_auto_fsr[16] = {};   // recorded FSR pressure per step
static volatile uint16_t fx_eff  = 0;            // FX1 effective mask (live OR automation)
static volatile uint16_t fx_eff2 = 0, fx_eff3 = 0, fx_eff4 = 0, fx_eff5 = 0, fx_eff6 = 0, fx_eff7 = 0, fx_eff8 = 0;
static volatile float    fx_fsr_eff  = 0.f;      // FSR the callback obeys (live or automated)
static inline bool fxOn(int b) { return ((fx_eff >> b) & 1u) != 0; }

// FX2 — second effect bank (right-encoder toggles bank on the FX page).
// SP404/OP-1/Elektron-flavored: dub echo, gated verb, trance gate, flanger /
// pshift, shimmer-delay, ringmod, formant / iso-lo, iso-mid, phone, comb /
// grain-freeze, wow-flutter, downsample, scatter. Held bits = live, fx2_eff
// is what the callback reads. fx_bank: 0 = FX1 page, 1 = FX2 page.
enum { F2_DUB=0,  F2_GVRB, F2_TGATE, F2_FLANG,   // time/space
       F2_RING,   F2_FOLD, F2_COMB,  F2_FORM,    // pitch/tone
       F2_ISOLO,  F2_ISOMD,F2_ISOHI, F2_PHONE,   // filter/EQ
       F2_GRAIN,  F2_WOW,  F2_DOWN,  F2_SCAT };   // texture
static const char* FX2N[16] = {
    "DUB",  "GVERB","TGATE","FLANG",
    "RING", "FOLD", "COMB", "VOWEL",
    "ISO-LO","ISO-MD","ISO-HI","PHONE",
    "GRAIN","WOW",  "DOWN", "SCAT" };
static int               fx_bank  = 0;   // 0=FX1 1=FX2 2=FX3 3=FX4 (right enc cycles)
static volatile uint16_t fx2_held = 0;
static inline bool f2On(int b){ return ((fx2_held >> b) & 1u) != 0; }

// FX3 — MUSICAL / SPATIAL bank. FX4 — EXPERIMENTAL / GLITCH / PITCH bank.
enum { E3_PLATE=0, E3_SHIM,  E3_PONG,  E3_SLAP,
       E3_PHAS,   E3_WAH,   E3_TREM,  E3_HAAS,
       E3_VIBR,   E3_DIM,   E3_SPRG,  E3_SIREN,
       E3_WARM,   E3_SUB,   E3_AIR,   E3_PUMP };
enum { E4_OCTU=0, E4_OCTD,  E4_DSTR,  E4_REPT,
       E4_RGRN,   E4_FRZ,   E4_METL,  E4_FORM,
       E4_FSHF,   E4_FUZZ,  E4_GSEQ,  E4_SRMP,
       E4_VSTP,   E4_RADIO, E4_NGAT,  E4_SHMD };
static const char* FX3N[16] = {
    "PLATE","SHIMR","PONG", "SLAP",
    "PHASE","WAH",  "TREM", "HAAS",
    "VIBRA","DIMEN","SPRNG","SIREN",
    "WARM", "SUB",  "AIR",  "PUMP" };
static const char* FX4N[16] = {
    "OCT+", "OCT-", "DSTRY","REPT",
    "RGRN", "FREEZ","METAL","FRMNT",
    "FSHFT","FUZZ", "GSEQ", "SRAMP",
    "VSTOP","RADIO","NGATE","SHMOD" };
static volatile uint16_t fx3_held = 0;
static volatile uint16_t fx4_held = 0;

// FX5 — POCKET-OPERATOR punch-ins (PO-12/28/32/33/35), only effects NOT in
// banks 2-4: beat-repeats, note-shuffle, 6/8 quantize, punch, fill, build,
// bouncing-ball, LP/HP kills, squash, scratch ×2, glitch, blinds, S&H.
enum { E5_REPEAT2=0, E5_REPEAT6, E5_NOTESHUF, E5_QUANT68,
       E5_PUNCH,     E5_FILLIN,  E5_BUILDUP,  E5_BOUNCE,
       E5_LPFIX,     E5_HPFIX,   E5_SQUASH,   E5_SCRATCH,
       E5_SCRTFAST,  E5_GLITCH,  E5_BLINDS,   E5_SAMPHLD };
static const char* FX5N[16] = {
    "RPT2","RPT6","SHUF","Q6/8","PUNCH","FILL","BUILD","BALL",
    "LPF","HPF","SQUSH","SCRTC","SCRTF","GLCH","BLIND","S&H" };
static volatile uint16_t fx5_held = 0;

// FX6 — DELAY / SPACE / PITCH (none duplicate banks 2-4): tempo delay,
// ping-pong, echo-fade, resonant feedback, drone-freeze, true freeze, unison
// hi/lo, detune cluster, pitch-LFO, bend up/down, warble, 5th harmony, sub.
enum { E6_DELAY=0,  E6_PINGPONG, E6_ECHOFADE, E6_FEEDBACK,
       E6_DRONE,    E6_FREEZE,   E6_UNISON,   E6_UNILOW,
       E6_DETUNE,   E6_PITCHLFO, E6_BENDUP,   E6_BENDDN,
       E6_WARBLE,   E6_HARM5,    E6_SUBOCT,   E6_LOOP };
static const char* FX6N[16] = {
    "DELAY","PNGPG","FADE","FBACK","DRONE","FREEZ","UNI","UNILO",
    "DETUN","PLFO","BND+","BND-","WARBL","5TH","SUB","LOOP" };
static volatile uint16_t fx6_held = 0;

// FX7 — LIVE / PERFORMANCE: grid gates (1/4·1/8·1/16·trance), tremolo, auto-pan,
// sidechain pump/swell, vinyl noise, Haas widener, mono, bright/dark tilt, drive,
// soft gate, kill. All cheap, grid-rate, tamed by the master limiter.
enum { E7_GATE4=0, E7_GATE8,  E7_GATE16, E7_TRANCE,
       E7_TREM,    E7_PAN,     E7_PUMP,   E7_SWELL,
       E7_NOISE,   E7_WIDE,    E7_MONO,   E7_TILTU,
       E7_TILTD,   E7_DRIVE,   E7_SOFTG,  E7_KILL };
static const char* FX7N[16] = {
    "GATE4","GATE8","GAT16","TRNCE","TREM","PAN","PUMP","SWELL",
    "NOISE","WIDE","MONO","BRITE","DARK","DRIVE","SGATE","KILL" };
static volatile uint16_t fx7_held = 0;

// FX8 — MIDI / SEQUENCE FX: ratchet retrigs (x2·trip·x4·x8), strum ramp, alt
// trance, pluck stab, ring-mod lo/hi, vibrato, random chop, half-time gate,
// double gate, glitch gate, reverse-env, silence. Note/rhythm flavored.
enum { E8_RATCH2=0, E8_RATCH3, E8_RATCH4, E8_RATCH8,
       E8_STRUM,    E8_TRANCE2, E8_STAB,  E8_RINGLO,
       E8_RINGHI,   E8_VIBR,   E8_CHOP,   E8_HALFG,
       E8_DBLG,     E8_GLCHG,  E8_REVG,   E8_SILENCE };
static const char* FX8N[16] = {
    "RAT2","RAT3","RAT4","RAT8","STRUM","TRNC2","STAB","RINGL",
    "RINGH","VIBR","CHOP","HALF","DBL","GLCH","REVG","SIL" };
static volatile uint16_t fx8_held = 0;

// ── Sessions — 16 slots, params in QSPI (instant), audio on SD (background) ──
static int      sess_cur    = 0;          // active session index 0..15
static uint16_t sess_exists = 0x0001;     // bitmask of slots that hold a session
static int      save_state  = 0;          // 0 saved, 1 dirty/saving (top-right icon)
static uint32_t sess_press[16] = {};      // pad press times for tap/hold gestures
static int      sess_warn   = -1;         // slot showing a delete warning (-1 none)

// Stutter/reverse capture ring — 2s stereo in SDRAM, written with the final
// mix every sample; frozen (write paused) while a buffer FX holds it.
#define RBUF_N 96000u
static float DSY_SDRAM_BSS rbufL[RBUF_N];
static float DSY_SDRAM_BSS rbufR[RBUF_N];
// FX2 delay buffers (SDRAM): dub echo (mono, ~0.68s) + tuned comb (~0.1s)
#define DUB_N 32768u
#define CMB_N 4096u
static float DSY_SDRAM_BSS f2dub[DUB_N];
static float DSY_SDRAM_BSS f2cmb[CMB_N];
// FX3 spatial buffers
#define F3PLT_N 24000u
#define F3PNG_N 96000u
#define F3SLP_N 12000u
static float DSY_SDRAM_BSS f3plt[F3PLT_N];
static float DSY_SDRAM_BSS f3pngL[F3PNG_N];
static float DSY_SDRAM_BSS f3pngR[F3PNG_N];
static float DSY_SDRAM_BSS f3slap[F3SLP_N];
// FX4 glitch buffers
#define F4REP_N 48000u
#define F4MTL_N 4096u
static float DSY_SDRAM_BSS f4rep[F4REP_N];
static float DSY_SDRAM_BSS f4mtl[F4MTL_N];
// FX5 (PO) buffers
static float DSY_SDRAM_BSS f5repL[24000];   // beat-repeat
static float DSY_SDRAM_BSS f5repR[24000];
static float DSY_SDRAM_BSS f5scrL[48000];   // scratch / glitch capture
static float DSY_SDRAM_BSS f5scrR[48000];
// FX7 (perform) buffer — Haas widener short delay
static float DSY_SDRAM_BSS f7dl[2048];
// FX6 (delay/space/pitch) buffers
static float DSY_SDRAM_BSS f6dL[48000];     // tempo delay
static float DSY_SDRAM_BSS f6dR[48000];
static float DSY_SDRAM_BSS f6ppL[48000];    // ping-pong
static float DSY_SDRAM_BSS f6ppR[48000];
static float DSY_SDRAM_BSS f6echo[24000];   // echo-fade / freeze tank
static float DSY_SDRAM_BSS f6loopL[96000];  // drone / loop (2s)
static float DSY_SDRAM_BSS f6loopR[96000];
static float DSY_SDRAM_BSS f6pchL[12000];   // pitch grain (unison/bend/harm/sub)
static float DSY_SDRAM_BSS f6pchR[12000];
// in-effect work buffers — kept in SDRAM (too big for the 128KB DTCM)
static float DSY_SDRAM_BSS f5rpt2[6000];
static float DSY_SDRAM_BSS f5fill[4000];
static float DSY_SDRAM_BSS f5bnc[8000];
static float DSY_SDRAM_BSS f6fb[4096];
static float DSY_SDRAM_BSS f6dtL[2048];
static float DSY_SDRAM_BSS f6dtR[2048];
static float DSY_SDRAM_BSS f6plfo[4096];
static float DSY_SDRAM_BSS f6bu[4096];
static float DSY_SDRAM_BSS f6bd[4096];
static float DSY_SDRAM_BSS f6wrb[4096];
// Clock sync for QUANTIZED stutters: the grid clock snapshots the ring write
// head at every step boundary and bumps a stamp; the callback counts samples
// since that boundary, so stutter loops start on the grid and repeat in time.
static volatile uint32_t rb_w        = 0;   // ring write head (callback-owned)
static volatile uint32_t step_rw     = 0;   // ring pos at last step boundary
static volatile float    fx_vs       = 1.f; // varispeed target (HALF/DBL FX)
// live-content latch: whatever sounded most recently (manual pads included)
// feeds the stutter/BZZ registers, so retrigs are ALWAYS audible
static volatile uint8_t  live_dr = 0;
static int               live_nt[4] = {};
static int               live_nn = 0;

// ── Sampler — 16 pad slots, recorded live from the audio input ──
// Hold a pad ≥300ms = record into that slot while held; short press = play it
// (and select it as the write target). MCH = the PO step sequencer (any slot
// on any step), EUC = euclid fires the selected slot. Samples play into the
// drum bus so they ride the accel filter. 2s mono per slot, lives in SDRAM.
#define SMPL_N 96000u
static float DSY_SDRAM_BSS smp[16][SMPL_N];
static volatile uint32_t smp_len[16]  = {};
static volatile int      rec_slot     = -1;   // callback records while >= 0
static volatile uint32_t rec_pos      = 0;
static volatile uint32_t smp_trig[16] = {};   // play-trigger counters
static uint8_t  smp_pat[16][16] = {};         // 128-step bitmask per slot
static int      sel_smp     = 0;              // last played slot = write target
static uint32_t smp_press[16] = {};           // press timestamps for hold-vs-click

// ── Persistence — sequences + settings survive power-off ─────
// QSPI-backed via PersistentStorage; auto-saves 3s after the last change
// (debounced — the flash erase blocks the control plane briefly, the audio
// IRQ never notices). Samples themselves are NOT persisted (6MB — later).
struct Persist {
    uint32_t step_ms;
    uint16_t dr_pat[4];
    uint16_t smp_pat[16];
    uint16_t fx_auto[16];
    uint8_t  dr_pad[4];
    int8_t   dr_k[4];
    int8_t   dr_rot[4];
    int8_t   eu_k_syn, eu_rot_syn, eu_k_smp;
    int8_t   seq_mode[3];
    int8_t   vol_pidx;
    int8_t   menu, sel_drum, sel_smp;
    int8_t   g_key, g_mode, g_swing, g_csyn, g_cpoly, g_cdrum;  // globals per patch
    int8_t   g_wave, kit_sel, g_csmp;
    int8_t   form_blen, form_bars;
    uint8_t  fx_auto_fsr[16];
    uint8_t  rsvd[32];   // reserved — future fields steal from here so the
                         // struct layout (and saved patches) stay compatible
};
// 16-session bank, lives in QSPI as one blob (~3KB — trivial in 8MB).
struct SessionBank {
    uint32_t magic;        // format/version guard
    uint16_t exists;       // bit i = slot i holds a session
    uint8_t  active;       // last-open slot
    Persist  s[16];
    bool operator==(const SessionBank &o) const { return memcmp(this,&o,sizeof(SessionBank))==0; }
    bool operator!=(const SessionBank &o) const { return !(*this==o); }
};
// v2 2026-09-13: bumped 'OPG3' -> 'OPG4' to FACTORY-RESET every saved session.
// sessionsBoot() rewrites all 16 slots from defaults when the magic does not
// match, which clears the stale patch that was auto-looping on boot.
// Bump this again any time you want a clean slate.
static const uint32_t SESS_MAGIC = 0x4F504734u;  // 'OPG4' — bumped for the new
                                                 // patch layout (audio + reserved
                                                 // padding); old data reinits clean
// LOAD-BEARING: sizeof(SessionBank) is baked into every saved patch on
// QSPI + SD (PATCHES.BIN/.BAK). If this ever fails to compile, something
// changed Persist/SessionBank's layout — STOP, that would silently corrupt
// or wipe every saved beat (patchLoadFile()'s size check would just start
// rejecting the user's real files as "different format"). New persisted
// fields MUST steal bytes from Persist::rsvd[32] instead (see its comment)
// so this number never moves. 2568 measured on the pre-USB-merge baseline
// (arm-none-eabi-g++, cortex-m7/fpv5-d16/hard-float, gnu++14) — 2026-08-12.
static_assert(sizeof(SessionBank) == 2568, "SessionBank layout changed — this would corrupt/wipe saved patches");
static volatile uint32_t save_at = 0;
static inline void markDirty(){ save_at = System::GetNow() + 3000; save_state = 1; }

// PO drum patterns: one 16-step bitmask per voice (full polyphony — all four
// voices can share a step). Loops while drum mode = MACHINE, wherever the UI
// is. dr_pad remembers each voice's last-played flavor pad so sequenced hits
// sound exactly like what you played.
static uint8_t  dr_pat[4][16] = {};   // 128-step bitmask per drum voice
static uint8_t  dr_pad[4] = {13, 10, 4, 0};  // defaults: KCK2, BELL, SNP1, HCL

// ── Master volume mode — hold BOTH encoder buttons to toggle ──
// Pads set the end-of-chain level, physical top-left (0%) → bottom-right
// (125%). >100% pushes the final tanh = soft tan loudness, never hard clip.
// Default 75% = physical pad index 9.
//
// Gain is looked up from kVolTable, not computed linearly from the pad
// index: a straight 0..15 -> 0..1.25 ramp gets loud far too fast at the
// bottom of the range (ears hear in dB, not in linear amplitude). The
// table below tapers pad 1..12 across -40dB..0dB in even ~3.64dB steps
// (40/11 dB per pad), so pad 12 lands on unity gain (0dB = 1.0, the "100%"
// UI tick), then pads 13..15 keep climbing in ~0.65dB steps up to the
// existing +1.25x max headroom (~+1.94dB) that leans into the limiter.
// Pad 0 is hard-wired to true silence rather than a very-low-dB value.
static const float kVolTable[16] = {
    0.00000f,                                      // 0: silence
    0.01000f, 0.01520f, 0.02310f, 0.03511f,         // 1..4:  -40.00 .. -29.09 dB
    0.05337f, 0.08111f, 0.12328f, 0.18738f,         // 5..8:  -25.45 .. -14.55 dB
    0.28480f, 0.43287f, 0.65793f, 1.00000f,         // 9..12: -10.91 ..   0.00 dB
    1.07720f, 1.16040f, 1.25000f,                   // 13..15: +0.65 .. +1.94 dB
};
static bool           vol_mode = false;
// v2 2026-09-13: pad 9 -> 12 -> 10. Pad 12 (unity) measured as slamming the
// final tanh limiter on real speakers -- unity at the DSP bus is already hot
// once the PAM8403's own fixed gain is in front of the drivers. Pad 10
// (-7.27 dB) is +3.6 dB over the old default and sits clear of the limiter,
// with pads 11..15 available above it when you want more.
static int            vol_pidx = 10;           // physical pad index 0..15
static volatile float mvol     = 0.43287f;     // kVolTable[10] = -7.27 dB
static int      sel_drum  = 0;               // last drum played = write target

// ── Mid-Air Thief voice character — every note is its own patch ──
// Detuned stack (or harmonic 2-op FM) through a per-note VCF envelope with
// random (always-stable) resonance, krell drift, occasional swells and
// glides. Processing is fixed: drive 2.5x, chorus 0.55 — no options.
static void voiceCharacter(int v, bool generative){
    Voice& V = voices[v];
    V.drift  = 0.f;
    V.driftT = ((float)(lcg() % 200) - 100.f) * 0.00008f;
    V.nosc   = 5;                       // default stack width (classic waves drop to 1)
    V.lp = 0.f; V.bp = 0.f;
    V.q  = 0.70f + (float)(lcg() % 35) * 0.01f;      // rings, never unstable
    // CLASSIC WAVEFORM modes (g_wave 0..4): PURE single-oscillator tone — no
    // detune stack, no drift, no vibrato. Clean, chill, Pocket-Operator vibe.
    if(g_wave < 5){
        V.type = 10 + g_wave;                        // 10 SAW 11 SQR 12 TRI 13 SIN 14 PULSE
        V.nosc = 1;                                  // MONO, one oscillator
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f; // dead-on tuning, no beating
        V.driftT = 0.f;
        V.fc  = 0.55f; V.fcT = 0.18f; V.fcR = 0.00006f;   // gentle open→close pluck
        V.fmi = 0.f;
        uint32_t ar2 = generative ? (lcg() % 100u) : 100u;
        V.atk = (ar2 < 20u) ? 0.012f : 0.08f;
        return;
    }
    if(g_wave == 7){
        // WAVETABLE — a clean detuned trio reading the morphable bank. SCAN sets
        // the base timbre; the VCF envelope also nudges the scan for movement.
        V.type = 20;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f + DETUNE[k];
        V.fc  = 0.55f; V.fcT = 0.14f; V.fcR = 0.00006f;
        V.fmi = 0.f;
        uint32_t arw = generative ? (lcg() % 100u) : 100u;
        V.atk = (arw < 20u) ? 0.012f : 0.08f;
        return;
    }
    if(g_wave == 8){
        // WAVEFOLDER (West-Coast/Buchla) — a sine driven hard into a triangle
        // folder; harmonics BLOOM as the fold depth rises. SCAN sets base fold,
        // the envelope sweeps it for that growling, opening complex-osc motion.
        V.type = 23;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f + DETUNE[k];
        V.fc  = 0.70f; V.fcT = 0.40f; V.fcR = 0.00005f;   // mostly open — folder is the timbre
        V.fmi = 0.f;
        uint32_t arf = generative ? (lcg() % 100u) : 100u;
        V.atk = (arf < 20u) ? 0.012f : 0.08f;
        return;
    }
    if(g_wave == 9){
        // PLUCK (Karplus-Strong) — noise burst into a damped delay line. SCAN sets
        // the decay (sustain). Physical, acoustic transients (guitar/harp/koto).
        V.type = 24; V.kinit = true;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f;
        V.fc  = 0.85f; V.fcT = 0.85f; V.fcR = 0.0002f;    // mostly bypass — the string is the tone
        V.atk = 0.5f;                                      // instant attack (it's plucked)
        return;
    }
    if(g_wave == 10){
        // SUPERSAW — 5 band-limited saws, WIDE detune (SCAN = spread). The JP-8000
        // trance wall. Reuses the band-limited saw render (type 10) with wide ratios.
        V.type = 10;
        float spread = 0.004f + (float)g_wtscan * 0.0016f;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f + DETUNE[k] * (spread / 0.01f);
        V.fc  = 0.55f; V.fcT = 0.20f; V.fcR = 0.00005f;
        V.fmi = 0.f;
        uint32_t ars = generative ? (lcg() % 100u) : 100u;
        V.atk = (ars < 20u) ? 0.012f : 0.06f;
        return;
    }
    if(g_wave == 11){
        // CZ (phase distortion) — Casio-style resonant sweep without a filter.
        // SCAN sets the resonance multiple. Hollow/reedy/buzzy digital character.
        V.type = 26;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f + DETUNE[k];
        V.fc  = 0.80f; V.fcT = 0.40f; V.fcR = 0.00006f;
        V.fmi = 0.f;
        uint32_t arc = generative ? (lcg() % 100u) : 100u;
        V.atk = (arc < 20u) ? 0.012f : 0.08f;
        return;
    }
    if(g_wave == 12){
        // ARCADE — clean static pulse, NO filter sweep, snappy: the chiptune /
        // Pocket-Operator retro videogame voice. Mono, dry, bright, no movement.
        V.type = 14;                                 // band-limited 25% pulse
        V.nosc = 1;
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f;
        V.driftT = 0.f;
        V.fc  = 0.92f; V.fcT = 0.92f; V.fcR = 0.001f; // filter parked WIDE OPEN — no wah
        V.q   = 0.5f;                                 // no resonance
        V.fmi = 0.f;
        V.atk = 0.5f;                                 // instant attack (arcade blip)
        return;
    }
    if(g_wave == 6){
        // FM mode — random index + ratio per note. Ratio is a WHOLE NUMBER
        // ~2/3 of the time (harmonic/clean), otherwise inharmonic (metallic).
        V.type = 2;
        if((lcg() % 3u) != 0u) V.ratio[1] = (float)(1u + (lcg() % 6u));        // whole 1..6
        else                   V.ratio[1] = 0.5f + (float)(lcg() % 750u) * 0.01f; // 0.5..8.0
        V.fmi  = 0.05f + (float)(lcg() % 70u) * 0.01f;   // random index 0.05..0.75
        V.fmiT = V.fmi * 0.4f;                            // index decays into the tail
        V.fc   = 0.55f; V.fcT = 0.18f; V.fcR = 0.00008f;
        uint32_t ar3 = generative ? (lcg() % 100u) : 100u;
        V.atk = (ar3 < 20u) ? 0.012f : 0.08f;
        return;
    }
    uint32_t r = lcg() % 100u;
    if(r < 30){
        // 2-op FM pluck — harmonic ratios only, stays in the Lydian vibe
        V.type = 2;
        static const float FMR[6] = {0.5f, 1.f, 1.003f, 2.f, 2.005f, 4.f};
        V.ratio[1] = FMR[lcg() % 6u];
        V.fmi  = 0.10f + (float)(lcg() % 32) * 0.01f;
        V.fmiT = 0.02f;
        V.fc   = 0.40f; V.fcT = 0.15f; V.fcR = 0.00008f;
    } else {
        V.type = (r < 85) ? 0 : 1;                   // gritty saws, some tri
        // ONE pitch per note — pure detune, no octave doubling, no intervals
        for(int k = 0; k < 5; k++) V.ratio[k] = 1.f + DETUNE[k];
        V.fc  = 0.22f + (float)(lcg() % 21) * 0.01f; // snap-open, stable ceiling
        V.fcT = 0.025f + (float)(lcg() % 8) * 0.01f;
        V.fcR = 0.00004f + (float)(lcg() % 9) * 0.00001f;
    }
    uint32_t ar = generative ? (lcg() % 100u) : 100u;
    bool swell = (ar < 15u);
    V.atk = swell ? 0.0025f : ((ar < 40u) ? 0.012f : 0.08f);  // some longer attacks
    if(swell){ V.fc = 0.04f; V.fcT = 0.36f; V.fcR = 0.00003f; }
    if((lcg() % 100u) < 10u)   // glide notes: pitch ramps across the whole note
        V.driftT = ((lcg() & 1u) ? 1.f : -1.f) * (0.06f + (float)(lcg() % 10) * 0.012f);
}

// Euclidean params. Synth: pad number sets pulses 1..16. Drums: per-voice
// pulses (0 = voice off), pad row picks voice, column picks k, re-press = off.
static int eu_k_syn = 5, eu_rot_syn = 0, eu_k_smp = 4;
static int dr_k[4]   = {4, 2, 8, 3};   // voice 0=kick 1=snare 2=hat 3=blip
static int dr_rot[4] = {0, 4, 0, 5};
static bool euclidHit(int k, int rot, int s) {
    if(k <= 0)  return false;
    if(k >= 16) return true;
    return ((s + 16 - rot) * k) % 16 < k;   // Bjorklund via Bresenham
}
static PersistentStorage<SessionBank> pstore(hw.qspi);
static bool g_sd_ok = false;   // SD mounted? (set at boot)
// Patches also mirror to the SD card so they survive a firmware reflash
// (QSPI can be wiped/reformatted on update; the SD file is durable). Small
// (~3KB), written best-effort in the control plane — never the audio path.
// ATOMIC patch save: write a temp, verify the whole blob landed, then promote
// it — keeping the previous good file as PATCHES.BAK. A failed/interrupted save
// can never truncate the live PATCHES.BIN, so beats are never lost mid-write.
static void patchSaveSD(){
    if(!g_sd_ok) return;
    FIL f;
    if(f_open(&f, "PATCHES.TMP", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return;
    UINT bw = 0;
    FRESULT wr = f_write(&f, &pstore.GetSettings(), sizeof(SessionBank), &bw);
    f_close(&f);
    if(wr != FR_OK || bw != sizeof(SessionBank)){ f_unlink("PATCHES.TMP"); return; }
    f_unlink("PATCHES.BAK");
    f_rename("PATCHES.BIN", "PATCHES.BAK");          // keep prior good (ok if first save)
    if(f_rename("PATCHES.TMP", "PATCHES.BIN") != FR_OK)
        f_rename("PATCHES.BAK", "PATCHES.BIN");      // promotion failed → restore backup
}
static bool patchLoadFile(const char* nm){
    FIL f;
    if(f_open(&f, nm, FA_READ) != FR_OK) return false;
    SessionBank tmp;
    UINT br = 0;
    FRESULT r = f_read(&f, &tmp, sizeof(SessionBank), &br);
    f_close(&f);
    if(r != FR_OK || br != sizeof(SessionBank) || tmp.magic != SESS_MAGIC) return false;
    pstore.GetSettings() = tmp;   // SD wins over QSPI
    return true;
}
static bool patchLoadSD(){     // returns true if a valid file was loaded
    if(!g_sd_ok) return false;
    if(patchLoadFile("PATCHES.BIN")) return true;
    return patchLoadFile("PATCHES.BAK");   // fall back to the backup copy
}

// ── Per-patch sampler audio on SD (background save / restore) ──
// File PATCHnn.AUD: [magic][ver][lens[16]] then int16 PCM for each non-empty
// slot in order, then a trailing magic that marks the file COMPLETE — so a
// reset mid-write can never load a half file. Saves run a chunk per control-
// loop pass (never the audio path); loads run at boot/patch-switch.
#define AUD_MAGIC 0x44554150u   // 'PAUD'
static FIL      aud_fil;
static bool     aud_saving = false;
static bool     aud_dirty  = false;   // sampler audio changed, needs a save
static int      aud_slot   = 0;
static uint32_t aud_pos    = 0;
static int      aud_p      = 0;       // patch number the in-flight save targets
static int16_t  aud_buf[1024];
static void audName(char* o, int p){ sprintf(o, "PATCH%02d.AUD", p & 15); }
static void audTmp (char* o, int p){ sprintf(o, "PATCH%02d.TAU", p & 15); }
static void audBak (char* o, int p){ sprintf(o, "PATCH%02d.ABK", p & 15); }

static bool audBeginSave(int p){
    if(!g_sd_ok) return false;
    aud_p = p & 15;
    char nm[16]; audTmp(nm, aud_p);   // write to a temp, promote only when COMPLETE
    if(f_open(&aud_fil, nm, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    UINT bw;
    uint32_t hdr[2] = { AUD_MAGIC, 1u };
    f_write(&aud_fil, hdr, sizeof(hdr), &bw);
    uint32_t lens[16];
    for(int i=0;i<16;i++) lens[i] = smp_len[i];
    f_write(&aud_fil, lens, sizeof(lens), &bw);
    aud_saving = true; aud_slot = 0; aud_pos = 0;
    return true;
}
static void audSaveTick(){            // one chunk per call
    if(!aud_saving) return;
    UINT bw;
    while(aud_slot < 16 && (smp_len[aud_slot] == 0 || aud_pos >= smp_len[aud_slot])){
        aud_slot++; aud_pos = 0;
    }
    if(aud_slot >= 16){               // done — trailing magic marks it complete
        uint32_t tail = AUD_MAGIC;
        f_write(&aud_fil, &tail, 4, &bw);
        f_close(&aud_fil);
        aud_saving = false;
        // promote the temp → real file; keep the prior good audio as a backup so
        // a future bad save can fall back instead of losing the recording.
        char nm[16], tm[16], bk[16];
        audName(nm, aud_p); audTmp(tm, aud_p); audBak(bk, aud_p);
        f_unlink(bk);
        f_rename(nm, bk);                         // prior good → backup (ok if none)
        if(f_rename(tm, nm) != FR_OK) f_rename(bk, nm);  // restore backup on failure
        return;
    }
    uint32_t n = smp_len[aud_slot] - aud_pos;
    if(n > 1024) n = 1024;
    for(uint32_t i=0;i<n;i++){
        float v = smp[aud_slot][aud_pos + i];
        if(v > 1.f) v = 1.f; if(v < -1.f) v = -1.f;
        aud_buf[i] = (int16_t)(v * 32767.f);
    }
    f_write(&aud_fil, aud_buf, n*2, &bw);
    aud_pos += n;
}
static void audFlushSave(){           // finish a pending save synchronously
    uint32_t guard = 0;
    while(aud_saving && guard++ < 200000u) audSaveTick();
}
// ensure the CURRENT working-set audio is on disk for patch `p` before we
// overwrite the RAM (called right before a patch switch)
static void audEnsureSaved(int p){
    // v2: guard on g_sd_ok like every other SD entry point. This is called on
    // EVERY patch switch and spins in audFlushSave(); with no card mounted
    // there is nothing to flush, and spinning here is how patch switching
    // could stall depending on which patch you picked. Every other SD
    // function already had this guard -- this one was missed.
    if(!g_sd_ok){ aud_saving = false; aud_dirty = false; return; }
    if(aud_saving) audFlushSave();
    if(aud_dirty){ if(audBeginSave(p)) audFlushSave(); aud_dirty = false; }
}
static bool audLoadFile(const char* nm){   // true if a valid header was found
    FIL f; UINT br;
    if(f_open(&f, nm, FA_READ) != FR_OK) return false;
    uint32_t hdr[2];
    if(f_read(&f, hdr, sizeof(hdr), &br) != FR_OK || br != sizeof(hdr) || hdr[0] != AUD_MAGIC){
        f_close(&f); return false;
    }
    uint32_t lens[16];
    if(f_read(&f, lens, sizeof(lens), &br) != FR_OK || br != sizeof(lens)){ f_close(&f); return false; }
    for(int s=0; s<16; s++){
        uint32_t n = lens[s]; if(n == 0){ smp_len[s] = 0; continue; } if(n > SMPL_N) n = SMPL_N;
        for(uint32_t off=0; off<n; ){
            uint32_t c = n - off; if(c > 1024) c = 1024;
            // partial read: keep whatever already loaded rather than wiping it all
            if(f_read(&f, aud_buf, c*2, &br) != FR_OK || br != c*2){ f_close(&f); return true; }
            for(uint32_t i=0;i<c;i++) smp[s][off+i] = (float)aud_buf[i] / 32767.f;
            off += c;
        }
        smp_len[s] = n;
    }
    f_close(&f);
    return true;
}
static void audLoad(int p){           // load a patch's audio into the working set
    for(int i=0;i<16;i++) smp_len[i] = 0;
    if(!g_sd_ok) return;
    char nm[16]; audName(nm, p);
    if(audLoadFile(nm)) return;
    char bk[16]; audBak(bk, p);
    audLoadFile(bk);                  // fall back to the backup audio file
}

// ── Per-patch FX automation on SD (ALL 8 banks) ──
// Kept in its own sidecar file (not the QSPI Persist struct) so existing presets
// stay byte-compatible. Atomic temp→rename with a backup, like the audio.
#define FXA_MAGIC 0x46584150u   // 'PAXF'
static void fxaName(char* o,int p){ sprintf(o,"PATCH%02d.FXA",p&15); }
static void fxaTmp (char* o,int p){ sprintf(o,"PATCH%02d.TFX",p&15); }
static void fxaBak (char* o,int p){ sprintf(o,"PATCH%02d.FXB",p&15); }
static void fxaSave(int p){
    if(!g_sd_ok) return;
    char tm[16]; fxaTmp(tm,p);
    FIL f; UINT bw; uint32_t m = FXA_MAGIC;
    if(f_open(&f,tm,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return;
    bool ok = (f_write(&f,&m,4,&bw)==FR_OK && bw==4);
    ok = ok && (f_write(&f,(const void*)fx_auto,sizeof(fx_auto),&bw)==FR_OK && bw==sizeof(fx_auto));
    ok = ok && (f_write(&f,(const void*)fx_auto_fsr,sizeof(fx_auto_fsr),&bw)==FR_OK && bw==sizeof(fx_auto_fsr));
    ok = ok && (f_write(&f,&m,4,&bw)==FR_OK && bw==4);   // trailing magic = complete
    f_close(&f);
    if(!ok){ f_unlink(tm); return; }
    char nm[16],bk[16]; fxaName(nm,p); fxaBak(bk,p);
    f_unlink(bk); f_rename(nm,bk);
    if(f_rename(tm,nm)!=FR_OK) f_rename(bk,nm);
}
static bool fxaLoadFile(const char* nm){
    FIL f; UINT br; uint32_t m;
    if(f_open(&f,nm,FA_READ)!=FR_OK) return false;
    if(f_read(&f,&m,4,&br)!=FR_OK || br!=4 || m!=FXA_MAGIC){ f_close(&f); return false; }
    if(f_read(&f,(void*)fx_auto,sizeof(fx_auto),&br)!=FR_OK || br!=sizeof(fx_auto)){ f_close(&f); return false; }
    if(f_read(&f,(void*)fx_auto_fsr,sizeof(fx_auto_fsr),&br)!=FR_OK || br!=sizeof(fx_auto_fsr)){ f_close(&f); return false; }
    uint32_t tail; if(f_read(&f,&tail,4,&br)!=FR_OK || br!=4 || tail!=FXA_MAGIC){ f_close(&f); return false; }
    f_close(&f); return true;
}
static void fxaLoad(int p){           // restore all 8 banks of automation
    if(!g_sd_ok) return;
    char nm[16]; fxaName(nm,p);
    if(fxaLoadFile(nm)) return;
    char bk[16]; fxaBak(bk,p);
    fxaLoadFile(bk);                  // fall back to the backup
}

// ── Per-patch full sequence on SD: 128-step drum/sampler patterns + the
// polyphonic synth sequence + song length (bars × beats). Kept out of the QSPI
// struct so old presets stay byte-compatible; atomic temp→rename with backup. ──
#define SEQ_MAGIC 0x51455350u   // 'PSEQ'
static void seqaName(char* o,int p){ sprintf(o,"PATCH%02d.SEQ",p&15); }
static void seqaTmp (char* o,int p){ sprintf(o,"PATCH%02d.TSQ",p&15); }
static void seqaBak (char* o,int p){ sprintf(o,"PATCH%02d.SQB",p&15); }
static void seqaSave(int p){
    if(!g_sd_ok) return;
    char tm[16]; seqaTmp(tm,p);
    FIL f; UINT bw; uint32_t m = SEQ_MAGIC;
    if(f_open(&f,tm,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return;
    uint8_t hdr[2] = { (uint8_t)form_bars, (uint8_t)form_blen };
    bool ok = (f_write(&f,&m,4,&bw)==FR_OK && bw==4);
    ok = ok && (f_write(&f,hdr,2,&bw)==FR_OK && bw==2);
    ok = ok && (f_write(&f,(const void*)dr_pat,sizeof(dr_pat),&bw)==FR_OK && bw==sizeof(dr_pat));
    ok = ok && (f_write(&f,(const void*)smp_pat,sizeof(smp_pat),&bw)==FR_OK && bw==sizeof(smp_pat));
    ok = ok && (f_write(&f,(const void*)syn_seq,sizeof(syn_seq),&bw)==FR_OK && bw==sizeof(syn_seq));
    ok = ok && (f_write(&f,&m,4,&bw)==FR_OK && bw==4);   // trailing magic = complete
    f_close(&f);
    if(!ok){ f_unlink(tm); return; }
    char nm[16],bk[16]; seqaName(nm,p); seqaBak(bk,p);
    f_unlink(bk); f_rename(nm,bk);
    if(f_rename(tm,nm)!=FR_OK) f_rename(bk,nm);
}
static bool seqaLoadFile(const char* nm){
    FIL f; UINT br; uint32_t m;
    if(f_open(&f,nm,FA_READ)!=FR_OK) return false;
    if(f_read(&f,&m,4,&br)!=FR_OK || br!=4 || m!=SEQ_MAGIC){ f_close(&f); return false; }
    uint8_t hdr[2];
    if(f_read(&f,hdr,2,&br)!=FR_OK || br!=2){ f_close(&f); return false; }
    if(f_read(&f,(void*)dr_pat,sizeof(dr_pat),&br)!=FR_OK || br!=sizeof(dr_pat)){ f_close(&f); return false; }
    if(f_read(&f,(void*)smp_pat,sizeof(smp_pat),&br)!=FR_OK || br!=sizeof(smp_pat)){ f_close(&f); return false; }
    if(f_read(&f,(void*)syn_seq,sizeof(syn_seq),&br)!=FR_OK || br!=sizeof(syn_seq)){ f_close(&f); return false; }
    uint32_t tail; if(f_read(&f,&tail,4,&br)!=FR_OK || br!=4 || tail!=SEQ_MAGIC){ f_close(&f); return false; }
    f_close(&f);
    form_bars = hdr[0]; if(form_bars<1) form_bars=1; if(form_bars>8)  form_bars=8;
    form_blen = hdr[1]; if(form_blen<1) form_blen=1; if(form_blen>16) form_blen=16;
    return true;
}
static void seqaLoad(int p){
    if(!g_sd_ok) return;
    char nm[16]; seqaName(nm,p);
    if(seqaLoadFile(nm)) return;
    char bk[16]; seqaBak(bk,p);
    seqaLoadFile(bk);
}

static void persistSnapshot(Persist &s){
    s.step_ms = step_ms;
    // QSPI keeps the FIRST 16 steps (bar 0) so old presets stay compatible; the
    // full 128-step patterns live in the SD .SEQ sidecar (seqaSave).
    for(int i=0;i<4;i++){ s.dr_pat[i]=(uint16_t)(dr_pat[i][0] | (dr_pat[i][1]<<8));
                          s.dr_pad[i]=dr_pad[i];
                          s.dr_k[i]=(int8_t)dr_k[i]; s.dr_rot[i]=(int8_t)dr_rot[i]; }
    for(int i=0;i<16;i++){ s.smp_pat[i]=(uint16_t)(smp_pat[i][0] | (smp_pat[i][1]<<8));
                           s.fx_auto[i]=fx_auto[0][i]; }
    s.eu_k_syn=(int8_t)eu_k_syn; s.eu_rot_syn=(int8_t)eu_rot_syn; s.eu_k_smp=(int8_t)eu_k_smp;
    for(int i=0;i<3;i++) s.seq_mode[i]=(int8_t)seq_mode[i];
    s.vol_pidx=(int8_t)vol_pidx; s.menu=(int8_t)menu;
    s.sel_drum=(int8_t)sel_drum; s.sel_smp=(int8_t)sel_smp;
    s.g_key=(int8_t)g_key; s.g_mode=(int8_t)g_mode; s.g_swing=(int8_t)g_swing;
    s.g_csyn=(int8_t)g_csyn; s.g_cpoly=(int8_t)g_cpoly; s.g_cdrum=(int8_t)g_cdrum;
    s.g_wave=(int8_t)g_wave; s.kit_sel=(int8_t)kit_sel; s.g_csmp=(int8_t)g_csmp;
    s.form_blen=(int8_t)form_blen; s.form_bars=(int8_t)form_bars;
    for(int i=0;i<16;i++) s.fx_auto_fsr[i]=fx_auto_fsr[i];
    // per-engine mix levels live in the reserved block (backwards compatible:
    // stored +1 so an old patch's zero reads as "unset" → default unity on load)
    s.rsvd[0]=(uint8_t)(g_vsyn+1); s.rsvd[1]=(uint8_t)(g_vdrum+1); s.rsvd[2]=(uint8_t)(g_vsmp+1);
    // (the polyphonic synth sequence + full 128-step patterns persist in the SD
    // .SEQ sidecar now — rsvd[3..18] is free again)
    s.rsvd[19] = (uint8_t)(fx_bank + 1);   // current FX sub-page (0 = unset → FX1)
    s.rsvd[20] = (uint8_t)(g_wtscan + 1);  // wavetable scan position
}
static void persistApply(const Persist &s){
    step_ms = s.step_ms;
    if(step_ms < 60) step_ms = 60;
    if(step_ms > 500) step_ms = 500;
    // unpack QSPI's 16-step patterns into bar 0 of the 128-step buffers; the SD
    // .SEQ sidecar (if present) overlays the full long pattern after this.
    for(int i=0;i<4;i++){ for(int j=0;j<16;j++) dr_pat[i][j]=0;
                          dr_pat[i][0]=(uint8_t)(s.dr_pat[i]&0xFF); dr_pat[i][1]=(uint8_t)(s.dr_pat[i]>>8);
                          dr_pad[i]=s.dr_pad[i] & 15;
                          dr_k[i]=s.dr_k[i]; dr_rot[i]=s.dr_rot[i] & 15; }
    for(int i=0;i<16;i++){ for(int j=0;j<16;j++) smp_pat[i][j]=0;
                           smp_pat[i][0]=(uint8_t)(s.smp_pat[i]&0xFF); smp_pat[i][1]=(uint8_t)(s.smp_pat[i]>>8);
                           fx_auto[0][i]=s.fx_auto[i]; }
    for(int i=0;i<MAXSTEPS;i++) syn_seq[i]=0; syn_arm=0;   // SD sidecar refills the synth seq
    for(int b=1;b<8;b++) for(int i=0;i<16;i++) fx_auto[b][i]=0;  // banks 2-8 auto = live only
    eu_k_syn = s.eu_k_syn; if(eu_k_syn < 1) eu_k_syn = 1; if(eu_k_syn > 16) eu_k_syn = 16;
    eu_rot_syn = s.eu_rot_syn & 15;
    eu_k_smp = s.eu_k_smp; if(eu_k_smp < 1) eu_k_smp = 1; if(eu_k_smp > 16) eu_k_smp = 16;
    for(int i=0;i<3;i++){ int m = s.seq_mode[i]; seq_mode[i] = (m >= 0 && m <= 2) ? m : 0; }
    // MASTER VOLUME is a global — NOT recalled per patch (loading a preset never
    // changes how loud the rig is). vol_pidx/mvol keep their live runtime value.
    int mn = s.menu; menu = (mn >= 0 && mn < MENU_COUNT) ? mn : MENU_SYNTH;
    if(menu == MENU_SYNTH) eng_focus = ENG_SYNTH;
    else if(menu == MENU_DRUM) eng_focus = ENG_DRUM;
    else if(menu == MENU_SMP)  eng_focus = ENG_SMP;
    sel_drum = s.sel_drum & 3;
    sel_smp  = s.sel_smp & 15;
    g_key   = s.g_key;   if(g_key < -24) g_key = -24; if(g_key > 24) g_key = 24;
    g_mode  = s.g_mode;  if(g_mode < 0 || g_mode > 8) g_mode = 3;
    g_swing = s.g_swing; if(g_swing < 0) g_swing = 0; if(g_swing > 9) g_swing = 9;
    g_csyn  = s.g_csyn;  if(g_csyn  < 0) g_csyn  = 0; if(g_csyn  > 9) g_csyn  = 9;
    g_cpoly = s.g_cpoly; if(g_cpoly < 0) g_cpoly = 0; if(g_cpoly > 9) g_cpoly = 9;
    g_cdrum = s.g_cdrum; if(g_cdrum < 0) g_cdrum = 0; if(g_cdrum > 9) g_cdrum = 9;
    g_wave  = s.g_wave;  if(g_wave  < 0 || g_wave  >= WAVE_COUNT) g_wave  = 5;
    kit_sel = s.kit_sel & 1;
    g_csmp  = s.g_csmp;  if(g_csmp  < 0) g_csmp  = 0; if(g_csmp  > 9) g_csmp  = 9;
    form_blen = s.form_blen; if(form_blen < 1) form_blen = 16; if(form_blen > 16) form_blen = 16;
    form_bars = s.form_bars; if(form_bars < 1) form_bars = 1;  if(form_bars > 16) form_bars = 16;
    for(int i=0;i<16;i++) fx_auto_fsr[i]=s.fx_auto_fsr[i];
    { int v;
      v=(int)s.rsvd[0]-1; g_vsyn  = (v>=0&&v<=12)?v:10;
      v=(int)s.rsvd[1]-1; g_vdrum = (v>=0&&v<=12)?v:10;
      v=(int)s.rsvd[2]-1; g_vsmp  = (v>=0&&v<=12)?v:10; }
    { int fb=(int)s.rsvd[19]-1; fx_bank=(fb>=0&&fb<8)?fb:0; }   // restore FX sub-page
    { int ws=(int)s.rsvd[20]-1; g_wtscan=(ws>=0&&ws<=15)?ws:0; } // restore wavetable scan
    rebuildScale();
}
// ── Session bank management (QSPI) ──
static void sessionsBoot(){
    SessionBank def = {};
    def.magic = SESS_MAGIC; def.exists = 0x0001; def.active = 0;
    persistSnapshot(def.s[0]);                 // current state seeds slot 0
    // v2 CLEAN SLATE: the boot state used to seed slot 0 carried whatever
    // pattern happened to be live during testing, and every slot copied it --
    // so a "default beat" appeared in fresh sessions. Wipe all pattern data
    // out of the seed so a new session is genuinely empty.
    for(int i=0;i<4;i++)  def.s[0].dr_pat[i] = 0;
    for(int i=0;i<16;i++){ def.s[0].smp_pat[i] = 0;
                           def.s[0].fx_auto[i] = 0;
                           def.s[0].fx_auto_fsr[i] = 0; }
    def.s[0].menu = MENU_SYNTH;
    for(int i=1;i<16;i++) def.s[i] = def.s[0];
    pstore.Init(def);
    SessionBank &b = pstore.GetSettings();
    if(b.magic != SESS_MAGIC){                 // first run / different format
        b = def; pstore.Save();
    }
    sess_exists = b.exists;
    sess_cur    = (b.active < 16) ? b.active : 0;
    persistApply(b.s[sess_cur]);
}
static void sessionStash(){                    // current live state → active slot (RAM)
    persistSnapshot(pstore.GetSettings().s[sess_cur & 15]);
    pstore.GetSettings().exists = sess_exists;
    pstore.GetSettings().active = (uint8_t)sess_cur;
}
static void sessionLoad(int idx){              // switch active session
    idx &= 15;
    audEnsureSaved(sess_cur);                  // flush current patch's audio to SD
    fxaSave(sess_cur);                         // and its FX automation (all banks)
    seqaSave(sess_cur);                         // and the full sequence + song length
    sessionStash();                            // keep what we were editing
    sess_cur = idx;
    sess_exists |= (1u << idx);
    pstore.GetSettings().active = (uint8_t)idx;
    pstore.GetSettings().exists = sess_exists;
    // v2: persistApply restores the saved `menu`, which yanked you off the
    // SESS page the instant you tapped a patch. Loading a patch should change
    // the SOUND, not where you are. Preserve the current page across it.
    int _menu_keep = menu;
    persistApply(pstore.GetSettings().s[idx]);
    menu = _menu_keep;
    audLoad(idx);                              // bring in the new patch's audio
    fxaLoad(idx);                              // and its FX automation
    seqaLoad(idx);                             // and the full sequence + song length
    markDirty();
}
static void sessionNew(int idx){               // fresh empty session in slot idx
    idx &= 15;
    audEnsureSaved(sess_cur);                  // flush current patch's audio first
    fxaSave(sess_cur); seqaSave(sess_cur);
    sessionStash();
    Persist clean = {};
    clean.step_ms = 125;
    for(int i=0;i<4;i++) clean.dr_pad[i] = (uint8_t)((int[]){13,10,4,0}[i]);
    clean.eu_k_syn=5; clean.eu_k_smp=4;
    clean.seq_mode[0]=clean.seq_mode[1]=clean.seq_mode[2]=0;
    clean.vol_pidx=9; clean.menu=MENU_SYNTH;
    clean.g_key=0; clean.g_mode=3; clean.g_swing=0;
    clean.g_csyn=6; clean.g_cpoly=5; clean.g_cdrum=9;
    clean.g_wave=5; clean.kit_sel=0; clean.g_csmp=9;
    clean.form_blen=16; clean.form_bars=1;
    pstore.GetSettings().s[idx] = clean;
    sess_cur = idx;
    sess_exists |= (1u << idx);
    pstore.GetSettings().exists = sess_exists;
    pstore.GetSettings().active = (uint8_t)idx;
    persistApply(clean);                       // clears all FX automation banks
    for(int i=0;i<16;i++) smp_len[i] = 0;      // a new patch starts with no audio
    fxaSave(idx);                              // write the cleared automation (no stale FXA)
    for(int v=0;v<4;v++) for(int j=0;j<16;j++) dr_pat[v][j]=0;   // fresh patch = empty
    for(int sl2=0;sl2<16;sl2++) for(int j=0;j<16;j++) smp_pat[sl2][j]=0;
    for(int i=0;i<MAXSTEPS;i++) syn_seq[i]=0; syn_arm=0;
    form_bars=1; form_blen=16;
    seqaSave(idx);                             // write the cleared sequence (no stale SEQ)
    markDirty();
}
static void sessionDelete(int idx){            // wipe a slot (slot 0 always kept)
    idx &= 15;
    if(idx == 0) return;                        // never delete the home session
    sess_exists &= ~(1u << idx);
    pstore.GetSettings().exists = sess_exists;
    if(sess_cur == idx) sessionLoad(0);         // fell out of active → go home
    markDirty();
}

static volatile float out_amp = 0.f; // output peak — drives the fractal visualizer
static volatile float cut_t   = 1.f; // normalized cutoff 0..1 — drives fractal angle

// Playable mode: trellis key → active voice (-1 = none). Bottom row = lowest notes,
// pan spreads by column. Lidar octave zone applies at press, same as generative.
static int8_t key_voice[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static uint16_t held_mask = 0;   // currently held pads (bit i = pad i)
static uint32_t kprs_t[16] = {};  // press times — stuck-note safety
static uint32_t fx_t[16]  = {};   // FX press times — stuck-FX safety
// Per-note synth filter key-tracking: higher notes open the voice's own VCF
// (brighter), lower notes darker — classic filter tracking. Call after phaseInc.
static void filterTrack(int v){
    float trk = (voices[v].phaseInc * 48000.f) / 220.f;   // 1.0 ≈ A3
    if(trk < 0.5f) trk = 0.5f; if(trk > 2.6f) trk = 2.6f;
    voices[v].fc  *= trk; voices[v].fcT *= trk;
    if(voices[v].fc  > 0.9f) voices[v].fc  = 0.9f;
    if(voices[v].fcT > 0.9f) voices[v].fcT = 0.9f;
}
static void keyNoteOn(uint8_t k){
    int v = -1;
    for(int i=0;i<12;i++) if(voices[i].envTarget==0.f && voices[i].env<0.02f){ v=i; break; }
    if(v < 0){ static int rr=0; v=rr; rr=(rr+1)%12; }
    key_voice[k] = (int8_t)v;
    kprs_t[k] = System::GetNow();
    int note = (3-(k/4))*4 + (k%4);
    for(int j=0;j<5;j++) voices[v].phase[j]=0.f;
    voiceCharacter(v, false);
    vgate_t[v] = System::GetNow();
    voices[v].phaseInc  = FLYD[note] * 0.5f / 48000.f;   // low octave, lidar unhooked
    filterTrack(v);
    voices[v].prise = (g_prise>0) && ((lcg()%9u) < (uint32_t)g_prise);   // pitch-rise chance
    voices[v].pmul = voices[v].prise ? 0.5f : 1.f; voices[v].pmulT = 1.f;
    voices[v].relCoef   = 0.0004f;            // prime manual release
    if(live_nn < 4) live_nt[live_nn++] = note; // latch for stutter/BZZ registers
    voices[v].pan       = 0.2f + 0.6f*(float)(k%4)/3.f;
    voices[v].envTarget = 0.9f;
}
static void keyNoteOff(uint8_t k){
    if(key_voice[k]>=0){ voices[key_voice[k]].envTarget=0.f; key_voice[k]=-1; }
}
static void allKeysOff(){ for(uint8_t k=0;k<16;k++) keyNoteOff(k); }

// ── Drum engine — full 808-flavored kit, modular weirdness ───
// 4 render channels (one per pad row, bottom row = ch0): tone (sine w/
// exponential pitch drop) + optional 2nd partial (ratio per hit) + noise mix
// (ch2 = highpassed for hats/ride). Triggered from main loop via stamp
// counters; audio callback latches params when stamp changes. Drums bypass
// LPF+chorus and hit the output saturation directly.
struct DrumGo {
    volatile uint32_t stamp = 0;
    float f=60.f, amp=0.8f, dec=0.9999f, noiz=0.f, drop=1.f, pan=0.5f, part=0.f;
};
static DrumGo dgo[4];
static void drumTrig(int v, float f, float amp, float dec, float noiz,
                     float drop, float pan, float part){
    DrumGo& g = dgo[v];
    g.f=f; g.amp=amp; g.dec=dec; g.noiz=noiz; g.drop=drop; g.pan=pan; g.part=part;
    g.stamp++;   // bumped last so the callback sees settled params
}

// The kit — one entry per pad (key order: row0/top first). f<=1 = randomized
// (modular blip). Layout per row, bottom to top:
//   row3 ch0: KCK1/KCK2/KCK3 (transient-forward, tiny-speaker tuned) + TOML
//   row2 ch1: SNP1/SNP2 (snappy choppy) + SNRF (fat) + CHOP (airy slice)
//   row1 ch2: HCL/HMD/HOP hats + RIDE (long noise wash)
//   row0 ch3: TOMM/TOMH (808 toms) + BELL (808 cowbell, 540+845Hz) + BLIP
struct DrumDef { float f, amp, dec, noiz, drop, pan, part; };
// Pads run logical = physical order (key 0 = top-left). Types grouped per
// row: hats top (HCL top-left), snares, percs, kicks bottom (KCK1 bottom-
// left). Phonk gear: BELL = 808 cowbell (540+800Hz bark), K808 = booming
// phonk kick — 52Hz fundamental + huge pitch-drop click so it READS on tiny
// speakers. Two 808 toms with the classic long pitch-glide whoop.
static const DrumDef KIT[16] = {
    // row0 → ch3 (HP noise): HCL HMD HOP RIDE
    {6000.f, 0.42f, 0.99800f, 1.f, 1.f, 0.62f, 0.f},       // HCL  closed hat
    {6000.f, 0.40f, 0.99900f, 1.f, 1.f, 0.62f, 0.f},       // HMD  mid hat
    {6000.f, 0.38f, 0.99965f, 1.f, 1.f, 0.62f, 0.f},       // HOP  open hat
    {6000.f, 0.33f, 0.99988f, 1.f, 1.f, 0.58f, 0.f},       // RIDE long noise wash
    // row1 → ch2: SNP1 SNP2 SNRF CHOP
    {210.f, 0.80f, 0.99950f, 0.85f, 1.5f, 0.45f, 0.f},     // SNP1 snappy
    {240.f, 0.78f, 0.99938f, 0.90f, 1.6f, 0.50f, 0.f},     // SNP2 choppier/brighter
    {175.f, 0.80f, 0.99968f, 0.78f, 1.4f, 0.45f, 0.f},     // SNRF fat snare
    {300.f, 0.72f, 0.99930f, 0.95f, 1.2f, 0.55f, 0.f},     // CHOP airy slice
    // row2 → ch1: TOMM TOMH BELL BLIP
    {130.f, 0.92f, 0.99982f, 0.04f, 2.2f, 0.42f, 0.f},     // TOMM 808 mid tom
    {190.f, 0.90f, 0.99978f, 0.04f, 2.2f, 0.60f, 0.f},     // TOMH 808 hi tom
    {540.f, 0.90f, 0.99945f, 0.04f, 1.0f, 0.55f, 1.48f},   // BELL phonk 808 cowbell
    {  0.f, 0.55f, 0.99940f, 0.06f, 1.8f, 0.50f, 2.717f},  // BLIP modular (random f/pan)
    // row3 → ch0: KCK1 KCK2 KCK3 K808 — club/808 split, fundamentals are
    // RETUNED to the global key at trigger time (see drumPadHit). The f values
    // here are nominal; only their RATIO to ~52Hz sets each kick's relative
    // pitch, so the whole row tracks the key together.
    { 56.f, 0.97f, 0.99984f, 0.06f, 4.2f, 0.50f, 0.f},     // KCK1 punchy club (tight)
    { 52.f, 1.00f, 0.99993f, 0.05f, 6.0f, 0.50f, 0.f},     // KCK2 808 (long sub)
    { 60.f, 0.97f, 0.99980f, 0.10f, 4.8f, 0.50f, 0.f},     // KCK3 punchy club (clicky)
    { 52.f, 1.00f, 0.99995f, 0.05f, 7.0f, 0.50f, 0.f},     // K808 phonk boom (the keeper)
};
static const char* PADN[16] = {"HCL", "HMD", "HOP", "RIDE",
                               "SNP1","SNP2","SNRF","CHOP",
                               "TOMM","TOMH","BELL","BLIP",
                               "KCK1","KCK2","KCK3","K808"};

// KIT 2 — ELECTRONIC kit (alt timbres): brighter metallic hats, clap/zap
// snares, FM/laser percs, 909-ish + hard techno kicks. Same engine, fresh
// voicing. Kicks (row3) still retune to the global key.
static const DrumDef KIT2[16] = {
    // row0 ch3 (HP noise): tight→open metallic hats
    {9000.f, 0.40f, 0.99770f, 1.f, 1.f, 0.60f, 0.f},   // EHAT tight
    {9000.f, 0.38f, 0.99880f, 1.f, 1.f, 0.64f, 0.f},   // EHAT mid
    {9000.f, 0.36f, 0.99955f, 1.f, 1.f, 0.66f, 0.f},   // EHAT open
    {7200.f, 0.34f, 0.99986f, 1.f, 1.f, 0.55f, 0.f},   // ECYM crash-ish
    // row1 ch1→ch2: clap / electro snares
    {1100.f, 0.70f, 0.99955f, 0.95f, 1.f, 0.45f, 0.f}, // CLAP
    {320.f,  0.72f, 0.99940f, 0.80f, 1.4f, 0.52f, 0.f},// ESNR electro
    {180.f,  0.78f, 0.99965f, 0.70f, 1.5f, 0.45f, 0.f},// RIM/snap
    {2200.f, 0.60f, 0.99935f, 0.92f, 1.f, 0.58f, 0.f}, // ZAP noise slice
    // row2 ch1: FM/laser percs + cosmic blip
    {220.f,  0.80f, 0.99955f, 0.04f, 3.0f, 0.42f, 1.50f},// LZTOM laser tom (FM)
    {330.f,  0.78f, 0.99950f, 0.04f, 3.0f, 0.60f, 2.01f},// FMBELL
    {660.f,  0.74f, 0.99930f, 0.05f, 1.0f, 0.55f, 3.46f},// COWB-2 metallic
    {0.f,    0.55f, 0.99935f, 0.05f, 2.2f, 0.50f, 4.21f},// COSMO random blip
    // row3 ch0: kicks — 909, distorted club, sub, hard techno (retune to key)
    {55.f, 1.00f, 0.99988f, 0.08f, 4.0f, 0.50f, 0.f},  // K909 punchy
    {52.f, 1.00f, 0.99992f, 0.12f, 5.5f, 0.50f, 0.f},  // KCLUB driven
    {50.f, 1.00f, 0.99996f, 0.03f, 3.0f, 0.50f, 0.f},  // KSUB clean sub
    {58.f, 1.00f, 0.99985f, 0.20f, 8.0f, 0.50f, 0.f},  // KHARD techno
};
static const char* PADN2[16] = {"EHAT1","EHAT2","EHAT3","ECYM",
                                "CLAP","ESNR","RIM","ZAP",
                                "LZTOM","FMBEL","MTL","COSMO",
                                "K909","KCLUB","KSUB","KHARD"};

static void drumPadHit(uint8_t k){
    k &= 15;
    const DrumDef& d = kit_sel ? KIT2[k] : KIT[k];
    float f = d.f, pan = d.pan;
    if(f <= 1.f) {   // modular blip: random pitch + pan every hit
        f   = 300.f + (float)(lcg()%2100);
        pan = 0.1f  + 0.8f*(float)(lcg()%100)/99.f;
    }
    // KICKS (row3, ch0) are tuned to the global key: F1 (43.65Hz) shifted by
    // g_key, scaled by each kick's nominal/52Hz ratio so club/808 keep their
    // relative pitch while the row sits in-key with the synth.
    if(k >= 12) {
        float kroot = 43.65f * powf(2.f, (float)g_key / 12.f);
        f = kroot * (d.f / 52.f);
    }
    drumTrig(3 - (k/4), f, d.amp, d.dec, d.noiz, d.drop, pan, d.part);
    live_dr |= (uint8_t)(1u << (3 - (k/4)));   // latch for stutter/BZZ registers
}

// Euclid pad k-tables per voice (col 0..3, ascending left to right)
static const uint8_t EUK[4][4] = {
    {2, 4, 5, 8},     // ch0 kick
    {1, 2, 3, 5},     // ch1 perc
    {2, 3, 5, 7},     // ch2 snare
    {4, 8, 12, 16},   // ch3 hat
};

static volatile float mic_amp = 0.f;
// Chamberlin SVF F1 = 2*sin(π*fc/48000); updated from ax. Upright ≈ 1kHz.
static volatile float filter_f = 0.62f;   // master tilt-filter: starts OPEN (only swept in FX)
// ── Grid clock — runs INSIDE the audio interrupt ─────────────
// The sequencer is real-time-plane: called once per audio block (1.3ms),
// so OLED rendering, trellis I2C and sensor reads can NEVER delay a step.
// Everything here is memory-only (no I2C, no blocking) — interrupt-safe.
static void gridTick(uint32_t now){
            static uint32_t next_step = 0;
            static bool     vplay[12] = {};
            static uint32_t vrel[12]  = {};
            static int      nextv     = 0;
            static bool     vinit     = false;
            if(!vinit){ vinit=true; next_step=now+step_ms; }

            for(int v=0;v<12;v++){
                if(vplay[v] && now >= vrel[v]){
                    voices[v].envTarget = 0.f;
                    vplay[v] = false;
                }
                // ABSOLUTE gate cap: no voice may sustain >8s, no matter what
                if(voices[v].envTarget > 0.f && now - vgate_t[v] > 8000u)
                    voices[v].envTarget = 0.f;
            }
            // orphan sweep: a gated voice owned by neither a held pad nor the
            // generative scheduler is a stray — kill it instantly, not in 30s
            {
                uint16_t owned = 0;
                for(int k2=0;k2<16;k2++)
                    if(key_voice[k2] >= 0) owned |= (1u << key_voice[k2]);
                for(int v=0;v<12;v++)
                    if(voices[v].envTarget > 0.f && !vplay[v] && !((owned >> v) & 1))
                        voices[v].envTarget = 0.f;
            }

            // stuck-note / stuck-FX safety: a lost trellis release event can
            // never drone forever — manual notes cap at 15s, FX holds at 20s
            for(int k2 = 0; k2 < 16; k2++){
                if(key_voice[k2] >= 0 &&
                   (!((held_mask >> k2) & 1) || now - kprs_t[k2] > 15000u))
                    keyNoteOff((uint8_t)k2);
                if(((fx_held >> k2) & 1) && now - fx_t[k2] > 20000u)
                    fx_held &= ~(1u << k2);
            }

            // Effective FX per bank = live pads override, else recorded
            // automation. Works for ALL 6 banks. FSR replays with automation.
            {
                int gs = gstep & 15;
                uint16_t lv0=fx_held, lv1=fx2_held, lv2=fx3_held,
                         lv3=fx4_held, lv4=fx5_held, lv5=fx6_held,
                         lv6=fx7_held, lv7=fx8_held;
                fx_eff  = lv0 ? lv0 : fx_auto[0][gs];
                fx_eff2 = lv1 ? lv1 : fx_auto[1][gs];
                fx_eff3 = lv2 ? lv2 : fx_auto[2][gs];
                fx_eff4 = lv3 ? lv3 : fx_auto[3][gs];
                fx_eff5 = lv4 ? lv4 : fx_auto[4][gs];
                fx_eff6 = lv5 ? lv5 : fx_auto[5][gs];
                fx_eff7 = lv6 ? lv6 : fx_auto[6][gs];
                fx_eff8 = lv7 ? lv7 : fx_auto[7][gs];
                uint16_t liveAny = lv0|lv1|lv2|lv3|lv4|lv5|lv6|lv7;
                uint16_t autoAny = fx_eff|fx_eff2|fx_eff3|fx_eff4|fx_eff5|fx_eff6|fx_eff7|fx_eff8;
                fx_fsr_eff = liveAny ? fx_fsr
                           : (autoAny ? (float)fx_auto_fsr[gs] / 255.f : fx_fsr);
            }

            // Sampler hold→record arming: a pad held ≥300ms starts recording
            if(rec_slot < 0) {
                for(int k2 = 0; k2 < 16; k2++) {
                    if(smp_press[k2] && now - smp_press[k2] > 300) {
                        rec_pos = 0;
                        rec_slot = k2;
                        smp_press[k2] = 0;
                        break;
                    }
                }
            }

            // WASH FX — force long blurry tails on every voice while held;
            // FSR squeeze stretches the wash even longer
            if(fxOn(FX_WASH)) {
                float wc = 0.00012f - fx_fsr * 0.00008f;   // floor = prime 3s tail max
                for(int v=0;v<12;v++) { voices[v].relCoef = wc; }
            }

            // ── TIMING GUARANTEE ───────────────────────────────────────────
            // The master clock ALWAYS runs at step_ms — no FX may touch it.
            // Tempo-bending FX play a shadow position (bstep) while the master
            // counts on silently; release = instant snap back into phase.
            // HALF/DBL: clean 0.5x / 2x, pitch glides via the slewed vss.
            const bool half_on = fxOn(FX_HALF);
            const bool dbl_on  = fxOn(FX_DBL);
            const bool stp_on  = fxOn(FX_STOP);   // tape stop: pitch grinds to ~0
            fx_vs = stp_on ? 0.06f : (half_on ? 0.5f : (dbl_on ? 2.f : 1.f));
            static int  bstep = 0;
            static bool bent_prev = false;
            {
                bool bent = half_on || dbl_on;
                if(bent && !bent_prev) bstep = gstep;
                bent_prev = bent;
            }

            // MIDI-style stutter: steps update the content registers; firing
            // happens on the grid-anchored timer below. PERIOD IS FLOAT so the
            // subdivision never truncates (was the 32N "starts a bit late" drift
            // — at 125ms/step, step_ms/2 = 62 not 62.5, accumulating error).
            float rt_per = 0.f;
            if(fxOn(FX_ST4))  rt_per = (float)step_ms * 4.0f;       // quarter (slowest)
            if(fxOn(FX_ST8))  rt_per = (float)step_ms * 2.0f;       // eighth
            if(fxOn(FX_ST16)) rt_per = (float)step_ms;             // sixteenth
            if(fxOn(FX_ST32)) rt_per = (float)step_ms * 0.5f;      // thirty-second
            if(rt_per > 0.f && rt_per < 6.f) rt_per = 6.f;
            const bool st_on = (rt_per > 0.f);
            static uint8_t  rt_dr = 0;
            static int      rt_nv = 0;
            static int      rt_notes[8] = {};
            static uint32_t step_t = 0;   // wall time of the last step boundary
            static uint32_t sct    = 0;   // master step counter

            // shared trigger path for steps AND retrigs — always quantized
            auto fireNote = [&](int note){
                int v = nextv;
                for(int i2=0;i2<12;i2++){
                    if(!vplay[(nextv+i2)%12]){ v=(nextv+i2)%12; break; }
                }
                nextv = (v+1)%12;
                for(int k=0;k<5;k++) voices[v].phase[k] = 0.f;
                voiceCharacter(v, true);
                vgate_t[v] = now;
                voices[v].phaseInc  = FLYD[note] * 0.5f / 48000.f; // low octave, lidar unhooked
                filterTrack(v);
                voices[v].prise = (g_prise>0) && ((lcg()%9u) < (uint32_t)g_prise);
                voices[v].pmul = voices[v].prise ? 0.5f : 1.f; voices[v].pmulT = 1.f;
                voices[v].envTarget = 0.6f + (float)(lcg()%35)/100.f;
                voices[v].pan       = 0.15f + (float)(lcg()%71)/100.f;
                float rr = (float)(lcg()%64) / 63.f;
                voices[v].relCoef  = 0.000048f * powf(30.f, rr);
                vplay[v] = true;
                vrel[v]  = now + 100;
            };

            const bool bz_on   = fxOn(FX_BZZ);
            const bool rt_hold = st_on || bz_on;   // a retrig FX is engaged

            // compute step content at a pattern position; fire the PATTERN
            // part if asked. Registers absorb anything played MANUALLY since
            // the last step (live latch) so stutters always have material,
            // and stay sticky while a retrig FX is held.
            auto stepContent = [&](int pos, bool fire){
                pos &= (MAXSTEPS - 1);
                uint8_t dmask = 0;
                // DRUMS — single unified mode: always play the step pattern.
                for(int v=0; v<4; v++)
                    if(seqBit(dr_pat[v], pos)) dmask |= (uint8_t)(1u << v);
                int nv = 0;
                int notes[8];
                int sm = seq_mode[ENG_SYNTH];
                if(sm == SYN_GEN){
                    // g_csyn = density (0 = silence, 9 = a note every step);
                    // g_cpoly = independent chord-extension chance per extra note
                    if((lcg() % 9u) < (uint32_t)g_csyn){
                        nv = 1; notes[0] = (int)(lcg() % 28u);
                        for(int e=1; e<4; e++){
                            if((lcg() % 9u) < (uint32_t)g_cpoly) notes[nv++] = (int)(lcg() % 28u);
                            else break;
                        }
                    }
                } else if(sm == SYN_SEQ){
                    // polyphonic step sequencer: fire every pad-note set on this step
                    uint16_t m = syn_seq[pos];
                    for(int p=0; p<16 && nv<8; p++)
                        if((m>>p)&1u) notes[nv++] = (3-(p/4))*4 + (p%4);
                }
                // SYN_ARP is resolved in the fire block (generative walk)
                if(fire){
                    if(sm == SYN_ARP){
                        // generative arpeggio: each step pick a random note from the
                        // set of notes used anywhere in the sequence
                        uint16_t uni = 0; int sl2 = songLen();
                        for(int q=0; q<sl2; q++) uni |= syn_seq[q];
                        nv = 0;
                        if(uni){
                            int cnt = __builtin_popcount(uni);
                            int pick = (int)(lcg() % (uint32_t)cnt);
                            for(int p=0; p<16; p++) if((uni>>p)&1u){
                                if(pick-- == 0){ notes[0] = (3-(p/4))*4 + (p%4); nv = 1; break; }
                            }
                            syn_arp_i = pick;   // (unused, keeps state warm)
                        }
                    }
                    // g_cdrum = master chance a sequenced hit actually lands
                    for(int v=0; v<4; v++)
                        if((dmask & (1u << v)) && (lcg() % 9u) < (uint32_t)g_cdrum)
                            drumPadHit(dr_pad[v] & 15);
                    for(int n=0; n<nv; n++) fireNote(notes[n]);
                }
                // registers: pattern ∪ live latch, sticky during retrig holds
                uint8_t newdr = dmask | live_dr;
                if(newdr || !rt_hold) rt_dr = newdr;
                if(nv == 0 && live_nn > 0){
                    nv = (live_nn > 8) ? 8 : live_nn;
                    for(int n=0; n<nv; n++) notes[n] = live_nt[n];
                }
                if(nv || !rt_hold){
                    rt_nv = nv;
                    for(int n=0; n<nv; n++) rt_notes[n] = notes[n];
                }
                live_dr = 0;
                live_nn = 0;
            };

            static uint32_t next_dbl = 0;   // DBL mid-step fire deadline
            if(now >= next_step){
                // MASTER CLOCK: absolute deadlines, constant tempo — FX can
                // never bend it. SWING delays the off-16ths (g_swing) while
                // keeping the average interval = step_ms, so tempo is unchanged.
                step_t = next_step;
                {
                    float sw = (float)g_swing / 9.f * 0.34f;   // up to ~⅓ swing
                    uint32_t lng = (uint32_t)((float)step_ms * (1.f + sw));
                    uint32_t sht = step_ms * 2u - lng;
                    next_step += ((gstep & 1) ? lng : sht);   // about to play even→odd long
                }
                if(now >= next_step) next_step = now + step_ms;
                sct++;
                { int sl = songLen(); gstep = gstep + 1; if(gstep >= sl) gstep = 0; }
                step_rw = rb_w;   // ring position at the step boundary (REV anchor)

                // SAMPLER — single unified mode: step patterns fire any slot.
                // g_csmp master chance gates each hit.
                for(int s = 0; s < 16; s++)
                    if(seqBit(smp_pat[s], gstep) && smp_len[s]
                       && (lcg() % 9u) < (uint32_t)g_csmp) smp_trig[s]++;

                // FX automation write head: right encoder held on the FX page
                // records the CURRENT bank's live mask into each step as the
                // loop passes (works on every bank; empty hands scrub clean).
                if(menu == MENU_FX && !vol_mode && enc2.psw == 0 && enc1.psw != 0) {
                    uint16_t lvb = (fx_bank==0)?fx_held:(fx_bank==1)?fx2_held:(fx_bank==2)?fx3_held
                                   :(fx_bank==3)?fx4_held:(fx_bank==4)?fx5_held:(fx_bank==5)?fx6_held
                                   :(fx_bank==6)?fx7_held:fx8_held;
                    fx_auto[fx_bank % 8][gstep & 15] = lvb;
                    fx_auto_fsr[gstep & 15] = (uint8_t)(fx_fsr * 255.f); markDirty();
                }
                // SYNTH live-record: holding the right encoder in the synth SEQ
                // editor writes the currently-held notes into each step as the
                // playhead passes (empty hands scrub the step clean) — like FX.
                if(menu == MENU_SYNTH && seq_view && seq_mode[ENG_SYNTH] == SYN_SEQ
                   && !vol_mode && enc2.psw == 0) {
                    syn_seq[gstep] = held_mask; markDirty();
                }

                // Engines: normal = fire the master step. HALF = the shadow
                // position advances (and sounds) every other master step.
                // DBL = shadow advances+fires here AND at the mid-step below.
                if(half_on){
                    if((sct & 1u) == 0u){ bstep = (bstep + 1) & 15; stepContent(bstep, !st_on && !stp_on); }
                }
                else if(dbl_on){
                    bstep = (bstep + 1) & 15;
                    stepContent(bstep, !st_on && !stp_on);
                    next_dbl = step_t + (step_ms >> 1);
                }
                else stepContent(gstep, !st_on && !stp_on);
            }

            // DBL mid-step fire (shadow plays double-time against the master)
            if(dbl_on && next_dbl && now >= next_dbl){
                next_dbl = 0;
                bstep = (bstep + 1) & 15;
                stepContent(bstep, !st_on && !stp_on);
            }
            if(!dbl_on) next_dbl = 0;

            // STUTTER scheduler — FULLY GRID-LOCKED. Every retrig lands on a real
            // grid subdivision (no free-running ms accumulator, so it can never
            // drift off the beat). On press it ARMS and waits for the next boundary
            // of its own subdivision (1/16 → next 16th, 1/8 → next even 16th, 1/4 →
            // next downbeat), then keeps firing exactly on grid. The only sub-16th
            // rate (1/32) fires a second hit at the step midpoint, re-anchored to
            // the real step-boundary time every step so it stays tight. Release
            // stops instantly.
            {
                static float    rt_prev  = 0.f;   // last rate (0 = idle)
                static bool     rt_armed = false; // waiting for the first subdivision
                static int      rt_lastg = -1;    // last gstep we acted on
                static uint32_t rt_half  = 0;     // 1/32 midpoint deadline (anchored to step_t)
                auto stutFire = [&](){
                    for(int v=0; v<4; v++)
                        if(rt_dr & (1u << v)) drumPadHit(dr_pad[v] & 15);
                    for(int n=0; n<rt_nv && n<8; n++) fireNote(rt_notes[n]);
                };
                if(st_on && !stp_on){
                    int  psteps = (int)(rt_per / (float)step_ms + 0.5f); if(psteps < 1) psteps = 1;
                    bool sub16  = (rt_per < (float)step_ms * 0.9f);   // 1/32 → 2 hits per 16th
                    if(rt_prev != rt_per){ rt_prev = rt_per; rt_armed = true; rt_half = 0; }
                    if(rt_lastg != gstep){            // a new 16th step just landed
                        rt_lastg = gstep;
                        if((gstep % psteps) == 0){    // this step is on our subdivision
                            rt_armed = false;
                            stutFire();
                            if(sub16) rt_half = step_t + (step_ms >> 1);
                        }
                    }
                    // 1/32 midpoint hit — anchored to the real step time, no drift
                    if(!rt_armed && sub16 && rt_half && now >= rt_half){ rt_half = 0; stutFire(); }
                } else { rt_prev = 0.f; rt_armed = false; rt_half = 0; rt_lastg = -1; }
            }

            // BZZ — generative oscillating roll: every 16th it PICKS A NEW
            // SPEED from a musical set (div 2,3,4,6,8 of the step) and rolls
            // at it, re-anchored to the grid. FSR biases toward faster picks.
            {
                static uint32_t next_bz = 0;
                static uint32_t bz_iv   = 60;
                static uint32_t bz_sct  = 0xFFFFFFFFu;
                if(bz_on && !stp_on){
                    if(bz_sct != sct){
                        bz_sct = sct;
                        static const uint8_t DIVS[5] = {2, 3, 4, 6, 8};
                        int idx = (int)(lcg() % 3u) + (int)(fx_fsr * 2.49f); // 0..4
                        bz_iv = step_ms / DIVS[idx];
                        if(bz_iv < 6u) bz_iv = 6u;
                        next_bz = step_t;
                        while(next_bz <= now) next_bz += bz_iv;
                    }
                    if(now >= next_bz){
                        next_bz += bz_iv;
                        if(now >= next_bz) next_bz = now + bz_iv;
                        for(int v=0; v<4; v++)
                            if(rt_dr & (1u << v)) drumPadHit(dr_pad[v] & 15);
                        for(int n=0; n<rt_nv && n<8; n++) fireNote(rt_notes[n]);
                    }
                } else bz_sct = 0xFFFFFFFFu;
            }
}
// ── USB UAC2 capture bridge — decouples this instrument's 64-sample audio
// block from UAC2's fixed 48-samples-per-1ms-frame contract
// (usb_audio_stream_push_block() requires n_frames == AUDIO_BLOCK_SAMPLES
// (48) exactly, see usb/usb_audio_stream.h). hw.SetAudioBlockSize() stays
// 64 (untouched — the sequencer/DSP are tuned around it); average sample
// rate is identical either way (48000 Hz), so a small ring buffer drained
// by exactly 48 whenever that many samples are available keeps the two
// block sizes in lock-step over time with no resampling. Deterministic
// 3-callback cycle at 64-in/48-out: buffered 64→drain48(16 left), +64=80→
// drain48(32 left), +64=96→drain48,drain48(0 left) — repeats forever, ring
// never overflows a 192-sample (=lcm(64,48)) capacity. Pushes the RAW ADC
// input, not the FX-processed output — a host recording over USB should
// hear the true input, matching the source project's capture-path design
// (see usb/board_stm32h7.h / clock_engine.c "USB capture gets the raw
// ADC input" comment). Audio-ISR-only state (AudioCallback is the only
// caller), so plain (non-atomic, non-volatile) statics are fine — same
// convention the rest of this callback already uses for its own statics.
#define USB_CAP_RING_N 192u
static float    usb_cap_l[USB_CAP_RING_N];
static float    usb_cap_r[USB_CAP_RING_N];
static uint32_t usb_cap_w      = 0;
static uint32_t usb_cap_r_idx  = 0;
static inline void usbCapturePush(const float* inL, const float* inR, size_t n) {
    for(size_t i = 0; i < n; i++) {
        usb_cap_l[usb_cap_w % USB_CAP_RING_N] = inL[i];
        usb_cap_r[usb_cap_w % USB_CAP_RING_N] = inR[i];
        usb_cap_w++;
    }
    while(usb_cap_w - usb_cap_r_idx >= 48u) {
        static float chL[48], chR[48];
        for(uint32_t k = 0; k < 48u; k++) {
            chL[k] = usb_cap_l[(usb_cap_r_idx + k) % USB_CAP_RING_N];
            chR[k] = usb_cap_r[(usb_cap_r_idx + k) % USB_CAP_RING_N];
        }
        // USB-audio fidelity test tone (CDC: SINE1/SINE0): replace the
        // capture content with a device-generated 440Hz sine. Everything
        // else (synth, sensors, UI, trellis) keeps running — this isolates
        // the TRANSPORT: any glitch the host measures in this tone happened
        // in the ISR->ring->drain->FIFO->USB path, not in the music.
        if(g_usb_sine) {
            static float sph = 0.f;
            const float w = 2.f * 3.14159265f * 440.f / 48000.f;
            for(uint32_t k = 0; k < 48u; k++) {
                float s = 0.5f * sinf(sph);
                sph += w; if(sph > 6.2831853f) sph -= 6.2831853f;
                chL[k] = s; chR[k] = s;
            }
        }
        float const* chp[2] = { chL, chR };
        // MECHANISM TEST: the proposed root cause is that this call --
        // running in the SAI DMA ISR at priority 0 -- races the OTG ISR
        // inside TinyUSB's unlocked ep_in_ff FIFO and wedges the DWC2 core.
        // Masking the OTG IRQ around it and raising USB's priority did NOT
        // fix the display+USB failure, so test the mechanism directly: with
        // NO audio pushed to USB there is no second FIFO writer at all.
        //   display works + USB survives -> race confirmed, structural fix
        //     (SPSC ring drained from the main loop) is the answer
        //   USB still dies            -> the race theory is wrong too
        if(g_en_usbaudio) usb_audio_stream_push_block(chp, 2, 48);
        usb_cap_r_idx += 48u;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t sz) {
    if(enc_ready){ ePoll(enc1); joyPoll(); }
    // Advance the clock engine's sample clock. This instrument does NOT
    // route audio through clock_engine_process_audio() (it has its own
    // synth path), so without this call g_sample_clock never moves, the
    // boot-listen window never elapses, and the device never emits MIDI
    // clock — host apps show no BPM (found 2026-08-16).
    clock_engine_note_audio_block((uint32_t)sz);
    gridTick(System::GetNow());   // sequencer in the real-time plane
    float pk=0.f, opk=0.f;
    // Q1 = damping = 1/Q_filter. 0.8 → Q≈1.25 — gentle musical resonance, no honk.
    const float Q1 = 0.8f;   // per-voice/drum filter damping (fixed)
    static float lpL=0,bpL=0, lpR=0,bpR=0;
    static float dlpL=0,dbpL=0, dlpR=0,dbpR=0;   // drum copy of the accel filter
    // Cutoff slews per-sample toward the 50ms sensor target (τ≈14ms) — no zipper steps.
    static float ff_s = 0.62f;
    // Stereo chorus: ~14ms center delay swept ±3.75ms, quadrature LFOs L/R.
    static float    cbufL[1024] = {}, cbufR[1024] = {};
    static uint32_t cw   = 0;
    static float    clfo = 0.f;
    // drum voice run-state (latched from dgo[] on stamp change)
    struct DRun { float env, ph, f, ft, dec, noiz, amp, panl, panr, part; };
    static DRun     drun[4]   = {};
    static uint32_t dstamp[4] = {};
    static uint32_t arng  = 0x12345678u;  // audio-rate noise rng
    static float    hatlp = 0.f;
    // sampler playback/record state
    static uint32_t sp_pos[16] = {};
    static uint32_t sp_st[16]  = {};
    static bool     sp_act[16] = {};
    static float    sp_fr[16]  = {};
    const int       rsl = rec_slot;       // one volatile read per block
    // ── punch-in FX state ──
    static float    crL = 0.f, crR = 0.f;            // CRUSH sample-and-hold
    static uint32_t crn = 0;
    static bool     buf_prev = false;                // buffer-FX edge detect
    static uint32_t fz_anchor = 0;                   // stutter/reverse anchor
    static float    rev_pos = 0.f;                   // reverse read position
    static float    xcL[4096] = {}, xcR[4096] = {};  // FX chorus w/ feedback
    static uint32_t xw = 0;
    static float    xlfo = 0.f, panph = 0.f;
    const uint16_t fh    = fx_eff;                   // one volatile read per block
    const float    ffsr  = fx_fsr_eff;   // live squeeze OR replayed automation FSR
    const bool     crush = ((fh >> FX_CRUSH) & 1u) != 0;
    const uint32_t cint  = 4u + (uint32_t)(ffsr * 20.f);   // crush rate ← FSR
    // varispeed slew (tape glide between speeds — click-free)
    static float vss = 1.f;
    const float  vst = fx_vs;
    const bool rev   = ((fh >> FX_REV) & 1u) != 0;
    const bool buffx = rev;
    if(buffx && !buf_prev){ fz_anchor = step_rw; rev_pos = 0.f; }
    buf_prev = buffx;
    uint32_t rw = rb_w;   // local ring head for this block
    // NaN/inf guard: a non-finite filter state would latch and kill audio
    // forever — flush instead (NaN*0 != 0 catches NaN and inf)
    if(!(lpL*0.f == 0.f) || !(bpL*0.f == 0.f)) { lpL = 0.f; bpL = 0.f; }
    if(!(lpR*0.f == 0.f) || !(bpR*0.f == 0.f)) { lpR = 0.f; bpR = 0.f; }
    if(!(dlpL*0.f == 0.f) || !(dbpL*0.f == 0.f)) { dlpL = 0.f; dbpL = 0.f; }
    if(!(dlpR*0.f == 0.f) || !(dbpR*0.f == 0.f)) { dlpR = 0.f; dbpR = 0.f; }
    if(!(ff_s*0.f == 0.f)) ff_s = 0.62f;
    const float wobd  = 0.f;    // vibrato/warble OFF — clean, pure, mono-simple tone
    const float brtd  = 0.f;    // tremolo/breathing OFF
    static float wobph = 0.f, brph = 0.f;
    const float cwet  = 0.f;     // chorus OFF — the swept delay was the "modulation"; dry = CLEAN
    const float drv   = 2.0f;    // a touch less drive — cleaner, more pure
    // master volume is applied at the VERY END (after every FX), so turning it
    // down kills FX feedback/static too. The mix saturates at a FIXED drive here.
    const float mgain = 2.0f;
    const float mvol_l = mvol;   // snapshot for the end-of-chain volume stage
    const float gvSyn = engVol(g_vsyn), gvDrum = engVol(g_vdrum), gvSmp = engVol(g_vsmp);
    // grid-locked phase for gate FX: re-sync the sub-step sample counter to the
    // real playhead at every step boundary so gates never drift off the beat.
    if(gstep != gstep_blk){ gstep_samp = 0; gstep_blk = gstep; }
    float gslen = (float)step_ms * 48.f; if(gslen < 1.f) gslen = 1.f;   // samples / 1/16
    for(size_t i=0;i<sz;i++){
        float s=in[0][i]; if(s<0)s=-s; if(s>pk)pk=s;
        vss += (vst - vss) * 0.0005f;   // fast tape rise (~40ms), rhythm stays on grid
        wobph += 0.9f  / 48000.f; if(wobph >= 1.f) wobph -= 1.f;
        brph  += 0.13f / 48000.f; if(brph  >= 1.f) brph  -= 1.f;
        float wx = wobph*2.f - 1.f;
        float bx = brph*2.f  - 1.f;
        float wob    = 1.f + wobd * 4.f*wx*(1.f - fabsf(wx));
        float breath = 1.f - brtd * (0.5f + 2.f*bx*(1.f - fabsf(bx)));
        float mixL=0.f, mixR=0.f;
        for(int v=0;v<12;v++){
            // idle-voice skip: silent voices cost nothing
            if(voices[v].env < 0.0004f && voices[v].envTarget == 0.f) continue;
            float eCoef = (voices[v].envTarget > voices[v].env) ? voices[v].atk : voices[v].relCoef;
            voices[v].env += (voices[v].envTarget - voices[v].env) * eCoef;
            voices[v].drift += (voices[v].driftT - voices[v].drift) * 0.00002f;
            // pitch-rise: glide the frequency multiplier toward its target (1.0 while
            // held, 0.5 = octave down on release) at ~1s per octave
            if(voices[v].prise){
                voices[v].pmulT = (voices[v].envTarget > 0.f) ? 1.f : 0.5f;
                float pr = 0.5f / 48000.f;   // 12 semitones over ~1 second
                if(voices[v].pmul < voices[v].pmulT){ voices[v].pmul += pr; if(voices[v].pmul > voices[v].pmulT) voices[v].pmul = voices[v].pmulT; }
                else if(voices[v].pmul > voices[v].pmulT){ voices[v].pmul -= pr; if(voices[v].pmul < voices[v].pmulT) voices[v].pmul = voices[v].pmulT; }
            }
            float inc = voices[v].phaseInc * wob * vss * (1.f + voices[v].drift)
                      * (voices[v].prise ? voices[v].pmul : 1.f);
            float raw;
            if(voices[v].type == 2){
                voices[v].fmi += (voices[v].fmiT - voices[v].fmi) * 0.00015f;
                voices[v].phase[1] += inc * voices[v].ratio[1];
                if(voices[v].phase[1] >= 1.f) voices[v].phase[1] -= 1.f;
                voices[v].phase[0] += inc;
                if(voices[v].phase[0] >= 1.f) voices[v].phase[0] -= 1.f;
                float p = voices[v].phase[0] + psin(voices[v].phase[1]) * voices[v].fmi;
                if(p >= 1.f) p -= 1.f; else if(p < 0.f) p += 1.f;
                raw = psin(p) * 3.2f;
            } else if(voices[v].type == 20){
                // WAVETABLE — morph position = SCAN + envelope motion, 3 detuned osc
                float pos = (float)g_wtscan / 15.f * (float)(WT_N - 1);
                pos += voices[v].env * 1.7f;                 // built-in timbral movement
                if(pos > (float)(WT_N-1)) pos = (float)(WT_N-1); if(pos < 0.f) pos = 0.f;
                int t0 = (int)pos, t1 = t0 + 1; if(t1 >= WT_N) t1 = WT_N - 1;
                float tf = pos - (float)t0;
                raw = 0.f;
                for(int k=0;k<3;k++){
                    voices[v].phase[k] += inc * voices[v].ratio[k];
                    while(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                    float idx = voices[v].phase[k] * (float)WT_LEN;
                    int i0 = (int)idx & (WT_LEN-1); float f = idx - (float)(int)idx;
                    int i1 = (i0 + 1) & (WT_LEN-1);
                    float a = wtbl[t0][i0] + (wtbl[t0][i1]-wtbl[t0][i0])*f;
                    float b = wtbl[t1][i0] + (wtbl[t1][i1]-wtbl[t1][i0])*f;
                    raw += a + (b - a) * tf;
                }
                raw *= 1.5f;
            } else if(voices[v].type == 23){
                // WAVEFOLDER — drive a sine and reflect it back into [-1,1]; SCAN +
                // envelope set the fold depth so harmonics bloom as the note opens
                float drive = 0.7f + (float)g_wtscan * 0.45f + voices[v].env * 3.0f;
                raw = 0.f;
                for(int k=0;k<3;k++){
                    voices[v].phase[k] += inc * voices[v].ratio[k];
                    while(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                    float s = psin(voices[v].phase[k]) * drive;
                    for(int it=0; it<6; it++){           // triangle fold (reflect at ±1)
                        if(s > 1.f)       s = 2.f - s;
                        else if(s < -1.f) s = -2.f - s;
                        else break;
                    }
                    raw += s;
                }
                raw *= 1.2f;
            } else if(voices[v].type == 24){
                // PLUCK (Karplus-Strong): noise burst → damped averaging delay line.
                // SCAN sets the decay (string sustain).
                if(voices[v].kinit){
                    int L = (int)(1.f / voices[v].phaseInc);   // samples per cycle = pitch
                    if(L < 2) L = 2; if(L > 671) L = 671;
                    voices[v].klen = L; voices[v].kpos = 0;
                    for(int j=0;j<L;j++)
                        kpbuf[v][j] = (float)(lcg() & 0xffff) / 32768.f - 1.f;  // white noise
                    voices[v].kinit = false;
                }
                int L = voices[v].klen, p = voices[v].kpos, pn = p+1; if(pn >= L) pn = 0;
                float damp = 0.990f + (float)g_wtscan * 0.00060f;   // SCAN → sustain
                float outk = kpbuf[v][p];
                kpbuf[v][p] = (kpbuf[v][p] + kpbuf[v][pn]) * 0.5f * damp;
                voices[v].kpos = pn;
                raw = outk * 2.6f;
            } else if(voices[v].type == 26){
                // CZ phase distortion — resonant sweep without a filter. SCAN = reso.
                float reso = 1.f + (float)g_wtscan * 0.7f;
                raw = 0.f;
                for(int k=0;k<3;k++){
                    voices[v].phase[k] += inc * voices[v].ratio[k];
                    while(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                    float ph = voices[v].phase[k];
                    float win = 1.f - ph;                 // window fades over the cycle
                    float rp = ph * reso; rp -= (float)(int)rp;
                    raw += win * psin(rp) * 1.4f;
                }
            } else {
                int wt = voices[v].type;   // 0/1 = thief saw/tri (naive); 10..14 = classic (band-limited)
                raw = 0.f;
                int no = voices[v].nosc;   // 1 = pure mono fundamental, 5 = detuned stack
                for(int k=0;k<no;k++){
                    float dt = inc * voices[v].ratio[k];   // this osc's cycles/sample
                    if(dt < 0.f) dt = -dt; if(dt > 0.45f) dt = 0.45f;
                    voices[v].phase[k] += inc * voices[v].ratio[k];
                    while(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                    while(voices[v].phase[k] <  0.f) voices[v].phase[k] += 1.f;
                    float ph = voices[v].phase[k], os;
                    switch(wt){
                    case 10: {  // band-limited saw
                        os = ph + ph - 1.f - polyBlep(ph, dt);
                    } break;
                    case 11: {  // band-limited square
                        float p2 = ph + 0.5f; if(p2 >= 1.f) p2 -= 1.f;
                        os = (ph < 0.5f ? 1.f : -1.f) + polyBlep(ph, dt) - polyBlep(p2, dt);
                        os *= 0.85f;
                    } break;
                    case 14: {  // band-limited pulse (25% duty)
                        float w = 0.25f; float p2 = ph - w; if(p2 < 0.f) p2 += 1.f;
                        os = (ph < w ? 1.f : -1.f) + polyBlep(ph, dt) - polyBlep(p2, dt);
                        os *= 0.85f;
                    } break;
                    case 12: {  // band-limited triangle (PolyBLAMP on its two corners)
                        float p2 = ph + 0.5f; if(p2 >= 1.f) p2 -= 1.f;
                        os = 4.f*fabsf(ph-0.5f) - 1.f;
                        os += 8.f * dt * (polyBlamp(p2, dt) - polyBlamp(ph, dt));
                    } break;
                    case 13:         os = psin(ph);                      break; // sine (already clean)
                    case 1:          os = 4.f*fabsf(ph-0.5f) - 1.f;      break; // THIEF tri (naive)
                    default:         os = ph + ph - 1.f;                 break; // THIEF saw (naive)
                    }
                    raw += os;
                }
                if(no == 1) raw *= 2.2f;   // level-match the pure mono tone to the stack
            }
            // per-note VCF pluck — states HARD-CLAMPED so ringing cannot grow
            voices[v].fc += (voices[v].fcT - voices[v].fc) * voices[v].fcR;
            voices[v].lp += voices[v].fc * voices[v].bp;
            float vhp = raw - voices[v].lp - voices[v].q * voices[v].bp;
            voices[v].bp += voices[v].fc * vhp;
            if(voices[v].lp >  6.f) voices[v].lp =  6.f;
            if(voices[v].lp < -6.f) voices[v].lp = -6.f;
            if(voices[v].bp >  6.f) voices[v].bp =  6.f;
            if(voices[v].bp < -6.f) voices[v].bp = -6.f;
            float sig = voices[v].lp * (0.05f * voices[v].env) * breath * gvSyn;
            // hardware L/R is flipped vs pad layout — swap here so pan tracks pads
            mixL += sig * voices[v].pan;
            mixR += sig * (1.f - voices[v].pan);
        }
        ff_s += (filter_f - ff_s) * 0.0015f;
        // Chamberlin 2-pole resonant LPF — update lp first for correct phase
        lpL += ff_s * bpL;
        float hpL = mixL - lpL - Q1*bpL; bpL += ff_s * hpL;
        lpR += ff_s * bpR;
        float hpR = mixR - lpR - Q1*bpR; bpR += ff_s * hpR;
        // Saturation drive (DRV param) — thickens, adds harmonics above the LPF
        // (FLT FX swaps in the bandpass output = the DJ sweep voice)
        float dL = tanhf(lpL * drv);
        float dR = tanhf(lpR * drv);
        // Chorus: write dry, read a swept fractional tap (integer math on cw — no
        // float-precision drift at long uptimes)
        cbufL[cw & 1023] = dL;
        cbufR[cw & 1023] = dR;
        clfo += 0.4f / 48000.f; if(clfo >= 1.f) clfo -= 1.f;
        float tapL = 672.f + 180.f * psin(clfo);
        float clfo2 = clfo + 0.25f; if(clfo2 >= 1.f) clfo2 -= 1.f;
        float tapR = 672.f + 180.f * psin(clfo2);
        uint32_t tiL = (uint32_t)tapL; float frL = tapL - (float)tiL;
        uint32_t tiR = (uint32_t)tapR; float frR = tapR - (float)tiR;
        float wL0 = cbufL[(cw - tiL) & 1023], wL1 = cbufL[(cw - tiL - 1) & 1023];
        float wR0 = cbufR[(cw - tiR) & 1023], wR1 = cbufR[(cw - tiR - 1) & 1023];
        float wL = wL0 + (wL1 - wL0) * frL;
        float wR = wR0 + (wR1 - wR0) * frR;
        cw++;
        // Drums — rendered dry, straight into the output saturation
        float drL = 0.f, drR = 0.f;
        for(int v = 0; v < 4; v++){
            DRun& d = drun[v];
            if(dstamp[v] != dgo[v].stamp){
                dstamp[v] = dgo[v].stamp;
                d.env = 1.f; d.ph = 0.f;
                // latch + clamp everything once — no torn per-sample reads, no
                // runaway pitch if a trigger write raced this latch
                d.ft   = dgo[v].f;
                if(d.ft < 5.f) d.ft = 5.f;
                if(d.ft > 16000.f) d.ft = 16000.f;
                d.f    = d.ft * dgo[v].drop;       // start high, glide down = 808 thump
                if(d.f > 20000.f) d.f = 20000.f;
                d.dec  = dgo[v].dec;  d.noiz = dgo[v].noiz; d.amp = dgo[v].amp;
                if(d.dec > 0.99999f) d.dec = 0.99999f;
                d.panl = 1.f - dgo[v].pan; d.panr = dgo[v].pan;
                d.part = dgo[v].part;
            }
            if(d.env > 0.0008f){
                d.env *= d.dec;
                d.f   += (d.ft - d.f) * 0.0012f;
                d.ph  += d.f * vss * (1.f/48000.f); if(d.ph >= 1.f) d.ph -= 1.f;
                float tone = sinf(6.2831853f * d.ph);
                if(d.part > 0.f)   // 2nd partial: cowbell 1.565x, modular blip 2.717x
                    tone = 0.6f*tone + 0.45f*sinf(6.2831853f * d.ph * d.part);
                arng = arng*1664525u + 1013904223u;
                float nz = ((float)(arng >> 9) * (1.f/4194304.f)) - 1.f;
                if(d.noiz > 0.97f){ hatlp += (nz - hatlp) * 0.12f; nz -= hatlp; }   // hat HP
                float sd = (tone * (1.f - d.noiz) + nz * d.noiz) * d.env * d.amp;
                drL += sd * d.panl * gvDrum; drR += sd * d.panr * gvDrum;
            }
        }
        // Sampler: record the input while a slot is armed; sum playing slots
        // into the drum bus (so they ride the accel filter too)
        if(rsl >= 0){
            uint32_t rp2 = rec_pos;
            if(rp2 < SMPL_N){ smp[rsl][rp2] = in[0][i]; rec_pos = rp2 + 1u; }
        }
        for(int s2 = 0; s2 < 16; s2++){
            if(sp_st[s2] != smp_trig[s2]){
                sp_st[s2]  = smp_trig[s2];
                sp_pos[s2] = 0;
                sp_act[s2] = (smp_len[s2] > 0);
            }
            if(sp_act[s2]){
                float sv = smp[s2][sp_pos[s2]];
                drL += sv * 0.9f * gvSmp; drR += sv * 0.9f * gvSmp;
                sp_fr[s2] += vss;                       // varispeed sample advance
                while(sp_fr[s2] >= 1.f){ sp_fr[s2] -= 1.f; sp_pos[s2]++; }
                if(sp_pos[s2] >= smp_len[s2]) sp_act[s2] = false;
            }
        }
        // Drums ride the accel filter too — their own SVF on the same cutoff
        // (still skipping the chorus), boosted 1.5x to gain-match the synth
        dlpL += ff_s * dbpL;
        float dhpL = drL - dlpL - Q1*dbpL; dbpL += ff_s * dhpL;
        dlpR += ff_s * dbpR;
        float dhpR = drR - dlpR - Q1*dbpR; dbpR += ff_s * dhpR;

        // Final soft clip — master volume sets the push into the tanh
        float drmL = dlpL;
        float drmR = dlpR;
        float oL = tanhf((dL * 0.8f + wL * cwet + drmL * 1.5f) * mgain);
        float oR = tanhf((dR * 0.8f + wR * cwet + drmR * 1.5f) * mgain);

        // ── punch-in FX chain: reverse → chorus → pan → mute → crush ──
        if(!buffx) {
            // capture ring runs whenever reverse isn't holding it
            rbufL[rw] = oL; rbufR[rw] = oR;
            rw = (rw + 1u) % RBUF_N;
        }
        else {
            // REVERSE: clean backwards loop of the last beat at original
            // pitch; edges faded — no wrap clicks
            uint32_t le = step_ms * 192u;
            if(le > RBUF_N - 64u) le = RBUF_N - 64u;
            rev_pos += 1.f;                       // clean 1:1 reverse, no pitch
            while(rev_pos >= (float)le) rev_pos -= (float)le;
            uint32_t ri = (uint32_t)rev_pos;
            uint32_t rp = (fz_anchor + RBUF_N - 1u - ri) % RBUF_N;
            float g = 1.f;
            if(ri < 480u)            g = (float)ri / 480.f;
            else if(le - ri < 480u)  g = (float)(le - ri) / 480.f;
            oL = rbufL[rp] * g; oR = rbufR[rp] * g;
        }

        // CHOR: full-mix chorus with feedback — deep sweep, FSR = depth+regen
        if((fh >> FX_CHOR) & 1u) {
            xlfo += 0.35f / 48000.f; if(xlfo >= 1.f) xlfo -= 1.f;
            float depth = 0.45f + 0.55f * ffsr;   // deep even untouched
            float xlfo2 = xlfo + 0.25f; if(xlfo2 >= 1.f) xlfo2 -= 1.f;
            float tapL  = 960.f + 740.f * depth * psin(xlfo);
            float tapR  = 960.f + 740.f * depth * psin(xlfo2);
            uint32_t tiL = (uint32_t)tapL; float frL2 = tapL - (float)tiL;
            uint32_t tiR = (uint32_t)tapR; float frR2 = tapR - (float)tiR;
            float a0 = xcL[(xw - tiL) & 4095], a1 = xcL[(xw - tiL - 1u) & 4095];
            float b0 = xcR[(xw - tiR) & 4095], b1 = xcR[(xw - tiR - 1u) & 4095];
            float wxL = a0 + (a1 - a0) * frL2;
            float wxR = b0 + (b1 - b0) * frR2;
            float fb  = 0.55f + 0.42f * ffsr;     // high-feedback scream zone
            xcL[xw & 4095] = oL + wxL * fb;
            xcR[xw & 4095] = oR + wxR * fb;
            xw++;
            oL = oL * 0.5f + wxL * 0.9f;
            oR = oR * 0.5f + wxR * 0.9f;
        } else {
            xcL[xw & 4095] = oL; xcR[xw & 4095] = oR; xw++;   // keep line warm
        }

        // PAN: hard autopan, FSR = speed (0.5Hz → 8Hz)
        if((fh >> FX_PAN) & 1u) {
            panph += (0.5f + 7.5f * ffsr) / 48000.f; if(panph >= 1.f) panph -= 1.f;
            float px = panph * 2.f - 1.f;
            float ps = 4.f * px * (1.f - fabsf(px));           // -1..1 parabolic sine
            oL *= (0.15f + 0.85f * 0.5f * (1.f + ps)) * 1.4f;
            oR *= (0.15f + 0.85f * 0.5f * (1.f - ps)) * 1.4f;
        }

        // MUT: clean full drop, short ramp so it never clicks
        {
            static float mg = 1.f;
            float mt = ((fh >> FX_MUTE) & 1u) ? 0.f : 1.f;
            mg += (mt - mg) * 0.004f;
            oL *= mg; oR *= mg;
        }
        // (DST / FILTER / CRUSH moved to the ordered master chain below RING)

        // ══ FX2 bank — SP404/OP-1-flavored, all cheap DSP, lidar on a few ══
        if(fx_eff2){
            const uint16_t h2 = fx_eff2;
            const float s = ffsr, lid = fx_lid;
            // callback-local beat phase (samples since last grid step) for the
            // tempo-synced FX2 effects — tracks the master gstep
            static uint32_t f2_since = 0; static int f2_lastg = -1;
            if(f2_lastg != gstep){ f2_lastg = gstep; f2_since = 0; }
            // DUB — mono dub echo, FSR=feedback(runaway), lidar=time(80ms..1/4)
            static uint32_t dubw = 0; static float dubt = 9000.f;
            if(h2 & (1u<<F2_DUB)){
                float tt = 3840.f + lid * (float)(DUB_N - 5000u);   // ~80ms..680ms
                dubt += (tt - dubt) * 0.0004f;                       // smear on move
                uint32_t dl = (uint32_t)dubt; if(dl > DUB_N-2u) dl = DUB_N-2u;
                float e = f2dub[(dubw + DUB_N - dl) % DUB_N];
                float fb = 0.45f + s * 0.6f;                         // up to runaway
                float in = (oL + oR) * 0.5f;
                float wr = in + e * fb; wr = tanhf(wr * 0.8f) * 1.25f;
                f2dub[dubw] = wr; dubw = (dubw + 1u) % DUB_N;
                oL += e * 0.9f; oR += e * 0.9f;
            } else { f2dub[dubw] = (oL+oR)*0.5f; dubw = (dubw+1u)%DUB_N; }
            // GVERB — quick comb-bank reverb gated by FSR hold length
            static const uint16_t GVL[4]={1201,1291,1487,1693};
            static uint16_t gvw = 0; static float genv = 0.f;
            if(h2 & (1u<<F2_GVRB)){
                float in = (oL+oR)*0.5f, rv = 0.f;
                for(int c=0;c<4;c++){ float r = f2cmb[(gvw + CMB_N - GVL[c]) % CMB_N];
                    rv += r; }
                rv *= 0.25f;
                f2cmb[gvw] = in + rv * 0.72f; gvw = (gvw+1u)%CMB_N;
                genv = (rv*rv > genv) ? rv*rv : genv * (0.99988f - s*0.0003f); // gate hold←FSR
                float g = (genv > 0.00005f) ? 1.f : 0.f;
                oL = oL*0.4f + rv*g*1.6f; oR = oR*0.4f + rv*g*1.6f;
            }
            // TGATE — tempo trance gate (16 steps), FSR = depth + double-time
            if(h2 & (1u<<F2_TGATE)){
                static const uint16_t PAT = 0xB6D6; // x.xx x.x. xx.x x.x.
                int st = (s > 0.6f) ? ((gstep*2 + (int)(f2_since/(step_ms*24+1))) & 15)
                                    : (gstep & 15);
                float on = ((PAT >> st) & 1) ? 1.f : (0.6f - s*0.6f);
                oL *= on; oR *= on;
            }
            // FLANG — flanger, FSR biased to FAST/short (jet sweep)
            static float fbuf[1024] = {}; static uint16_t fw = 0; static float flf = 0.f;
            if(h2 & (1u<<F2_FLANG)){
                float ctr = 6.f - 5.4f*s;                       // 6ms..0.6ms (squeeze=short)
                float rate = 0.15f + 6.f * (s*s);               // exp bias to fast
                flf += rate / 48000.f; if(flf>=1.f) flf-=1.f;
                float lfo = 0.5f - 0.5f*cosf(flf*6.2831853f);
                float dms = (ctr + (0.5f+4.5f*s)*lfo) * 48.f;   // samples
                float in = (oL+oR)*0.5f;
                float rd = (float)fw - dms; while(rd<0) rd += 1024.f;
                int ri=(int)rd; float fr=rd-ri;
                float a=fbuf[ri&1023], b=fbuf[(ri+1)&1023];
                float wet=a+(b-a)*fr;
                fbuf[fw]=in + wet*0.7f; fw=(fw+1u)&1023;
                oL = oL*0.5f + wet; oR = oR*0.5f + wet;
            } else { fbuf[fw]=(oL+oR)*0.5f; fw=(fw+1u)&1023; }
            // RING — moved to the ordered master chain (ring → dist → filter → crush)
            // FOLD — wavefolder, FSR=fold gain (bold harmonics, cheap)
            if(h2 & (1u<<F2_FOLD)){
                float g = 1.f + s*6.f;
                oL = sinf(oL*g*1.5707f)*0.8f; oR = sinf(oR*g*1.5707f)*0.8f;
            }
            // COMB — tuned resonator, lidar=pitch(scale), FSR=feedback
            static uint16_t cw2 = 0;
            if(h2 & (1u<<F2_COMB)){
                static const float PENT[8]={130.8f,146.8f,164.8f,196.f,220.f,261.6f,293.7f,329.6f};
                float cf = PENT[(int)(lid*7.99f)];
                uint32_t dl = (uint32_t)(48000.f/cf); if(dl>=CMB_N) dl=CMB_N-1;
                float r = f2cmb[(cw2 + CMB_N - dl) % CMB_N];
                float fb = 0.8f + s*0.19f;
                float in = (oL+oR)*0.5f;
                f2cmb[cw2] = in + r*fb; cw2 = (cw2+1u)%CMB_N;
                oL = oL*0.5f + r*0.7f; oR = oR*0.5f + r*0.7f;
            }
            // VOWEL — 2-bandpass formant morph U→A→I via FSR
            static float v1=0,v2=0,v1b=0,v2b=0;
            if(h2 & (1u<<F2_FORM)){
                float f1 = 300.f + s*700.f;       // 300..1000
                float f2f= 900.f + s*1700.f;      // 900..2600
                float in=(oL+oR)*0.5f;
                float c1=2.f*sinf(3.14159f*f1/48000.f), c2=2.f*sinf(3.14159f*f2f/48000.f);
                v1 += c1*(in - v1 - 0.2f*v1b); v1b += c1*v1;   // resonant bp #1 (state-var)
                v2 += c2*(in - v2 - 0.2f*v2b); v2b += c2*v2;
                float wet = (v1b + v2b)*0.9f;
                oL = oL*0.25f + wet; oR = oR*0.25f + wet;
            }
            // ISO trio — 2 one-pole crossovers → low/mid/high, kill the band
            if(h2 & ((1u<<F2_ISOLO)|(1u<<F2_ISOMD)|(1u<<F2_ISOHI))){
                static float xl=0, xh=0, xlR=0, xhR=0;
                const float a1=0.026f, a2=0.30f;        // ~200Hz, ~3kHz
                xl += a1*(oL-xl); xh += a2*(oL-xh);
                float lo=xl, hi=oL-xh, mid=xh-xl;
                xlR+=a1*(oR-xlR); xhR+=a2*(oR-xhR);
                float loR=xlR, hiR=oR-xhR, midR=xhR-xlR;
                float gl=1,gm=1,gh=1;
                if(h2&(1u<<F2_ISOLO)) gl = (1.f-s);     // squeeze = full kill
                if(h2&(1u<<F2_ISOMD)) gm = (1.f-s);
                if(h2&(1u<<F2_ISOHI)) gh = (1.f-s);
                oL = lo*gl + mid*gm + hi*gh;
                oR = loR*gl + midR*gm + hiR*gh;
            }
            // PHONE — telephone bandpass + grit, FSR tightens/crushes
            static float ph_h=0, ph_l=0, ph_hR=0, ph_lR=0;
            if(h2 & (1u<<F2_PHONE)){
                float la=0.30f - s*0.12f, ha=0.04f + s*0.04f;
                ph_h += ha*(oL-ph_h); float b=oL-ph_h; ph_l += la*(b-ph_l);
                ph_hR+= ha*(oR-ph_hR);float bR=oR-ph_hR;ph_lR+=la*(bR-ph_lR);
                float q=8.f - s*5.f;
                oL = (floorf(ph_l*q)/q)*1.4f; oR=(floorf(ph_lR*q)/q)*1.4f;
            }
            // GRAIN — granular freeze from the capture ring; FSR=size, lidar=scrub
            static uint32_t gpos=0; static uint32_t glen=4800;
            if(h2 & (1u<<F2_GRAIN)){
                glen = (uint32_t)(5760.f - s*5400.f); if(glen<240) glen=240;
                uint32_t base = (uint32_t)(lid * (RBUF_N-glen-2));
                gpos++; if(gpos>=glen) gpos=0;
                uint32_t rp = (base + gpos) % RBUF_N;
                float w = 0.5f-0.5f*cosf(6.2831853f*(float)gpos/(float)glen);
                oL = rbufL[rp]*w + oL*(1.f-w)*0.3f;
                oR = rbufR[rp]*w + oR*(1.f-w)*0.3f;
            }
            f2_since++;
            // WOW — vinyl wow/flutter + crackle, FSR=degradation
            static float wphs=0, wphf=0, wd=0;
            if(h2 & (1u<<F2_WOW)){
                wphs += 0.7f/48000.f; if(wphs>=1.f)wphs-=1.f;
                wphf += 8.f/48000.f;  if(wphf>=1.f)wphf-=1.f;
                float mod=(psin(wphs)*0.6f+psin(wphf)*0.4f)*s*0.4f;
                wd += (mod - wd)*0.5f;
                // tiny crackle
                arng=arng*1664525u+1013904223u;
                float crk = ((arng>>9)*(1.f/4194304.f)-1.f);
                crk = (fabsf(crk)>0.985f) ? crk*s*0.5f : 0.f;
                oL = oL*(1.f+wd)+crk; oR = oR*(1.f+wd)+crk;
            }
            // DOWN — sample-rate reducer (aliasing), FSR=destroy
            static float dhL=0,dhR=0; static uint32_t dhc=0;
            if(h2 & (1u<<F2_DOWN)){
                uint32_t hold = 2u + (uint32_t)(s*40.f);
                if(dhc==0){ dhL=oL; dhR=oR; }
                dhc=(dhc+1u)%hold; oL=dhL; oR=dhR;
            } else dhc=0;
            // SCAT — beat-repeat scatter from the ring (16 slices, FSR=chaos)
            if(h2 & (1u<<F2_SCAT)){
                uint32_t slice = (uint32_t)(step_ms*48.f);   // 16th in samples
                if(slice<480) slice=480; if(slice>RBUF_N/2) slice=RBUF_N/2;
                static uint32_t scbase=0; static int scg=-1;
                if(scg!=gstep){ scg=gstep;
                    if((lcg()%100) < (uint32_t)(20+s*70)) scbase=(lcg()%8)*slice; }
                uint32_t rp = (step_rw + RBUF_N - (scbase + (f2_since % slice))) % RBUF_N;
                float w = (s>0.05f)?1.f:0.f;
                oL = rbufL[rp]*w + oL*(1.f-w);
                oR = rbufR[rp]*w + oR*(1.f-w);
            }
        }

        // ══ FX3 bank — MUSICAL / SPATIAL ══
        if(fx_eff3){
            const uint16_t h3 = fx_eff3;
            const float s = ffsr, lid = fx_lid;
            static uint32_t f3_since = 0; static int f3_lastg = -1;
            if(f3_lastg != gstep){ f3_lastg = gstep; f3_since = 0; }
            static const uint16_t PLC[4]={1789,2143,2647,3119};
            static uint16_t plw=0; static uint16_t paw=0; static float plapbuf[512]={};
            if(h3 & (1u<<E3_PLATE)){
                float in=(oL+oR)*0.5f, rv=0.f; float dec=0.74f + s*0.21f;
                for(int c=0;c<4;c++) rv += f3plt[(plw+F3PLT_N-PLC[c])%F3PLT_N];
                rv*=0.25f; f3plt[plw]=in + rv*dec; plw=(plw+1u)%F3PLT_N;
                float ar=plapbuf[(paw+512-241)&511];
                float aw=rv + ar*0.5f; plapbuf[paw]=aw; paw=(paw+1u)&511;
                float wet=ar - aw*0.5f; oL=oL*0.5f + wet; oR=oR*0.5f + wet;
            }
            static uint16_t shw=0; static float shfb=0.f; static int shsgn=1;
            if(h3 & (1u<<E3_SHIM)){
                float in=(oL+oR)*0.5f;
                float t=f3plt[(shw+F3PLT_N-4799)%F3PLT_N];
                shsgn=-shsgn; float oct=fabsf(t)*shsgn;
                shfb = in + (t*0.6f + oct*(0.4f+s*0.45f));
                f3plt[shw]=tanhf(shfb*0.8f); shw=(shw+1u)%F3PLT_N;
                oL=oL*0.5f + t*0.8f; oR=oR*0.5f + t*0.8f;
            }
            static uint32_t pngw=0;
            if(h3 & (1u<<E3_PONG)){
                uint32_t dl=(uint32_t)(step_ms*48.f*1.5f);
                if(dl>=F3PNG_N) dl=F3PNG_N-1; if(dl<480) dl=480;
                float eL=f3pngR[(pngw+F3PNG_N-dl)%F3PNG_N];
                float eR=f3pngL[(pngw+F3PNG_N-dl)%F3PNG_N];
                float fb=0.35f + lid*0.6f;
                f3pngL[pngw]=tanhf(oL + eL*fb); f3pngR[pngw]=tanhf(oR + eR*fb);
                pngw=(pngw+1u)%F3PNG_N;
                float m=0.3f+s*0.6f; oL=oL*(1.f-m*0.5f)+eL*m; oR=oR*(1.f-m*0.5f)+eR*m;
            } else { f3pngL[pngw]=oL; f3pngR[pngw]=oR; pngw=(pngw+1u)%F3PNG_N; }
            static uint32_t slw=0;
            if(h3 & (1u<<E3_SLAP)){
                uint32_t dl=2880u+(uint32_t)(s*9000.f); if(dl>=F3SLP_N) dl=F3SLP_N-1;
                float e=f3slap[(slw+F3SLP_N-dl)%F3SLP_N]; float in=(oL+oR)*0.5f;
                f3slap[slw]=in + e*0.25f; slw=(slw+1u)%F3SLP_N;
                oL=oL+e*0.55f; oR=oR+e*0.55f;
            } else { f3slap[slw]=(oL+oR)*0.5f; slw=(slw+1u)%F3SLP_N; }
            static float phl[4]={},phr[4]={},php=0.f;
            if(h3 & (1u<<E3_PHAS)){
                float rate=0.08f + s*1.6f; php+=rate/48000.f; if(php>=1.f)php-=1.f;
                float g=0.15f + 0.7f*(0.5f-0.5f*cosf(php*6.2831853f));
                float xL=oL, xR=oR;
                for(int k=0;k<4;k++){ float yL=-g*xL+phl[k]; phl[k]=xL+g*yL; xL=yL;
                                      float yR=-g*xR+phr[k]; phr[k]=xR+g*yR; xR=yR; }
                oL=oL*0.5f+xL*0.5f; oR=oR*0.5f+xR*0.5f;
            }
            static float waf=0,wab=0,wafR=0,wabR=0,wenv=0.f;
            if(h3 & (1u<<E3_WAH)){
                float in=(oL+oR)*0.5f, a=fabsf(in);
                wenv += (a-wenv)*((a>wenv)?0.05f:0.002f);
                float fc=300.f + (wenv*(2.f+s*6.f))*3000.f; if(fc>5500.f)fc=5500.f;
                float f=2.f*sinf(3.14159f*fc/48000.f), q=0.30f;
                waf+=f*(in - waf - q*wab); wab+=f*waf;
                wafR+=f*(oR - wafR - q*wabR); wabR+=f*wafR;
                oL=oL*0.3f + wab*1.4f; oR=oR*0.3f + wabR*1.4f;
            }
            static float trp=0.f;
            if(h3 & (1u<<E3_TREM)){
                float div=(s>0.6f)?8.f:4.f;
                float rate=div/(step_ms*0.001f*16.f);
                trp+=rate/48000.f; if(trp>=1.f)trp-=1.f;
                float depth=0.4f+s*0.6f;
                float g=1.f - depth*(0.5f-0.5f*cosf(trp*6.2831853f));
                oL*=g; oR*=g;
            }
            static float hbuf[256]={}; static uint16_t hbw=0;
            if(h3 & (1u<<E3_HAAS)){
                hbuf[hbw]=oR; uint16_t d=(uint16_t)(8.f+s*22.f);
                float dr=hbuf[(hbw+256-d)&255]; hbw=(hbw+1u)&255;
                float m=(oL+oR)*0.5f;
                oL=m + (oL-m)*(1.f+s*0.6f); oR=m + (dr-m)*(1.f+s*0.6f);
            }
            static float vbuf[1024]={}; static uint16_t vbw=0; static float vbp=0.f;
            if(h3 & (1u<<E3_VIBR)){
                vbuf[vbw]=(oL+oR)*0.5f; vbp+=5.5f/48000.f; if(vbp>=1.f)vbp-=1.f;
                float dms=(12.f + (4.f+s*22.f)*(0.5f-0.5f*cosf(vbp*6.2831853f)));
                float rd=(float)vbw-dms; while(rd<0)rd+=1024.f;
                int ri=(int)rd; float fr=rd-ri;
                float a=vbuf[ri&1023], b=vbuf[(ri+1)&1023];
                vbw=(vbw+1u)&1023; float w=a+(b-a)*fr; oL=w; oR=w;
            } else { vbuf[vbw]=(oL+oR)*0.5f; vbw=(vbw+1u)&1023; }
            static float cbuf[1024]={}; static uint16_t cbw=0; static float cbp=0.f;
            if(h3 & (1u<<E3_DIM)){
                cbuf[cbw]=(oL+oR)*0.5f; cbp+=0.6f/48000.f; if(cbp>=1.f)cbp-=1.f;
                float l1=0.5f-0.5f*cosf(cbp*6.2831853f);
                float l2=0.5f-0.5f*cosf((cbp+0.25f)*6.2831853f);
                float d1=(14.f+(2.f+s*14.f)*l1), d2=(14.f+(2.f+s*14.f)*l2);
                float r1=(float)cbw-d1; while(r1<0)r1+=1024.f; int i1=(int)r1; float f1=r1-i1;
                float r2=(float)cbw-d2; while(r2<0)r2+=1024.f; int i2=(int)r2; float f2c=r2-i2;
                float w1=cbuf[i1&1023]+(cbuf[(i1+1)&1023]-cbuf[i1&1023])*f1;
                float w2=cbuf[i2&1023]+(cbuf[(i2+1)&1023]-cbuf[i2&1023])*f2c;
                cbw=(cbw+1u)&1023; oL=oL*0.6f + w1*0.7f; oR=oR*0.6f + w2*0.7f;
            } else { cbuf[cbw]=(oL+oR)*0.5f; cbw=(cbw+1u)&1023; }
            static float spa[3]={},spaR[3]={}; static uint16_t spw=0;
            if(h3 & (1u<<E3_SPRG)){
                float in=(oL+oR)*0.5f; float t=f3slap[(spw+F3SLP_N-2400)%F3SLP_N];
                float g=0.6f; float xL=in+t*(0.5f+s*0.4f), xR=xL;
                for(int k=0;k<3;k++){ float yL=-g*xL+spa[k]; spa[k]=xL+g*yL; xL=yL;
                                      float yR=-g*xR+spaR[k];spaR[k]=xR+g*yR; xR=yR; }
                f3slap[spw]=tanhf(xL*0.9f); spw=(spw+1u)%F3SLP_N;
                oL=oL*0.5f+xL*0.9f; oR=oR*0.5f+xR*0.9f;
            }
            static float sirp=0.f, sirm=0.f;
            if(h3 & (1u<<E3_SIREN)){
                sirm+=6.f/48000.f; if(sirm>=1.f)sirm-=1.f;
                float f=180.f + lid*900.f + psin(sirm)*60.f*s;
                sirp+=f/48000.f; if(sirp>=1.f)sirp-=1.f;
                float t=tanhf(psin(sirp)*2.2f); float lvl=0.25f+s*0.5f;
                oL=oL*0.7f + t*lvl; oR=oR*0.7f + t*lvl;
            }
            static float wlpL=0,wlpR=0;
            if(h3 & (1u<<E3_WARM)){
                float dr=1.f+s*4.f;
                float yL=tanhf(oL*dr)*(1.f/(1.f+s*1.5f));
                float yR=tanhf(oR*dr)*(1.f/(1.f+s*1.5f));
                wlpL+=0.45f*(yL-wlpL); wlpR+=0.45f*(yR-wlpR);
                oL=yL*0.5f+wlpL*0.5f; oR=yR*0.5f+wlpR*0.5f;
            }
            static float subLP=0.f, subph=0.f, subenv=0.f;
            if(h3 & (1u<<E3_SUB)){
                float in=(oL+oR)*0.5f; subLP+=0.05f*(in-subLP);
                float a=fabsf(subLP); subenv+=(a-subenv)*0.01f;
                subph+=0.0006f; if(subph>=1.f)subph-=1.f;
                float sub=psin(subph)*subenv*(2.f+s*3.f); oL+=sub; oR+=sub;
            }
            static float airL=0,airR=0;
            if(h3 & (1u<<E3_AIR)){
                airL+=0.6f*(oL-airL); float hL=oL-airL;
                airR+=0.6f*(oR-airR); float hR=oR-airR;
                oL+=tanhf(hL*3.f)*0.5f*(0.3f+s*0.9f);
                oR+=tanhf(hR*3.f)*0.5f*(0.3f+s*0.9f);
            }
            static float pumpenv=1.f;
            if(h3 & (1u<<E3_PUMP)){
                uint32_t beat=(uint32_t)(step_ms*48.f*4.f); if(beat<480)beat=480;
                uint32_t pos=(f3_since + (uint32_t)gstep*(uint32_t)(step_ms*48.f))%beat;
                float t=(float)pos/(float)beat;
                float duck=1.f - (0.3f+s*0.65f)*expf(-t*6.f);
                pumpenv+=(duck-pumpenv)*0.05f; oL*=pumpenv; oR*=pumpenv;
            }
            f3_since++;
        }

        // ══ FX4 bank — EXPERIMENTAL / GLITCH / PITCH ══
        if(fx_eff4){
            const uint16_t h4 = fx_eff4;
            const float s = ffsr;
            static uint32_t f4_since = 0; static int f4_lastg = -1;
            if(f4_lastg != gstep){ f4_lastg = gstep; f4_since = 0; }
            static uint32_t rng4 = 0xC0FFEEu;
            static float ouL=0,ouR=0;
            if(h4 & (1u<<E4_OCTU)){
                float rL=fabsf(oL)*2.f-0.5f, rR=fabsf(oR)*2.f-0.5f;
                ouL+=0.002f*(rL-ouL); ouR+=0.002f*(rR-ouR);
                oL=oL*(1.f-s)+(rL-ouL)*s*1.2f; oR=oR*(1.f-s)+(rR-ouR)*s*1.2f;
            }
            static int odpL=0,odpR=0,odfL=1,odfR=1;
            if(h4 & (1u<<E4_OCTD)){
                int sL=(oL>0.f); if(sL!=odpL){ odpL=sL; if(sL)odfL=-odfL; }
                int sR=(oR>0.f); if(sR!=odpR){ odpR=sR; if(sR)odfR=-odfR; }
                oL=oL*(1.f-s)+fabsf(oL)*odfL*s*1.3f; oR=oR*(1.f-s)+fabsf(oR)*odfR*s*1.3f;
            }
            static float dzL=0,dzR=0; static uint32_t dzc=0;
            if(h4 & (1u<<E4_DSTR)){
                uint32_t hold=1u+(uint32_t)(s*30.f);
                if(dzc==0){ dzL=oL; dzR=oR; } dzc=(dzc+1u)%hold;
                float q=powf(2.f, 8.f - s*7.f); oL=floorf(dzL*q)/q; oR=floorf(dzR*q)/q;
            } else dzc=0;
            static uint32_t rpw=0;
            if(h4 & (1u<<E4_REPT)){
                uint32_t slc=(uint32_t)(step_ms*48.f*0.5f);
                if(slc<240)slc=240; if(slc>F4REP_N/2)slc=F4REP_N/2;
                static uint32_t rpbase=0; static int rpg=-1;
                if(rpg!=gstep){ rpg=gstep; rng4=rng4*1664525u+1013904223u;
                    if((rng4>>24)<(uint32_t)(40+s*200)) rpbase=((rng4>>8)%4u)*slc; }
                uint32_t rd=(rpw+F4REP_N-(rpbase+(f4_since%slc)))%F4REP_N;
                oL=f4rep[rd]; oR=f4rep[rd];
            }
            f4rep[rpw]=(oL+oR)*0.5f; rpw=(rpw+1u)%F4REP_N;
            static uint32_t rgp=0; static uint32_t rglen=4800;
            if(h4 & (1u<<E4_RGRN)){
                rglen=(uint32_t)(6000.f-s*5500.f); if(rglen<300)rglen=300;
                rgp++; if(rgp>=rglen)rgp=0;
                uint32_t rd=(rpw+F4REP_N-rgp)%F4REP_N;
                float w=0.5f-0.5f*cosf(6.2831853f*(float)rgp/(float)rglen);
                oL=f4rep[rd]*w + oL*(1.f-w)*0.3f; oR=f4rep[rd]*w + oR*(1.f-w)*0.3f;
            }
            static uint32_t fzp=0; static uint32_t fzlen=2400; static bool fzcap=false;
            if(h4 & (1u<<E4_FRZ)){
                if(!fzcap){ fzcap=true; fzp=0; }
                fzlen=600u+(uint32_t)((1.f-s)*4800.f);
                uint32_t rd=(rpw+F4REP_N-fzlen+(fzp%fzlen))%F4REP_N; fzp++;
                float w=0.5f-0.5f*cosf(6.2831853f*(float)(fzp%fzlen)/(float)fzlen);
                oL=f4rep[rd]*(0.6f+w*0.4f); oR=f4rep[rd]*(0.6f+w*0.4f);
            } else fzcap=false;
            static const uint16_t MTC[4]={149,179,211,251}; static uint16_t mtw=0;
            if(h4 & (1u<<E4_METL)){
                float in=(oL+oR)*0.5f, rv=0.f; float scale=0.6f+s*0.8f;
                for(int c=0;c<4;c++){ uint16_t d=(uint16_t)(MTC[c]*scale);
                    if(d>=F4MTL_N)d=F4MTL_N-1; rv+=f4mtl[(mtw+F4MTL_N-d)%F4MTL_N]; }
                rv*=0.25f; f4mtl[mtw]=in + rv*0.85f; mtw=(mtw+1u)%F4MTL_N;
                oL=oL*0.3f + rv*1.3f; oR=oR*0.3f + rv*1.3f;
            }
            static float qa=0,qb=0,qaR=0,qbR=0;
            if(h4 & (1u<<E4_FORM)){
                float sh=1.f+s*1.6f; float f1=500.f*sh, f2f=1500.f*sh;
                if(f2f>6000.f)f2f=6000.f; float in=(oL+oR)*0.5f;
                float c1=2.f*sinf(3.14159f*f1/48000.f), c2=2.f*sinf(3.14159f*f2f/48000.f);
                qa+=c1*(in-qa-0.18f*qb); qb+=c1*qa;
                qaR+=c2*(in-qaR-0.18f*qbR); qbR+=c2*qaR;
                float wet=(qb+qbR)*0.9f; oL=oL*0.3f+wet; oR=oR*0.3f+wet;
            }
            static float fsp=0.f;
            if(h4 & (1u<<E4_FSHF)){
                float fc=20.f + s*s*900.f; fsp+=fc/48000.f; if(fsp>=1.f)fsp-=1.f;
                float c=psin(fsp), cq=psin(fsp+0.25f);
                float wL=oL*c - oR*cq, wR=oR*c + oL*cq;
                oL=oL*0.3f + wL*0.9f; oR=oR*0.3f + wR*0.9f;
            }
            if(h4 & (1u<<E4_FUZZ)){
                float g=2.f+s*18.f; float yL=oL*g+0.15f, yR=oR*g+0.15f;
                yL=(yL>0.f)?(1.f-expf(-yL)):(-1.f+expf(yL));
                yR=(yR>0.f)?(1.f-expf(-yR)):(-1.f+expf(yR));
                oL=yL*0.6f; oR=yR*0.6f;
            }
            static uint16_t gpat=0xACE1u; static int gseqg=-1;
            if(h4 & (1u<<E4_GSEQ)){
                if(gseqg!=gstep && gstep==0){ rng4=rng4*1664525u+1013904223u; gpat=(uint16_t)(rng4>>8); }
                gseqg=gstep; int thr=(int)(s*16.f);
                float on=(((gpat>>(gstep&15))&1) || ((gstep&3)<thr)) ? 1.f : 0.f;
                oL*=on; oR*=on;
            }
            static uint32_t srbase=0; static int srg=-1; static float srrd=0.f;
            if(h4 & (1u<<E4_SRMP)){
                uint32_t slc=(uint32_t)(step_ms*48.f*0.5f); if(slc<480)slc=480;
                if(srg!=gstep){ srg=gstep; srbase=(rpw+F4REP_N-slc)%F4REP_N; srrd=0.f; }
                float spd=1.f + s*1.5f*(f4_since/(float)(step_ms*48.f+1.f));
                srrd+=spd; if(srrd>=(float)slc)srrd-=(float)slc;
                uint32_t rd=(srbase+(uint32_t)srrd)%F4REP_N; oL=f4rep[rd]; oR=f4rep[rd];
            }
            static float vsrd=0.f; static bool vsinit=false;
            if(h4 & (1u<<E4_VSTP)){
                if(!vsinit){ vsinit=true; vsrd=0.f; }
                float spd=0.4f + s*2.5f; vsrd+=spd; float back=vsrd;
                if(back>F4REP_N-2)back=F4REP_N-2;
                uint32_t rd=(rpw+F4REP_N-(uint32_t)back)%F4REP_N; float damp=1.f - s*0.5f;
                oL=f4rep[rd]*damp; oR=f4rep[rd]*damp;
            } else vsinit=false;
            static float rdh=0,rdl=0,rdhR=0,rdlR=0,rdcar=0.f;
            if(h4 & (1u<<E4_RADIO)){
                float la=0.35f, ha=0.05f;
                rdh+=ha*(oL-rdh); float bL=oL-rdh; rdl+=la*(bL-rdl);
                rdhR+=ha*(oR-rdhR);float bR=oR-rdhR;rdlR+=la*(bR-rdlR);
                rdcar+=(60.f + s*200.f)/48000.f; if(rdcar>=1.f)rdcar-=1.f;
                float am=0.6f+0.4f*psin(rdcar);
                rng4=rng4*1664525u+1013904223u;
                float hiss=((rng4>>9)*(1.f/4194304.f)-1.f)*s*0.15f;
                oL=rdl*am*1.6f + hiss; oR=rdlR*am*1.6f + hiss;
            }
            static float ngenv=0.f;
            if(h4 & (1u<<E4_NGAT)){
                float a=fabsf(oL)+fabsf(oR);
                ngenv+=(a-ngenv)*((a>ngenv)?0.4f:0.004f);
                float g=(ngenv>(0.02f + s*0.5f))?1.f:0.f; oL*=g; oR*=g;
            }
            static float shval=0.f, shlp=0.f; static uint32_t shc=0;
            if(h4 & (1u<<E4_SHMD)){
                uint32_t rate=2400u-(uint32_t)(s*2200.f); if(rate<60)rate=60;
                if(shc==0){ rng4=rng4*1664525u+1013904223u; shval=(rng4>>9)*(1.f/4194304.f)-1.f; }
                shc=(shc+1u)%rate; shlp+=0.02f*(shval-shlp);
                float mod=0.5f+0.5f*shlp; oL*=mod; oR*=mod;
            } else shc=0;
            f4_since++;
        }

        // ══ FX5 — POCKET OPERATOR punch-ins ══
        if(fx_eff5){
            const uint16_t h5 = fx_eff5; const float s = ffsr, lid = fx_lid;
            static uint32_t f5_since=0; static int f5_lastg=-1;
            if(f5_lastg!=gstep){ f5_lastg=gstep; f5_since=0; }
            static uint32_t rng5 = 0x1234abcdu;
            if(h5 & (1u<<E5_REPEAT2)){            // 1/2-step beat-slice repeat
                static int wp=0; float* buf=f5rpt2;
                int slc=(int)(step_ms*48*0.5f*(0.4f+0.6f*s)); if(slc<64)slc=64; if(slc>5999)slc=5999;
                buf[wp]=oL; int rp=wp-(int)(f5_since%slc); if(rp<0)rp+=6000;
                oL=oR=buf[rp]; if(++wp>=6000)wp=0;
            }
            if(h5 & (1u<<E5_REPEAT6)){            // ~3/4-step beat repeat
                static int wp=0; uint32_t len=(uint32_t)(step_ms*48*(0.5f+1.0f*s));
                if(len<128)len=128; if(len>23999)len=23999;
                f5repL[wp]=oL; f5repR[wp]=oR; uint32_t ph=f5_since%len; int rp=(int)wp-(int)ph; if(rp<0)rp+=24000;
                oL=f5repL[rp]; oR=f5repR[rp]; if((uint32_t)(++wp)>=24000)wp=0;
            }
            if(h5 & (1u<<E5_NOTESHUF)){           // offbeat swing gate
                uint32_t sw=(uint32_t)(step_ms*48u*0.33f*s);
                float g=(f5_since<sw && (gstep&1))?0.f:1.f;
                static float env=1.f; env+=(g-env)*0.01f; oL*=env; oR*=env;
            }
            if(h5 & (1u<<E5_QUANT68)){            // triplet tremolo-gate
                static float ph=0.f; float rate=3.f/(float)(step_ms*0.001f*2.f);
                ph+=rate/48000.f; if(ph>=1.f)ph-=1.f;
                float g=0.5f+0.5f*psin(ph); g=1.f-s*(1.f-g); oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_PUNCH)){              // transient pump
                static float env=0.f; float in=fabsf(oL)+fabsf(oR); env+=(in-env)*((in>env)?0.5f:0.002f);
                float g=1.f+s*2.0f*(1.f/(1.f+8.f*env)); oL=tanhf(oL*g); oR=tanhf(oR*g);
            }
            if(h5 & (1u<<E5_FILLIN)){             // accelerating retrigger burst
                static int wp=0; float* buf=f5fill;
                int slc=(int)(step_ms*48*0.25f*(1.0f-0.7f*s)); if(slc<48)slc=48; if(slc>3999)slc=3999;
                buf[wp]=oL; int rp=wp-(int)(f5_since%slc); if(rp<0)rp+=4000; oL=oR=buf[rp]; if(++wp>=4000)wp=0;
            }
            if(h5 & (1u<<E5_BUILDUP)){            // riser, lidar = position
                static float hp=0.f; float fc=0.02f+0.45f*lid; hp+=(oL-hp)*fc; float h=oL-hp;
                oL=oR=h; static float ph=0.f; ph+=(4.f+lid*40.f)/48000.f; if(ph>=1.f)ph-=1.f;
                float g=(ph<(0.5f+0.5f*lid))?1.f:0.2f; oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_BOUNCE)){             // accelerating decaying retrigger
                static int wp=0; static float t=0.f; static float per=4000.f; float* buf=f5bnc;
                buf[wp]=oL; t+=1.f; if(t>=per){t=0.f; per*=0.7f; if(per<400.f)per=4000.f*(0.5f+0.5f*s);}
                int rp=wp-(int)t; if(rp<0)rp+=8000; float dec=1.f-(t/per)*0.8f; oL=oR=buf[rp]*dec; if(++wp>=8000)wp=0;
            }
            if(h5 & (1u<<E5_LPFIX)){              // fixed lowpass kill
                static float z1=0,z2=0; float fc=0.03f+0.5f*(1.f-s); z1+=fc*(oL-z1); z2+=fc*(z1-z2); oL=z2; oR=z2;
            }
            if(h5 & (1u<<E5_HPFIX)){              // fixed highpass kill
                static float z=0; float fc=0.02f+0.4f*s; z+=fc*(oL-z); oL=oR=oL-z;
            }
            if(h5 & (1u<<E5_SQUASH)){             // limiter/clip
                float g=1.f+s*10.f; float k=0.7f/(0.3f+s); oL=tanhf(oL*g)*k; oR=tanhf(oR*g)*k;
            }
            if(h5 & (1u<<E5_SCRATCH)){            // vinyl jog
                static int wp=0; static float rp=0.f; static float ph=0.f;
                f5scrL[wp]=oL; f5scrR[wp]=oR; ph+=(2.f+s*4.f)/480.f; if(ph>=1.f)ph-=1.f;
                rp+=psin(ph)*60.f*(0.5f+s); float rr=wp-rp; while(rr<0)rr+=48000; while(rr>=48000)rr-=48000;
                int ri=(int)rr; oL=f5scrL[ri]; oR=f5scrR[ri]; if(++wp>=48000)wp=0;
            }
            if(h5 & (1u<<E5_SCRTFAST)){           // faster jog
                static float rp=0.f; static float ph=0.f; static int wp2=0;
                f5scrL[wp2]=oL; ph+=(8.f+s*16.f)/480.f; if(ph>=1.f)ph-=1.f;
                rp+=psin(ph)*40.f; float rr=wp2-rp; while(rr<0)rr+=48000; while(rr>=48000)rr-=48000;
                oL=oR=f5scrL[(int)rr]; if(++wp2>=48000)wp2=0;
            }
            if(h5 & (1u<<E5_GLITCH)){             // random slice repeat
                static int wp=0; static int hold=0; static int len=2000;
                f5scrR[wp]=oL;
                if(--hold<=0){ rng5=rng5*1664525u+1013904223u;
                    if(((rng5>>20)&255)<(int)(s*255)){ len=300+(int)((rng5>>8)&8191); hold=len*2; } else hold=200; }
                if(hold>0 && len){ int rp=wp-(int)(f5_since%len); if(rp<0)rp+=48000; oL=oR=f5scrR[rp]; }
                if(++wp>=48000)wp=0;
            }
            if(h5 & (1u<<E5_BLINDS)){             // hard shutter tremolo
                static float ph=0.f; ph+=(8.f+s*60.f)/48000.f; if(ph>=1.f)ph-=1.f;
                float g=(ph<0.5f)?1.f:0.f; oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_SAMPHLD)){            // random-rate sample & hold
                static float hL=0,hR=0; static int c=0; static int per=200;
                if(--c<=0){ rng5=rng5*1664525u+1013904223u; per=20+(int)((1.f-s)*1500); c=per; hL=oL; hR=oR; }
                oL=hL; oR=hR;
            }
            f5_since++;
        }

        // ══ FX6 — DELAY / SPACE / PITCH ══
        if(fx_eff6){
            const uint16_t h6 = fx_eff6; const float s = ffsr, lid = fx_lid;
            if(h6 & (1u<<E6_DELAY)){             // tempo 1/8 delay, S=feedback
                static int wp=0; int dt=(int)(step_ms*48*2); if(dt<1)dt=1; if(dt>47999)dt=47999;
                int rp=wp-dt; if(rp<0)rp+=48000; float dl=f6dL[rp], dr=f6dR[rp]; float fb=0.2f+0.55f*s;
                f6dL[wp]=tanhf(oL+dl*fb); f6dR[wp]=tanhf(oR+dr*fb); oL+=dl*0.6f; oR+=dr*0.6f; if(++wp>=48000)wp=0;
            }
            if(h6 & (1u<<E6_PINGPONG)){          // cross-fb stereo delay
                static int wp=0; int dt=(int)(step_ms*48*2); if(dt<1)dt=1; if(dt>47999)dt=47999;
                int rp=wp-dt; if(rp<0)rp+=48000; float dl=f6ppL[rp], dr=f6ppR[rp]; float fb=0.3f+0.55f*s;
                f6ppL[wp]=tanhf(oL+dr*fb); f6ppR[wp]=tanhf(oR+dl*fb); oL+=dl*0.6f; oR+=dr*0.6f; if(++wp>=48000)wp=0;
            }
            if(h6 & (1u<<E6_ECHOFADE)){          // feedback ramps up while held
                static int wp=0; static float fb=0.f; fb+=(0.85f-fb)*0.0008f;
                int dt=(int)(step_ms*48*(1.f+s*2.f)); if(dt<1)dt=1; if(dt>23999)dt=23999;
                int rp=wp-dt; if(rp<0)rp+=24000; float d=f6echo[rp];
                f6echo[wp]=tanhf(oL+d*fb); oL+=d*0.6f; oR+=d*0.6f; if(++wp>=24000)wp=0;
            }
            if(h6 & (1u<<E6_FEEDBACK)){          // resonant short-loop howl
                static int wp=0; float* buf=f6fb; int dt=200+(int)((1.f-s)*1400);
                int rp=wp-dt; if(rp<0)rp+=4096; float d=buf[rp&4095]; float fb=0.85f+0.13f*s;
                buf[wp]=tanhf(oL*0.5f+d*fb); oL=oR=oL*0.3f+d*0.8f; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_DRONE)){             // capture & loop a bed, lid=mix
                static int wp=0; static int frozen=0; static int len=12000;
                if(!frozen){ f6loopL[wp]=oL; f6loopR[wp]=oR; if(++wp>=len){frozen=1;wp=0;} }
                else { float gL=f6loopL[wp],gR=f6loopR[wp]; if(++wp>=len)wp=0;
                       oL=oL*(1.f-lid)+gL*lid; oR=oR*(1.f-lid)+gR*lid; }
            }
            if(h6 & (1u<<E6_FREEZE)){            // hold a slice, S=freeze
                static int a=0; int la=8000; int ra=a-la; if(ra<0)ra+=24000;
                float fa=f6echo[ra]; float fb=0.9f+0.099f*s;
                f6echo[a]=tanhf((oL+oR)*0.5f*(1.f-s)+fa*fb);
                oL=oL*(1.f-s)+fa*s; oR=oR*(1.f-s)+fa*s; if(++a>=24000)a=0;
            }
            if(h6 & (1u<<E6_UNISON)){            // detuned doubling up
                static int wp=0; static float rp=0.f; f6pchL[wp]=oL; f6pchR[wp]=oR;
                rp+=1.f+0.012f*s; if(rp>=6000.f)rp-=6000.f; int rd=wp-(int)rp; if(rd<0)rd+=12000;
                oL=oL*0.6f+f6pchL[rd]*0.6f; oR=oR*0.6f+f6pchR[rd]*0.6f; if(++wp>=12000)wp=0;
            }
            if(h6 & (1u<<E6_UNILOW)){            // detuned doubling down
                static int wp=0; static float rp=0.f; f6pchR[wp]=oL;
                rp+=1.f-0.02f*s; if(rp<0.f)rp+=6000.f; if(rp>=6000.f)rp-=6000.f; int rd=wp-(int)rp; if(rd<0)rd+=12000;
                oL=oL*0.6f+f6pchR[rd]*0.6f; oR=oR*0.6f+f6pchR[rd]*0.6f; if(++wp>=12000)wp=0;
            }
            if(h6 & (1u<<E6_DETUNE)){            // 2-voice chorus-detune
                static int wp=0; static float p1=0,p2=0; float* fl=f6dtL; float* fr=f6dtR;
                p1+=0.7f/48000.f; p2+=0.9f/48000.f; if(p1>=1.f)p1-=1.f; if(p2>=1.f)p2-=1.f;
                fl[wp&2047]=oL; fr[wp&2047]=oR; float d1=300.f+250.f*s*psin(p1), d2=300.f+250.f*s*psin(p2);
                int a=(wp-(int)d1)&2047, b=(wp-(int)d2)&2047;
                oL=oL*0.5f+fl[a]*0.5f; oR=oR*0.5f+fr[b]*0.5f; if(++wp>=2048)wp=0;
            }
            if(h6 & (1u<<E6_PITCHLFO)){          // vibrato wobble
                static int wp=0; static float ph=0.f; float* buf=f6plfo;
                ph+=6.f/48000.f; if(ph>=1.f)ph-=1.f; buf[wp&4095]=oL;
                float d=1000.f+900.f*s*psin(ph); int ip=(int)d; float fr=d-ip;
                int r0=(wp-ip)&4095, r1=(wp-ip-1)&4095; oL=oR=buf[r0]+(buf[r1]-buf[r0])*fr; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_BENDUP)){            // pitch glide up while held
                static int wp=0; static float spd=1.f; static float rp=0.f; float* buf=f6bu;
                spd+=((1.f+s)-spd)*0.0006f; buf[wp&4095]=oL; rp+=spd; if(rp>=4000.f)rp-=4000.f;
                int rd=(wp-(int)rp)&4095; oL=oR=buf[rd]; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_BENDDN)){            // pitch dive while held
                static int wp=0; static float spd=1.f; static float rp=0.f; float* buf=f6bd;
                spd+=((1.f-0.6f*s)-spd)*0.0006f; buf[wp&4095]=oL; rp+=spd; if(rp<0.f)rp+=4000.f; if(rp>=4000.f)rp-=4000.f;
                int rd=(wp-(int)rp)&4095; oL=oR=buf[rd]; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_WARBLE)){            // fast tape flutter
                static int wp=0; static float ph=0.f; float* buf=f6wrb;
                ph+=(7.f+s*12.f)/48000.f; if(ph>=1.f)ph-=1.f; buf[wp&4095]=oL;
                float d=800.f+700.f*psin(ph)*s; int ip=(int)d; float fr=d-ip;
                int r0=(wp-ip)&4095,r1=(wp-ip-1)&4095; oL=oR=buf[r0]+(buf[r1]-buf[r0])*fr; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_HARM5)){             // +shimmer harmony grain
                static int wp=0; static float rp=0.f; f6pchL[wp&8191]=oL;
                rp+=0.5f; if(rp>=3000.f)rp-=3000.f; int rd=(wp-(int)rp)&8191;
                float fifth=f6pchL[rd]; oL=oL*(1.f-s*0.5f)+fifth*s*0.7f; oR=oL; if(++wp>=8192)wp=0;
            }
            if(h6 & (1u<<E6_SUBOCT)){            // zero-cross sub generator
                static float prev=0.f; static float flip=1.f;
                if(prev<=0.f && oL>0.f) flip=-flip; prev=oL;
                float sub=flip*fabsf(oL); oL+=sub*s; oR+=sub*s;
            }
            if(h6 & (1u<<E6_LOOP)){              // capture & loop a bar, S=length
                static int pp=0; static int cap=1; static int len=48000;
                if(cap){ len=(int)(step_ms*48*16*(0.25f+0.75f*s)); if(len>96000)len=96000; if(len<2400)len=2400; cap=0; }
                f6loopL[pp]=oL; f6loopR[pp]=oR; oL=f6loopL[pp]; oR=f6loopR[pp]; if(++pp>=len)pp=0;
            }
        }

        // ══ FX7 — LIVE / PERFORMANCE (grid gates, motion, dynamics) ══
        if(fx_eff7){
            const uint16_t h7 = fx_eff7; const float s = ffsr;
            float sl = gslen;                                   // samples per 1/16
            float gpos = (float)gstep*sl + (float)gstep_samp;   // grid-locked position
            // grid gate: N sixteenths per cycle, 50% duty, lightly smoothed. Phase
            // is derived from the playhead so the gate edges sit exactly on grid.
            #define GATE_N(env, N) { float ph=gpos/(sl*(N)); ph-=floorf(ph); \
                float gt=(ph<0.5f)?1.f:0.f; static float env=1.f; env+=(gt-env)*0.02f; oL*=env; oR*=env; }
            if(h7 & (1u<<E7_GATE4))  GATE_N(e7g4, 4.f)
            if(h7 & (1u<<E7_GATE8))  GATE_N(e7g8, 2.f)
            if(h7 & (1u<<E7_GATE16)) GATE_N(e7g16, 1.f)
            if(h7 & (1u<<E7_TRANCE)){            // 16-step trance gate, locked to gstep
                static const uint16_t P=0xB3A6; int st=gstep&15;
                float gt=((P>>st)&1)?1.f:0.f; static float e=1.f; e+=(gt-e)*0.03f; oL*=e; oR*=e;
            }
            if(h7 & (1u<<E7_TREM)){              // sine tremolo at 1/8
                static float ph=0.f; ph+=1.f/(sl*2.f); if(ph>=1.f)ph-=1.f;
                float d=0.4f+0.55f*s; float m=1.f-d*0.5f*(1.f+psin(ph)); oL*=m; oR*=m;
            }
            if(h7 & (1u<<E7_PAN)){               // auto-pan LFO at 1/4
                static float ph=0.f; ph+=1.f/(sl*4.f); if(ph>=1.f)ph-=1.f;
                float p=0.5f+0.5f*psin(ph)*(0.4f+0.6f*s); oL*=(1.f-p)*1.4f; oR*=p*1.4f;
            }
            if(h7 & (1u<<E7_PUMP)){              // sidechain duck on the beat
                float ph=gpos/(sl*4.f); ph-=floorf(ph);
                float duck=0.25f+0.75f*ph; oL*=duck; oR*=duck;
            }
            if(h7 & (1u<<E7_SWELL)){             // reverse swell into the beat
                float ph=gpos/(sl*4.f); ph-=floorf(ph);
                float sw=0.25f+0.75f*(1.f-ph); oL*=sw; oR*=sw;
            }
            if(h7 & (1u<<E7_NOISE)){             // vinyl noise overlay
                float n=((float)(lcg()&0xffff)-32768.f)/32768.f;
                float a=0.02f+0.06f*s; oL+=n*a; oR+=n*a;
            }
            if(h7 & (1u<<E7_WIDE)){              // Haas widener — delay R a touch
                static int wp=0; f7dl[wp&2047]=oR; int rd=(wp-360)&2047;
                oR=oR*0.4f+f7dl[rd]*0.7f; if(++wp>=2048)wp=0;
            }
            if(h7 & (1u<<E7_MONO)){ float m=(oL+oR)*0.5f; oL=m; oR=m; }
            if(h7 & (1u<<E7_TILTU)){             // bright tilt (add highs)
                static float lpL=0.f,lpR=0.f; lpL+=(oL-lpL)*0.25f; lpR+=(oR-lpR)*0.25f;
                oL+= (oL-lpL)*(0.6f+s); oR+=(oR-lpR)*(0.6f+s);
            }
            if(h7 & (1u<<E7_TILTD)){             // dark tilt (low-pass)
                static float lpL=0.f,lpR=0.f; float c=0.35f-0.25f*s;
                lpL+=(oL-lpL)*c; lpR+=(oR-lpR)*c; oL=lpL; oR=lpR;
            }
            if(h7 & (1u<<E7_DRIVE)){ float g=1.f+4.f*s; oL=tanhf(oL*g)*0.8f; oR=tanhf(oR*g)*0.8f; }
            if(h7 & (1u<<E7_SOFTG)){             // smooth pulsing gate at 1/8
                float ph=gpos/(sl*2.f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.006f; oL*=e; oR*=e;
            }
            if(h7 & (1u<<E7_KILL)){ oL=0.f; oR=0.f; }
            #undef GATE_N
        }

        // ══ FX8 — MIDI / SEQUENCE FX (ratchets, ring, melodic gates) ══
        if(fx_eff8){
            const uint16_t h8 = fx_eff8; const float s = ffsr;
            float sl = gslen;                                   // samples per 1/16
            float gpos = (float)gstep*sl + (float)gstep_samp;   // grid-locked position
            #define RAT_N(env, N) { float ph=gpos/(sl*(N)); ph-=floorf(ph); \
                float gt=(ph<0.5f)?1.f:0.f; static float env=1.f; env+=(gt-env)*0.05f; oL*=env; oR*=env; }
            if(h8 & (1u<<E8_RATCH2)) RAT_N(e8r2, 0.5f)
            if(h8 & (1u<<E8_RATCH3)) RAT_N(e8r3, 0.3333f)
            if(h8 & (1u<<E8_RATCH4)) RAT_N(e8r4, 0.25f)
            if(h8 & (1u<<E8_RATCH8)) RAT_N(e8r8, 0.125f)
            if(h8 & (1u<<E8_STRUM)){             // rising ramp across each 1/8
                float ph=gpos/(sl*2.f); ph-=floorf(ph); oL*=ph; oR*=ph;
            }
            if(h8 & (1u<<E8_TRANCE2)){           // alt 16-step pattern, locked to gstep
                static const uint16_t P=0xC5B2; int st=gstep&15;
                float gt=((P>>st)&1)?1.f:0.f; static float e=1.f; e+=(gt-e)*0.03f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_STAB)){              // pluck decay each 1/16
                float ph=gpos/sl; ph-=floorf(ph); float a=1.f-ph; a*=a; oL*=a; oR*=a;
            }
            if(h8 & (1u<<E8_RINGLO)){            // ring mod ~90Hz
                static float ph=0.f; ph+=90.f/48000.f; if(ph>=1.f)ph-=1.f; float c=psin(ph);
                float m=0.4f+0.55f*s; oL=oL*(1.f-m)+oL*c*m; oR=oR*(1.f-m)+oR*c*m;
            }
            if(h8 & (1u<<E8_RINGHI)){            // ring mod ~360Hz
                static float ph=0.f; ph+=360.f/48000.f; if(ph>=1.f)ph-=1.f; float c=psin(ph);
                float m=0.4f+0.55f*s; oL=oL*(1.f-m)+oL*c*m; oR=oR*(1.f-m)+oR*c*m;
            }
            if(h8 & (1u<<E8_VIBR)){              // fast amp vibrato
                static float ph=0.f; ph+=7.f/48000.f*(sl>1.f?1.f:1.f); ph+=0.00010f; if(ph>=1.f)ph-=1.f;
                float m=1.f-(0.2f+0.3f*s)*0.5f*(1.f+psin(ph)); oL*=m; oR*=m;
            }
            if(h8 & (1u<<E8_CHOP)){              // random S&H gate each 1/16, on grid
                static int st=-1; static float lv=1.f; int cs=gstep;
                if(cs!=st){ st=cs; lv=((lcg()&3u)!=0u)?1.f:0.f; } oL*=lv; oR*=lv;
            }
            if(h8 & (1u<<E8_HALFG)){             // half-note gate
                float ph=gpos/(sl*8.f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.02f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_DBLG)){              // two pulses per 1/16
                float ph=gpos/(sl*0.5f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.06f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_GLCHG)){             // glitch gate: random short bursts, on grid
                static int st=-1; static float lv=1.f; int cs=(int)(gpos/(sl*0.5f));
                if(cs!=st){ st=cs; lv=((lcg()%5u)<2u)?0.f:1.f; } oL*=lv; oR*=lv;
            }
            if(h8 & (1u<<E8_REVG)){              // reverse env swell each 1/16
                float ph=gpos/sl; ph-=floorf(ph); oL*=ph*ph; oR*=ph*ph;
            }
            if(h8 & (1u<<E8_SILENCE)){ oL=0.f; oR=0.f; }
            #undef RAT_N
        }

        // ══ ORDERED MASTER CHAIN: RING → DIST → FILTER → CRUSH ══
        // RING (FX2 F2_RING bit) — must precede distortion + filter
        {
            static float rphase=0.f;
            if((fx_eff2 >> F2_RING) & 1u){
                float fc = 40.f*powf(37.5f,ffsr)*(1.f + 0.3f*fx_lid*sinf(rphase*0.07f));
                rphase += fc/48000.f; if(rphase>=1.f) rphase-=1.f;
                float c=psin(rphase);
                oL = oL*(1.f-ffsr*0.9f) + (oL*c)*ffsr*0.9f;
                oR = oR*(1.f-ffsr*0.9f) + (oR*c)*ffsr*0.9f;
            }
        }
        // DIST
        if((fh >> FX_DIST) & 1u){
            float g=4.f+ffsr*10.f; float dc=tanhf(0.08f*g)*0.62f;
            oL=tanhf((oL+0.08f)*g)*0.62f-dc; oR=tanhf((oR+0.08f)*g)*0.62f-dc;
        }
        // FILTER (FX_FLT) — post-mix resonant bandpass DJ sweep, FSR = LFO speed
        {
            static float mlp=0,mbp=0,mlpR=0,mbpR=0,mlfo=0.f;
            if((fh >> FX_FLT) & 1u){
                mlfo += 0.025f + ffsr*0.30f; if(mlfo>=1.f) mlfo-=1.f;
                float tri = 1.f - fabsf(2.f*mlfo-1.f);
                float fc = 200.f * powf(17.5f, tri);
                float f = 2.f*sinf(3.14159265f*fc/48000.f), q=0.3f;
                mlp += f*mbp;  float hp = oL - mlp - q*mbp;  mbp += f*hp;
                mlpR+= f*mbpR; float hpR= oR - mlpR- q*mbpR; mbpR+= f*hpR;
                oL = mbp; oR = mbpR;
                if(mlp> 4.f)mlp= 4.f; else if(mlp<-4.f)mlp=-4.f;
                if(mbp> 4.f)mbp= 4.f; else if(mbp<-4.f)mbp=-4.f;
                if(mlpR> 4.f)mlpR= 4.f; else if(mlpR<-4.f)mlpR=-4.f;
                if(mbpR> 4.f)mbpR= 4.f; else if(mbpR<-4.f)mbpR=-4.f;
            } else { mlp=mbp=mlpR=mbpR=0.f; }
        }
        // CRUSH (after the filter)
        if(crush){
            if(crn==0){ float qs=24.f-ffsr*18.f; crL=(float)(int)(oL*qs)/qs; crR=(float)(int)(oR*qs)/qs; }
            crn=(crn+1u)%cint; oL=crL; oR=crR;
        } else crn=0;

        // MASTER VOLUME — applied here, after EVERY effect, so volume-down also
        // tames any FX feedback/static. (0..1.25; >1 leans into the limiter.)
        oL *= mvol_l; oR *= mvol_l;

        // hard safety limiter — last line of defence before the codec
        if(oL >  1.f) oL =  1.f; else if(oL < -1.f) oL = -1.f;
        if(oR >  1.f) oR =  1.f; else if(oR < -1.f) oR = -1.f;

        out[0][i] = oL;
        out[1][i] = oR;
        float am = (oL<0.f)?-oL:oL; if(am>opk) opk=am;
        gstep_samp++;   // advance the grid-locked sub-step position
    }
    rb_w = rw;
    mic_amp=pk;
    out_amp=opk;

    // USB capture carries the INSTRUMENT'S OUTPUT MIX (post-FX, post-volume,
    // post-limiter) — what the player hears — so the Mac monitor app gets the
    // actual performance. It used to stream the raw ADC input, which is
    // silence unless something is plugged into the input jack; users read
    // that as "USB audio broken" while playing the pads (2026-08-16).
    usbCapturePush(out[0], out[1], sz);
}

// ============================================================
// SPI SD card — bit-bang on D26/D27/D25/D28
// D26(PD11)=CLK  D27(PG9)=MOSI  D25(PA0)=MISO  D28(PA2)=CS
// ============================================================
static dsy_gpio ssd_clk, ssd_mosi, ssd_miso, ssd_cs;
static bool     ssd_sdhc = false;
static DSTATUS  ssd_stat = STA_NOINIT;
static bool     ssd_fast = false;   // after init, drop the 5µs delays (~80x faster
                                    // for bulk audio I/O; init must stay slow per spec)

static void sclk(int v)  { dsy_gpio_write(&ssd_clk,  v); }
static void smosi(int v) { dsy_gpio_write(&ssd_mosi, v); }
static void scs(int v)   { dsy_gpio_write(&ssd_cs,   v); }
static int  smiso()      { return (int)dsy_gpio_read(&ssd_miso); }

static uint8_t spiXfer(uint8_t b) {
    uint8_t rx = 0;
    if(ssd_fast){
        // fast path: raw GPIO toggling (~10 MHz), no explicit delay — the
        // function-call overhead alone keeps it inside the card's SPI limit
        for (int i = 7; i >= 0; i--) {
            smosi((b >> i) & 1);
            sclk(1);
            if (smiso()) rx |= (1 << i);
            sclk(0);
        }
        return rx;
    }
    // 5us half-period. REVERTED from a 2us "optimisation" 2026-09-13: the
    // faster init clock made the card fail to mount (SD:-- on the dash).
    // This value is what the working Aug-18 build used. DO NOT speed it up
    // again -- the spec headroom is theoretical, this wiring is not.
    for (int i = 7; i >= 0; i--) {
        smosi((b >> i) & 1);
        System::DelayUs(5);
        sclk(1);
        System::DelayUs(5);
        if (smiso()) rx |= (1 << i);
        sclk(0);
    }
    return rx;
}

static uint8_t sdCmd(uint8_t cmd, uint32_t arg) {
    uint8_t crc = (cmd == 0) ? 0x95 : (cmd == 8) ? 0x87 : 0xFF;
    spiXfer(0x40 | cmd);
    spiXfer((uint8_t)(arg >> 24));
    spiXfer((uint8_t)(arg >> 16));
    spiXfer((uint8_t)(arg >>  8));
    spiXfer((uint8_t) arg);
    spiXfer(crc);
    uint8_t r = 0xFF;
    for (int i = 0; i < 8 && (r & 0x80); i++) r = spiXfer(0xFF);
    return r;
}

// SD INIT — REVERTED 2026-09-14 to the EXACT code from the working Aug-18
// build (POCKETGORATOR_CODE.cpp), byte for byte. The "hardening" tried on
// 2026-09-13 (250ms settle, 160-clock preamble, 5s ACMD41, 3 retries) did NOT
// fix the ACMD41 timeout, so it was change without benefit. Known-good beats
// clever. Do not re-apply it without evidence it helps.
static DSTATUS ssd_initialize(BYTE) {
    ssd_clk  = {hw.GetPin(26), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    ssd_mosi = {hw.GetPin(27), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    ssd_miso = {hw.GetPin(25), DSY_GPIO_MODE_INPUT,     DSY_GPIO_PULLUP};
    ssd_cs   = {hw.GetPin(28), DSY_GPIO_MODE_OUTPUT_PP, DSY_GPIO_NOPULL};
    dsy_gpio_init(&ssd_clk);  dsy_gpio_init(&ssd_mosi);
    dsy_gpio_init(&ssd_miso); dsy_gpio_init(&ssd_cs);
    sclk(0); smosi(1); scs(1);
    System::Delay(10);

    // 80 clocks with CS high to put card into SPI idle
    for (int i = 0; i < 10; i++) spiXfer(0xFF);

    // CMD0 — reset into SPI mode
    scs(0);
    uint8_t r = sdCmd(0, 0);
    scs(1); spiXfer(0xFF);
    if (r != 0x01) return STA_NOINIT;

    // CMD8 — check voltage / detect SDHC capability
    scs(0);
    r = sdCmd(8, 0x000001AA);
    bool v2 = (r == 0x01);
    if (v2) { for (int i = 0; i < 4; i++) spiXfer(0xFF); } // skip R7
    scs(1); spiXfer(0xFF);

    // ACMD41 — activate init; HCS=1 to negotiate SDHC
    uint32_t hcs = v2 ? 0x40000000u : 0u;
    uint32_t deadline = System::GetNow() + 2000;
    do {
        scs(0); sdCmd(55, 0); scs(1); spiXfer(0xFF);
        scs(0); r = sdCmd(41, hcs); scs(1); spiXfer(0xFF);
        // This negotiation can spin for up to 2 s. Starving tud_task() that
        // long makes the host time the device out and drop it off the bus —
        // the same failure ntInit() and the intro animation had. Runs in
        // thread context (main), so pumping here is safe.
        tud_task();
    } while (r == 0x01 && System::GetNow() < deadline);
    if (r != 0x00) return STA_NOINIT;

    // CMD58 — read OCR, determine SDHC (CCS bit)
    ssd_sdhc = false;
    if (v2) {
        scs(0);
        r = sdCmd(58, 0);
        if (r == 0x00) {
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = spiXfer(0xFF);
            ssd_sdhc = (ocr[0] & 0x40) != 0;
        }
        scs(1); spiXfer(0xFF);
    }

    // CMD16 — force 512-byte blocks for SDSC cards
    if (!ssd_sdhc) {
        scs(0); r = sdCmd(16, 512); scs(1); spiXfer(0xFF);
        if (r != 0x00) return STA_NOINIT;
    }

    ssd_stat = 0;
    ssd_fast = true;   // init done — switch to fast bulk transfers
    return 0;
}

static DSTATUS ssd_status(BYTE) { return ssd_stat; }

static DRESULT ssd_read(BYTE, BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
        // Multi-second file operations (patch/audio autosave, sample load)
        // run through here sector by sector; without this pump they starve
        // the USB plane whole seconds at a time — measured as a 3.2s hole
        // in the MIDI clock (host BPM display drops out, 2026-08-16).
        pmor_usb_service();
        scs(0);
        if (sdCmd(17, sector) != 0x00) { scs(1); return RES_ERROR; }
        uint32_t t = System::GetNow() + 200;
        uint8_t tok;
        do { tok = spiXfer(0xFF); } while (tok == 0xFF && System::GetNow() < t);
        if (tok != 0xFE) { scs(1); return RES_ERROR; }
        for (int i = 0; i < 512; i++) buf[i] = spiXfer(0xFF);
        spiXfer(0xFF); spiXfer(0xFF); // CRC
        scs(1); spiXfer(0xFF);
        if (!ssd_sdhc) sector += 512; else sector++;
    }
    return RES_OK;
}

static DRESULT ssd_write(BYTE, const BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
        pmor_usb_service();   // see ssd_read — keeps MIDI clock + USB audio
                              // alive through multi-second autosaves
        scs(0);
        if (sdCmd(24, sector) != 0x00) { scs(1); return RES_ERROR; }
        spiXfer(0xFF);
        spiXfer(0xFE); // data token
        for (int i = 0; i < 512; i++) spiXfer(buf[i]);
        spiXfer(0xFF); spiXfer(0xFF); // dummy CRC
        if ((spiXfer(0xFF) & 0x1F) != 0x05) { scs(1); return RES_ERROR; }
        uint32_t t = System::GetNow() + 500;
        while (spiXfer(0xFF) == 0x00 && System::GetNow() < t) {}
        scs(1); spiXfer(0xFF);
        if (!ssd_sdhc) sector += 512; else sector++;
    }
    return RES_OK;
}

static DRESULT ssd_ioctl(BYTE, BYTE cmd, void* buf) {
    if (cmd == CTRL_SYNC)       return RES_OK;
    if (cmd == GET_SECTOR_SIZE) { *(WORD*)buf  = 512; return RES_OK; }
    if (cmd == GET_BLOCK_SIZE)  { *(DWORD*)buf = 1;   return RES_OK; }
    return RES_PARERR;
}

static const Diskio_drvTypeDef SPISD_Driver = {
    ssd_initialize, ssd_status, ssd_read, ssd_write, ssd_ioctl
};

// ============================================================
// OLED helpers
// ============================================================
static char row[32];
static void orow(int y, const char* s) {
    display.SetCursor(0,y); display.WriteString(s,Font_6x8,true);
}

// ── USB CDC console — line-buffered, feeds clock_engine's command handler
// (SY mode / STATUS / RATE / DIAG / BEAT / HELP / buttonless DFU trigger).
// Ported from firmware-tinyusb/'s main.cpp PollCdc(); the only debugging
// channel on this enclosed device (see "buttonless DFU" below). Control
// plane only — never called from the audio ISR.
// ── One example beat, written ONLY into a slot the user hasn't touched ──
// Boot-time convenience so a fresh (or freshly-reflashed-but-not-yet-
// patched) unit makes sound out of the box. Absolute rule (repeated,
// explicit user requirement): NEVER touch a slot whose `exists` bit is
// already set — that is a real saved beat. Finds the first CLEAR bit
//0..15 and writes there; if all 16 are occupied, writes nothing and
// returns false so the caller can surface that instead of silently no-
// oping. Does NOT change sess_cur/active — the user's current patch stays
// selected; the example just becomes visible (existing, unlit "P" box) in
// the PATCHES/SESS page like any other saved slot. Runs once — after the
// first boot that plants it, its `exists` bit is set, so this is a no-op
// on every later boot.
static bool g_example_beat_full = false;   // true if all 16 slots were already occupied
static void plantExampleBeatIfRoomExists() {
    int idx = -1;
    for(int i = 0; i < 16; i++) if(!((sess_exists >> i) & 1u)) { idx = i; break; }
    if(idx < 0) { g_example_beat_full = true; return; }   // all 16 full — write nothing

    Persist beat = {};
    beat.step_ms = 125;                                   // 120 BPM
    beat.dr_pad[0] = 13; beat.dr_pad[1] = 10; beat.dr_pad[2] = 4; beat.dr_pad[3] = 0; // stock flavor pads
    beat.dr_k[0]=4; beat.dr_k[1]=2; beat.dr_k[2]=8; beat.dr_k[3]=3;
    beat.dr_rot[0]=0; beat.dr_rot[1]=4; beat.dr_rot[2]=0; beat.dr_rot[3]=5;
    // simple 4/4 pattern, bar-0/16-step bitmask (bit i = step i), MACHINE mode:
    beat.dr_pat[0] = 0x1111;   // kick  — steps 0,4,8,12  (beats 1/2/3/4)
    beat.dr_pat[1] = 0x1010;   // snare — steps 4,12       (backbeat 2/4)
    beat.dr_pat[2] = 0x5555;   // hat   — every other step (8ths)
    beat.dr_pat[3] = 0x0000;   // 4th voice unused
    beat.eu_k_syn = 5; beat.eu_rot_syn = 0; beat.eu_k_smp = 4;
    beat.seq_mode[ENG_SYNTH] = SYN_SEQ;
    beat.seq_mode[ENG_DRUM]  = SEQ_MACHINE;   // drum pattern actually plays on load
    beat.seq_mode[ENG_SMP]   = SEQ_MANUAL;
    beat.vol_pidx = 9; beat.menu = MENU_DRUM;
    beat.g_key = 0; beat.g_mode = 3; beat.g_swing = 0;
    beat.g_csyn = 6; beat.g_cpoly = 5; beat.g_cdrum = 9;
    beat.g_wave = 5; beat.kit_sel = 0; beat.g_csmp = 9;
    beat.form_blen = 16; beat.form_bars = 1;
    // smp_pat / fx_auto / fx_auto_fsr / rsvd all left zero (= empty sampler,
    // no FX automation, per-engine mix levels default to unity on load).

    pstore.GetSettings().s[idx] = beat;
    sess_exists |= (1u << idx);
    pstore.GetSettings().exists = sess_exists;
    pstore.Save();          // QSPI
    patchSaveSD();          // durable SD mirror, same as any other save
}

static void PollCdc(void) {
    if(!tud_cdc_connected() || !tud_cdc_available()) return;
    static char    line[64];
    static uint8_t len = 0;
    uint8_t        buf[64];
    uint32_t       n = tud_cdc_read(buf, sizeof(buf));
    for(uint32_t i = 0; i < n; i++) {
        char c = (char)buf[i];
        if(c == '\r') continue;
        if(c == '\n') {
            line[len] = '\0';
            clock_engine_handle_cdc_line(line);
            len = 0;
            continue;
        }
        if(len < sizeof(line) - 1) line[len++] = c;
        // else: silently drop overlong-line overflow, never corrupt adjacent memory
    }
}

// ============================================================
// main
// ============================================================

static void PollCdc(void);

// ── LIVE SUBSYSTEM GATES (2026-08-14) ────────────────────────────────────
// The instrument enumerates fine through ALL of init (chunks 0-7 pass) but
// USB dies once the main loop runs. Bisecting that by flashing costs a
// physical DFU per hypothesis, because a build that kills USB also kills the
// CDC channel used to trigger DFU.
//
// So: gate the heavy main-loop subsystems behind runtime flags, default OFF,
// and expose them over CDC. Each enable is TIME-BOXED — it auto-reverts to
// OFF after g_probe_ms — so if a subsystem does kill USB, the device turns it
// back off by itself and USB (and therefore CDC, and therefore DFU) comes
// back. That makes the experiment repeatable with zero flashes.
//   CDC:  TRel 1|0   TKeys 1|0   TOled 1|0   TAll 1|0   TMs <ms>
#ifdef PMOR_OLED_USB_ONLY
volatile bool     g_en_trel = false;   // PB5 belongs to the display in this build
#else
volatile bool     g_en_trel = true;
#endif
#ifdef PMOR_OLED_USB_ONLY
volatile bool     g_en_keys = false;
#else
volatile bool     g_en_keys = true;
#endif
// Display OFF by default for this test. intro-on/loop-off and
// intro-off/loop-on BOTH kill USB, and the only thing they share is drawing
// to the OLED. If USB survives with the loop running and the screen dark,
// the display is the sole culprit — and CDC will be alive so the rest can be
// toggled live ("TOled 1") without another flash.
// Display OFF. This is the only configuration measured to run CLEAN:
// full instrument + USB audio + USB MIDI + CDC, no glitching.
// Hardware SPI drives the panel correctly but kills USB enumeration; soft
// SPI coexists with USB but never lit the panel and starved the audio
// stream. Screen+USB together is UNSOLVED — see the notes above Update().
volatile bool     g_en_oled = true;
// Master gate: with this OFF the main loop does nothing but service USB.
// Needed because USB died even with trel/keys/oled all disabled, so the
// culprit is elsewhere in the loop body (sensors, encoders, ADC, menu,
// clock, patch autosave...). Proves "USB alone survives the loop" first,
// then we switch pieces on from a known-good state.
#ifdef PMOR_QUIET_BOOT
// Quiet-boot bisect build: the whole main loop is gated OFF at boot — the
// loop body does nothing but pump USB + CDC. Subsystems are then enabled
// one at a time over CDC (TLoop/TOled/TIntro/...) to find the USB-killer
// live, without reflashing.
volatile bool     g_en_loop = false;
#else
volatile bool     g_en_loop = true;
#endif
volatile bool     g_en_intro = false;
volatile uint32_t g_probe_ms = 4000;
volatile uint32_t g_probe_until = 0;

// One-stop USB service: task pump + capture-ring drain + MIDI clock/roles.
// Called from the main loop, from OLED Update() between page blits, and from
// usbDelay() — so no code path can starve the USB plane for more than one
// OLED page (~0.4ms). All call sites are main-loop thread context; none are
// reachable from inside clock_engine_poll() itself, so this cannot re-enter.
extern "C" void pmor_usb_service(void) {
    tud_task();
    usb_audio_stream_task();
    clock_engine_poll();
}

// ── Surgical CDC test hooks (2026-08-16 audio×OLED hunt) ─────────────────
// Called from PollCdc's parser, i.e. main-loop thread context. Let the
// audio×display interaction be A/B'd live: blit one page or a full frame on
// demand, and stop/start the audio engine, all while watching USB stay up
// or die over the same CDC link.
// Raw 2-byte command straight to the panel, bypassing OledDisplay (whose
// driver_ member is private). Same pins, same timing as the bit-bang driver.
static void oledCmdRaw(uint8_t c1, uint8_t c2) {
    static GPIO rsclk, rmosi, rcs, rdc;
    static bool rinit = false;
    if(!rinit) {
        rsclk.Init(seed::D8,  GPIO::Mode::OUTPUT);
        rmosi.Init(seed::D10, GPIO::Mode::OUTPUT);
        rcs.Init(seed::D7,   GPIO::Mode::OUTPUT);
        rdc.Init(seed::D3,   GPIO::Mode::OUTPUT);
        rinit = true;
    }
    rdc.Write(false); rcs.Write(false);
    uint8_t bytes[2] = {c1, c2};
    for(int b = 0; b < 2; b++)
        for(int bit = 7; bit >= 0; bit--) {
            rmosi.Write((bytes[b] >> bit) & 1);
            for(volatile int k = 0; k < 8; k++) {}
            rsclk.Write(true);
            for(volatile int k = 0; k < 8; k++) {}
            rsclk.Write(false);
        }
    rcs.Write(true); rmosi.Write(true); rsclk.Write(false);
}

extern "C" void pmor_oled_test(int what) {
    if(what == 1) {                       // one dirty page (~131 bytes bit-banged)
        display.DrawPixel(0, 0, true);
        display.DrawPixel(0, 0, false);   // net no visual change, marks page 0 dirty
        display.Update();
    } else if(what == 2) {                // full white then full black (LEGACY OBF)
        display.Fill(true);  display.Update();
        display.Fill(false); display.Update();
    } else if(what == 3) {                // OBB: two full-frame blits, ALL BLACK
        // Same bus traffic as OBF but near-zero panel current (OLED pixel
        // current ~ lit pixels). Splits "blit traffic kills" from "panel
        // current kills".
        display.Fill(false); display.Update();   // Fill() marks every page dirty,
        display.Fill(false); display.Update();   // so each Update blits all 8 pages
    } else if(what == 4) {                // OBW: one full-frame blit, ALL WHITE
        display.Fill(true);  display.Update();
    } else if(what == 5) {                // restore dark screen
        display.Fill(false); display.Update();
    } else if(what == 6) {                // OBC: 50% checkerboard, full frame
        for(int y = 0; y < 64; y++)
            for(int x = 0; x < 128; x++)
                display.DrawPixel(x, y, ((x ^ y) & 1) != 0);
        display.Update();
    } else if(what == 7) {                // OBH: top half white
        display.Fill(false);
        for(int y = 0; y < 32; y++)
            for(int x = 0; x < 128; x++)
                display.DrawPixel(x, y, true);
        display.Update();
    } else if(what == 8) {                // OBS: full white, one page per 50ms
        for(int y = 0; y < 64; y += 8) {
            for(int yy = y; yy < y + 8; yy++)
                for(int x = 0; x < 128; x++)
                    display.DrawPixel(x, yy, true);
            display.Update();             // only the newly-dirty page blits
            uint32_t t0 = System::GetNow();
            while(System::GetNow() - t0 < 50) { tud_task(); usb_audio_stream_task(); }
        }
    }
}
// PB5-level test, panel deselected (CS high): pixels cannot change, so this
// isolates the electrical effect of the MOSI/PB5 line level from any panel
// behaviour. White blits (MOSI high-heavy) kill USB, black blits (low-heavy)
// don't, at ANY contrast — so test the wire, not the screen.
extern "C" void pmor_pb5_test(int high) {
    static GPIO tmosi;
    static bool tinit = false;
    if(!tinit) { tmosi.Init(seed::D10, GPIO::Mode::OUTPUT); tinit = true; }
    uint32_t t0 = System::GetNow();
    tmosi.Write(high != 0);
    while(System::GetNow() - t0 < 10) {}   // hold 10ms — ~10x a white page blit
    tmosi.Write(true);                     // release to bus idle (high)
}
extern "C" void pmor_oled_contrast(int level) {
    // 0x81 = SSD1309 contrast command. Panel current scales with this.
    oledCmdRaw(0x81, (uint8_t)level);
}
extern "C" void pmor_audio_ctl(int on) {
    if(on) hw.StartAudio(AudioCallback);
    else   hw.StopAudio();
}

extern "C" void pmor_probe_set(const char* what, int on) {
    g_probe_until = System::GetNow() + g_probe_ms;
    if(!strcmp(what,"trel")) g_en_trel = on;
    else if(!strcmp(what,"keys")) g_en_keys = on;
    else if(!strcmp(what,"oled")) g_en_oled = on;
    else if(!strcmp(what,"loop")) g_en_loop = on;
    else if(!strcmp(what,"intro")) g_en_intro = on;
    else if(!strcmp(what,"all")) { g_en_trel = g_en_keys = g_en_oled = g_en_loop = on; }
}
static inline void pmor_probe_tick() {
    if(g_probe_until && System::GetNow() > g_probe_until) {
        g_en_trel = g_en_keys = g_en_oled = false;   // auto-revert
        g_probe_until = 0;
    }
}


// ── Staged bring-up helper (see PMOR_STAGE calls in main) ────────────────
// Build with `make CHUNK=n`; the firmware halts after stage n, pumping only
// TinyUSB, so we can flash each stage and see exactly which init breaks USB
// enumeration. With CHUNK unset the macro compiles to nothing.
// DFU escape inside PMOR_STAGE MUST be a DAISY mode, not STM (2026-08-15).
// BootloaderMode::STM boots the ST ROM loader, whose DFU exposes ONLY internal
// flash (0x08000000) and option bytes -- it has no QSPI region at all, so
// every scripted write of this app to 0x90040000 was rejected with
// "Last page ... is not writeable". INFINITE_TIMEOUT also removes the
// 2-second window race, so the flasher can never miss it.
// (Comment lives OUT here because // comments inside a \-continued macro
// terminate the definition early and break the build.)
#ifdef PMOR_CHUNK
#define PMOR_STAGE(n)                                                        \
    do { if((n) >= PMOR_CHUNK) {                                             \
        extern volatile uint32_t g_usb_fs_irqs;                              \
        uint32_t _t0 = System::GetNow(); bool _l = false; char _b[32];        \
        for(;;) { tud_task(); usb_audio_stream_task(); PollCdc();            \
            if(clock_engine_dfu_requested()) { System::Delay(50);            \
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT); }    \
            if(System::GetNow() - _t0 > 250) { _t0 = System::GetNow();        \
                _l = !_l; hw.SetLed(_l);                                     \
                /* Show USB state AT THIS STAGE. Guessing from the host      \
                   side cannot tell "never enumerated" from "enumerated      \
                   then dropped"; this can. */                               \
                display.Fill(false);                                         \
                display.SetCursor(0,0);                                      \
                snprintf(_b,sizeof _b,"STAGE %d", (int)(n));                 \
                display.WriteString(_b, Font_7x10, true);                    \
                display.SetCursor(0,14);                                     \
                snprintf(_b,sizeof _b,"irq %lu",                             \
                         (unsigned long)g_usb_fs_irqs);                      \
                display.WriteString(_b, Font_7x10, true);                    \
                display.SetCursor(0,28);                                     \
                snprintf(_b,sizeof _b,"mnt %d rdy %d",                       \
                         tud_mounted()?1:0, tud_ready()?1:0);                \
                display.WriteString(_b, Font_7x10, true);                    \
                display.Update(); }                                          \
        }                                                                    \
    } } while(0)
#else
#define PMOR_STAGE(n) do { } while(0)
#endif

int main(void)
{
    hw.Configure(); hw.Init(true);   // 480MHz boost — full tilt
                                     // (ruled OUT as a USB-enumeration cause
                                     //  2026-08-14: non-boost behaved identically)

    // ── HARDWARE DFU ESCAPE HATCH: hold BOTH encoder buttons at power-on ──
    // The CDC `dfu` command is the normal way in, but it is useless in the
    // exact situation you need it most: if USB fails to enumerate, there is
    // no serial port to send it to, and recovery needs physical BOOT+RESET
    // on a device that is meant to live inside a sealed enclosure. This
    // gesture depends on nothing but GPIO, so it works even when USB, the
    // OLED, or the whole audio path is broken.
    // enc1 SW = D2, enc2 SW = D17 (see pin map below); both are active-low
    // with pull-ups. Read them directly — the Encoder objects are not
    // initialised this early, and deliberately so: fewer moving parts.
    {
        GPIO b1, b2;
        b1.Init(seed::D2,  GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
        b2.Init(seed::D17, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
        System::Delay(0);                  // let the pull-ups settle
        if(!b1.Read() && !b2.Read())       // both held == both low
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
    }

    // ── USB bring-up — BEFORE the OLED, with a settle delay before
    // tusb_init() (ported ordering discipline from firmware-tinyusb/: on
    // that project's hardware, initializing the OLED before USB silently
    // killed enumeration; this project's OLED is a different SPI
    // peripheral, but paying the same ordering cost here is free insurance
    // against the same class of bug). usb_audio_stream_init()/
    // clock_engine_init() first (plain state init, no hardware side
    // effects yet) — same order firmware-tinyusb/'s main.cpp uses.
    usb_audio_stream_init();
    clock_engine_init();
    // Install the OLED per-page yield BEFORE anything can draw: Update()
    // blits the framebuffer as eight back-to-back blocking SPI transfers,
    // and without yielding between them the host never finishes enumerating.
    // DO NOT re-add an OLED page-yield that calls tud_task(). It was added
    // to fight what looked like task starvation, but the real defect is a
    // FIFO race between the priority-0 audio ISR and the priority-6 OTG ISR
    // -- and calling tud_task() from inside display.Update() just adds a
    // THIRD concurrent toucher of ep_in_ff from thread context, widening the
    // race instead of closing it.
    dsy_oled_page_yield_hook = nullptr;

    // NO_USB test build: skip USB entirely so the bit-bang display driver can
    // be judged ON ITS OWN. Every test so far ran driver+USB together, which
    // cannot distinguish "my driver is broken" from "they cannot coexist".
    //   panel lights here  -> driver is CORRECT; the problem is purely the
    //                         electrical interaction on the shared PB5 pin
    //   panel still dark   -> the driver has a bug, and all the USB work has
    //                         been chasing a second problem on top of a first
#ifndef PMOR_NO_USB
    board_usb_init();
    System::Delay(500);   // USB PHY/VBUS-sense settle, mirrors the source project's own delay
    tusb_init();

    // ── FORCE A BUS-VISIBLE DISCONNECT/RECONNECT ─────────────────────────
    // The Daisy bootloader enumerated as a DFU device moments ago. When it
    // jumps here, the host still believes an already-configured device is
    // sitting at that address, so it never starts a fresh enumeration — our
    // descriptors are never requested and nothing appears on the Mac.
    // This matches the measured evidence exactly: the OTG interrupt fires
    // continuously (~320/s, counter climbed past 1900) so the core is alive
    // and the host IS driving the bus, yet the device never mounts.
    // Asserting soft-disconnect (D+ pull-up off) for 150ms makes the host
    // see a real unplug, drop the stale address, and re-enumerate us cleanly.
    tud_disconnect();
    System::Delay(150);
    tud_connect();
    g_t_usb = System::GetNow();
#endif  // PMOR_NO_USB

    // ── BISECT BUILD: USB ONLY, NO INSTRUMENT HARDWARE ───────────────────
    // Build with `make USB_ONLY=1` to stop here and do nothing but pump
    // TinyUSB. Everything below (OLED, NeoTrellis, bit-banged I2C, sensors,
    // SD, audio callback, sequencer) is skipped.
    //
    // WHY: the working USB firmware and this one differ in TWO ways at once
    // — the instrument code AND running from the Daisy bootloader/SRAM
    // instead of internal flash. Guessing which one breaks enumeration has
    // cost hours. This isolates it to one variable:
    //   enumerates here  -> the bootloader/BOOT_SRAM handoff is FINE, and
    //                       something in the instrument's own init kills USB
    //                       (then bisect that: OLED, I2C, SD, ADC...)
    //   still dead here  -> BOOT_SRAM/bootloader is the cause, and no amount
    //                       of instrument-side work will help
    // Staged bring-up. `make CHUNK=n` stops after stage n and just pumps USB,
    // so each stage can be flashed and checked for "does USB still enumerate".
    // CHUNK 0 (USB only) is PROVEN to enumerate from the bootloader, so the
    // bootloader/BOOT_SRAM handoff is NOT the problem — something in the
    // instrument's own init is. Stages, in init order:
    //   0 usb only   1 +OLED   2 +sensors(I2C)   3 +SD   4 +encoders/ADC
    //   5 +patches/SDRAM clear   6 +audio   7 +NeoTrellis   (unset = full)
    PMOR_STAGE(0);

    // ── Hold the OLED off until the host has FINISHED enumerating ────────
    // Ordering alone was not enough. Measured on hardware 2026-08-14: this
    // exact firmware WITHOUT the OLED enumerates every time (audio + MIDI +
    // CDC all appear); WITH the OLED initialised here, nothing enumerates at
    // all — no audio, no MIDI, no serial — even though the app itself runs.
    // Cause is current, not sequence: the OLED module's onboard 12V boost
    // pushes total draw past what a bus/dock port supplies, and the brownout
    // lands squarely in the enumeration window (PocketPassthru-FLASHING.md
    // §6 documented the same failure for OLED + CDC alone).
    //
    // So: pump tud_task() until tud_mounted(), THEN power the display. The
    // 3s cap is mandatory — standalone on battery there is no host and
    // tud_mounted() never becomes true; the instrument must still boot with
    // a working screen. Do not "simplify" this into a bare wait.
    {
        uint32_t t0 = System::GetNow();
        while(!tud_mounted() && (System::GetNow() - t0) < 3000)
            tud_task();
    }

    // ── HOLD EVERYTHING UNTIL USB HAS ENUMERATED ─────────────────────────
    // Every build that touches the OLED before the host finishes enumerating
    // fails (irq climbing past 60000, tud_mounted() stuck at 0); every build
    // that leaves the display alone enumerates fine. Per-page tud_task()
    // yields inside Update() did NOT fix it (verified compiled in and called
    // via objdump), and battery power did NOT fix it — so this is not
    // task-starvation and not a brownout: enumeration itself is the fragile
    // window, and display activity during it is what breaks it.
    //
    // So: do nothing but pump USB until the host has us mounted. Only then
    // bring the screen up and run the instrument normally. The 6s cap keeps
    // the device fully usable standalone, where no host will ever mount it.
    {
        uint32_t t0 = System::GetNow();
        bool led = false;
        while(!tud_mounted() && (System::GetNow() - t0) < 6000) {
            tud_task();
            if(((System::GetNow() - t0) % 250) < 2) { led = !led; hw.SetLed(led); }
        }

        // THE FIX. The boot intro animation is what kills USB — proven by
        // elimination: the full instrument enumerates with it skipped, and
        // dies with it enabled, after everything else (bootloader, all
        // hardware init, every main-loop subsystem, the whole loop body,
        // power, cable, OLED driver) had been individually cleared.
        // Adding tud_task()/usbDelay() inside it did not help, so it is not
        // simple task starvation.
        //
        // It is pure decoration, so: play it when running standalone (no
        // host, nothing to break) and skip it when a host is present. The
        // instrument keeps its intro on battery and gains USB on the desk.
        // Intro HARD OFF for now. Making it conditional on !tud_mounted()
        // was not a clean test — that build also switched the whole main
        // loop on at the same time. One variable at a time: prove the
        // instrument runs fully with USB when the intro simply never plays,
        // then decide whether it can come back for the standalone case.
#ifndef PMOR_QUIET_BOOT
        g_en_intro = true;   // re-enabled: it was the amplifier, not the bug
#endif
    }

    MyOled::Config oc;
    // VERIFIED 2026-08-15 against libDaisy daisy_seed.h:204-208 —
    //   D7=PG10  D8=PG11  D9=PB4  D10=PB5  D11=PB8
    // and against the SSD130x 4-wire SPI transport this replaces, whose
    // defaults are sclk=PG11, mosi=PB5, nss=PG10. The previous values here
    // (D9/D11/D8) resolved to PB4/PB8/PG11: the MISO pin as clock, data on a
    // pin the panel isn't wired to, and the real clock used as chip-select.
    // The panel could not have lit in ANY electrical configuration.
    oc.driver_config.sclk  = seed::D8;    // PG11
    oc.driver_config.mosi  = seed::D10;   // PB5  (shared with NeoTrellis SCL)
    oc.driver_config.cs    = seed::D7;    // PG10 (was SPI1 NSS/HARD_OUTPUT)
    oc.driver_config.dc    = seed::D3;
    oc.driver_config.reset = seed::D4;
    // Bit-bang half-period padding. 0 toggles as fast as a 480MHz core can,
    // which overruns the SSD1309 (its SPI ceiling is a few MHz) and leaves the
    // panel dark. A blank screen with correct wiring is nearly always "too
    // fast"; garbage on screen would be the opposite problem.
    // Push-pull during transactions means edges are fast again, so the clock
    // no longer has to wait on an RC rise. 8 NOPs ~= 16ns per half-period is
    // still far inside the SSD1309's tolerance and keeps a full frame ~1ms
    // instead of the 65ms that made the UI unusable.
    // 24 NOPs/half-period ≈ 2-3MHz — comfortably inside the SSD1309's spec
    // (8 ran right at the ~10MHz ceiling and produced sporadic miswritten
    // bytes on screen, 2026-08-16). Full frame is still ~2-3ms.
    // v2 2026-09-13: 24 -> 48. At 24 this panel corrupted command bytes at
    // runtime (mirrored, then split-and-swapped halves after minutes) --
    // the same "miswritten bytes" failure the 8->24 change fixed before,
    // just at a lower threshold on these flying leads. 48 halves the clock
    // again. Ladder if it recurs: 48 -> 64 -> 96.
    oc.driver_config.nop_delay = 48;
    display.Init(oc);

    // ── DISABLE SPI1's INTERRUPT ─────────────────────────────────────────
    // libDaisy's SpiHandle::Init() unconditionally does
    //     HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
    //     HAL_NVIC_EnableIRQ(SPI1_IRQn);
    // (per/spi.cpp:999) -- preempt priority ZERO, the highest on the chip.
    // The OLED transport only ever uses HAL_SPI_Transmit (polling), so that
    // interrupt serves no purpose here, but once enabled it outranks the OTG
    // ISR and can preempt it mid-TXFIFO-copy, which wedges the DWC2 packet
    // state machine permanently (device off the bus until a power cycle).
    //
    // This is why: raising USB from priority 6 to 1 changed nothing (0 still
    // wins); tud_task() yields changed nothing (thread mode cannot outrank an
    // ISR); removing the audio-ISR FIFO write changed nothing (wrong writer);
    // and soft SPI "fixed" USB only because it never touches the peripheral
    // at all. The single common factor across every failing build was SPI1
    // actually transmitting.
    NVIC_DisableIRQ(SPI1_IRQn);
    // POWER TEST: the OLED refresh kills USB *instantly* (reproduced live over
    // CDC: "TOled 1" -> device off the bus mid-reply). Instant, not gradual,
    // and unaffected by tud_task() yields — that points at current draw, not
    // timing, matching PocketPassthru-FLASHING.md §6 (the panel's onboard 12V
    // boost pushing total draw past what the port supplies).
    // Drive it at minimum contrast to cut that draw and see if USB survives.
    // (contrast set inside the driver init — SendCommand is private here)
    display.Fill(false);
    g_t_oled = System::GetNow();
    display.Update();   // dark during hardware init — intro animation follows
    PMOR_STAGE(1);

    sbus.init(hw.GetPin(13), hw.GetPin(14));
    sbus.us = 3;   // sensor bus ~160kHz — IMU/mag/lidar all handle it
    tbus.init(hw.GetPin(11), hw.GetPin(12));

    // IMU at 0x68 -- MPU6050 (6-axis) on v2, MPU9250 also accepted
    sbus.w8(0x68,0x6B,0x80); System::Delay(100); // DEVICE_RESET
    sbus.w8(0x68,0x6B,0x01); System::Delay(50);  // wake + PLL clock
    uint8_t imu_id = sbus.r8(0x68,0x75);
    // v2: accept the MPU6050 (returns 0x68) as well as the MPU9250 family
    // (0x71/0x73/0x70) and common clones (0x69/0x98). Before this, a 6050 was
    // rejected outright -- imu_ok never went true and the watchdog below
    // reinit-stormed it once a second.
    bool imu_ok = (imu_id==0x71||imu_id==0x73||imu_id==0x70||
                   imu_id==0x68||imu_id==0x69||imu_id==0x98);
    PMOR_STAGE(2);

    // v2: the IMU is an MPU6050 (6-axis) now, NOT an MPU9250. It has NO
    // magnetometer. The probe below simply fails (nothing answers at 0x0C),
    // mag_ok stays false, and the read site is already guarded by mag_ok --
    // so this degrades cleanly. Left in place so an MPU9250 still works if
    // one is ever fitted again.
    // AK8963 magnetometer via MPU bypass (0x0C)
    // v2: an MPU6050 (id 0x68) has NO magnetometer. Probing 0x0C anyway
    // costs three 10ms delays plus failed I2C transactions, each of which
    // can burn the 5ms waitScl stretch timeout -- and the watchdog repeats
    // the probe every second, forever. Skip it unless a 9250 is fitted.
    bool has_mag_hw = (imu_id==0x71||imu_id==0x73||imu_id==0x70);
    bool mag_ok = false;
    if(has_mag_hw) {
    sbus.w8(0x68,0x37,0x02); System::Delay(10); // BYPASS_EN
    sbus.w8(0x0C,0x0A,0x00); System::Delay(10); // power-down before mode change
    mag_ok = (sbus.r8(0x0C,0x00)==0x48);   // WIA = 0x48
    sbus.w8(0x0C,0x0A,0x16); System::Delay(10); // 16-bit continuous 100Hz
    }

    // VL53L1X lidar (0x29)
#ifdef PMOR_NO_LIDAR
    // Bisect build: lidar completely untouched, to isolate whether its init
    // is what wedges the USB core (stage-2→3 kill, 2026-08-16 session).
    bool lidar_ok = false;
#else
    g_t_sens = System::GetNow();
    bool lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(100);
#endif

    // SD card — SPI bit-bang on D22(CLK) D23(MOSI) D24(MISO) D25(CS)
    static char  sdPath[4] = {};
    static FATFS sdFS;
#ifdef PMOR_NO_SD
    // Bisect build: SD never initialized or mounted, card can stay in the
    // slot. Patches still work from QSPI (sessionsBoot below); only the SD
    // mirror/override is skipped.
    bool sd_ok = false;
#else
    bool sd_ok = (FATFS_LinkDriver(&SPISD_Driver, sdPath) == 0) &&
                 (f_mount(&sdFS, sdPath, 1) == FR_OK);
#endif
    PMOR_STAGE(3);
    g_sd_ok = sd_ok;

    // enc1 unchanged: A=D0 B=D1 SW=D2
    eInit(enc1,hw.GetPin(0), hw.GetPin(1), hw.GetPin(2));

    // enc2 is now the JOYSTICK (v2). Same struct, different source.
    //   X=D16(A1 adc1)  Y=D17(A2 adc2)  SW=D24
    joy_sw_g = {hw.GetPin(24), DSY_GPIO_MODE_INPUT, DSY_GPIO_PULLUP};
    dsy_gpio_init(&joy_sw_g);
    enc2.psw = 1; enc2.delta = 0; enc2.clicked = false; enc2.lab = 0;
    enc_ready = true;

    // FSR analog (D15) stays channel 0 -- every hw.adc.GetFloat(0) untouched.
    // Joystick axes added as channels 1 and 2.
    AdcChannelConfig adc_cfg[3];
    adc_cfg[0].InitSingle(hw.GetPin(15));   // FSR   -> ch0
    adc_cfg[1].InitSingle(hw.GetPin(16));   // JOY X -> ch1
    adc_cfg[2].InitSingle(hw.GetPin(17));   // JOY Y -> ch2
    hw.adc.Init(adc_cfg,3);
    hw.adc.Start();
    PMOR_STAGE(4);

    // restore patches: QSPI first, then let the SD card override (SD survives
    // firmware reflash; QSPI may not). Either way apply the active patch.
    g_t_sd = System::GetNow();
    sessionsBoot();
    g_t_boot = System::GetNow();
    if(patchLoadSD()){
        SessionBank &b = pstore.GetSettings();
        sess_exists = b.exists;
        sess_cur    = (b.active < 16) ? b.active : 0;
        persistApply(b.s[sess_cur]);
        pstore.Save();                // mirror SD → QSPI so they agree
    }
    // v2 2026-09-13: DISABLED. This planted a hardcoded demo beat (kick on
    // 1/2/3/4, snare backbeat, 8th hats) into the first empty slot on EVERY
    // boot -- which is where the phantom extra patches came from. It also set
    // seq_mode[ENG_DRUM] = SEQ_MACHINE ("drum pattern actually plays on load")
    // and beat.menu = MENU_DRUM, so tapping that patch started a beat AND
    // threw you off the patches page. Clean slate means no demo content.
    // Re-enable by uncommenting if you ever want the example back.
    // plantExampleBeatIfRoomExists();
    rebuildScale();                   // build the live scale from key + mode
    audLoad(sess_cur);                // restore the active patch's sampler audio
    fxaLoad(sess_cur);                // restore all-bank FX automation from SD
    seqaLoad(sess_cur);               // restore the full sequence + song length

    // zero every FX delay/reverb buffer — SDRAM isn't cleared at reset, so
    // un-cleared buffers read as garbage = loud static the moment an FX engages
    memset(rbufL,0,sizeof(rbufL));   memset(rbufR,0,sizeof(rbufR));
    memset(f2dub,0,sizeof(f2dub));   memset(f2cmb,0,sizeof(f2cmb));
    memset(f3plt,0,sizeof(f3plt));   memset(f3pngL,0,sizeof(f3pngL)); memset(f3pngR,0,sizeof(f3pngR));
    memset(f3slap,0,sizeof(f3slap)); memset(f4rep,0,sizeof(f4rep));   memset(f4mtl,0,sizeof(f4mtl));
    memset(f5repL,0,sizeof(f5repL)); memset(f5repR,0,sizeof(f5repR));
    memset(f5scrL,0,sizeof(f5scrL)); memset(f5scrR,0,sizeof(f5scrR));
    memset(f6dL,0,sizeof(f6dL));     memset(f6dR,0,sizeof(f6dR));
    memset(f6ppL,0,sizeof(f6ppL));   memset(f6ppR,0,sizeof(f6ppR));
    memset(f6echo,0,sizeof(f6echo)); memset(f6loopL,0,sizeof(f6loopL)); memset(f6loopR,0,sizeof(f6loopR));
    memset(f6pchL,0,sizeof(f6pchL)); memset(f6pchR,0,sizeof(f6pchR));
    memset(f5rpt2,0,sizeof(f5rpt2)); memset(f5fill,0,sizeof(f5fill)); memset(f5bnc,0,sizeof(f5bnc));
    memset(f6fb,0,sizeof(f6fb));     memset(f6dtL,0,sizeof(f6dtL));   memset(f6dtR,0,sizeof(f6dtR));
    memset(f6plfo,0,sizeof(f6plfo)); memset(f6bu,0,sizeof(f6bu));     memset(f6bd,0,sizeof(f6bd));
    memset(f6wrb,0,sizeof(f6wrb));
    buildWavetables();               // generate the band-limited wavetable bank

    hw.SetAudioBlockSize(64);
    hw.StartAudio(AudioCallback);
    PMOR_STAGE(6);

    // NeoTrellis init LAST — prevents SDMMC slowness from resetting freshly-inited seesaw
#ifdef PMOR_OLED_USB_ONLY
    // OLED+USB COEXISTENCE TEST (2026-08-15)
    // The NeoTrellis I2C clock line IS PB5 — the same pin as the OLED's MOSI.
    // Every previous test drove that pin with the trellis bus (pull-ups + the
    // seesaw chip) live on it, so "OLED kills USB" was never actually isolated
    // from "OLED and the trellis bus fight over PB5".
    // Here the trellis is NEVER initialized and never polled, so PB5 belongs to
    // the display alone and can be driven push-pull (see oled_pmor.h).
    //   panel lit + USB alive -> the conflict is PB5 ARBITRATION, not the OLED.
    //                            Fix = own the pin, reconfigure per subsystem.
    //   USB dies anyway       -> the display transaction itself is fatal.
    bool trel_ok = false;
#else
    bool trel_ok = ntInit();
#endif
    PMOR_STAGE(7);

    // ~1s startup: tunnel-dive intro straight into the fractal page.
    // (Peripheral statuses still live on the DASH page if you need them.)
    // Intro gated OFF by default. It is the ONLY thing left between chunk 7
    // (halts here, USB works) and the full build (USB dead) now that the
    // whole main-loop body has been ruled out. My earlier stage-8 test of it
    // was confounded by a diagnostic macro that redrew the screen, so this
    // never got a clean verdict. Enable over CDC with "TIntro 1".
    for(int f = 0; g_en_intro && f < 8; f++) {
        display.Fill(false);
        float prog = (float)f / 19.f;
        for(int k = 0; k < 5; k++) {
            float s = prog * 2.f + (float)k * 0.2f;
            s -= (int)s;
            s *= s;
            int w = 3 + (int)(s * 61.f);
            int h = 2 + (int)(s * 30.f);
            display.DrawRect(64 - w, 32 - h, 63 + w, 31 + h, true, false);
        }
        if(f >= 4) {   // name punches in once the tunnel is moving
            display.SetCursor(25, 29);
            display.WriteString("POCKETGORATOR", Font_6x8, true);
        }
        // Surfaced, not silent: if plantExampleBeatIfRoomExists() found all 16
        // patch slots already occupied (nothing was written — see its comment),
        // say so once during boot instead of quietly doing nothing.
        if(g_example_beat_full && f >= 10) {
            display.SetCursor(4, 44);
            display.WriteString("16/16 PATCHES FULL", Font_6x8, true);
        }
        display.Update();
        // Pump USB immediately after the SPI blit as well as during the
        // frame delay. usbDelay() alone was NOT enough: stage 7 (halt right
        // after ntInit) enumerated fine while stage 8 (halt after this
        // intro) did not, so the stall that kills USB is inside
        // display.Update() itself, not the inter-frame wait.
        tud_task();
        // usbDelay, not System::Delay: this intro runs 20 frames x 30ms =
        // ~600ms AFTER USB has enumerated. A hard spin that long stops
        // tud_task() from servicing control transfers, the host times out,
        // and the device drops off the bus — the same failure ntInit() had.
        usbDelay(30);
    }

    // Light-yellow lava lamp state — slow drift, varied per-pixel speed
    static const uint8_t DRIFT[16]={1,2,1,2, 2,1,2,1, 1,2,2,1, 2,1,1,2};
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    // Per-subsystem state hoisted so they survive across loop iterations
    uint16_t dist=0;
    uint32_t lidar_seen = System::GetNow();
    float    ax=0,ay=0,az=0,gx=0,gy=0,gz=0,mx=0,my=0,mz=0;
    // Mirrored to globals so the CDC monitor page can report the whole IMU,
    // not just the ax value the tilt-filter consumes.
    extern volatile float g_ax,g_ay,g_az,g_gx,g_gy,g_gz;
    uint16_t fsr=0;
    char     e1s[6]="--",e2s[6]="--";
    uint32_t e1until=0,e2until=0;

    // Each subsystem runs on its own timestamp — no blocking Sleep in main loop.
    // NeoTrellis @ ~17Hz (60ms): slower rate reduces I2C noise burst on audio path.
    // OLED @ 20Hz (50ms): display update rate independent of sensor read speed.
    // Sensors @ 50ms: reduced from 20ms to lower I2C EMI energy on audio path.
    uint32_t next_trel = 0;
    uint32_t next_keys = 0;
    uint32_t next_oled = 0;
    int menu_anim = 0, menu_anim_dir = 1;   // dive-transition frames left, direction

    // Stage 8: everything initialised AND the intro animation finished,
    // but the main loop has not started. Splits "intro still breaks USB"
    // from "something in the main loop breaks USB".
    PMOR_STAGE(8);

    while(1)
    {
        uint32_t now = System::GetNow();

        // ── USB tasks + MIDI-clock/role engine — control plane only, every
        // iteration, unconditionally (tud_task() must never be rate-limited).
        // gridTick() (audio ISR) remains the ONE tempo authority; this is the
        // sole place step_ms and clock_engine's g_bpm are reconciled — see
        // usb/clock_engine.h "Host-instrument tempo integration".
        tud_task();
        usb_audio_stream_task();   // drain the audio ISR's ring into TinyUSB
                                   // (the ONLY writer of the UAC2 FIFO now)
        PollCdc();        // MUST run before the gate below, or CDC commands
                          // (including the probes and `dfu`) are never parsed
                          // while the loop is gated — which is exactly when
                          // we need them most.
        pmor_probe_tick();
        // Buttonless DFU — checked BEFORE the loop gate, so a quiet-booted
        // (g_en_loop=false) device can always be reflashed over CDC. It used
        // to sit below the gate, where a gated loop would parse `dfu` but
        // never act on it.
        if(clock_engine_dfu_requested()) {
            System::Delay(50);
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
        }
        if(!g_en_loop) { continue; }   // USB-only pass; nothing else runs
        if(clock_engine_get_role() == CLOCK_ROLE_OPG_MASTER) {
            // We're master (the default): derive BPM from the grid's own
            // step_ms (16th-note grid -> quarter-note BPM) and hand it to
            // clock_engine every pass; master_tick_poll() emits 24 PPQN
            // 0xF8 / Start / Stop / CC116 / CC119 FROM that value alone —
            // clock_engine never runs a second, independent tempo.
            clock_engine_set_local_bpm(60000.0f / ((float)step_ms * 4.0f));
        } else {
            // CC116=2 received: Demiurge is master. clock_engine's follower
            // PLL (unchanged, clock_engine.c) derives BPM from the incoming
            // 0xF8 stream; feed THAT back into step_ms so gridTick() follows
            // the bus. Still exactly one clock — only who steers it changes.
            float bus_bpm = clock_engine_get_bpm();
            if(bus_bpm > 1.f) {
                uint32_t sm = (uint32_t)(60000.0f / (bus_bpm * 4.0f) + 0.5f);
                if(sm < 60)  sm = 60;    // same clamp persistApply() already uses
                if(sm > 500) sm = 500;
                step_ms = sm;
            }
        }
        clock_engine_poll();
        PollCdc();
        // Buttonless DFU (MUST PRESERVE — enclosed device, no physical
        // access to BOOT/RESET without disassembly). Checked every loop
        // iteration, same as the source project's own main().
        if(clock_engine_dfu_requested()) {
            System::Delay(50);
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
        }

        // ── NeoTrellis — own 40ms schedule ──────────────────────────────────
        // Recovery: poll ssR1 every pass when down (~1.3ms overhead); ntSoftInit
        // when SAMD09 comes back after watchdog reset.
        if(!trel_ok) {
            // events are being lost while the trellis is down — release
            // everything so nothing can drone or stay punched in
            if(held_mask || fx_held){ held_mask = 0; fx_held = 0; allKeysOff(); }
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
        if(g_en_trel && trel_ok && now >= next_trel) {
            next_trel = now + 80;   // gentler bus pacing — fewer SAMD09 overruns

            for(uint8_t i=0;i<16;i++) pphase[i]+=DRIFT[i];
            uint8_t grb[48];
            if(vol_mode) {
                // volume fader meter: filled up to the set level (physical
                // TL→BR), green → amber → red past 100%, set pad bright
                for(uint8_t i=0;i<16;i++){
                    int pi = (int)i;   // pad i = level i — same L→R order as the sequencer
                    uint8_t r=0,g=0,b=0;
                    if(pi <= vol_pidx){
                        if(pi <= 9)       { r=30;  g=120; b=20; }  // ≤75%
                        else if(pi <= 12) { r=120; g=90;  b=10; }  // →100%
                        else              { r=160; g=25;  b=10; }  // tan-push zone
                        if(pi == vol_pidx){ r=(uint8_t)(r+60); g=(uint8_t)(g+60); b=(uint8_t)(b+30); }
                    }
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(form_mode && seq_view) {
                // FORM/structure: bars 0..form_bars-1 lit cyan, current bar
                // bright white. Beyond the song length, dark.
                int curbar = (form_blen > 0) ? (gstep / form_blen) : 0;
                for(uint8_t i=0;i<16;i++){
                    uint8_t r=0,g=0,b=0;
                    if((int)i < form_bars){
                        if((int)i == curbar){ r=g=b=200; }
                        else                { r=10; g=90; b=120; }
                    }
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(menu == MENU_FX) {
                // Punch-in FX page: one hue per row, a distinct palette per bank.
                static const uint8_t FXC[8][4][3] = {
                  {{255,215,65},{70,150,255},{155,65,245},{185,65,220}},   // FX1 lava
                  {{40,230,180},{80,255,80},{255,100,200},{255,150,40}},   // FX2 cool
                  {{120,200,255},{60,255,160},{255,80,120},{200,120,255}}, // FX3 spatial
                  {{255,90,40},{255,40,120},{120,255,40},{255,230,40}},    // FX4 glitch
                  {{255,170,0},{255,90,160},{0,200,255},{160,255,90}},     // FX5 PO
                  {{90,160,255},{160,90,255},{0,255,200},{255,120,90}},    // FX6 space
                  {{255,120,200},{120,255,120},{255,200,80},{80,200,255}}, // FX7 perform
                  {{200,80,255},{255,80,80},{80,255,200},{255,255,80}} };  // FX8 midi
                uint16_t fh = (fx_bank==0)?fx_eff:(fx_bank==1)?fx_eff2:(fx_bank==2)?fx_eff3
                              :(fx_bank==3)?fx_eff4:(fx_bank==4)?fx_eff5:(fx_bank==5)?fx_eff6
                              :(fx_bank==6)?fx_eff7:fx_eff8;
                for(uint8_t i=0;i<16;i++){
                    const uint8_t* c = FXC[fx_bank%8][(i>>2)&3];
                    uint8_t r=c[0], g=c[1], b=c[2];
                    if(!((fh >> i) & 1)) { r>>=4; g>>=4; b>>=4; }  // dim idle
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(menu == MENU_SESS) {
                // session slots: existing = soft amber, active = bright white,
                // delete-warning = pulsing red
                for(uint8_t i=0;i<16;i++){
                    uint8_t r=0,g=0,b=0;
                    if((int)i == sess_warn)            { r=255; g=0; b=0; }
                    else if((int)i == sess_cur)        { r=g=b=200; }
                    else if(sess_exists & (1u<<i))     { r=120; g=70; b=10; }
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(eng_focus == ENG_SMP) {
                if(seq_view) {
                    // sampler step display: teal placed steps, white playhead.
                    // The view PAGES with the playhead so long songs scroll bars.
                    int page = gstep >> 4; view_page = page; int sl = songLen();
                    uint8_t* placed = smp_pat[sel_smp & 15];
                    uint8_t sh = seq_place ? 1 : 2;
                    for(uint8_t i=0;i<16;i++){
                        int st = page*16 + i; uint8_t r,g,b;
                        if(st >= sl)            { r=g=b=0; }                   // past song end
                        else if(st == gstep)    { r=g=b=140; }                // playhead
                        else if(seqBit(placed,st)) { r=40>>sh; g=220>>sh; b=170>>sh; }
                        else { lavaColor(pphase[i],r,g,b); r>>=5; g>>=5; b>>=5; }
                        grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                    }
                } else {
                    // slot map: filled = teal, recording = bright red, empty = dim lava
                    for(uint8_t i=0;i<16;i++){
                        uint8_t r,g,b;
                        if(rec_slot == (int)i)   { r=220; g=30;  b=20; }
                        else if(smp_len[i])      { r=10;  g=55;  b=42; }
                        else { lavaColor(pphase[i],r,g,b); r>>=5; g>>=5; b>>=5; }
                        grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                    }
                }
            }
            else if(eng_focus == ENG_DRUM && seq_view) {
                // PO step display: placed steps glow in the write-target's color,
                // playhead sweeps bright white. Brighter while placing.
                static const uint8_t VCOL[4][3] = {
                    {255, 60, 40},   // ch0 kick
                    {200, 60,255},   // ch1 perc (toms/bell/blip)
                    {255,200, 40},   // ch2 snare
                    { 60,220,255},   // ch3 hat
                };
                uint8_t* placed   = dr_pat[sel_drum & 3];
                const uint8_t* c  = VCOL[sel_drum & 3];
                uint8_t sh = seq_place ? 1 : 2;
                int page = gstep >> 4; view_page = page; int sl = songLen();
                for(uint8_t i=0;i<16;i++){
                    int st = page*16 + i;   // pad i = step within the shown bar/page
                    uint8_t r,g,b;
                    if(st >= sl)                  { r=g=b=0; }                 // past song end
                    else if(st == gstep)          { r=g=b=140; }              // playhead
                    else if(seqBit(placed,st))    { r=c[0]>>sh; g=c[1]>>sh; b=c[2]>>sh; }
                    else { lavaColor(pphase[i],r,g,b); r>>=5; g>>=5; b>>=5; }  // dim idle
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(eng_focus == ENG_SYNTH && seq_view) {
                // Synth note editor: pads are the 16 notes. Armed (to-place) notes
                // glow white; notes on the edit step show in synth blue; notes the
                // playhead is sounding flash bright.
                uint16_t atEdit = syn_seq[seq_edit % MAXSTEPS];
                uint16_t atPlay = syn_seq[gstep % MAXSTEPS];
                for(uint8_t i=0;i<16;i++){
                    uint8_t r,g,b;
                    if((syn_arm>>i)&1u)      { r=200; g=200; b=255; }   // armed
                    else if((atPlay>>i)&1u)  { r=120; g=180; b=255; }   // sounding now
                    else if((atEdit>>i)&1u)  { r=20;  g=40;  b=120; }   // on the edit step
                    else { lavaColor(pphase[i],r,g,b); r>>=5; g>>=5; b>>=5; }
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else for(uint8_t i=0;i<16;i++){
                uint8_t r,g,b;
                lavaColor(pphase[i],r,g,b);
                if((held_mask>>i)&1) {
                    // pressed base = ~6% (2× idle); FSR 1200→2600 ramps to full
                    if(fsr >= 2600) {
                        // full — no change
                    } else if(fsr <= 1200) {
                        r>>=2; g>>=2; b>>=2;  // ~25%
                    } else {
                        uint32_t t = (uint32_t)(fsr - 1200); // 0..1400
                        r = (uint8_t)((uint32_t)r/4 + ((uint32_t)r - (uint32_t)r/4)*t/1400);
                        g = (uint8_t)((uint32_t)g/4 + ((uint32_t)g - (uint32_t)g/4)*t/1400);
                        b = (uint8_t)((uint32_t)b/4 + ((uint32_t)b - (uint32_t)b/4)*t/1400);
                    }
                } else {
                    r>>=5; g>>=5; b>>=5;  // idle ~3%
                }
                grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
            }

            // global brightness -30% — power headroom on the 5V rail
            for(int gi = 0; gi < 48; gi++) grb[gi] = (uint8_t)((grb[gi] * 179) >> 8);

            // only send frames that actually changed — most are identical in
            // seq/FX views, so this slashes I2C traffic (= fewer SAMD09 upsets)
            static uint8_t grb_prev[48] = {1};
            bool lchg = (memcmp(grb, grb_prev, 48) != 0);
            if(lchg) memcpy(grb_prev, grb, 48);
            bool show_ok = lchg ? ntShow(grb) : true;
            if(!show_ok) {
                trel_ok = false;
            } else if(lchg) {
                // After SHOW, SAMD09 drives WS2812B for ~530µs with I2C deaf —
                // push the next keypad poll past that window.
                System::Delay(1);
                next_keys = now + 3;
            }
        }

        // ── Keypad poll — DECOUPLED from LED frames, every 20ms ─────────────
        // Pad→sound latency was tied to the 80ms LED tick (up to ~80ms felt
        // lag on drums); polling on its own fast schedule cuts that ~4x.
        tud_task();   // service USB before NeoTrellis key FIFO read (bit-banged I2C + 500us waits)
        if(g_en_keys && trel_ok && now >= next_keys) {
            next_keys = now + 20;
            {
                // Mask the OTG interrupt across the whole seesaw read
                // transaction (2026-08-16): keys-on kills USB in 40-120s
                // (rounds A/B: LED writes 300s clean, key reads dead at
                // 73s). Whatever the wedge mechanism is, the OTG ISR can't
                // interleave with a seesaw read while masked. ~1.5ms per
                // poll at 50Hz; the 16-packet audio FIFO rides it.
                uint8_t kcount = tbus.ssR1(TREL,0x10,0x04);
                uint8_t toread = 0;
                uint8_t kbuf[8];
                if(!tbus.stretch_err && kcount > 0 && kcount < 32) {
                    toread = (kcount > 8) ? 8 : kcount;
                    tbus.ssRN(TREL,0x10,0x10,kbuf,toread);
                }
                if(tbus.stretch_err) {
                    // Seesaw held the clock past 5ms: the bus may be
                    // desynced. Recover it, drop this poll's data, and
                    // back off 250ms so a wedged slave gets air.
                    tbus.stretch_err = false;
                    tbus.recover();
                    toread = 0;
                    next_keys = now + 250;
                }
                if(toread) {
                    for(uint8_t i=0;i<toread;i++){
                        // NUM field is seesaw GPIO pin — convert to logical key 0-15
                        // NEO_TRELLIS_SEESAW_KEY: (pin/8)*4 + (pin%8)
                        uint8_t seesaw_pin = kbuf[i] >> 2;
                        uint8_t key  = (seesaw_pin/8)*4 + (seesaw_pin%8);
                        uint8_t edge = kbuf[i] & 0x03;
                        if(key < 16) {
                            if(edge == 3) {                          // EDGE_RISING = pressed
                                last_enc = now;   // any pad hit wakes from the screensaver
                                held_mask |=  (1u << key);
                                if(vol_mode) {
                                    // pads = volume fader, top-left → bottom-right
                                    vol_pidx = (int)key;
                                    mvol     = kVolTable[vol_pidx];
                                    markDirty();
                                }
                                else if(menu == MENU_FX) {
                                    // FX page: punch-in effects. Bank 0 = FX1,
                                    // bank 1 = FX2 (right encoder switches bank).
                                    if(fx_bank == 0) { fx_held |= (1u << key); fx_t[key] = now; }
                                    else if(fx_bank == 1) fx2_held |= (1u << key);
                                    else if(fx_bank == 2) fx3_held |= (1u << key);
                                    else if(fx_bank == 3) fx4_held |= (1u << key);
                                    else if(fx_bank == 4) fx5_held |= (1u << key);
                                    else if(fx_bank == 5) fx6_held |= (1u << key);
                                    else if(fx_bank == 6) fx7_held |= (1u << key);
                                    else                  fx8_held |= (1u << key);
                                }
                                else if(menu == MENU_SESS) {
                                    sess_press[key] = now;   // tap=load, hold=new/delete
                                }
                                else if(form_mode && (eng_focus == ENG_DRUM || eng_focus == ENG_SMP)) {
                                    // FORM mode: pad N = song length of N+1 bars
                                    // (contiguous — clicking fills all before it)
                                    form_bars = (int)key + 1;
                                    markDirty();
                                }
                                else {
                                bool synEdit = (eng_focus == ENG_SYNTH &&
                                                seq_mode[ENG_SYNTH] == SYN_SEQ && seq_view);
                                if(seq_place && eng_focus != ENG_SYNTH) {
                                    // place mode: pads = the 16 steps of the shown page
                                    int st = view_page*16 + (int)key;
                                    if(st < songLen()){
                                        if(eng_focus == ENG_DRUM) seqTog(dr_pat[sel_drum & 3],  st);
                                        else                      seqTog(smp_pat[sel_smp & 15], st);
                                        markDirty();
                                    }
                                }
                                else if(eng_focus == ENG_SYNTH) {
                                    keyNoteOn(key);              // always playable / audition
                                    if(synEdit) syn_arm ^= (uint16_t)(1u << key);  // build the chord to place
                                }
                                else if(eng_focus == ENG_SMP) {
                                    smp_press[key] = now;        // hold→rec / click→play on release
                                }
                                else {
                                    drumPadHit(key);             // play live + remember write target
                                    sel_drum = 3 - (key/4);
                                    dr_pad[3 - (key/4)] = key;
                                    markDirty();
                                }
                                }
                            } else if(edge == 2) {                   // EDGE_FALLING = released
                                held_mask &= ~(1u << key);
                                fx_held   &= ~(1u << key);  // FX off on release (any page — defensive)
                                fx2_held  &= ~(1u << key);
                                fx3_held  &= ~(1u << key);
                                fx4_held  &= ~(1u << key);
                                fx5_held  &= ~(1u << key);
                                fx6_held  &= ~(1u << key);
                                fx7_held  &= ~(1u << key);
                                fx8_held  &= ~(1u << key);
                                keyNoteOff(key);  // no-op unless key holds a manual synth note
                                // SESSIONS: short tap = load that session (if it
                                // exists); long-holds handled by the watcher below.
                                if(menu == MENU_SESS && sess_press[key]){
                                    uint32_t held = now - sess_press[key];
                                    if(held < 800 && (sess_exists & (1u << key)))
                                        sessionLoad(key);
                                    sess_press[key] = 0;
                                }
                                // sampler: end recording / fire a short-press play
                                if(rec_slot == (int)key) {
                                    smp_len[key] = rec_pos;
                                    rec_slot = -1;
                                    sel_smp = (int)key;
                                    aud_dirty = true;   // queue background SD save
                                } else if(smp_press[key]) {
                                    if(smp_len[key]) smp_trig[key]++;
                                    sel_smp = (int)key;
                                }
                                smp_press[key] = 0;
                            }
                        }
                    }
                }
            }
        }

        // (grid clock now lives in gridTick(), called from the audio callback)

        // ── Encoders → menus ────────────────────────────────────────────────
        // enc1 (left)  = primary: cycles DASH / FRACTAL / MODE with dive animation.
        // enc2 (right) = param: active on MODE page only — gen/play toggle, click = tap tempo.
        // Accumulate ±2 counts per step so one detent ≈ one action, not several.
        now = System::GetNow();
        {
            static int      acc1 = 0, acc2 = 0;
            static uint32_t tap_prev = 0, tap_gaps[4] = {};
            static int      tap_n = 0, tap_i = 0;

            int  d1 = enc1.delta;   enc1.delta = 0;
            int  d2 = enc2.delta;   enc2.delta = 0;
            bool c1 = enc1.clicked; enc1.clicked = false;
            bool c2 = enc2.clicked; enc2.clicked = false;
            if(d1 != 0 || d2 != 0 || c1 || c2 || enc1.psw == 0 || enc2.psw == 0)
                last_enc = now;   // screensaver wake

            // ── Both-buttons-held = toggle master volume mode ───────────────
            // Clicks are deferred 130ms so the two presses of the gesture never
            // fire as single clicks first (tap-tempo intervals are unaffected —
            // every tap shifts by the same delay).
            bool doClear = false;   // set by a LEFT-button hold (≥700ms)
            {
                static uint32_t c1_pend = 0, c2_pend = 0;
                static bool both_latch = false;
                if(c1){ c1_pend = now; c1 = false; }
                if(c2){ c2_pend = now; c2 = false; }
                bool h1 = (enc1.psw == 0), h2 = (enc2.psw == 0);
                if(h1 && h2 && !both_latch){
                    both_latch = true;
                    vol_mode   = !vol_mode;
                    c1_pend = c2_pend = 0;          // swallow the gesture's clicks
                }
                if(!h1 && !h2) both_latch = false;
                if(both_latch){ c1_pend = c2_pend = 0; }
                // LEFT button grammar: a quick release = BACK; a hold ≥700ms = CLEAR
                // (context-aware), and the back-click is swallowed so a hold never
                // also backs out. Right button keeps its 130ms debounce.
                static bool c1_cleared = false;
                if(c1_pend && c1_cleared && !h1){ c1_pend = 0; c1_cleared = false; }
                else if(c1_pend){
                    if(h1 && !c1_cleared && now - c1_pend >= 700){ doClear = true; c1_cleared = true; }
                    else if(!h1){ c1 = true; c1_pend = 0; }
                }
                if(c2_pend && now - c2_pend >= 130){ c2 = true; c2_pend = 0; }
            }
            if(vol_mode){ d1 = 0; d2 = 0; c1 = false; c2 = false; }  // modal overlay

            // dashboard activity strings (unchanged behavior)
            if(d1>0)      { sprintf(e1s,">");   e1until=now+250; }
            else if(d1<0) { sprintf(e1s,"<");   e1until=now+250; }
            if(c1)        { sprintf(e1s,"CLK"); e1until=now+250; }
            if(now>=e1until) sprintf(e1s,"--");
            if(d2>0)      { sprintf(e2s,">");   e2until=now+250; }
            else if(d2<0) { sprintf(e2s,"<");   e2until=now+250; }
            if(c2)        { sprintf(e2s,"CLK"); e2until=now+250; }
            if(now>=e2until) sprintf(e2s,"--");

            acc1 += d1;
            if(acc1 >= 4 || acc1 <= -4) {
                menu_anim = 6; menu_anim_dir = (acc1 > 0) ? +1 : -1;
                menu = (menu + (acc1 > 0 ? 1 : MENU_COUNT-1)) % MENU_COUNT;
                acc1 = 0;
                seq_view = false; seq_place = false; param_edit = false; mode_cur = 0;
                form_mode = false;
                fx_held=0; fx2_held=0; fx3_held=0; fx4_held=0; fx5_held=0; fx6_held=0; fx7_held=0; fx8_held=0; // kill on rotate
                markDirty();   // menu position persists
                if(menu == MENU_SYNTH) eng_focus = ENG_SYNTH;        // pads follow page
                if(menu == MENU_DRUM)  eng_focus = ENG_DRUM;
                if(menu == MENU_SMP)   eng_focus = ENG_SMP;
            }

            // LEFT-HOLD (≥700ms) = CLEAR, context-aware and deliberate so nothing is
            // ever wiped by an accidental tap.
            if(doClear) {
                if(menu == MENU_FX) {                            // wipe all FX automation
                    for(int b2=0;b2<8;b2++) for(int a=0;a<16;a++) fx_auto[b2][a]=0;
                    for(int a=0;a<16;a++) fx_auto_fsr[a]=0;
                }
                else if(menu == MENU_SYNTH && seq_mode[ENG_SYNTH] == SYN_SEQ) {
                    for(int i=0;i<MAXSTEPS;i++) syn_seq[i] = 0; syn_arm = 0;   // clear synth seq
                }
                else if(seq_view && eng_focus == ENG_DRUM) {     // clear this drum voice
                    for(int j=0;j<16;j++) dr_pat[sel_drum & 3][j] = 0;
                }
                else if(seq_view && eng_focus == ENG_SMP) {      // clear this sample slot
                    for(int j=0;j<16;j++) smp_pat[sel_smp & 15][j] = 0;
                }
                markDirty();
            }
            // LEFT-CLICK = BACK / exit one level. NEVER destroys data.
            else if(c1) {
                if(param_edit)         param_edit = false;       // exit value edit
                else if(form_mode)     form_mode = false;        // exit song-form
                else if(seq_view)      { seq_view = false; seq_place = false; }  // exit step editor
                else { /* top level: menus are the LEFT-turn */ }
            }

            bool eng_page = (menu == MENU_SYNTH || menu == MENU_DRUM || menu == MENU_SMP);
            if((menu == MENU_DRUM || menu == MENU_SMP) && seq_view && form_mode) {
                // FORM mode: R-turn sets beats-per-bar (meter); pads pick bars.
                acc2 += d2;
                if(acc2 >= 4 || acc2 <= -4) {
                    form_blen += (acc2 > 0) ? 1 : -1;
                    if(form_blen < 1) form_blen = 1; if(form_blen > 16) form_blen = 16;
                    markDirty(); acc2 = 0;
                }
            }
            else if((menu == MENU_DRUM || menu == MENU_SMP) && seq_view) {
                // Sequencer submenu: R-turn cycles the write target,
                // R-click toggles place mode (pads = 16 steps).
                acc2 += d2;
                if(acc2 >= 4 || acc2 <= -4) {
                    int dir = (acc2 > 0) ? 1 : -1;
                    if(menu == MENU_DRUM) sel_drum = (sel_drum + 4 + dir) % 4;
                    else                  sel_smp  = (sel_smp + 16 + dir) & 15;
                    markDirty();
                    acc2 = 0;
                }
                if(c2) seq_place = !seq_place;
            }
            else if(menu == MENU_SYNTH && seq_view) {
                // Synth step editor: R-turn moves the edit cursor; R-click places
                // the armed chord at the cursor and advances. (Holding R writes the
                // played notes live, handled at the step boundary in gridTick.)
                acc2 += d2;
                if(acc2 >= 4 || acc2 <= -4) {
                    int dir = (acc2 > 0) ? 1 : -1; int sl = songLen();
                    seq_edit = (seq_edit + sl + dir) % sl; acc2 = 0;
                }
                if(c2 && enc2.psw != 0) {       // a real click, not the start of a record-hold
                    int sl = songLen();
                    syn_seq[seq_edit % MAXSTEPS] = syn_arm;     // place the chord (0 = rest)
                    seq_edit = (seq_edit + 1) % sl; syn_arm = 0; markDirty();
                }
            }
            else if(eng_page) {
                // Cursor layout: [MODES][params...][TAP]. SYNTH has 3 modes
                // (SEQ/ARP/GEN); DRUM & SMP have ONE unified mode (its tile opens
                // the step sequencer).
                //   SYNTH params (4): WAVE, DENS(g_csyn), POLY(g_cpoly), VOL(g_vsyn)
                //   DRUM  params (3): KIT, CHANCE(g_cdrum), VOL(g_vdrum)
                //   SMP   params (2): CHANCE(g_csmp), VOL(g_vsmp)
                int nmodes = (menu == MENU_SYNTH) ? 3 : 1;
                // engine params + 2 shared song params (BARS, BEATS)
                int nparam = ((menu == MENU_SYNTH) ? 6 : (menu == MENU_DRUM) ? 3 : 2) + 2;
                int nit    = nmodes + nparam + 1;   // modes + params + TAP
                int tapidx = nit - 1;
                acc2 += d2;
                int n2 = (acc2 >= 4) ? 1 : (acc2 <= -4) ? -1 : 0;
                if(n2){
                    acc2 = 0;
                    if(param_edit && mode_cur >= nmodes && mode_cur < tapidx){
                        engAdj(menu, mode_cur - nmodes, n2); markDirty();
                    } else {
                        mode_cur = (mode_cur + nit + n2) % nit;
                    }
                }
                if(c2 && mode_cur < nmodes) {        // mode tile
                    if(menu == MENU_SYNTH){
                        if(seq_mode[ENG_SYNTH] != mode_cur){ seq_mode[ENG_SYNTH] = mode_cur; allKeysOff(); markDirty(); }
                        else if(mode_cur == SYN_SEQ){ seq_view = true; seq_place = false; }  // re-click SEQ → editor
                    } else {
                        // DRUM/SMP single tile → open the step sequencer grid
                        seq_view = true; seq_place = false;
                    }
                }
                else if(c2 && mode_cur >= nmodes && mode_cur < tapidx) {  // enter/exit param edit
                    param_edit = !param_edit;
                }
                else if(c2) {                       // TAP tempo (cursor on TAP)
                    // Tap tempo: taps are quarter notes, grid is 16ths → step = gap/4.
                    // Average of last ≤4 taps; out-of-range gap restarts the window.
                    uint32_t g = now - tap_prev; tap_prev = now;
                    if(g >= 240 && g <= 2000) {
                        tap_gaps[tap_i] = g; tap_i = (tap_i+1)&3; if(tap_n<4) tap_n++;
                        uint32_t s=0; for(int i=0;i<tap_n;i++) s += tap_gaps[i];
                        uint32_t st = (s/tap_n)/4;
                        if(st < 60)  st = 60;   // 250 BPM cap
                        if(st > 500) st = 500;  // 30 BPM floor
                        step_ms = st;
                        markDirty();
                    } else { tap_n = 0; tap_i = 0; }
                }
            }
            else if(menu == MENU_FX) {
                // right encoder flips FX bank (FX1 ↔ FX2); clears held punch-ins
                acc2 += d2;
                if(acc2 >= 4 || acc2 <= -4) { fx_bank = (fx_bank + 8 + ((acc2>0)?1:-1)) % 8;
                    fx_held=0; fx2_held=0; fx3_held=0; fx4_held=0; fx5_held=0; fx6_held=0; fx7_held=0; fx8_held=0; acc2 = 0; }
            }
            else if(menu == MENU_GLOB) {
                // 4 params: BPM KEY MODE SWING. (chance / wave / kit live on the
                // engine pages now.) R-turn = cursor, click = edit, turn = value.
                acc2 += d2;
                int n2 = (acc2 >= 4) ? 1 : (acc2 <= -4) ? -1 : 0;
                if(n2){
                    acc2 = 0;
                    if(param_edit){ globAdj(mode_cur, n2); markDirty(); }
                    else mode_cur = (mode_cur + 4 + n2) % 4;
                }
                if(c2) param_edit = !param_edit;
            }
            else acc2 = 0;
        }

        // ── Sensors — 50ms rate limit to reduce I2C noise on audio path ────
        {
            static uint32_t next_sensor = 0;
            if(now >= next_sensor) {
                next_sensor = now + 50;
                if(imu_ok) {
                    ax=sbus.r16be(0x68,0x3B)/16384.f;
                    ay=sbus.r16be(0x68,0x3D)/16384.f;
                    az=sbus.r16be(0x68,0x3F)/16384.f;
                    gx=sbus.r16be(0x68,0x43)/131.f;
                    gy=sbus.r16be(0x68,0x45)/131.f;
                    gz=sbus.r16be(0x68,0x47)/131.f;
                    g_ax=ax; g_ay=ay; g_az=az; g_gx=gx; g_gy=gy; g_gz=gz;
                }
                if(mag_ok && (sbus.r8(0x0C,0x02)&0x01)) {
                    mx=sbus.r16be(0x0C,0x03)*0.15f;
                    my=sbus.r16be(0x0C,0x05)*0.15f;
                    mz=sbus.r16be(0x0C,0x07)*0.15f;
                    sbus.r8(0x0C,0x09);
                }
                if(lidar_ok && lidar.dataReady()) { dist = lidar.read(); lidar_seen = now; }
                { float L = (float)dist / 600.f; if(L>1.f)L=1.f; fx_lid = L; }  // FX2 macro

                // ── sensor watchdog — PROBE-ONLY in normal operation (2 cheap
                // reads per second). Reinit fires only after a sensor has been
                // LOST >1s (two consecutive failed probes). Lidar reinit (a
                // long blocking call) only if it was up, silent >5s, and at
                // least 10s since the last attempt — never an init storm.
                {
                    static uint32_t next_schk = 0;
                    static uint8_t  imu_bad = 0, mag_bad = 0;
                    static uint32_t lidar_retry = 0;
                    if(now >= next_schk) {
                        next_schk = now + 1000;
                        uint8_t id = sbus.r8(0x68, 0x75);
                        if(id==0x71||id==0x73||id==0x70||id==0x68||id==0x69||id==0x98) { imu_bad = 0; imu_ok = true; }
                        else if(++imu_bad >= 2) {
                            sbus.w8(0x68, 0x6B, 0x01);
                            sbus.w8(0x68, 0x37, 0x02);
                            sbus.w8(0x0C, 0x0A, 0x16);
                            id = sbus.r8(0x68, 0x75);
                            imu_ok = (id==0x71||id==0x73||id==0x70||id==0x68||id==0x69||id==0x98);
                            if(imu_ok) imu_bad = 0;
                        }
                        uint8_t wia = sbus.r8(0x0C, 0x00);
                        if(wia == 0x48) { mag_bad = 0; mag_ok = true; }
                        else if(++mag_bad >= 2) {
                            sbus.w8(0x0C, 0x0A, 0x00);
                            sbus.w8(0x0C, 0x0A, 0x16);
                            mag_ok = (sbus.r8(0x0C, 0x00) == 0x48);
                            if(mag_ok) mag_bad = 0;
                        }
                        // ONE re-init attempt, ever. Against an unresponsive
                        // sensor a full init is seconds of I2C timeouts; the
                        // old every-10s retry put a multi-second hole in the
                        // main loop forever (13s MIDI-clock gap measured
                        // 2026-08-16). A sensor that failed twice isn't
                        // coming back without a power cycle anyway.
                        static uint8_t lidar_reinits = 0;
                        if(lidar_ok && now - lidar_seen > 5000 && now >= lidar_retry) {
                            lidar_retry = now + 10000;
                            if(lidar_reinits++ >= 1) { lidar_ok = false; }
                            else {
                                lidar_ok = lidar.init();
                                if(lidar_ok) { lidar.startContinuous(100); lidar_seen = now; }
                            }
                        }
                    }
                }

                // ax controls SVF cutoff across nearly the full tilt range:
                // open at ax≤-0.2, closed at ax≥+0.9 — 1.1g span (old: 0.6g), wider sweep.
                // ax lowpassed here + per-sample slew in audio callback = smooth, no steps.
                // The accel TILT-FILTER only sweeps while you're on the FX page —
                // elsewhere it FREEZES at wherever it was when you left FX, so
                // tilting the unit during normal play never moves the filter.
                if(menu == MENU_FX) {
                    static float axf = 0.f;
                    axf += (ax - axf) * 0.35f;        // settles in ~3 reads @50ms
                    float t = (0.9f - axf) / 1.1f;    // ax=-0.2→1(open) … +0.9→0(closed)
                    if(t > 1.f) t = 1.f;
                    if(t < 0.f) t = 0.f;
                    float fc = 500.f * powf(10.f, t); // 500Hz..5kHz log sweep
                    filter_f = 2.f * sinf(3.14159265f * fc / 48000.f);
                    cut_t    = t; // visualizer reads this
                }
                // Lidar → octave zones (mm), close-in scaling:
                //   0/invalid → base | <150 → base | 150-300 → +1 | 300-450 → +2
                //   450-600 → +3 | 600-750 → +3 held (~0.5ft grace)
                //   >750 → base (hand gone — reset to lowest octave)
                {
                    if(dist == 0 || dist > 750) {
                        oct_shift = 0;
                    } else {
                        int zt = (dist < 150) ? 0 : (dist < 300) ? 1 : (dist < 450) ? 2 : 3;
                        // 20mm hysteresis so the octave doesn't flutter at a zone edge
                        static const uint16_t zlo[4] = {0, 150, 300, 450};
                        static const uint16_t zhi[4] = {150, 300, 450, 750};
                        if(zt > oct_shift && dist >= zlo[zt] + 20)      oct_shift = zt;
                        else if(zt < oct_shift && dist <= zhi[zt] - 20) oct_shift = zt;
                    }
                }
            }
        }
        // FSR: 8-sample circular median, sampled at 10ms intervals.
        // Gating prevents all 8 samples from landing in the same glitch window,
        // and median-of-8 requires 4+ simultaneous spikes to corrupt the output.
        {
            static uint16_t fsrbuf[8]={}; static uint8_t fsridx=0;
            static uint32_t fsr_next=0;
            if(System::GetNow() >= fsr_next) {
                fsr_next = System::GetNow() + 10;
                fsrbuf[fsridx]=(uint16_t)(hw.adc.GetFloat(0)*4095.f);
                fsridx=(fsridx+1)%8;
            }
            // Median of 8: copy, insertion-sort, take middle two average
            uint16_t s[8]; for(int i=0;i<8;i++) s[i]=fsrbuf[i];
            for(int i=1;i<8;i++){uint16_t k=s[i];int j=i-1;while(j>=0&&s[j]>k){s[j+1]=s[j];j--;}s[j+1]=k;}
            fsr=(uint16_t)(((uint32_t)s[3]+s[4])>>1);
            // v2: the old fixed 400..2600 linear window was tuned for the v1
            // 10k pulldown. With the 330R pulldown + big panel FSR the raw
            // values sit compressed outside that range, so it snapped
            // low-to-high with nothing usable in between.
            //
            // FsrScale auto-calibrates to whatever this sensor actually
            // produces and applies the cubed curve (an FSR is ~1/force, so
            // linear always feels like a hair trigger). See _fsr_scale.h.
            fx_fsr = g_fsr.Process(fsr);
        }
        // SESSIONS — long-hold gestures (3s new on empty / 10s delete existing,
        // warning shown the last 5s). Runs in the control plane, never audio.
        sess_warn = -1;
        if(menu == MENU_SESS){
            for(int k=0;k<16;k++){
                if(!sess_press[k]) continue;
                uint32_t held = now - sess_press[k];
                bool exist = (sess_exists >> k) & 1;
                if(!exist && held > 3000){ sessionNew(k); sess_press[k] = 0; }
                else if(exist && k != 0){
                    if(held > 5000 && held < 10000) sess_warn = k;
                    if(held >= 10000){ sessionDelete(k); sess_press[k] = 0; }
                }
            }
        } else for(int k=0;k<16;k++) sess_press[k] = 0;

        // debounced auto-save → QSPI (sequences + settings, per session)
        if(save_at && now >= save_at) {
            save_at = 0;
            sessionStash();      // live state → active slot + exists/active
            pstore.Save();       // brief QSPI write (control plane only)
            patchSaveSD();       // durable mirror to SD (survives reflash)
            fxaSave(sess_cur);   // all-bank FX automation → SD (survives reboot)
            seqaSave(sess_cur);  // full sequence + song length → SD
            save_state = 0;      // top-right icon back to "saved"
        }

        // sampler audio → SD: background chunked save (a slice per loop pass so
        // it never glitches playback). Starts when audio is dirty and idle.
        if(aud_dirty && !aud_saving && rec_slot < 0) {
            if(audBeginSave(sess_cur)) aud_dirty = false;
        }
        audSaveTick();

        // ── OLED — menu pages @20Hz ──────────────────────────────────────────
        now = System::GetNow();
        tud_task();   // service USB before OLED blit (8 blocking SPI page transfers)
        if(g_en_oled && now >= next_oled) {
            // 15fps active / 5fps idle. The old 8/4fps throttle existed to
            // protect USB audio from blit CPU time; the SPSC ring + main-loop
            // drain (usb_audio_stream_task) made that moot — the audio ISR
            // no longer competes with TinyUSB over a FIFO during blits.
            next_oled = now + ((now - last_enc > 25000) ? 200 : 66);
            // Refresh slowed from 20fps. Soft-SPI bit-bangs ~1KB per frame and
            // that CPU time comes straight out of USB audio's buffer servicing
            // -- at 20fps the stream underran audibly (glitchy). The display is
            // a status readout, not animation; 8fps reads fine and leaves the
            // audio clean. Tune here if you want smoother vs cleaner.
            display.Fill(false);
            bool saver = (now - last_enc > 25000) && !vol_mode && menu_anim == 0;

            if(vol_mode) {   // modal master-volume overlay
                display.SetCursor(31, 6); display.WriteString("MASTER VOL", Font_6x8, true);
                int pct = (vol_pidx * 125 + 7) / 15;
                sprintf(row, "%d%%", pct);
                display.SetCursor(64 - (int)strlen(row)*3, 20);
                display.WriteString(row, Font_6x8, true);
                // fat bar: 0..125%, tick at 100%
                int bw = (vol_pidx * 124) / 15;
                display.DrawRect(2, 32, 126, 48, true, false);
                if(bw > 0) display.DrawRect(2, 32, 2 + bw, 48, true, true);
                display.DrawRect(101, 29, 101, 51, true, true);   // 100% tick
                display.SetCursor(7, 56);
                display.WriteString(pct > 100 ? "TAN ZONE  BOTH=EXIT" : "PADS SET  BOTH=EXIT", Font_6x8, true);
            }
            else if(menu_anim > 0) {
                // Dive transition: tunnel of nested rects zooming through the screen
                // (+1 = diving in toward next menu, -1 = pulling back out)
                menu_anim--;
                float prog = 1.f - (float)menu_anim / 6.f;
                for(int k = 0; k < 5; k++) {
                    float s = prog * 0.99f + (float)k * 0.2f;
                    s -= (int)s;                         // wrap to 0..1
                    if(menu_anim_dir < 0) s = 1.f - s;
                    s *= s;                              // ease: fast near center, slow at edge
                    int w = 3 + (int)(s * 61.f);
                    int h = 2 + (int)(s * 30.f);
                    display.DrawRect(64 - w, 32 - h, 63 + w, 31 + h, true, false);
                }
                static const char* MN[MENU_COUNT] = {"SYNTH","DRUMS","SAMPLER","FX","PATCHES","DASH","GLOBALS"};
                display.SetCursor(64 - (int)strlen(MN[menu]) * 3, 29);
                display.WriteString(MN[menu], Font_6x8, true);
            }
            else if(menu == MENU_DASH && !saver) {
                // Full sensor dashboard (restored from backup layout)
                float amp = out_amp;
                auto fmtf=[](char* o, float v)->int{
                    char s=(v<0.f)?'-':'+'; float a=(v<0.f)?-v:v;
                    int w=(int)a; int f=(int)((a-w)*10.f+.5f);
                    if(f>=10){w++;f=0;} return sprintf(o,"%c%d.%d",s,w,f);
                };
                int n=0;
                n+=sprintf(row+n,"A:"); n+=fmtf(row+n,ax); row[n++]=' ';
                n+=fmtf(row+n,ay); row[n++]=' '; n+=fmtf(row+n,az); row[n]=0; orow(0,row);
                n=0;
                n+=sprintf(row+n,"G:"); n+=fmtf(row+n,gx); row[n++]=' ';
                n+=fmtf(row+n,gy); row[n++]=' '; n+=fmtf(row+n,gz); row[n]=0; orow(9,row);
                // v2: the MPU6050 has no magnetometer, so this row showed
                // three zeroes forever. Joystick goes here instead: real X/Y
                // deflection (-1000..+1000) and the click state.
                // Shows SCALED axes plus the RAW ADC counts. If raw sits at a
                // fixed number while you move the stick, that axis is not
                // wired / the pot is open. If raw moves but scaled does not,
                // it is the centre calibration.
                sprintf(row,"J%+5d%+5d C%d r%04X/%04X",
                        (int)g_joy_x, (int)g_joy_y, g_joy_sw ? 1 : 0,
                        (unsigned)hw.adc.Get(1), (unsigned)hw.adc.Get(2));
                orow(18,row);
                sprintf(row,"D:%4dmm S:%02X O:+%d",dist,lidar.lastStatus(),oct_shift); orow(27,row);
                sprintf(row,"ENC:%-3s JOY:%-3s",e1s,e2s);              orow(36,row);
                { int f=(int)(amp*19); if(f>19)f=19;
                  row[0]='['; for(int i=0;i<19;i++) row[1+i]=(i<f)?'#':'.'; row[20]=']'; row[21]=0; }
                orow(45,row);
                sprintf(row,"F:%4d SD:%-2s K:%02d :D",fsr,sd_ok?"OK":"--",__builtin_popcount(held_mask));
                orow(54,row);
            }
            else if(saver) {
                // SCREENSAVER — a flowing HARMONOGRAPH: smooth connected Lissajous
                // curves (not scattered pixels) that pulse and bloom with the audio
                // and reshape as you tilt. Two mirrored, counter-rotating loops fill
                // the panel with a single cohesive figure. Any button/pad wakes it.
                static float pe=0.f, sx_a=0.f, sy_a=0.f, sz_a=0.f, gsp=0.f;
                pe   += (out_amp - pe)   * 0.25f;          // audio energy (size pump)
                sx_a += (ax - sx_a) * 0.05f;
                sy_a += (ay - sy_a) * 0.05f;
                sz_a += (az - sz_a) * 0.05f;
                // GYRO (spin), not accel, drives how fast the figure moves
                float gmag = sqrtf(gx*gx + gy*gy + gz*gz) / 250.f; if(gmag > 1.f) gmag = 1.f;
                gsp += (gmag - gsp) * 0.15f;
                static float ph = 0.f; ph += 0.012f + gsp * 0.22f;  // spins faster the more you spin it
                if(ph > 62.831853f) ph -= 62.831853f;
                // frequency ratios from the accelerometer → the figure's whole shape
                float fa = 2.0f + sx_a * 1.6f;
                float fb = 3.0f + sy_a * 1.6f;
                float fc = 4.0f + sz_a * 1.6f;
                float fd = 5.0f - sx_a * 1.4f;
                // VOLUME drives the size — quiet = small, loud blooms out to the edges
                // PIXEL BUDGET CAP (2026-08-16): at R=60 the two loops light
                // a third-to-half of the panel — and half-panel-lit is past
                // the measured OLED-boost load that kills USB (even at min
                // contrast). R≤30 keeps the worst-case figure well under the
                // threshold; this was the "device dies idle after a while"
                // slow killer — it struck whenever audio bloomed the figure.
                float R  = 8.f + pe * 40.f; if(R > 30.f) R = 30.f;
                const int N = 110;
                for(int pass = 0; pass < 2; pass++){
                    float dir = pass ? -1.f : 1.f;         // second loop counter-rotates
                    float rr  = pass ? R * 0.62f : R;
                    int px_p = 0, py_p = 0; bool first = true;
                    for(int i = 0; i <= N; i++){
                        float u = (float)i / (float)N * 6.2831853f;
                        float fx = 64.f + rr * ( sinf(fa*u + ph*dir) + 0.5f*sinf(fc*u - ph*0.7f) );
                        float fy = 32.f + rr * 0.55f * ( sinf(fb*u + ph*1.1f*dir) + 0.5f*sinf(fd*u) );
                        int xi = (int)fx, yi = (int)fy;
                        if(xi < 0) xi = 0; else if(xi > 127) xi = 127;
                        if(yi < 0) yi = 0; else if(yi > 63)  yi = 63;
                        if(!first) display.DrawLine((uint_fast8_t)px_p, (uint_fast8_t)py_p,
                                                    (uint_fast8_t)xi,   (uint_fast8_t)yi, true);
                        px_p = xi; py_p = yi; first = false;
                    }
                }
            }
            else if(seq_view && form_mode) { // SONG FORM editor (LEDs = bars)
                display.SetCursor(34, 0); display.WriteString("SONG FORM", Font_6x8, true);
                sprintf(row, "BEATS/BAR  %2d", form_blen); orow(14, row);
                sprintf(row, "BARS       %2d", form_bars); orow(24, row);
                sprintf(row, "LEN %2d STEPS (=16max)", songLen()); orow(34, row);
                sprintf(row, "BAR %d/%d", (form_blen>0?gstep/form_blen:0)+1, form_bars); orow(44, row);
                orow(54, "RT=BEATS PAD=BARS L=X");
            }
            else if(seq_view && menu == MENU_SMP) { // sampler sequencer — info only
                display.SetCursor(0, 0); display.WriteString("SAMPLE SEQ", Font_6x8, true);
                uint32_t sl = smp_len[sel_smp & 15];
                sprintf(row, "SLOT:%02d  %d.%01ds", (sel_smp & 15) + 1,
                        (int)(sl / 48000u), (int)((sl % 48000u) / 4800u));
                orow(11, row);
                sprintf(row, "%s  STEP %02d", seq_place ? ">PLACING<" : "PLAYING", gstep + 1);
                orow(21, row);
                sprintf(row, "STEPS:%02d", seqCount(smp_pat[sel_smp & 15]));
                orow(31, row);
                sprintf(row, "BPM %3d", (int)(15000u/step_ms));
                orow(41, row);
                sprintf(row, "L=BACK R=%s RT=SLOT", seq_place ? "PLAY" : "PLACE");
                orow(54, row);
            }
            else if(seq_view) { // drum sequencer — LEDs are the grid; OLED = info only
                static const char* CHN[4] = {"KICK","PERC","SNARE","HAT"};
                int sd = sel_drum & 3;
                display.SetCursor(0, 0); display.WriteString("DRUM MACHINE", Font_6x8, true);
                sprintf(row, "SND: %s %s", CHN[sd],
                        (kit_sel ? PADN2 : PADN)[dr_pad[sd] & 15]);
                orow(11, row);
                sprintf(row, "%s  STEP %02d", seq_place ? ">PLACING<" : "PLAYING", gstep + 1);
                orow(21, row);
                sprintf(row, "K:%02d P:%02d S:%02d H:%02d",
                        seqCount(dr_pat[0]), seqCount(dr_pat[1]),
                        seqCount(dr_pat[2]), seqCount(dr_pat[3]));
                orow(31, row);
                sprintf(row, "BPM %3d", (int)(15000u/step_ms));
                orow(41, row);
                sprintf(row, "L=BACK R=%s RT=SND", seq_place ? "PLAY" : "PLACE");
                orow(54, row);
            }
            else if(menu == MENU_SYNTH) { // SYNTH page: modes + WAVE/DENS/POLY + TAP
                display.SetCursor(49, 0); display.WriteString("SYNTH", Font_6x8, true);
                static const char* ML[3] = {"SEQ","ARP","GEN"};
                for(int i = 0; i < 3; i++){
                    int x0 = 2 + i*43;
                    bool act = (seq_mode[ENG_SYNTH] == i);
                    display.DrawRect(x0, 10, x0+39, 20, true, act);
                    display.SetCursor(x0+11, 11); display.WriteString(ML[i], Font_6x8, !act);
                    if(mode_cur == i) display.DrawRect(x0-2, 8, x0+41, 22, true, false);
                }
                if(seq_view && seq_mode[ENG_SYNTH] == SYN_SEQ){
                    int sl = songLen(); int bpb = form_blen > 0 ? form_blen : 16;
                    sprintf(row, "EDIT %02d/%02d BAR%d", seq_edit+1, sl, seq_edit/bpb + 1); orow(25, row);
                    sprintf(row, "PLAY %02d     BAR%d", (gstep%sl)+1, (gstep%sl)/bpb + 1); orow(35, row);
                    sprintf(row, "ARM:%d  HOLD R=REC", __builtin_popcount(syn_arm)); orow(45, row);
                    sprintf(row, "RCLK=PLACE  L=BACK"); orow(55, row);
                } else {
                    // clean single-column param list — selected row is one solid
                    // highlight bar (no glyph soup), scrolls to keep the cursor shown
                    static const char* LB[8] = {"WAVE","DENS","POLY","VOL","SCAN","BARS","BEATS","TAP"};
                    char VL[8][12];
                    sprintf(VL[0], "%s",   WAVE_NAMES[g_wave]);
                    sprintf(VL[1], "%d",   g_csyn);
                    sprintf(VL[2], "%d",   g_cpoly);
                    sprintf(VL[3], "%d%%", g_vsyn*10);
                    sprintf(VL[4], "%d",   g_wtscan);
                    sprintf(VL[5], "%d",   form_bars);
                    sprintf(VL[6], "%d",   form_blen);
                    sprintf(VL[7], "%d",   (int)(15000u/step_ms));
                    int sel = (mode_cur >= 3) ? mode_cur - 3 : -1;   // 0..7, -1 on a mode tile
                    int top = (sel <= 1) ? 0 : (sel >= 6 ? 4 : sel - 1);
                    for(int r=0;r<4;r++){
                        int idx = top + r; if(idx >= 8) break;
                        int y = 25 + r*10; bool s = (idx == sel);
                        if(s) display.DrawRect(0, y-1, 127, y+8, true, true);   // highlight bar
                        char ln[28]; sprintf(ln, " %-6s %s", LB[idx], VL[idx]);
                        display.SetCursor(0, y); display.WriteString(ln, Font_6x8, !s);
                        if(s && param_edit){ display.SetCursor(120, y); display.WriteString("\x10", Font_6x8, !s); }
                    }
                }
            }
            else if(menu == MENU_FX) { // FX page: active effect names + FSR amount
                static const char* FXN[16] = {
                    "STUT 4",  "STUT 8",  "STUT 16", "STUT 32",
                    "REVERSE", "CHORUS",  "PAN",     "CRUSH",
                    "HALF",    "DOUBLE",  "BUZZ",    "MUTE",
                    "FILTER",  "WASH",    "DIST",    "STOP",
                };
                static const char* const* FXNB[8] = { FXN, FX2N, FX3N, FX4N, FX5N, FX6N, FX7N, FX8N };
                const char* const* nm = FXNB[fx_bank % 8];
                display.SetCursor(46, 0);
                sprintf(row, "\x11 FX%d \x10", (fx_bank % 8) + 1);
                display.WriteString(row, Font_6x8, true);
                uint16_t fe = (fx_bank==0)?fx_eff:(fx_bank==1)?fx_eff2:(fx_bank==2)?fx_eff3
                              :(fx_bank==3)?fx_eff4:(fx_bank==4)?fx_eff5:(fx_bank==5)?fx_eff6
                              :(fx_bank==6)?fx_eff7:fx_eff8;
                int ln = 0;
                for(int b = 0; b < 16 && ln < 3; b++){
                    if((fe >> b) & 1){ orow(11 + ln*10, nm[b]); ln++; }
                }
                if(ln == 0) orow(11, "HOLD PADS  RT=BANK");
                int fw = (int)(fx_fsr * 124.f);
                display.DrawRect(2, 44, 126, 52, true, false);
                if(fw > 0) display.DrawRect(2, 44, 2 + fw, 52, true, true);
                sprintf(row, "FSR%3d%%   BPM:%3d", (int)(fx_fsr * 100.f), (int)(15000u/step_ms));
                orow(56, row);
                if(fx_bank == 0 && enc2.psw == 0 && enc1.psw != 0){  // automation REC armed
                    display.DrawRect(0, 0, 23, 9, true, true);
                    display.SetCursor(2, 1); display.WriteString("REC", Font_6x8, false);
                }
            }
            else if(menu == MENU_SMP) { // SAMPLER page: modes + CHANCE + TAP
                display.SetCursor(43, 0); display.WriteString("SAMPLER", Font_6x8, true);
                {   // one unified tile — opens the step sequencer; pads also play live
                    bool act = seq_view;
                    display.DrawRect(2, 10, 41, 20, true, act);
                    display.SetCursor(8, 11); display.WriteString("SEQ EDIT", Font_6x8, !act);
                    if(mode_cur == 0) display.DrawRect(0, 8, 43, 22, true, false);
                }
                if(seq_view) sprintf(row, "BAR %d/%d  SLOT%02d",
                        (gstep/(form_blen>0?form_blen:16))+1, form_bars, (sel_smp&15)+1);
                else if(rec_slot >= 0) sprintf(row, "REC PAD %02d...", rec_slot + 1);
                else { int nf=0; for(int s=0;s<16;s++) if(smp_len[s]) nf++;
                       sprintf(row, "SLOTS:%02d HOLD=REC", nf); }
                orow(23, row);
                {   static const char* LB[5] = {"CHANCE","VOL","BARS","BEATS","TAP"};
                    char VL[5][12];
                    sprintf(VL[0],"%d",g_csmp); sprintf(VL[1],"%d%%",g_vsmp*10);
                    sprintf(VL[2],"%d",form_bars); sprintf(VL[3],"%d",form_blen);
                    sprintf(VL[4],"%d",(int)(15000u/step_ms));
                    int sel = (mode_cur >= 1) ? mode_cur - 1 : -1;
                    for(int r=0;r<3;r++){ int idx=r + (sel>=3?2:0); if(idx>=5)break;
                        int y = 33 + r*10; bool s=(idx==sel);
                        if(s) display.DrawRect(0,y-1,127,y+8,true,true);
                        char ln[28]; sprintf(ln," %-6s %s", LB[idx], VL[idx]);
                        display.SetCursor(0,y); display.WriteString(ln, Font_6x8, !s);
                        if(s && param_edit){ display.SetCursor(120,y); display.WriteString("\x10",Font_6x8,!s); }
                    }
                }
            }
            else if(menu == MENU_SESS) { // SESSIONS — 4 pages x 4 slots (still the
                // same 16 physical slots/pads — this is a pure display/labeling
                // layer, no storage-format or pad-mapping change: each pad ROW
                // already *is* one page of 4, since the NeoTrellis is wired 4x4
                // and i/4 has always been the row. Labeling it "PAGE A..D" just
                // makes that grouping explicit instead of leaving it implicit.
                static const char PAGE_LETTER[4] = {'A','B','C','D'};
                display.SetCursor(31, 0); display.WriteString("PATCHES", Font_6x8, true);
                for(int i=0;i<16;i++){
                    int x0 = 2 + (i%4)*14, y0 = 12 + (i/4)*10;
                    bool ex = (sess_exists >> i) & 1;
                    display.DrawRect(x0, y0, x0+11, y0+8, true, ex);   // filled = exists
                    if(i == sess_cur)
                        display.DrawRect(x0-1, y0-1, x0+12, y0+9, true, false); // active halo
                }
                for(int p=0;p<4;p++){   // page-letter gutter, one per row
                    char pl[2] = { PAGE_LETTER[p], 0 };
                    display.SetCursor(118, 12 + p*10); display.WriteString(pl, Font_6x8, true);
                }
                if(sess_warn >= 0) sprintf(row, "RELEASE=KEEP P%02d!", sess_warn+1);
                else sprintf(row, "PG%c.%d TAP:GO HOLD:NEW", PAGE_LETTER[sess_cur/4], (sess_cur%4)+1);
                orow(56, row);
            }
            else if(menu == MENU_GLOB) { // GLOBALS — BPM/KEY/MODE/SWING
                display.SetCursor(40, 0); display.WriteString("GLOBALS", Font_6x8, true);
                char ks[6]; sprintf(ks, "%+d", g_key);
                static char gl[4][20];
                sprintf(gl[0], "BPM   %3d", (int)(15000u/step_ms));
                sprintf(gl[1], "KEY   %s", ks);
                sprintf(gl[2], "MODE  %s", MODE_NAMES[g_mode]);
                sprintf(gl[3], "SWING %d", g_swing);
                for(int p=0; p<4; p++){
                    int y = 16 + p*11; bool s = (mode_cur == p);
                    if(s) display.DrawRect(0, y-1, 127, y+8, true, true);   // highlight bar
                    char ln[24]; sprintf(ln, " %s", gl[p]);
                    display.SetCursor(0, y); display.WriteString(ln, Font_6x8, !s);
                    if(s && param_edit){ display.SetCursor(120, y); display.WriteString("\x10", Font_6x8, !s); }
                }
            }
            else { // DRUMS page: modes + KIT/CHANCE + TAP
                display.SetCursor(49, 0); display.WriteString("DRUMS", Font_6x8, true);
                {   // one unified tile — opens the step sequencer; pads also play live
                    bool act = seq_view;
                    display.DrawRect(2, 10, 41, 20, true, act);
                    display.SetCursor(8, 11); display.WriteString("SEQ EDIT", Font_6x8, !act);
                    if(mode_cur == 0) display.DrawRect(0, 8, 43, 22, true, false);
                }
                if(seq_view){ sprintf(row, "BAR %d/%d  %s",
                        (gstep/(form_blen>0?form_blen:16))+1, form_bars, PADN[dr_pad[sel_drum&3]&15]); orow(23, row); }
                {   static const char* LB[6] = {"KIT","CHANCE","VOL","BARS","BEATS","TAP"};
                    char VL[6][12];
                    sprintf(VL[0],"%s",kit_sel?"ELEC":"CLASS"); sprintf(VL[1],"%d",g_cdrum);
                    sprintf(VL[2],"%d%%",g_vdrum*10); sprintf(VL[3],"%d",form_bars);
                    sprintf(VL[4],"%d",form_blen); sprintf(VL[5],"%d",(int)(15000u/step_ms));
                    int sel = (mode_cur >= 1) ? mode_cur - 1 : -1;   // 0..5
                    int top = (sel <= 1) ? 0 : (sel >= 4 ? 3 : sel - 1);
                    int y0 = seq_view ? 33 : 25;   int rows = seq_view ? 3 : 4;
                    for(int r=0;r<rows;r++){ int idx=top+r; if(idx>=6)break;
                        int y = y0 + r*10; bool s=(idx==sel);
                        if(s) display.DrawRect(0,y-1,127,y+8,true,true);
                        char ln[28]; sprintf(ln," %-6s %s", LB[idx], VL[idx]);
                        display.SetCursor(0,y); display.WriteString(ln, Font_6x8, !s);
                        if(s && param_edit){ display.SetCursor(120,y); display.WriteString("\x10",Font_6x8,!s); }
                    }
                }
            }

            // audio-reactive dancing particles around ALL FOUR borders — count
            // rides the output level, scattered symmetrically so it stays even.
            if(!vol_mode && menu_anim == 0 && !saver && menu != MENU_FX) {
                int np = (int)(out_amp * 16.f);
                for(int p = 0; p < np; p++){
                    uint32_t rr2 = lcg();
                    int edge = (rr2 >> 18) & 3;
                    if(edge == 0){            // top edge (mirror L/R)
                        int x = (int)(rr2 & 63), y = (int)((rr2 >> 8) & 2);
                        display.DrawPixel(x, y, true); display.DrawPixel(127 - x, y, true);
                    } else if(edge == 1){     // bottom edge (mirror L/R)
                        int x = (int)(rr2 & 63), y = 63 - (int)((rr2 >> 8) & 2);
                        display.DrawPixel(x, y, true); display.DrawPixel(127 - x, y, true);
                    } else if(edge == 2){     // left edge (mirror T/B)
                        int y = (int)(rr2 & 31), x = (int)((rr2 >> 8) & 2);
                        display.DrawPixel(x, y, true); display.DrawPixel(x, 63 - y, true);
                    } else {                  // right edge (mirror T/B)
                        int y = (int)(rr2 & 31), x = 127 - (int)((rr2 >> 8) & 2);
                        display.DrawPixel(x, y, true); display.DrawPixel(x, 63 - y, true);
                    }
                }
            }

            // menu position pips, top-right — hidden on the screensaver (clean fractal)
            if(!saver && (menu != MENU_DASH || menu_anim > 0)) {
                for(int i = 0; i < MENU_COUNT; i++)
                    display.DrawRect(92 + i*4, 0, 94 + i*4, 2, true, i == menu);
            }

            // save-state icon, far top-right: spinning arc while saving/syncing,
            // a STAR once everything (settings + sampler audio) is on the card
            if(!saver && !vol_mode){
                bool busy = save_state || aud_saving || aud_dirty;
                if(busy){
                    static int sp = 0; sp = (sp + 1) & 7;
                    int cx = 124, cy = 4;
                    int dx = (sp<2)?2:(sp<4)?0:(sp<6)?-2:0;
                    int dy = (sp<2)?0:(sp<4)?2:(sp<6)?0:-2;
                    display.DrawPixel(cx+dx, cy+dy, true);
                    display.DrawPixel(cx, cy, true);
                } else {
                    // 5-point star (saved + synced)
                    int cx = 123, cy = 4;
                    display.DrawPixel(cx,   cy-3, true);
                    display.DrawPixel(cx,   cy-2, true);
                    display.DrawLine(cx-3, cy+3, cx+3, cy-1, true);
                    display.DrawLine(cx+3, cy+3, cx-3, cy-1, true);
                    display.DrawLine(cx-3, cy-1, cx+3, cy-1, true);
                }
            }

            display.Update();
        }
    }
}
