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
//   SYNTH   — synth engine page: MAN/MCH/EUC/SEQ radio + TAP. SEQ opens the
//             PO-style sequencer submenu (R-turn = write target, R-click =
//             place mode where pads are the 16 steps, playhead sweeps LEDs)
//   DRUMS   — same structure for the drum engine
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
    uint8_t  lab, psw;
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
struct Voice { float phase[5]={}, phaseInc=0, env=0, envTarget=0, pan=0.5f, relCoef=0.001f; };
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
enum { MENU_VIS = 0, MENU_SYNTH = 1, MENU_DRUM = 2, MENU_DASH = 3, MENU_COUNT = 4 };
static int      menu       = MENU_VIS;
static uint32_t step_ms    = 125;    // shared 16th-note grid; set by tap tempo

// Both engines run in parallel; pads control the focused one (= last
// SYNTH/DRUMS page visited), so you can play while watching the fractal.
enum { ENG_SYNTH = 0, ENG_DRUM = 1 };
enum { SEQ_MANUAL = 0, SEQ_MACHINE = 1, SEQ_EUCLID = 2, SEQ_STEP = 3 };
static int  eng_focus   = ENG_SYNTH;
static int  seq_mode[2] = { SEQ_MANUAL, SEQ_MANUAL }; // boot: both playable, silent
static int  mode_cur    = 0;         // engine-page cursor: 0..3 modes, 4 = TAP
static bool seq_view    = false;     // inside the sequencer submenu
static bool seq_place   = false;     // place mode: pads = 16 steps
static int  gstep       = 0;         // global 16-step grid position

// PO-style patterns. Drums: one 16-step bitmask per voice (full polyphony —
// all four voices can share a step). Synth: per-step bitmask of pad-notes
// 0..15 (chords on a step are fine). Patterns keep looping while mode = SEQ,
// wherever the UI is. dr_pad remembers each voice's last-played flavor pad so
// sequenced hits sound exactly like what you played.
static uint16_t dr_pat[4]   = {0,0,0,0};
static uint8_t  dr_pad[4]   = {14, 9, 5, 1};   // default flavors (kick = tight/small-spkr)
static int      sel_drum    = 0;               // last drum played = write target
static uint16_t syn_pat[16] = {};
static int      last_note   = 5;               // last synth pad-note = write target

// Euclidean params. Synth: pad number sets pulses 1..16. Drums: per-voice
// pulses (0 = voice off), pad row picks voice, column picks k, re-press = off.
static int eu_k_syn = 5, eu_rot_syn = 0;
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
static void keyNoteOn(uint8_t k){
    int v = -1;
    for(int i=0;i<12;i++) if(voices[i].envTarget==0.f && voices[i].env<0.02f){ v=i; break; }
    if(v < 0){ static int rr=0; v=rr; rr=(rr+1)%12; }
    key_voice[k] = (int8_t)v;
    int note = (3-(k/4))*4 + (k%4);
    for(int j=0;j<5;j++) voices[v].phase[j]=0.f;
    voices[v].phaseInc  = FLYD[note] * 0.5f * (float)(1u<<oct_shift) / 48000.f;
    voices[v].relCoef   = 0.0004f;
    voices[v].pan       = 0.2f + 0.6f*(float)(k%4)/3.f;
    voices[v].envTarget = 0.9f;
}
static void keyNoteOff(uint8_t k){
    if(key_voice[k]>=0){ voices[key_voice[k]].envTarget=0.f; key_voice[k]=-1; }
}
static void allKeysOff(){ for(uint8_t k=0;k<16;k++) keyNoteOff(k); }

// ── Drum engine — 808 body, modular weirdness ────────────────
// 4 voices: 0=kick (sine w/ exponential pitch drop), 1=snare (tone+noise),
// 2=hat (highpassed noise), 3=blip (two inharmonic partials, random pitch/pan —
// the "modular hit"). Triggered from main loop via stamp counters; audio
// callback latches params when stamp changes. Drums bypass LPF+chorus and hit
// the output saturation directly — punchy against the washy saws.
struct DrumGo {
    volatile uint32_t stamp = 0;
    float f=60.f, amp=0.8f, dec=0.9999f, noiz=0.f, drop=1.f, pan=0.5f;
};
static DrumGo dgo[4];
static void drumTrig(int v, float f, float amp, float dec, float noiz, float drop, float pan){
    DrumGo& g = dgo[v];
    g.f=f; g.amp=amp; g.dec=dec; g.noiz=noiz; g.drop=drop; g.pan=pan;
    g.stamp++;   // bumped last so the callback sees settled params
}

