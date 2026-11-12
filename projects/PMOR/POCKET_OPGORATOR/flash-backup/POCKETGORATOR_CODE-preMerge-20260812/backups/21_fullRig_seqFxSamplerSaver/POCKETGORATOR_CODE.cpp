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
// OLED UI (128×64) — 4 primary menus, left encoder rotates (tunnel-dive
// transition), LEFT CLICK backs out of any submenu to the primary page:
//   FRACTAL — default. Julia set morphed by accel/gyro + output level
//   SYNTH   — MAN/MCH/EUC radio + deep param list (WAVE/SPRD/WOBL/BRTH/
//             CHOR/DRV/TAIL): R-turn scrolls, R-click edits, L-click backs out
//   DRUMS   — MAN/MCH/EUC radio; MCH *is* the PO sequencer submenu (R-turn =
//             write target, R-click = place mode where pads are the 16 steps,
//             playhead sweeps the LEDs; OLED shows info only — grid lives on LEDs)
//   FX      — PO-style punch-in effects: pads are hold-to-engage (press = on,
//             release = off) — retrig/tempo/order/pitch/filter/mute/crush/wash
//   DASH    — full sensor readout (accel/gyro/mag/lidar/enc/amp/FSR)

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "VL53L1X_mini.h"
#include "fatfs.h"
#include "ff_gen_drv.h"

using namespace daisy;
using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed    hw;
MyOled       display;
VL53L1X_Mini lidar;

// ============================================================
// Bit-bang I2C  (open-drain GPIO, 100 kHz)
// ============================================================
struct BBus {
    dsy_gpio scl_g, sda_g;