// Manual pads: row picks voice (bottom row = kick), column picks flavor.
static void drumPadHit(uint8_t k){
    int row = k/4, col = k%4;
    switch(row){
    case 3: { // kicks, deep → clicky: higher fundamental + more click noise +
              // steeper pitch drop = reads loud and punchy on tiny speakers
        static const float kf[4]   = {44.f,     55.f,     72.f,     95.f};
        static const float kdrop[4]= {2.8f,     3.5f,     4.2f,     5.5f};
        static const float knz[4]  = {0.03f,    0.07f,    0.14f,    0.28f};
        static const float kdec[4] = {0.99994f, 0.99992f, 0.99988f, 0.99982f};
        drumTrig(0, kf[col], 0.95f, kdec[col], knz[col], kdrop[col], 0.5f);
    } break;
    case 2: drumTrig(1, 165.f + 25.f*col, 0.75f, 0.9996f + 0.0001f*col, 0.8f, 1.4f, 0.42f + 0.05f*col); break;
    case 1: { static const float hd[4] = {0.9982f, 0.999f, 0.9995f, 0.9998f};   // closed → open
              drumTrig(2, 6000.f, 0.4f, hd[col], 1.f, 1.f, 0.62f); } break;
    default: { float f = 420.f + (float)(lcg()%200) + 600.f*col;
               drumTrig(3, f, 0.55f, 0.9994f, 0.06f, 1.8f, 0.15f + 0.7f*(float)(lcg()%100)/99.f); } break;
    }
}

// Euclid default hits (used when the pattern fires a voice)
static void drumDefaultHit(int v){
    switch(v){
    case 0: drumTrig(0, 60.f, 0.95f, 0.9999f, 0.12f, 4.f, 0.5f); break; // small-spkr punch
    case 1: drumTrig(1, 185.f, 0.7f, 0.99965f, 0.82f, 1.4f, 0.45f); break;
    case 2: drumTrig(2, 6000.f, 0.35f, 0.9988f, 1.f, 1.f, 0.62f); break;
    default: { float f = 300.f + (float)(lcg()%2100);
               drumTrig(3, f, 0.5f, 0.9994f, 0.08f, 1.8f, 0.1f + 0.8f*(float)(lcg()%100)/99.f); } break;
    }
}

// Machine groove: four-ish on the floor, backbeat snare, 8th hats w/ opens,
// sparse random blips. All probabilities, so it never loops exactly.
static void machineDrumStep(int s){
    uint32_t r = lcg()%100;
    if(s%8 == 0 || r < 6 || (s%4 == 2 && r < 18))
        drumTrig(0, 60.f, 0.95f, 0.9999f, 0.12f, 4.f, 0.5f); // small-spkr punch
    r = lcg()%100;
    if((s%8 == 4 && r < 88) || r < 4)
        drumTrig(1, 175.f + (float)(lcg()%40), 0.7f, 0.99965f, 0.82f, 1.4f, 0.45f);
    r = lcg()%100;
    if((s%2 == 0 && r < 72) || r < 20){
        bool open = (lcg()%100) < 12;
        drumTrig(2, 6000.f, 0.35f, open ? 0.9997f : 0.9988f, 1.f, 1.f, 0.62f);
    }
    r = lcg()%100;
    if(r < 9){
        float f = 300.f + (float)(lcg()%2100);
        drumTrig(3, f, 0.5f, 0.999f + (float)(lcg()%50)*0.00001f, 0.08f,
                 1.5f + (float)(lcg()%20)*0.1f, 0.1f + 0.8f*(float)(lcg()%100)/99.f);
    }
}

// Euclid pad k-tables per voice (column 0..3)
static const uint8_t EUK[4][4] = {
    {2, 4, 5, 8},     // kick
    {2, 3, 5, 7},     // snare
    {4, 8, 12, 16},   // hat
    {1, 2, 3, 5},     // blip
};

static volatile float mic_amp = 0.f;
// Chamberlin SVF F1 = 2*sin(π*fc/48000); updated from ax. Upright ≈ 1kHz.
static volatile float filter_f = 0.065f;
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t sz) {
    if(enc_ready){ ePoll(enc1); ePoll(enc2); }
    float pk=0.f, opk=0.f;
    // Q1 = damping = 1/Q_filter. 0.8 → Q≈1.25 — gentle musical resonance, no honk.
    static const float Q1 = 0.8f;
    static float lpL=0,bpL=0, lpR=0,bpR=0;
    // Cutoff slews per-sample toward the 50ms sensor target (τ≈14ms) — no zipper steps.
    static float ff_s = 0.065f;
    // Stereo chorus: ~14ms center delay swept ±3.75ms, quadrature LFOs L/R.
    static float    cbufL[1024] = {}, cbufR[1024] = {};
    static uint32_t cw   = 0;
    static float    clfo = 0.f;
    // drum voice run-state (latched from dgo[] on stamp change)
    struct DRun { float env, ph, f, dec, noiz, amp, panl, panr; };
    static DRun     drun[4]   = {};
    static uint32_t dstamp[4] = {};
    static uint32_t arng  = 0x12345678u;  // audio-rate noise rng
    static float    hatlp = 0.f;
    for(size_t i=0;i<sz;i++){
        float s=in[0][i]; if(s<0)s=-s; if(s>pk)pk=s;
        float mixL=0.f, mixR=0.f;
        for(int v=0;v<12;v++){
            float eCoef = (voices[v].envTarget > voices[v].env) ? 0.08f : voices[v].relCoef;
            voices[v].env += (voices[v].envTarget - voices[v].env) * eCoef;
            float saw=0.f;
            for(int k=0;k<5;k++){
                voices[v].phase[k] += voices[v].phaseInc * (1.f + DETUNE[k]);
                if(voices[v].phase[k] >= 1.f) voices[v].phase[k] -= 1.f;
                saw += voices[v].phase[k] * 2.f - 1.f;
            }
            float sig = saw * (0.07f * voices[v].env);
            mixL += sig * (1.f - voices[v].pan);
            mixR += sig * voices[v].pan;
        }
        ff_s += (filter_f - ff_s) * 0.0015f;
        // Chamberlin 2-pole resonant LPF — update lp first for correct phase
        lpL += ff_s * bpL;
        float hpL = mixL - lpL - Q1*bpL; bpL += ff_s * hpL;
        lpR += ff_s * bpR;
        float hpR = mixR - lpR - Q1*bpR; bpR += ff_s * hpR;
        // Saturation drive — thickens the saws, adds harmonics back above the LPF
        float dL = tanhf(lpL * 2.5f);
        float dR = tanhf(lpR * 2.5f);
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
                d.f    = dgo[v].f * dgo[v].drop;   // start high, glide down = 808 thump
                d.dec  = dgo[v].dec;  d.noiz = dgo[v].noiz; d.amp = dgo[v].amp;
                d.panl = 1.f - dgo[v].pan; d.panr = dgo[v].pan;
            }
            if(d.env > 0.0008f){
                d.env *= d.dec;
                d.f   += (dgo[v].f - d.f) * 0.0012f;
                d.ph  += d.f * (1.f/48000.f); if(d.ph >= 1.f) d.ph -= 1.f;
                float tone = sinf(6.2831853f * d.ph);
                if(v == 3) tone = 0.6f*tone + 0.45f*sinf(6.2831853f * d.ph * 2.717f); // inharmonic
                arng = arng*1664525u + 1013904223u;
                float nz = ((float)(arng >> 9) * (1.f/4194304.f)) - 1.f;
                if(v == 2){ hatlp += (nz - hatlp) * 0.12f; nz -= hatlp; }             // hat HP
                float sd = (tone * (1.f - d.noiz) + nz * d.noiz) * d.env * d.amp;
                drL += sd * d.panl; drR += sd * d.panr;
            }
        }
        // Final soft clip with makeup gain — peaks land near full scale, much louder out
        float oL = tanhf((dL * 0.8f + wL * 0.55f + drL) * 1.5f);
        float oR = tanhf((dR * 0.8f + wR * 0.55f + drR) * 1.5f);
        out[0][i] = oL;
        out[1][i] = oR;
        float am = (oL<0.f)?-oL:oL; if(am>opk) opk=am;
    }
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
            if(tbus.ssR1(TREL,0x00,0x01) == 0x55)
                trel_ok = ntSoftInit();
        }

        if(trel_ok && now >= next_trel) {
            next_trel = now + 60;

            for(uint8_t i=0;i<16;i++) pphase[i]+=DRIFT[i];
            uint8_t grb[48];
            if(seq_mode[eng_focus] == SEQ_STEP) {
                // PO step display: placed steps glow in the write-target's color,
                // playhead sweeps bright white. Brighter while placing.
                static const uint8_t VCOL[5][3] = {
                    {255, 60, 40},   // kick
                    {255,200, 40},   // snare
                    { 60,220,255},   // hat
                    {200, 60,255},   // blip
                    { 60,120,255},   // synth note
                };
                uint16_t placed = 0;
                const uint8_t* c;
                if(eng_focus == ENG_DRUM) {
                    placed = dr_pat[sel_drum & 3];
                    c = VCOL[sel_drum & 3];
                } else {
                    for(uint8_t i=0;i<16;i++) if(syn_pat[i]) placed |= (1u << i);
                    c = VCOL[4];
                }
                uint8_t sh = seq_place ? 1 : 2;
                for(uint8_t i=0;i<16;i++){
                    uint8_t r,g,b;
                    if(i == (uint8_t)gstep)        { r=g=b=140; }              // playhead
                    else if((placed>>i)&1)         { r=c[0]>>sh; g=c[1]>>sh; b=c[2]>>sh; }
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

            if(!ntShow(grb)) {
                trel_ok = false;
            } else {
                // After SHOW, SAMD09 drives WS2812B for ~530µs with I2C deaf.
                // 1ms gives 470µs margin — if SAMD09's SHOW ISR takes extra time
                // before starting WS2812B, we're still safe.
                System::Delay(1);

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
                                int em = seq_mode[eng_focus];
                                if(seq_place && em == SEQ_STEP) {
                                    // place mode: pads ARE the 16 steps — toggle write target
                                    if(eng_focus == ENG_SYNTH) syn_pat[key] ^= (1u << (last_note & 15));
                                    else                       dr_pat[sel_drum & 3] ^= (1u << key);
                                }
                                else if(eng_focus == ENG_SYNTH) {
                                    if(em == SEQ_EUCLID) { eu_k_syn = key + 1; eu_rot_syn = 0; }
                                    else { // playable in MAN/MACH/SEQ — last note = write target
                                        keyNoteOn(key);
                                        last_note = (3 - (key/4))*4 + (key%4);
                                    }
                                } else {
                                    int dv = 3 - (key/4), col = key%4;   // bottom row = kick
                                    if(em == SEQ_EUCLID) {
                                        int nk = (int)EUK[dv][col];
                                        dr_k[dv]   = (dr_k[dv] == nk) ? 0 : nk;  // re-press = voice off
                                        dr_rot[dv] = (dv == 1) ? 4 : (dv == 3 ? (int)(lcg()%16) : 0);
                                    } else { // playable in MAN/MACH/SEQ — last drum = write target
                                        drumPadHit(key);
                                        sel_drum = dv;
                                        dr_pad[dv] = key;
                                    }
                                }
                            } else if(edge == 2) {                   // EDGE_FALLING = released
                                held_mask &= ~(1u << key);
                                keyNoteOff(key);  // no-op unless key holds a manual synth note
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
            }

            if(now >= next_step){
                next_step = now + step_ms;
                gstep = (gstep + 1) & 15;

                // DRUMS — machine groove, euclid patterns, or the PO step pattern
                // (manual = pads only). Step hits replay each voice's last flavor pad.
                if(seq_mode[ENG_DRUM] == SEQ_MACHINE) machineDrumStep(gstep);
                else if(seq_mode[ENG_DRUM] == SEQ_EUCLID) {
                    for(int v=0; v<4; v++)
                        if(euclidHit(dr_k[v], dr_rot[v], gstep)) drumDefaultHit(v);
                }
                else if(seq_mode[ENG_DRUM] == SEQ_STEP) {
                    for(int v=0; v<4; v++)
                        if(dr_pat[v] & (1u << gstep)) drumPadHit(dr_pad[v] & 15);
                }

                // SYNTH — machine: logarithmic poly roll (20% silence, 52% mono,
                // 17% duo, 7% trio, 4% quad). Euclid: one note per pattern hit.
                // Step: every note placed on this step (chords fine, capped at 8).
                // Manual: pads only, nothing fires from the grid.
                int nv = 0;
                int step_notes[8];
                if(seq_mode[ENG_SYNTH] == SEQ_MACHINE){
                    uint32_t roll = lcg() % 100;
                    nv = (roll < 20) ? 0 :
                         (roll < 72) ? 1 :
                         (roll < 89) ? 2 :
                         (roll < 96) ? 3 : 4;
                    for(int n=0; n<nv; n++) step_notes[n] = (int)(lcg() % 28u);
                } else if(seq_mode[ENG_SYNTH] == SEQ_EUCLID
                          && euclidHit(eu_k_syn, eu_rot_syn, gstep)){
                    nv = 1;
                    step_notes[0] = (int)(lcg() % 28u);
                } else if(seq_mode[ENG_SYNTH] == SEQ_STEP){
                    uint16_t m = syn_pat[gstep & 15];
                    for(int nb=0; nb<16 && nv<8; nb++)
                        if(m & (1u << nb)) step_notes[nv++] = nb;
                }

                for(int n=0; n<nv; n++){
                    // prefer free voice; steal nextv if all busy
                    int v = nextv;
                    for(int i=0;i<12;i++){
                        if(!vplay[(nextv+i)%12]){ v=(nextv+i)%12; break; }
                    }
                    nextv = (v+1)%12;

                    int note = step_notes[n];
                    for(int k=0;k<5;k++) voices[v].phase[k] = 0.f;
                    // 0.5f = base tuning 1 octave down; lidar zone raises 0/+1/+2 octaves.
                    // Applied at trigger only — held notes never bend.
                    voices[v].phaseInc  = FLYD[note] * 0.5f * (float)(1u << oct_shift) / 48000.f;
                    voices[v].envTarget = 0.6f + (float)(lcg()%35)/100.f;
                    voices[v].pan       = 0.15f + (float)(lcg()%71)/100.f;
                    // Random release: log-distributed 0.1s (coef≈0.00144) to 3s (coef≈0.000048)
                    float rr = (float)(lcg()%64) / 63.f;
                    voices[v].relCoef  = 0.000048f * powf(30.f, rr);
                    vplay[v] = true;
                    vrel[v]  = now + 100;
                }
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
            if(acc1 >= 2 || acc1 <= -2) {
                menu_anim = 6; menu_anim_dir = (acc1 > 0) ? +1 : -1;
                menu = (menu + (acc1 > 0 ? 1 : MENU_COUNT-1)) % MENU_COUNT;
                acc1 = 0;
                seq_view = false; seq_place = false; mode_cur = 0;   // leave any submenu
                if(menu == MENU_SYNTH) eng_focus = ENG_SYNTH;        // pads follow page
                if(menu == MENU_DRUM)  eng_focus = ENG_DRUM;
            }

            // LEFT CLICK = back out of submenu to the primary page
            if(c1 && seq_view) { seq_view = false; seq_place = false; }

            bool eng_page = (menu == MENU_SYNTH || menu == MENU_DRUM);
            if(eng_page && seq_view) {
                // Sequencer submenu: R-turn cycles the write target, R-click
                // toggles place mode (pads = 16 steps).
                acc2 += d2;
                if(acc2 >= 2 || acc2 <= -2) {
                    int dir = (acc2 > 0) ? 1 : -1;
                    if(eng_focus == ENG_DRUM) sel_drum  = (sel_drum + 4 + dir) % 4;
                    else                      last_note = (last_note + 16 + dir) & 15;
                    acc2 = 0;
                }
                if(c2) seq_place = !seq_place;
            }
            else if(eng_page) {
                acc2 += d2;
                if(acc2 >= 2)       { mode_cur = (mode_cur + 1) % 5; acc2 = 0; }
                else if(acc2 <= -2) { mode_cur = (mode_cur + 4) % 5; acc2 = 0; }
                if(c2 && mode_cur < 4) {            // mode radio: MAN/MACH/EUC/SEQ
                    if(seq_mode[eng_focus] != mode_cur){ seq_mode[eng_focus] = mode_cur; allKeysOff(); }
                    if(mode_cur == SEQ_STEP) { seq_view = true; seq_place = false; }
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
        }
        // ── OLED — menu pages @20Hz ──────────────────────────────────────────
        now = System::GetNow();
        if(now >= next_oled) {
            next_oled = now + 50;
            display.Fill(false);

            if(menu_anim > 0) {
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
                static const char* MN[4] = {"FRACTAL", "SYNTH", "DRUMS", "DASH"};
                display.SetCursor(64 - (int)strlen(MN[menu]) * 3, 29);
                display.WriteString(MN[menu], Font_6x8, true);
            }
            else if(menu == MENU_DASH) {
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
            else if(menu == MENU_VIS) {
                // Motion+audio-reactive Julia set. No rotation: accel x/y walk c
                // straight through parameter space (pure shape morph), gyro spin
                // energy pushes the set outward/shatters it, output level breathes
                // it. Octave zone zooms mildly (hard-clamped) and buys extra
                // iterations — deeper complexity, never blown-up pixels.
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
                for(int py = 0; py < 32; py++) {
                    float zy0 = ((float)py - 15.5f) * stp;
                    for(int px = 0; px < 64; px++) {
                        float zx = ((float)px - 31.5f) * stp, zy = zy0;
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
            else if(seq_view) { // sequencer submenu — PO-style 4x4 step grid + info
                static const char* DRN[4]  = {"KICK","SNR","HAT","BLIP"};
                static const char* NN[16]  = {"F3","G3","A3","B3","C4","D4","E4","F4",
                                              "G4","A4","B4","C5","D5","E5","F5","G5"};
                uint16_t placed = 0;
                if(eng_focus == ENG_DRUM) placed = dr_pat[sel_drum & 3];
                else for(int i=0;i<16;i++) if(syn_pat[i]) placed |= (1u << i);
                for(int i = 0; i < 16; i++){
                    int x0 = 2 + (i%4)*14, y0 = 2 + (i/4)*14;
                    display.DrawRect(x0, y0, x0+11, y0+11, true, (placed>>i)&1);
                    if(i == gstep)   // playhead halo sweeps the grid
                        display.DrawRect(x0-2, y0-2, x0+13, y0+13, true, false);
                }
                display.SetCursor(64, 2);
                display.WriteString(eng_focus ? "DRUM SEQ" : "SYN SEQ", Font_6x8, true);
                if(eng_focus == ENG_DRUM) sprintf(row, "SND:%s", DRN[sel_drum & 3]);
                else                      sprintf(row, "SND:%s",  NN[last_note & 15]);
                display.SetCursor(64, 14); display.WriteString(row, Font_6x8, true);
                display.SetCursor(64, 26);
                display.WriteString(seq_place ? ">PLACE<" : "PLAY", Font_6x8, true);
                sprintf(row, "BPM%4d", (int)(15000u/step_ms));
                display.SetCursor(64, 38); display.WriteString(row, Font_6x8, true);
                display.SetCursor(64, 48); display.WriteString("L=BACK", Font_6x8, true);
                display.SetCursor(64, 56);
                display.WriteString(seq_place ? "R=PLAY" : "R=PLACE", Font_6x8, true);
            }
            else { // engine primary page (SYNTH or DRUMS): mode radio + TAP
                display.SetCursor(eng_focus ? 49 : 49, 0);
                display.WriteString(eng_focus ? "DRUMS" : "SYNTH", Font_6x8, true);
                static const char* ML[4] = {"MAN","MCH","EUC","SEQ"};
                for(int i = 0; i < 4; i++){
                    int x0 = 2 + i*32;
                    bool act = (seq_mode[eng_focus] == i);
                    display.DrawRect(x0, 14, x0+27, 26, true, act);
                    display.SetCursor(x0+5, 17);
                    display.WriteString(ML[i], Font_6x8, !act);
                    if(mode_cur == i)
                        display.DrawRect(x0-2, 12, x0+29, 28, true, false);
                }
                switch(seq_mode[eng_focus]){
                case SEQ_MANUAL:  sprintf(row, "PADS PLAY %s", eng_focus ? "DRUMS" : "NOTES"); break;
                case SEQ_MACHINE: sprintf(row, "MACHINE GROOVING"); break;
                case SEQ_EUCLID:
                    if(eng_focus == ENG_SYNTH) sprintf(row, "EUC K:%2d PAD SETS K", eu_k_syn);
                    else sprintf(row, "K k%d s%d h%d b%d", dr_k[0], dr_k[1], dr_k[2], dr_k[3]);
                    break;
                default:          sprintf(row, "SEQ LOOPING  R>OPEN"); break;
                }
                orow(38, row);
                sprintf(row, "TAP BPM:%3d", (int)(15000u/step_ms));
                display.SetCursor(31, 54); display.WriteString(row, Font_6x8, true);
                if(mode_cur == 4)
                    display.DrawRect(27, 51, 100, 62, true, false);
            }

            // menu position pips, top-right (dashboard rows use full width — skip there)
            if(menu != MENU_DASH || menu_anim > 0) {
                for(int i = 0; i < MENU_COUNT; i++)
                    display.DrawRect(112 + i*4, 0, 114 + i*4, 2, true, i == menu);
            }

            display.Update();
        }
    }
}