    void init(dsy_gpio_pin sp, dsy_gpio_pin dp) {
        scl_g = {sp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        sda_g = {dp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        dsy_gpio_init(&scl_g); dsy_gpio_init(&sda_g);
        scl(1); sda(1); System::Delay(2);
    }
    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { System::DelayUs(5); }

    // After releasing SCL (open-drain high), the slave can hold it low (clock stretch)
    // while its ISR processes the previous byte. Poll IDR until the pin is actually high
    // before starting the SCL-high hold period. Without this, we charge ahead during
    // the stretch and the slave samples wrong bit values → corrupted pixel data.
    void waitScl() {
        for(uint32_t i=10000; i && !dsy_gpio_read(&scl_g); --i) {}
    }

    void start() { sda(1);hd();scl(1);hd();sda(0);hd();scl(0);hd(); }
    void stop()  { sda(0);hd();scl(1);hd();sda(1);hd(); }

    bool byteW(uint8_t b) {
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
        start(); bool ok=byteW((a7<<1)|0);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd(); return ok;
    }
    void w8(uint8_t a7, uint8_t reg, uint8_t val) {
        uint8_t b[2]={reg,val}; send(a7,b,2);
    }
    uint8_t r8(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    int16_t r16be(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t hi=byteR(false),lo=byteR(true);
        stop();hd(); return (int16_t)((hi<<8)|lo);
    }
    void w16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }
    void r16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
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

void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg (uint16_t reg, uint8_t* d, uint8_t n) { sbus.r16a(0x29,reg,d,n); }

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
static bool ntInit() {
    const uint8_t rst[]={0x00,0x7F,0xFF}; tbus.send(TREL,rst,3);
    System::Delay(500);

    if(tbus.ssR1(TREL,0x00,0x01) != 0x55) return false;

    const uint8_t pin[] ={0x0E,0x01,3};         tbus.send(TREL,pin, 3); System::Delay(10);
    const uint8_t spd[] ={0x0E,0x02,0x01};      tbus.send(TREL,spd, 3); System::Delay(10);
    const uint8_t blen[]={0x0E,0x03,0x00,0x30}; tbus.send(TREL,blen,4); System::Delay(10);

    uint8_t blank[28]={};
    blank[0]=0x0E; blank[1]=0x04;
    tbus.send(TREL,blank,28); System::Delay(2);
    blank[3]=24;
    tbus.send(TREL,blank,28); System::Delay(2);
    const uint8_t sh[]={0x0E,0x05}; tbus.send(TREL,sh,2);
    System::Delay(10);

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
// F Lydian: F G A B C D E (B natural, raised 4th), 4 octaves F3-E7
static const float FLYD[28] = {
    174.61f, 196.00f, 220.00f, 246.94f, 261.63f, 293.66f, 329.63f, // F3..E4
    349.23f, 392.00f, 440.00f, 493.88f, 523.25f, 587.33f, 659.25f, // F4..E5
    698.46f, 783.99f, 880.00f, 987.77f,1046.50f,1174.66f,1318.51f, // F5..E6
   1396.91f,1567.98f,1760.00f,1975.53f,2093.00f,2349.32f,2637.02f, // F6..E7
};
static const float DETUNE[5] = {-0.012f, -0.006f, 0.f, +0.006f, +0.012f};
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
    int   type = 0;                    // 0 saw stack, 1 tri stack, 2 FM pair
    float fc = 0.2f, fcT = 0.1f, fcR = 0.0001f;
    float q  = 0.9f, lp = 0.f, bp = 0.f;
    float fmi = 0.f, fmiT = 0.f;
};
static inline float psin(float ph){
    float x = ph + ph - 1.f;
    return 4.f * x * (1.f - fabsf(x));
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
enum { MENU_SYNTH = 0, MENU_DRUM = 1, MENU_SMP = 2, MENU_FX = 3, MENU_DASH = 4, MENU_COUNT = 5 };
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
static int  eng_focus   = ENG_SYNTH;
static int  seq_mode[3] = { SEQ_MANUAL, SEQ_MANUAL, SEQ_MANUAL }; // boot: all playable
static int  mode_cur    = 0;         // engine-page cursor
static bool seq_view    = false;     // inside the drum sequencer submenu
static bool seq_place   = false;     // place mode: pads = 16 steps
static bool param_edit  = false;     // editing a synth param (R-turn = value)
static int  gstep       = 0;         // global 16-step grid position

// ── Punch-in FX (MENU_FX) — PO-style hold-to-engage pad effects ──────
// While the FX page is current, the 16 pads set/clear bits here (press = on,
// release = off). Bits are PHYSICAL pad positions (columns mirrored at the
// key handler) so the layout below reads left-to-right on the unit.
// EVERY effect scales with FSR squeeze (fx_fsr 0..1) — lean in for more.
// Physical layout (row 0 = top):
enum { FX_ST8 = 0,  FX_ST8T,  FX_ST16,  FX_ST32,   // grid-locked MIDI stutters
       FX_REV,      FX_CHOR,  FX_PAN,   FX_CRUSH,  // buffer/space
       FX_HALF,     FX_DBL,   FX_BZZ,   FX_MUTE,   // varispeed / gen-buzz / drop
       FX_FLT,      FX_WASH,  FX_DIST,  FX_STOP }; // bp-sweep / wash / dist / tape-stop
static volatile uint16_t fx_held = 0;
static volatile float    fx_fsr  = 0.f;   // FSR squeeze 0..1, updated @ FSR rate

// FX automation: hold the RIGHT encoder on the FX page to write the live pad
// mask into the current step as the loop passes — pads held get recorded,
// empty hands scrub that step clean. Playback: live pads override temporarily,
// otherwise the recorded automation drives the FX. fx_eff is what everything
// (audio callback, grid clock, filter) actually obeys.
static uint16_t          fx_auto[16] = {};
static volatile uint16_t fx_eff      = 0;
static inline bool fxOn(int b) { return ((fx_eff >> b) & 1u) != 0; }

// Stutter/reverse capture ring — 2s stereo in SDRAM, written with the final
// mix every sample; frozen (write paused) while a buffer FX holds it.
#define RBUF_N 96000u
static float DSY_SDRAM_BSS rbufL[RBUF_N];
static float DSY_SDRAM_BSS rbufR[RBUF_N];
// Clock sync for QUANTIZED stutters: the grid clock snapshots the ring write
// head at every step boundary and bumps a stamp; the callback counts samples
// since that boundary, so stutter loops start on the grid and repeat in time.
static volatile uint32_t rb_w        = 0;   // ring write head (callback-owned)
static volatile uint32_t step_rw     = 0;   // ring pos at last step boundary
static volatile float    fx_vs       = 1.f; // varispeed target (HALF/DBL FX)
static volatile float    fx_q        = 0.8f;// master-filter damping (FLT FX lowers it)
static volatile bool     fx_bp       = false;// FLT outputs bandpass while held
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
static uint16_t smp_pat[16] = {};             // step mask per slot
static int      sel_smp     = 0;              // last played slot = write target
static uint32_t smp_press[16] = {};           // press timestamps for hold-vs-click

// PO drum patterns: one 16-step bitmask per voice (full polyphony — all four
// voices can share a step). Loops while drum mode = MACHINE, wherever the UI
// is. dr_pad remembers each voice's last-played flavor pad so sequenced hits
// sound exactly like what you played.
static uint16_t dr_pat[4] = {0,0,0,0};
static uint8_t  dr_pad[4] = {13, 10, 4, 0};  // defaults: KCK2, BELL, SNP1, HCL

// ── Master volume mode — hold BOTH encoder buttons to toggle ──
// Pads set the end-of-chain level, physical top-left (0%) → bottom-right
// (125%). >100% pushes the final tanh = soft tan loudness, never hard clip.
// Default 75% = physical pad index 9.
static bool           vol_mode = false;
static int            vol_pidx = 9;            // physical pad index 0..15
static volatile float mvol     = 0.75f;        // 1.25 * vol_pidx / 15
static int      sel_drum  = 0;               // last drum played = write target

// ── Mid-Air Thief voice character — every note is its own patch ──
// Detuned stack (or harmonic 2-op FM) through a per-note VCF envelope with
// random (always-stable) resonance, krell drift, occasional swells and
// glides. Processing is fixed: drive 2.5x, chorus 0.55 — no options.
static const float STK[5][5] = {
    {1.f,  1.f,  1.f,  1.f,  1.f },   // unison supersaw
    {1.f,  1.f,  1.f,  2.f,  2.f },   // octave-up shimmer
    {0.5f, 1.f,  1.f,  1.5f, 2.f },   // sub + fifth stack
    {1.f,  1.5f, 2.f,  0.5f, 1.f },   // power-chord modular
    {0.5f, 0.5f, 1.f,  1.f,  1.5f},   // deep sub-fifth
};
static void voiceCharacter(int v, bool generative){
    Voice& V = voices[v];
    V.drift  = 0.f;
    V.driftT = ((float)(lcg() % 200) - 100.f) * 0.00008f;
    V.lp = 0.f; V.bp = 0.f;
    V.q  = 0.70f + (float)(lcg() % 35) * 0.01f;      // rings, never unstable
    uint32_t r = lcg() % 100u;
    if(r < 30){
        // 2-op FM pluck — harmonic ratios only, stays in the Lydian vibe
        V.type = 2;
        static const float FMR[6] = {0.5f, 1.f, 2.f, 2.005f, 3.f, 4.f};
        V.ratio[1] = FMR[lcg() % 6u];
        V.fmi  = 0.10f + (float)(lcg() % 32) * 0.01f;
        V.fmiT = 0.02f;
        V.fc   = 0.40f; V.fcT = 0.15f; V.fcR = 0.00008f;
    } else {
        V.type = (r < 85) ? 0 : 1;                   // gritty saws, some tri
        int s = (int)(lcg() % 5u);
        for(int k = 0; k < 5; k++) V.ratio[k] = STK[s][k] * (1.f + DETUNE[k]);
        V.fc  = 0.22f + (float)(lcg() % 21) * 0.01f; // snap-open, stable ceiling
        V.fcT = 0.025f + (float)(lcg() % 8) * 0.01f;
        V.fcR = 0.00004f + (float)(lcg() % 9) * 0.00001f;
    }
    bool swell = generative && (lcg() % 100u) < 20u;
    V.atk = swell ? 0.0025f : 0.08f;
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
static volatile float out_amp = 0.f; // output peak — drives the fractal visualizer
static volatile float cut_t   = 1.f; // normalized cutoff 0..1 — drives fractal angle

// Playable mode: trellis key → active voice (-1 = none). Bottom row = lowest notes,
// pan spreads by column. Lidar octave zone applies at press, same as generative.
static int8_t key_voice[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static uint32_t kprs_t[16] = {};  // press times — stuck-note safety
static uint32_t fx_t[16]  = {};   // FX press times — stuck-FX safety
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
    // row3 → ch0: KCK1 KCK2 KCK3 K808
    { 58.f, 0.95f, 0.99990f, 0.05f, 5.0f, 0.50f, 0.f},     // KCK1 punch
    { 72.f, 0.95f, 0.99986f, 0.07f, 5.5f, 0.50f, 0.f},     // KCK2 tight
    { 92.f, 0.95f, 0.99980f, 0.12f, 6.5f, 0.50f, 0.f},     // KCK3 clicky transient
    { 52.f, 1.00f, 0.99995f, 0.05f, 7.0f, 0.50f, 0.f},     // K808 phonk boom (long)
};
static const char* PADN[16] = {"HCL", "HMD", "HOP", "RIDE",
                               "SNP1","SNP2","SNRF","CHOP",
                               "TOMM","TOMH","BELL","BLIP",
                               "KCK1","KCK2","KCK3","K808"};

static void drumPadHit(uint8_t k){
    k &= 15;
    const DrumDef& d = KIT[k];
    float f = d.f, pan = d.pan;
    if(f <= 1.f) {   // modular blip: random pitch + pan every hit
        f   = 300.f + (float)(lcg()%2100);
        pan = 0.1f  + 0.8f*(float)(lcg()%100)/99.f;
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
static volatile float filter_f = 0.065f;
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t sz) {
    if(enc_ready){ ePoll(enc1); ePoll(enc2); }
    float pk=0.f, opk=0.f;
    // Q1 = damping = 1/Q_filter. 0.8 → Q≈1.25 — gentle musical resonance, no honk.
    const float Q1 = fx_q;
    static float lpL=0,bpL=0, lpR=0,bpR=0;
    static float dlpL=0,dbpL=0, dlpR=0,dbpR=0;   // drum copy of the accel filter
    // Cutoff slews per-sample toward the 50ms sensor target (τ≈14ms) — no zipper steps.
    static float ff_s = 0.065f;
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
    const float    ffsr  = fx_fsr;
    const bool     crush = ((fh >> FX_CRUSH) & 1u) != 0;
    const uint32_t cint  = 4u + (uint32_t)(ffsr * 20.f);   // crush rate ← FSR
    const bool     usebp = fx_bp;                           // FLT bandpass routing
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
    if(!(ff_s*0.f == 0.f)) ff_s = 0.065f;
    const float wobd  = 0.0065f;                   // tape warble
    const float brtd  = 0.11f;                     // gentle breathing
    static float wobph = 0.f, brph = 0.f;
    const float cwet  = 0.55f;   // fixed prime chorus amount
    const float drv   = 2.5f;    // fixed prime drive
    // master volume scales drive into the final tanh: 100% = the old 1.5x,
    // 125% leans in for soft tan loudness, 0% = silence
    const float mgain = mvol * 2.0f;
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
            float inc = voices[v].phaseInc * wob * vss * (1.f + voices[v].drift);
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
            } else {
                raw = 0.f;
                for(int k=0;k<5;k++){
                    voices[v].phase[k] += inc * voices[v].ratio[k];
                    while(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                    float ph = voices[v].phase[k];
                    raw += (voices[v].type == 1) ? (4.f*fabsf(ph - 0.5f) - 1.f)
                                                 : (ph + ph - 1.f);
                }
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
            float sig = voices[v].lp * (0.05f * voices[v].env) * breath;
            mixL += sig * (1.f - voices[v].pan);
            mixR += sig * voices[v].pan;
        }
        ff_s += (filter_f - ff_s) * 0.0015f;
        // Chamberlin 2-pole resonant LPF — update lp first for correct phase
        lpL += ff_s * bpL;
        float hpL = mixL - lpL - Q1*bpL; bpL += ff_s * hpL;
        lpR += ff_s * bpR;
        float hpR = mixR - lpR - Q1*bpR; bpR += ff_s * hpR;
        // Saturation drive (DRV param) — thickens, adds harmonics above the LPF
        // (FLT FX swaps in the bandpass output = the DJ sweep voice)
        float dL = tanhf((usebp ? bpL : lpL) * drv);
        float dR = tanhf((usebp ? bpR : lpR) * drv);
        // Chorus: write dry, read a swept fractional tap (integer math on cw — no
        // float-precision drift at long uptimes)
        cbufL[cw & 1023] = dL;
        cbufR[cw & 1023] = dR;
        clfo += 0.4f / 48000.f; if(clfo >= 1.f) clfo -= 1.f;
        float phs  = clfo * 6.2831853f;
        float tapL = 672.f + 180.f * sinf(phs);
        float tapR = 672.f + 180.f * sinf(phs + 1.5708f);
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
                drL += sd * d.panl; drR += sd * d.panr;
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
                drL += sv * 0.9f; drR += sv * 0.9f;
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
        float drmL = usebp ? dbpL : dlpL;
        float drmR = usebp ? dbpR : dlpR;
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
            float xph   = xlfo * 6.2831853f;
            float depth = 0.45f + 0.55f * ffsr;   // deep even untouched
            float tapL  = 960.f + 740.f * depth * sinf(xph);
            float tapR  = 960.f + 740.f * depth * sinf(xph + 1.5708f);
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

        // DST: aggressive soft clip, level-matched to the dry signal
        if((fh >> FX_DIST) & 1u) {
            float g  = 4.f + ffsr * 10.f;
            float dc = tanhf(0.08f * g) * 0.62f;
            oL = tanhf((oL + 0.08f) * g) * 0.62f - dc;
            oR = tanhf((oR + 0.08f) * g) * 0.62f - dc;
        }

        // MUT: clean full drop, short ramp so it never clicks
        {
            static float mg = 1.f;
            float mt = ((fh >> FX_MUTE) & 1u) ? 0.f : 1.f;
            mg += (mt - mg) * 0.004f;
            oL *= mg; oR *= mg;
        }

        // CRUSH: lo-fi decimation, FSR = how crushed (hold 4..24 samples)
        if(crush) {
            if(crn == 0) {
                // rate decimation + bit reduction — proper nasty crush
                float qs = 24.f - ffsr * 18.f;
                crL = (float)(int)(oL * qs) / qs;
                crR = (float)(int)(oR * qs) / qs;
            }
            crn = (crn + 1u) % cint;
            oL = crL; oR = crR;
        } else { crn = 0; }

        // hard safety limiter — chorus feedback / pan boost can exceed ±1,
        // and the codec must never see that (was an audible blowup risk)
        if(oL >  1.f) oL =  1.f; else if(oL < -1.f) oL = -1.f;
        if(oR >  1.f) oR =  1.f; else if(oR < -1.f) oR = -1.f;

        out[0][i] = oL;
        out[1][i] = oR;
        float am = (oL<0.f)?-oL:oL; if(am>opk) opk=am;
    }
    rb_w = rw;
    mic_amp=pk;
    out_amp=opk;
}

// ============================================================
// SPI SD card — bit-bang on D26/D27/D25/D28
// D26(PD11)=CLK  D27(PG9)=MOSI  D25(PA0)=MISO  D28(PA2)=CS
// ============================================================
static dsy_gpio ssd_clk, ssd_mosi, ssd_miso, ssd_cs;
static bool     ssd_sdhc = false;
static DSTATUS  ssd_stat = STA_NOINIT;

static void sclk(int v)  { dsy_gpio_write(&ssd_clk,  v); }
static void smosi(int v) { dsy_gpio_write(&ssd_mosi, v); }
static void scs(int v)   { dsy_gpio_write(&ssd_cs,   v); }
static int  smiso()      { return (int)dsy_gpio_read(&ssd_miso); }

static uint8_t spiXfer(uint8_t b) {
    uint8_t rx = 0;
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
    return 0;
}

static DSTATUS ssd_status(BYTE) { return ssd_stat; }

static DRESULT ssd_read(BYTE, BYTE* buf, DWORD sector, UINT count) {
    if (ssd_stat & STA_NOINIT) return RES_NOTRDY;
    if (!ssd_sdhc) sector *= 512;
    for (UINT n = 0; n < count; n++, buf += 512) {
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

// ============================================================
// main
// ============================================================
int main(void)
{
    hw.Configure(); hw.Init();

    MyOled::Config oc;
    oc.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oc.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oc);
    display.Fill(false);
    display.Update();   // dark during hardware init — intro animation follows

    sbus.init(hw.GetPin(13), hw.GetPin(14));
    tbus.init(hw.GetPin(11), hw.GetPin(12));

    // MPU9250 (0x68)
    sbus.w8(0x68,0x6B,0x80); System::Delay(100); // DEVICE_RESET
    sbus.w8(0x68,0x6B,0x01); System::Delay(50);  // wake + PLL clock
    uint8_t imu_id = sbus.r8(0x68,0x75);
    bool imu_ok = (imu_id==0x71||imu_id==0x73||imu_id==0x70);

    // AK8963 magnetometer via MPU bypass (0x0C)
    sbus.w8(0x68,0x37,0x02); System::Delay(10); // BYPASS_EN
    sbus.w8(0x0C,0x0A,0x00); System::Delay(10); // power-down before mode change
    bool mag_ok = (sbus.r8(0x0C,0x00)==0x48);   // WIA = 0x48
    sbus.w8(0x0C,0x0A,0x16); System::Delay(10); // 16-bit continuous 100Hz

    // VL53L1X lidar (0x29)
    bool lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(100);

    // SD card — SPI bit-bang on D22(CLK) D23(MOSI) D24(MISO) D25(CS)
    static char  sdPath[4] = {};
    static FATFS sdFS;
    bool sd_ok = (FATFS_LinkDriver(&SPISD_Driver, sdPath) == 0) &&
                 (f_mount(&sdFS, sdPath, 1) == FR_OK);

    // Encoders: enc1 A=D0 B=D1 SW=D2 / enc2 A=D24 B=D16 SW=D17
    eInit(enc1,hw.GetPin(0), hw.GetPin(1), hw.GetPin(2));
    eInit(enc2,hw.GetPin(24),hw.GetPin(16),hw.GetPin(17));
    enc_ready = true;

    // FSR analog (D15)
    AdcChannelConfig adc_cfg;
    adc_cfg.InitSingle(hw.GetPin(15));
    hw.adc.Init(&adc_cfg,1);
    hw.adc.Start();

    hw.SetAudioBlockSize(64);
    hw.StartAudio(AudioCallback);

    // NeoTrellis init LAST — prevents SDMMC slowness from resetting freshly-inited seesaw
    bool trel_ok = ntInit();

    // ~1s startup: tunnel-dive intro straight into the fractal page.
    // (Peripheral statuses still live on the DASH page if you need them.)
    for(int f = 0; f < 20; f++) {
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
        display.Update();
        System::Delay(30);
    }

    // Light-yellow lava lamp state — slow drift, varied per-pixel speed
    static const uint8_t DRIFT[16]={1,2,1,2, 2,1,2,1, 1,2,2,1, 2,1,1,2};
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    // Per-subsystem state hoisted so they survive across loop iterations
    uint16_t dist=0;
    float    ax=0,ay=0,az=0,gx=0,gy=0,gz=0,mx=0,my=0,mz=0;
    uint16_t fsr=0;
    char     e1s[6]="--",e2s[6]="--";
    uint32_t e1until=0,e2until=0;
    uint16_t held_mask=0;   // bitmask of currently held keys (bit i = key i pressed)

    // Each subsystem runs on its own timestamp — no blocking Sleep in main loop.
    // NeoTrellis @ ~17Hz (60ms): slower rate reduces I2C noise burst on audio path.
    // OLED @ 20Hz (50ms): display update rate independent of sensor read speed.
    // Sensors @ 50ms: reduced from 20ms to lower I2C EMI energy on audio path.
    uint32_t next_trel = 0;
    uint32_t next_oled = 0;
    int menu_anim = 0, menu_anim_dir = 1;   // dive-transition frames left, direction

    while(1)
    {
        uint32_t now = System::GetNow();

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

        if(trel_ok && now >= next_trel) {
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
            else if(menu == MENU_FX) {
                // Punch-in FX page: one hue per row from the lava-lamp family,
                // dim idle / bright while held (bits are physical → mirror LEDs)
                static const uint8_t FXC[4][3] = {   // {r,g,b}
                    {255, 215,  65},   // row 0 stutters — warm yellow
                    { 70, 150, 255},   // row 1 space    — light blue
                    {155,  65, 245},   // row 2 grid     — blue-purple
                    {185,  65, 220},   // row 3 tone     — light purple
                };
                uint16_t fh = fx_eff;   // shows live holds AND playing automation
                for(uint8_t i=0;i<16;i++){
                    const uint8_t* c = FXC[(i>>2)&3];
                    uint8_t r=c[0], g=c[1], b=c[2];
                    if(!((fh >> i) & 1)) { r>>=4; g>>=4; b>>=4; }  // dim idle
                    grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
                }
            }
            else if(eng_focus == ENG_SMP) {
                if(seq_mode[ENG_SMP] == SEQ_MACHINE) {
                    // sampler step display: teal placed steps, white playhead
                    uint16_t placed = smp_pat[sel_smp & 15];
                    uint8_t sh = seq_place ? 1 : 2;
                    for(uint8_t i=0;i<16;i++){
                        uint8_t r,g,b;
                        if(i == (uint8_t)gstep) { r=g=b=140; }
                        else if((placed>>i)&1)  { r=40>>sh; g=220>>sh; b=170>>sh; }
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
            else if(eng_focus == ENG_DRUM && seq_mode[ENG_DRUM] == SEQ_MACHINE) {
                // PO step display: placed steps glow in the write-target's color,
                // playhead sweeps bright white. Brighter while placing.
                static const uint8_t VCOL[4][3] = {
                    {255, 60, 40},   // ch0 kick
                    {200, 60,255},   // ch1 perc (toms/bell/blip)
                    {255,200, 40},   // ch2 snare
                    { 60,220,255},   // ch3 hat
                };
                uint16_t placed   = dr_pat[sel_drum & 3];
                const uint8_t* c  = VCOL[sel_drum & 3];
                uint8_t sh = seq_place ? 1 : 2;
                for(uint8_t i=0;i<16;i++){
                    uint8_t st = i;   // pad i = step i — L→R per row, top to bottom
                    uint8_t r,g,b;
                    if(st == (uint8_t)gstep)       { r=g=b=140; }              // playhead
                    else if((placed>>st)&1)        { r=c[0]>>sh; g=c[1]>>sh; b=c[2]>>sh; }
                    else { lavaColor(pphase[i],r,g,b); r>>=5; g>>=5; b>>=5; }  // dim idle
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

            // only send frames that actually changed — most are identical in
            // seq/FX views, so this slashes I2C traffic (= fewer SAMD09 upsets)
            static uint8_t grb_prev[48] = {1};
            bool lchg = (memcmp(grb, grb_prev, 48) != 0);
            if(lchg) memcpy(grb_prev, grb, 48);
            bool show_ok = lchg ? ntShow(grb) : true;
            if(!show_ok) {
                trel_ok = false;
            } else {
                // After SHOW, SAMD09 drives WS2812B for ~530µs with I2C deaf.
                // 1ms margin — only needed when a frame was actually sent.
                if(lchg) System::Delay(1);

                uint8_t kcount = tbus.ssR1(TREL,0x10,0x04);
                if(kcount > 0 && kcount < 32) {
                    uint8_t toread = (kcount > 8) ? 8 : kcount;
                    uint8_t kbuf[8];
                    tbus.ssRN(TREL,0x10,0x10,kbuf,toread);
                    for(uint8_t i=0;i<toread;i++){
                        // NUM field is seesaw GPIO pin — convert to logical key 0-15
                        // NEO_TRELLIS_SEESAW_KEY: (pin/8)*4 + (pin%8)
                        uint8_t seesaw_pin = kbuf[i] >> 2;
                        uint8_t key  = (seesaw_pin/8)*4 + (seesaw_pin%8);
                        uint8_t edge = kbuf[i] & 0x03;
                        if(key < 16) {
                            if(edge == 3) {                          // EDGE_RISING = pressed
                                held_mask |=  (1u << key);
                                if(vol_mode) {
                                    // pads = volume fader, top-left → bottom-right
                                    vol_pidx = (int)key;
                                    mvol     = 1.25f * (float)vol_pidx / 15.f;
                                }
                                else if(menu == MENU_FX) {
                                    // FX page: pads are punch-in effects only
                                    fx_held |= (1u << key);
                                    fx_t[key] = now;
                                }
                                else {
                                int em = seq_mode[eng_focus];
                                if(seq_place && em == SEQ_MACHINE && eng_focus != ENG_SYNTH) {
                                    // place mode: pads ARE the 16 steps, L→R per row
                                    if(eng_focus == ENG_DRUM) dr_pat[sel_drum & 3]  ^= (1u << key);
                                    else                      smp_pat[sel_smp & 15] ^= (1u << key);
                                }
                                else if(eng_focus == ENG_SYNTH) {
                                    if(em == SEQ_EUCLID) { eu_k_syn = key + 1; eu_rot_syn = 0; }
                                    else keyNoteOn(key);   // playable in MAN/MACH
                                }
                                else if(eng_focus == ENG_SMP) {
                                    if(em == SEQ_EUCLID) eu_k_smp = key + 1;
                                    else smp_press[key] = now;   // hold→rec / click→play on release
                                }
                                else {
                                    int dv = 3 - (key/4), col = key%4;   // bottom row = kick
                                    if(em == SEQ_EUCLID) {
                                        int nk = (int)EUK[dv][col];
                                        dr_k[dv]   = (dr_k[dv] == nk) ? 0 : nk;  // re-press = voice off
                                        dr_rot[dv] = (dv == 1) ? 4 : (dv == 3 ? (int)(lcg()%16) : 0);
                                    } else { // playable in MAN/MACH — last drum = write target
                                        drumPadHit(key);
                                        sel_drum = dv;
                                        dr_pad[dv] = key;
                                    }
                                }
                                }
                            } else if(edge == 2) {                   // EDGE_FALLING = released
                                held_mask &= ~(1u << key);
                                fx_held   &= ~(1u << key);  // FX off on release (any page — defensive)
                                keyNoteOff(key);  // no-op unless key holds a manual synth note
                                // sampler: end recording / fire a short-press play
                                if(rec_slot == (int)key) {
                                    smp_len[key] = rec_pos;
                                    rec_slot = -1;
                                    sel_smp = (int)key;
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

        // ── Grid clock — one shared 16th-note step drives both engines ───────
        {
            now = System::GetNow();
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
                // ABSOLUTE gate cap: no voice may sustain >20s, no matter what
                if(voices[v].envTarget > 0.f && now - vgate_t[v] > 20000u)
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

            // Effective FX = live pads override, else the recorded automation
            fx_eff = fx_held ? fx_held : fx_auto[gstep & 15];

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
            // happens on the grid-anchored timer below — super defined rates.
            uint32_t rt_div = 0;
            if(fxOn(FX_ST8))  rt_div = step_ms * 2u;
            if(fxOn(FX_ST8T)) rt_div = (step_ms * 4u) / 3u;
            if(fxOn(FX_ST16)) rt_div = step_ms;
            if(fxOn(FX_ST32)) rt_div = step_ms / 2u;
            if(rt_div && rt_div < 6u) rt_div = 6u;   // clamp ONLY when a stutter is held
            const bool st_on = (rt_div != 0);
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
                pos &= 15;
                uint8_t dmask = 0;
                if(seq_mode[ENG_DRUM] == SEQ_MACHINE) {
                    for(int v=0; v<4; v++)
                        if(dr_pat[v] & (1u << pos)) dmask |= (uint8_t)(1u << v);
                }
                else if(seq_mode[ENG_DRUM] == SEQ_EUCLID) {
                    for(int v=0; v<4; v++)
                        if(euclidHit(dr_k[v], dr_rot[v], pos)) dmask |= (uint8_t)(1u << v);
                }
                int nv = 0;
                int notes[8];
                if(seq_mode[ENG_SYNTH] == SEQ_MACHINE){
                    uint32_t roll = lcg() % 100;
                    nv = (roll < 20) ? 0 :
                         (roll < 72) ? 1 :
                         (roll < 89) ? 2 :
                         (roll < 96) ? 3 : 4;
                    for(int n=0; n<nv; n++) notes[n] = (int)(lcg() % 28u);
                } else if(seq_mode[ENG_SYNTH] == SEQ_EUCLID
                          && euclidHit(eu_k_syn, eu_rot_syn, pos)){
                    nv = 1;
                    notes[0] = (int)(lcg() % 28u);
                }
                if(fire){
                    for(int v=0; v<4; v++)
                        if(dmask & (1u << v)) drumPadHit(dr_pad[v] & 15);
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
                // MASTER CLOCK: absolute deadlines at constant step_ms — FX
                // can never bend, sag, or shift this. Resync only if starved.
                step_t = next_step;
                next_step += step_ms;
                if(now >= next_step) next_step = now + step_ms;
                sct++;
                gstep = (gstep + 1) & 15;
                step_rw = rb_w;   // ring position at the step boundary (REV anchor)

                // SAMPLER — step patterns fire any slot; euclid fires the
                // selected slot (manual = pads only). Always on the master grid.
                if(seq_mode[ENG_SMP] == SEQ_MACHINE) {
                    for(int s = 0; s < 16; s++)
                        if((smp_pat[s] & (1u << gstep)) && smp_len[s]) smp_trig[s]++;
                }
                else if(seq_mode[ENG_SMP] == SEQ_EUCLID) {
                    if(euclidHit(eu_k_smp, 0, gstep) && smp_len[sel_smp & 15])
                        smp_trig[sel_smp & 15]++;
                }

                // FX automation write head: while the right encoder is held on
                // the FX page, the live pad mask is stamped into each step as
                // the loop passes — held pads record, empty hands scrub clean.
                if(menu == MENU_FX && !vol_mode && enc2.psw == 0 && enc1.psw != 0)
                    fx_auto[gstep & 15] = fx_held;

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

            // STUTTER scheduler — deadlines anchored to the step boundary so
            // 4N / 8N / 8NT / 16N each land exactly on their subdivision and
            // sound completely distinct. Never touches the master clock.
            {
                static uint32_t next_rt  = 0;
                static uint32_t rt_prevd = 0;
                if(st_on && !stp_on){
                    if(rt_prevd != rt_div){           // engage or rate change:
                        rt_prevd = rt_div;            // re-anchor to the grid
                        next_rt = step_t + rt_div;
                        while(next_rt <= now) next_rt += rt_div;
                    }
                    if(now >= next_rt){
                        next_rt += rt_div;
                        if(now >= next_rt) next_rt = now + rt_div;
                        for(int v=0; v<4; v++)
                            if(rt_dr & (1u << v)) drumPadHit(dr_pad[v] & 15);
                        for(int n=0; n<rt_nv && n<8; n++) fireNote(rt_notes[n]);
                    }
                } else rt_prevd = 0;
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
                if(c1_pend && now - c1_pend >= 130){ c1 = true; c1_pend = 0; }
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
                fx_held = 0;   // defensive: kill any punch-in FX when the menu rotates
                if(menu == MENU_SYNTH) eng_focus = ENG_SYNTH;        // pads follow page
                if(menu == MENU_DRUM)  eng_focus = ENG_DRUM;
                if(menu == MENU_SMP)   eng_focus = ENG_SMP;
            }

            // LEFT CLICK = back out one level; on the FX page it panic-wipes
            // all recorded FX automation (stuck loops die instantly)
            if(c1) {
                if(menu == MENU_FX) { for(int a=0;a<16;a++) fx_auto[a] = 0; }
                else if(param_edit)    param_edit = false;
                else if(seq_view) { seq_view = false; seq_place = false; }
            }

            bool eng_page = (menu == MENU_SYNTH || menu == MENU_DRUM || menu == MENU_SMP);
            if((menu == MENU_DRUM || menu == MENU_SMP) && seq_view) {
                // Sequencer submenu: R-turn cycles the write target,
                // R-click toggles place mode (pads = 16 steps).
                acc2 += d2;
                if(acc2 >= 4 || acc2 <= -4) {
                    int dir = (acc2 > 0) ? 1 : -1;
                    if(menu == MENU_DRUM) sel_drum = (sel_drum + 4 + dir) % 4;
                    else                  sel_smp  = (sel_smp + 16 + dir) & 15;
                    acc2 = 0;
                }
                if(c2) seq_place = !seq_place;
            }
            else if(eng_page) {
                // Item count: every engine page = 3 modes + TAP
                int nit = 4;
                acc2 += d2;
                if(acc2 >= 4)       { mode_cur = (mode_cur + 1) % nit;       acc2 = 0; }
                else if(acc2 <= -4) { mode_cur = (mode_cur + nit - 1) % nit; acc2 = 0; }
                if(c2 && mode_cur < 3) {            // mode radio: MAN/MCH/EUC
                    if(seq_mode[eng_focus] != mode_cur){ seq_mode[eng_focus] = mode_cur; allKeysOff(); }
                    if((menu == MENU_DRUM || menu == MENU_SMP) && mode_cur == SEQ_MACHINE)
                        { seq_view = true; seq_place = false; }   // MCH = the sequencer
                }
                else if(c2) {                       // TAP tempo
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
                    } else { tap_n = 0; tap_i = 0; }
                }
            } else acc2 = 0;
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
                }
                if(mag_ok && (sbus.r8(0x0C,0x02)&0x01)) {
                    mx=sbus.r16be(0x0C,0x03)*0.15f;
                    my=sbus.r16be(0x0C,0x05)*0.15f;
                    mz=sbus.r16be(0x0C,0x07)*0.15f;
                    sbus.r8(0x0C,0x09);
                }
                if(lidar_ok && lidar.dataReady()) dist=lidar.read();

                // ax controls SVF cutoff across nearly the full tilt range:
                // open at ax≤-0.2, closed at ax≥+0.9 — 1.1g span (old: 0.6g), wider sweep.
                // ax lowpassed here + per-sample slew in audio callback = smooth, no steps.
                {
                    static float axf = 0.f;
                    axf += (ax - axf) * 0.35f;        // settles in ~3 reads @50ms
                    float t = (0.9f - axf) / 1.1f;    // ax=-0.2→1(open) … +0.9→0(closed)
                    if(t > 1.f) t = 1.f;
                    if(t < 0.f) t = 0.f;
                    float fc = 500.f * powf(10.f, t); // 500Hz..5kHz log sweep
                    filter_f = 2.f * sinf(3.14159265f * fc / 48000.f);
                    cut_t    = t; // visualizer reads this
                    // FLT = DJ bandpass sweep: triangle LFO rides low→high→low,
                    // hearty Q, whole mix switches to the BANDPASS output.
                    // FSR = LFO speed.
                    if(fxOn(FX_FLT)) {
                        static float djp = 0.f;
                        djp += 0.025f + fx_fsr * 0.30f;
                        if(djp >= 1.f) djp -= 1.f;
                        float tri = 1.f - fabsf(2.f*djp - 1.f);
                        float fdj = 200.f * powf(17.5f, tri);   // 200Hz..3.5kHz log
                        filter_f = 2.f * sinf(3.14159265f * fdj / 48000.f);
                        fx_q  = 0.3f;
                        fx_bp = true;
                    } else { fx_q = 0.8f; fx_bp = false; }
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
            // normalized squeeze for the FX page: ~400 = touch, ~2600 = lean
            {
                float t = ((float)fsr - 400.f) / 2200.f;
                if(t < 0.f) t = 0.f;
                if(t > 1.f) t = 1.f;
                fx_fsr = t;
            }
        }
        // ── OLED — menu pages @20Hz ──────────────────────────────────────────
        now = System::GetNow();
        if(now >= next_oled) {
            next_oled = now + 50;
            display.Fill(false);
            bool saver = (now - last_enc > 10000) && !vol_mode && menu_anim == 0;

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
                static const char* MN[MENU_COUNT] = {"SYNTH", "DRUMS", "SAMPLER", "FX", "DASH"};
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
                n=0;
                n+=sprintf(row+n,"M:"); n+=fmtf(row+n,mx); row[n++]=' ';
                n+=fmtf(row+n,my); row[n++]=' '; n+=fmtf(row+n,mz); row[n]=0; orow(18,row);
                sprintf(row,"D:%4dmm S:%02X O:+%d",dist,lidar.lastStatus(),oct_shift); orow(27,row);
                sprintf(row,"E1:%-3s E2:%-3s",e1s,e2s);                orow(36,row);
                { int f=(int)(amp*19); if(f>19)f=19;
                  row[0]='['; for(int i=0;i<19;i++) row[1+i]=(i<f)?'#':'.'; row[20]=']'; row[21]=0; }
                orow(45,row);
                sprintf(row,"F:%4d SD:%-2s K:%02d :D",fsr,sd_ok?"OK":"--",__builtin_popcount(held_mask));
                orow(54,row);
            }
            else if(saver) {
                // SCREENSAVER — the motion+audio-reactive Julia set, fading in
                // after 10s of encoder stillness. Any encoder action wakes
                // the pages back up.
                // Accel x/y walk c straight through parameter space (pure shape
                // morph), gyro spin energy pushes the set outward/shatters it,
                // output level breathes it. Octave zone zooms mildly (clamped)
                // and buys extra iterations — depth, never blown-up pixels.
                static float vamp=0.f, sax=0.f, say=0.f, sgm=0.f;
                vamp += (out_amp - vamp) * 0.5f;
                sax  += (ax - sax) * 0.3f;
                say  += (ay - say) * 0.3f;
                float gm = sqrtf(gx*gx + gy*gy + gz*gz) / 500.f; // spin energy
                if(gm > 1.f) gm = 1.f;
                sgm += (gm - sgm) * 0.3f;
                float cr = -0.62f + sax * 0.35f;
                float ci =  0.42f + say * 0.35f;
                float push = 1.f + sgm * 0.25f + vamp * 0.10f;
                cr *= push; ci *= push;
                float sc = 1.f - 0.08f * (float)oct_shift;  // mild zoom per octave…
                if(sc < 0.70f) sc = 0.70f;                  // …never super zoomed
                int maxit = 12 + oct_shift * 5;             // zoom buys detail instead
                float stp = 0.052f * sc;
                // Bilateral symmetry drive: left/right mirrored, with a slow
                // breathing imbalance — even vibes, sometimes uneven, balances out
                static float symph = 0.f;
                symph += 0.011f; if(symph > 6.2831853f) symph -= 6.2831853f;
                float aw = 0.07f * sinf(symph);
                for(int py = 0; py < 32; py++) {
                    float zy0 = ((float)py - 15.5f) * stp;
                    for(int px = 0; px < 64; px++) {
                        float xs = (float)px - 31.5f;
                        float zx = fabsf(xs) * stp * (1.f + ((xs < 0.f) ? aw : -aw));
                        float zy = zy0;
                        int n = 0;
                        while(n < maxit) {
                            float x2 = zx*zx, y2 = zy*zy;
                            if(x2 + y2 > 4.f) break;
                            zy = 2.f*zx*zy + ci;
                            zx = x2 - y2 + cr;
                            n++;
                        }
                        // interior solid, escape bands striped, far outside dark
                        bool onpix = (n >= maxit) ? true : ((n >= 2) && (n & 1));
                        if(onpix)
                            display.DrawRect(px*2, py*2, px*2+1, py*2+1, true, true);
                    }
                }
            }
            else if(seq_view && menu == MENU_SMP) { // sampler sequencer — info only
                display.SetCursor(0, 0); display.WriteString("SAMPLE SEQ", Font_6x8, true);
                uint32_t sl = smp_len[sel_smp & 15];
                sprintf(row, "SLOT:%02d  %d.%01ds", (sel_smp & 15) + 1,
                        (int)(sl / 48000u), (int)((sl % 48000u) / 4800u));
                orow(11, row);
                sprintf(row, "%s  STEP %02d", seq_place ? ">PLACING<" : "PLAYING", gstep + 1);
                orow(21, row);
                sprintf(row, "STEPS:%02d", __builtin_popcount(smp_pat[sel_smp & 15]));
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
                sprintf(row, "SND: %s %s", CHN[sd], PADN[dr_pad[sd] & 15]);
                orow(11, row);
                sprintf(row, "%s  STEP %02d", seq_place ? ">PLACING<" : "PLAYING", gstep + 1);
                orow(21, row);
                sprintf(row, "K:%02d P:%02d S:%02d H:%02d",
                        __builtin_popcount(dr_pat[0]), __builtin_popcount(dr_pat[1]),
                        __builtin_popcount(dr_pat[2]), __builtin_popcount(dr_pat[3]));
                orow(31, row);
                sprintf(row, "BPM %3d", (int)(15000u/step_ms));
                orow(41, row);
                sprintf(row, "L=BACK R=%s RT=SND", seq_place ? "PLAY" : "PLACE");
                orow(54, row);
            }
            else if(menu == MENU_SYNTH) { // SYNTH page: mode radio + TAP
                display.SetCursor(49, 0); display.WriteString("SYNTH", Font_6x8, true);
                static const char* ML[3] = {"MAN","MCH","EUC"};
                for(int i = 0; i < 3; i++){
                    int x0 = 2 + i*43;
                    bool act = (seq_mode[ENG_SYNTH] == i);
                    display.DrawRect(x0, 14, x0+39, 26, true, act);
                    display.SetCursor(x0+11, 17);
                    display.WriteString(ML[i], Font_6x8, !act);
                    if(mode_cur == i)
                        display.DrawRect(x0-2, 12, x0+41, 28, true, false);
                }
                switch(seq_mode[ENG_SYNTH]){
                case SEQ_MANUAL:  sprintf(row, "PADS PLAY NOTES"); break;
                case SEQ_MACHINE: sprintf(row, "MACHINE GROOVING"); break;
                default:          sprintf(row, "EUC K:%2d PAD SETS K", eu_k_syn); break;
                }
                orow(38, row);
                sprintf(row, "TAP BPM:%3d", (int)(15000u/step_ms));
                display.SetCursor(31, 54); display.WriteString(row, Font_6x8, true);
                if(mode_cur == 3)
                    display.DrawRect(27, 51, 100, 62, true, false);
            }
            else if(menu == MENU_FX) { // FX page: active effect names + FSR amount
                display.SetCursor(58, 0); display.WriteString("FX", Font_6x8, true);
                static const char* FXN[16] = {
                    "STUT 8",  "STUT 8T", "STUT 16", "STUT 32",
                    "REVERSE", "CHORUS",  "PAN",     "CRUSH",
                    "HALF",    "DOUBLE",  "BUZZ",    "MUTE",
                    "FILTER",  "WASH",    "DIST",    "STOP",
                };
                uint16_t fe = fx_eff;
                int ln = 0;
                for(int b = 0; b < 16 && ln < 3; b++){
                    if((fe >> b) & 1){ orow(11 + ln*10, FXN[b]); ln++; }
                }
                if(ln == 0) orow(11, "HOLD PADS");
                int fw = (int)(fx_fsr * 124.f);
                display.DrawRect(2, 44, 126, 52, true, false);
                if(fw > 0) display.DrawRect(2, 44, 2 + fw, 52, true, true);
                sprintf(row, "FSR%3d%%   BPM:%3d", (int)(fx_fsr * 100.f), (int)(15000u/step_ms));
                orow(56, row);
                if(enc2.psw == 0 && enc1.psw != 0)   // automation REC armed
                    display.DrawRect(1, 1, 4, 4, true, true);
            }
            else if(menu == MENU_SMP) { // SAMPLER primary page: mode radio + TAP
                display.SetCursor(43, 0); display.WriteString("SAMPLER", Font_6x8, true);
                static const char* ML[3] = {"MAN","MCH","EUC"};
                for(int i = 0; i < 3; i++){
                    int x0 = 2 + i*43;
                    bool act = (seq_mode[ENG_SMP] == i);
                    display.DrawRect(x0, 14, x0+39, 26, true, act);
                    display.SetCursor(x0+11, 17);
                    display.WriteString(ML[i], Font_6x8, !act);
                    if(mode_cur == i)
                        display.DrawRect(x0-2, 12, x0+41, 28, true, false);
                }
                if(rec_slot >= 0) sprintf(row, "REC PAD %02d...", rec_slot + 1);
                else {
                    int nf = 0;
                    for(int s = 0; s < 16; s++) if(smp_len[s]) nf++;
                    sprintf(row, "SLOTS:%02d HOLD=REC", nf);
                }
                orow(38, row);
                sprintf(row, "TAP BPM:%3d", (int)(15000u/step_ms));
                display.SetCursor(31, 54); display.WriteString(row, Font_6x8, true);
                if(mode_cur == 3)
                    display.DrawRect(27, 51, 100, 62, true, false);
            }
            else { // DRUMS primary page: mode radio + TAP
                display.SetCursor(49, 0); display.WriteString("DRUMS", Font_6x8, true);
                static const char* ML[3] = {"MAN","MCH","EUC"};
                for(int i = 0; i < 3; i++){
                    int x0 = 2 + i*43;
                    bool act = (seq_mode[ENG_DRUM] == i);
                    display.DrawRect(x0, 14, x0+39, 26, true, act);
                    display.SetCursor(x0+11, 17);
                    display.WriteString(ML[i], Font_6x8, !act);
                    if(mode_cur == i)
                        display.DrawRect(x0-2, 12, x0+41, 28, true, false);
                }
                switch(seq_mode[ENG_DRUM]){
                case SEQ_MANUAL:  sprintf(row, "ROWS:HAT SNR PRC KCK"); break;
                case SEQ_MACHINE: sprintf(row, "SEQ LOOPING  R>OPEN"); break;
                default: sprintf(row, "K k%d s%d h%d b%d", dr_k[0], dr_k[1], dr_k[2], dr_k[3]); break;
                }
                orow(38, row);
                sprintf(row, "TAP BPM:%3d", (int)(15000u/step_ms));
                display.SetCursor(31, 54); display.WriteString(row, Font_6x8, true);
                if(mode_cur == 3)
                    display.DrawRect(27, 51, 100, 62, true, false);
            }

            // subtle audio-reactive shimmer on the utility pages — mirrored
            // pixel pairs along the top edge, count rides the output level
            // (FRACTAL and FX are already fully reactive)
            if(!vol_mode && menu_anim == 0 && !saver && menu != MENU_FX) {
                int np = (int)(out_amp * 9.f);
                for(int p = 0; p < np; p++){
                    uint32_t rr2 = lcg();
                    int sx = (int)(rr2 & 31);
                    int sy = (int)((rr2 >> 8) & 3);
                    display.DrawPixel(sx, sy, true);
                    display.DrawPixel(127 - sx, sy, true);   // even vibes
                }
            }

            // menu position pips, top-right (dashboard rows use full width — skip there)
            if(menu != MENU_DASH || menu_anim > 0) {
                for(int i = 0; i < MENU_COUNT; i++)
                    display.DrawRect(102 + i*4, 0, 104 + i*4, 2, true, i == menu);
            }

            display.Update();
        }
    }
}
