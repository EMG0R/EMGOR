// INPUT_TEST — PMOR bring-up step 2: joystick + encoder, verified on the NeoTrellis.
//
// No serial needed. Everything is reported on the pads:
//
//   JOYSTICK MOVE  -> a bright cursor pad tracks the stick across the 4x4 grid.
//                     Full left/right/up/down should reach all four edges.
//   ENCODER TURN   -> cursor colour walks the lava palette. One detent = one step.
//   ENCODER CLICK  -> cursor turns WHITE while held.
//   JOYSTICK CLICK -> whole grid flashes the cursor colour while held.
//   TILT L/R       -> a whole COLUMN lights and tracks accelerometer X.
//                     Level = column 1-2. Tilt left/right walks it to an edge.
//   SD CARD        -> whole grid GREEN for 2s at boot = mounted + write/read
//                     round-trip verified. RED = failed. Pad 15 (bottom-right)
//                     then holds that verdict permanently.
//   FSR PRESS      -> takes over the grid as a 16-step bar, AUTO-RANGED to
//                     the min/max this sensor actually produces, so the full
//                     16 pads get used. BAR LENGTH = how hard, relative to
//                     your own range. BAR COLOUR = raw 12-bit value:
//                       BLUE  < 1200  (under PMOR's velocity floor)
//                       GREEN 1200-2600 (inside PMOR's window)
//                       RED   > 2600  (over the top of it)
//                     Click the joystick while pressing to reset the range.
//                     LOCKED: 330R pulldown, CUBED curve, full scale at
//                     13/16 of span -- a firm press hits max, harder clamps.
//   AUDIO          -> straight passthrough, input jack -> output jack.
//                     No plug in IN  = MIC is the source (hardware switch).
//                     No plug in OUT = SPEAKERS play (hardware switch).
//                     ENCODER sets monitor gain. Starts at 6% because
//                     mic -> speakers in one box WILL howl if you open it up.
//   OLED           -> lidar distance large, status below. Old sparse page, 10fps: encoder count, joystick raw,
//                     FSR raw, IMU address + accel X, SD + trellis verdicts.
//                     DELIBERATELY sparse/dark -- lit pixel count is what
//                     kills USB on this panel, not bus traffic.
//   background     -> dim lava drift (proves the trellis is still alive)
//
//   Onboard LED: solid = all good
//                4Hz flutter = trellis OK but MPU6050 not found
//                2Hz blink   = trellis never ACKed
//
//   BUTTONLESS DFU: printf 'dfu\n' > /dev/cu.usbmodem*   (no BOOT+RESET needed)
//
// PINS (locked, see WIRING-v2.md)
//   Trellis  tbus   D11 = SCL, D12 = SDA          (module on 5V)
//   Encoder  KY-040 D0 = CLK/A, D1 = DT/B, D2 = SW  (breakout on 3V3 DIGITAL, pin 38)
//   Joystick        D16 = X (A1), D17 = Y (A2), D24 = SW
//   MPU6050  sbus   D13 = SCL, D14 = SDA  (VCC 3V3, GND DGND)
//   microSD  bitbang D25 = MISO, D26 = CLK, D27 = MOSI, D28 = CS (VCC 3V3)
//   FSR             D15 (A0): 3V3 -> FSR -> D15 -> 4k7 -> DGND, +100nF to GND
//                   address auto-detected, 0x68 or 0x69 — leave AD0 unwired
//                   pot on 3V3 ANALOG (pin 21) + AGND (pin 20); button to DGND
//
//   D17 is NOT 5V tolerant. 3V3 only.
//
// BBus / NeoTrellis driver / Enc + ePoll + eInit are VERBATIM from
// POCKETGORATOR_CODE.cpp (Aug 18 build). Only edit: usbDelay -> System::Delay,
// since there is no USB stack here.

#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "../POCKETGORATOR_CODE/oled_pmor.h"
#include "../POCKETGORATOR_CODE/VL53L1X_mini.h"
#include "fatfs.h"
#include "ff_gen_drv.h"
#include "../_bringup_dfu.h"
#include "../_fsr_scale.h"

using namespace daisy;

static DaisySeed hw;

// PMOR's bit-banged 4-wire SPI SSD1309 driver, used verbatim.
using MyOled = OledDisplay<pmor::BitBangSSD130x128x64>;
static MyOled display;

// PMOR's BBus pumps USB per-byte so bit-banged I2C can't starve TinyUSB.
// No USB in this build -- no-op, kept so the driver stays byte-identical.
extern "C" void pmor_usb_service(void) {}   // matches oled_pmor.h's declaration
// PMOR's SD init pumps TinyUSB during the slow card-init spec delays.
// No TinyUSB here either -- libDaisy's CDC is interrupt-driven.
static inline void tud_task(void) {}

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

static BBus tbus;   // NeoTrellis — D11=SCL, D12=SDA
static BBus sbus;   // sensors    — D13=SCL, D14=SDA

// VL53L1X — verbatim from POCKETGORATOR_CODE.cpp:279-280
static VL53L1X_Mini lidar;
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg (uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.r16a(0x29,reg,d,n); }

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
static inline void usbDelay(uint32_t ms) { System::Delay(ms); }

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

struct Enc {
    dsy_gpio a, b, sw;
    uint8_t  lab;
    volatile uint8_t psw;   // current switch level (0 = held) — read by main loop
    volatile int8_t delta;
    volatile bool   clicked;
};

static Enc enc1;   // PMOR declares `static Enc enc1, enc2;` — enc2 is the joystick now

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
// MPU6050 — 6-axis (accel + gyro). Replaces the MPU9250; the 9250 was
// register-compatible for these, so sbus's existing helpers work unchanged.
// Address 0x68 (AD0 low). WHO_AM_I reads 0x68 here, vs 0x71 on the 9250.
// ============================================================
// NB: cannot be called MPU — CMSIS core_cm7.h defines that as the
// Memory Protection Unit pointer.
//
// ADDRESS IS AUTO-DETECTED. AD0 low/floating = 0x68, AD0 high = 0x69.
// mpuInit() probes both and keeps whichever answers, so AD0 never has to be
// wired and a board that comes up at either address just works.
static uint8_t MPU6050 = 0x68;

#define MPU_SMPLRT_DIV   0x19
#define MPU_CONFIG       0x1A
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_ACCEL_XOUT_H 0x3B
#define MPU_GYRO_XOUT_H  0x43
#define MPU_PWR_MGMT_1   0x6B
#define MPU_WHO_AM_I     0x75

// Probe one address. WHO_AM_I on a genuine MPU6050 reads 0x68 at BOTH
// addresses (the register reports the address with the AD0 bit masked off),
// but clones return all sorts — 0x70, 0x72, 0x73, 0x98 are all in the wild.
// So treat anything that is not a floating/stuck bus (0x00 / 0xFF) as present.
static bool mpuProbe(uint8_t addr)
{
    uint8_t id = sbus.r8(addr, MPU_WHO_AM_I);
    return (id != 0x00 && id != 0xFF);
}

static bool mpuInit()
{
    // --- find it: 0x68 (AD0 low/floating) or 0x69 (AD0 high) ---------------
    if     (mpuProbe(0x68)) MPU6050 = 0x68;
    else if(mpuProbe(0x69)) MPU6050 = 0x69;
    else                    return false;

    sbus.w8(MPU6050, MPU_PWR_MGMT_1, 0x80);   // DEVICE_RESET
    System::Delay(100);
    sbus.w8(MPU6050, MPU_PWR_MGMT_1, 0x01);   // wake, clock = PLL w/ X gyro ref
    System::Delay(10);

    sbus.w8(MPU6050, MPU_SMPLRT_DIV,   0x07); // 1kHz / 8 = 125Hz
    sbus.w8(MPU6050, MPU_CONFIG,       0x03); // DLPF 44Hz — smooths hand tremor
    sbus.w8(MPU6050, MPU_GYRO_CONFIG,  0x00); // +/-250 deg/s
    sbus.w8(MPU6050, MPU_ACCEL_CONFIG, 0x00); // +/-2g  -> 16384 LSB/g
    return true;
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

// ------------------------------------------------------------
// SD self-test: link driver, mount, write a file, read it back,
// byte-compare. Returns true only if every step passed.
// ------------------------------------------------------------
static char  sd_path[4] = {};
static FATFS sd_fs;

static bool sdSelfTest()
{
    if(FATFS_LinkDriver(&SPISD_Driver, sd_path) != 0) return false;
    if(f_mount(&sd_fs, sd_path, 1) != FR_OK)          return false;

    static const char msg[] = "PMOR SD OK";
    char  back[sizeof(msg)] = {};
    FIL   f;
    UINT  n = 0;

    if(f_open(&f, "PMORTEST.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    FRESULT wr = f_write(&f, msg, sizeof(msg), &n);
    f_close(&f);
    if(wr != FR_OK || n != sizeof(msg)) return false;

    if(f_open(&f, "PMORTEST.TXT", FA_READ) != FR_OK) return false;
    FRESULT rd = f_read(&f, back, sizeof(back), &n);
    f_close(&f);
    if(rd != FR_OK || n != sizeof(msg)) return false;

    for(uint32_t i = 0; i < sizeof(msg); i++)
        if(back[i] != msg[i]) return false;

    return true;
}

// ============================================================
// Audio — straight mic/line passthrough
// ============================================================
// IN comes from the input jack. With NO plug inserted the jack's NC contacts
// (RM/LM) connect the MAX4466 to TIP/RING, so the mic is the source. Insert a
// cable and the springs lift and the external input takes over. Hardware
// auto-switch, no code involved.
//
// OUT goes to the output jack TIP/RING; its NC contacts feed the PAM8403, so
// the speakers play whenever headphones are NOT plugged in.
//
// g_mon_gain starts LOW on purpose: mic -> speakers in one small box is an
// acoustic feedback loop. Turn the ENCODER to raise it and find the point
// where it starts to howl.
static volatile float g_mon_gain = 0.06f;

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    const float g = g_mon_gain;
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i] * g;
        out[1][i] = in[1][i] * g;
    }
}

// ============================================================
// main
// ============================================================
int main(void)
{
    hw.Configure();
    hw.Init(true);   // 480MHz boost — MATCHES PMOR (POCKETGORATOR_CODE.cpp:3343)

    // Buttonless DFU FIRST -- USB must enumerate before the slow peripheral
    // init below, or the host gives up (PocketPassthru-FLASHING.md).
    // Reflash with:  printf 'dfu\n' > /dev/cu.usbmodem*
    bringup_dfu_init(hw);

    // --- OLED (4-wire SPI, NOT I2C) --------------------------------------
    // MODULE SILKSCREEN vs FUNCTION -- these boards keep I2C-style labels on
    // pins that act as SPI when the module is strapped for SPI mode:
    //
    //     silkscreen        function          Daisy
    //     SCL           ->  SPI clock  (SCLK)  D8   (PG11)
    //     SDA           ->  SPI data   (MOSI)  D10  (PB5)
    //     CS            ->  chip select        D7   (PG10)
    //     DC            ->  data/command       D3
    //     RES           ->  reset              D4
    //     VCC/GND       ->  3V3 (pin 38) / DGND (pin 40)
    //
    // The presence of CS and DC is the proof this is SPI: I2C has neither.
    // Do NOT hang this on tbus (D11/D12) with the NeoTrellis -- this driver
    // has no I2C path, and that bus already produced the seesaw clock-stretch
    // bug that killed USB in 35-120s.
    //
    // If the panel stays dark with correct wiring, check the module's mode
    // straps (BS1/BS2 jumpers, or an R1/R3 0-ohm pair). Strapped for I2C, it
    // ignores every SPI byte.
    //
    // Pins VERIFIED 2026-08-15 against libDaisy. An earlier pin map here
    // clocked MISO and sent data to an unconnected pin -- the panel could not
    // have lit in ANY electrical configuration. Do not "fix" these from memory.
    //
    // nop_delay = 24 (~2-3MHz). 0 overruns the SSD1309 and leaves it DARK;
    // 8 rides its ~10MHz ceiling and sporadically miswrites bytes.
    // DARK panel  => too fast / wrong pins.  GARBAGE on panel => too fast.
    //
    // Contrast is forced to 0x01 inside InitPanel. DO NOT RAISE IT: the
    // panel module's boost converter under lit-pixel load kills the Daisy's
    // USB (device off bus until power cycle). It scales with LIT PIXEL COUNT,
    // not bus traffic -- full-black blits are harmless, half-white kills even
    // at minimum contrast. That is why this screen is sparse text only.
    MyOled::Config oc;
    oc.driver_config.sclk      = seed::D8;
    oc.driver_config.mosi      = seed::D10;
    oc.driver_config.cs        = seed::D7;
    oc.driver_config.dc        = seed::D3;
    oc.driver_config.reset     = seed::D4;
    // 24 is PMOR's shipping value. If the panel shows GARBAGE / "pixely",
    // the clock is still too fast for it -- raise this. 8 was measured to
    // produce sporadic miswritten bytes (2026-08-16); 24 fixed it there.
    // Ladder if 24 is not enough on this panel: 32 -> 48 -> 64.
    oc.driver_config.nop_delay = 48;
    display.Init(oc);
    display.Fill(false);
    display.Update();

    // --- ADC: joystick X (D16/A1), Y (D17/A2) ---------------------------
    AdcChannelConfig adc_cfg[3];
    adc_cfg[0].InitSingle(hw.GetPin(16));   // JOY_X -> Get(0)
    adc_cfg[1].InitSingle(hw.GetPin(17));   // JOY_Y -> Get(1)
    adc_cfg[2].InitSingle(hw.GetPin(15));   // FSR   -> Get(2)   (A0)
    hw.adc.Init(adc_cfg, 3);
    hw.adc.Start();

    // --- Encoder on D0/D1/D2 (internal pullups, KY-040 needs no resistors)
    eInit(enc1, hw.GetPin(0), hw.GetPin(1), hw.GetPin(2));

    // --- Joystick button on D24, internal pullup, active low -------------
    dsy_gpio joy_sw = {hw.GetPin(24), DSY_GPIO_MODE_INPUT, DSY_GPIO_PULLUP};
    dsy_gpio_init(&joy_sw);

    // --- Sensor bus: D13 = SCL, D14 = SDA -------------------------------
    sbus.init(hw.GetPin(13), hw.GetPin(14));
    sbus.us = 3;                    // ~160kHz, same as PMOR
    bool imu_ok = mpuInit();

    // VL53L1X lidar (0x29) — same as POCKETGORATOR_CODE.cpp:3956-3957
    bool lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(100);

    // --- Trellis: D11 = SCL, D12 = SDA ----------------------------------
    tbus.init(hw.GetPin(11), hw.GetPin(12));
    bool ok = ntInit();

    // --- SD self-test: D25=MISO D26=CLK D27=MOSI D28=CS -----------------
    bool sd_ok = sdSelfTest();

    // Unmissable boot verdict: whole grid GREEN if the SD mounted AND a
    // write/read/compare round-tripped, RED if any step failed. ~2s, then
    // pad 15 (bottom-right) keeps showing the same verdict permanently.
    if(ok) {
        uint8_t grb[48];
        for(uint8_t i=0;i<16;i++) {
            grb[i*3+0] = sd_ok ? 180 : 0;    // G
            grb[i*3+1] = sd_ok ? 0 : 180;    // R
            grb[i*3+2] = 0;                  // B
        }
        ntShow(grb);
        System::Delay(2000);
    }

    // --- audio last, once the slow peripheral init is done ---------------
    hw.SetAudioBlockSize(64);
    hw.StartAudio(AudioCallback);

    static const uint8_t DRIFT[16]={1,2,1,2, 2,1,2,1, 1,2,2,1, 2,1,1,2};
    uint8_t  pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};
    uint32_t next_trel = 0;
    uint32_t next_oled = 0;
    int32_t  enc_total = 0;
    int16_t  last_ax   = 0;
    uint16_t dist      = 0;
    uint8_t  menu_sel  = 0;
    bool     led = false;
    uint8_t  hue = 0;          // driven by encoder rotation
    uint16_t fsr12 = 0;        // FSR, 0..4095 (PMOR scale)
    // Auto-range: an FSR never reaches either rail (rest is ~open, fully
    // pressed still leaves 1.5-2k), so a fixed 0..4095 bar wastes both ends.
    // Track what THIS sensor+resistor actually produces and stretch the bar
    // across it. Click the joystick to reset the range.
    FsrScale fsr;              // canonical scaling: 330R, cubed, 13/16 ceiling

    while(1)
    {
        // Poll the encoder as fast as possible -- ntShow() blocks ~4ms per
        // frame, which is still far shorter than a physical detent.
        ePoll(enc1);
        bringup_dfu_poll();

        uint32_t now = System::GetNow();

        if(!ok) {
            if(now >= next_trel) { next_trel = now + 250; led = !led; hw.SetLed(led); }
            static uint32_t retry = 0;
            if(now >= retry) { retry = now + 2000; ok = ntInit(); }
            continue;
        }
        hw.SetLed(imu_ok ? true : ((now / 120) & 1));   // flutter = IMU not found

        // --- lidar, as POCKETGORATOR_CODE.cpp:4729 ----------------------
        if(lidar_ok && lidar.dataReady()) dist = lidar.read();

        // --- OLED sensor menu, 10fps, sparse text only -------------------
        // Same idea as PMOR's status page, with enc1 + joystick replacing the
        // old two-encoder nav. Encoder OR joystick-Y moves the > cursor.
        if(now >= next_oled) {
            next_oled = now + 100;
            char ln[26];

            int jy = (int)(hw.adc.Get(1) >> 12);          // 0..15
            if(jy < 5 && menu_sel > 0) menu_sel--;
            if(jy > 10 && menu_sel < 5) menu_sel++;

            display.Fill(false);

            // --- LIDAR, large ---------------------------------------------
            if(lidar_ok) snprintf(ln, sizeof(ln), "%u mm", (unsigned)dist);
            else         snprintf(ln, sizeof(ln), "LID ---");
            display.SetCursor(0, 0);
            display.WriteString(ln, Font_11x18, true);

            // --- compact status below -------------------------------------
            snprintf(ln, sizeof(ln), "MON %u%%", (unsigned)(g_mon_gain * 100.f + 0.5f));
            display.SetCursor(0, 22); display.WriteString(ln, Font_6x8, true);

            snprintf(ln, sizeof(ln), "FSR %u  ENC %+ld", (unsigned)fsr12, (long)enc_total);
            display.SetCursor(0, 32); display.WriteString(ln, Font_6x8, true);

            snprintf(ln, sizeof(ln), "JOY %u %u", (unsigned)(hw.adc.Get(0) >> 8),
                     (unsigned)(hw.adc.Get(1) >> 8));
            display.SetCursor(0, 42); display.WriteString(ln, Font_6x8, true);

            snprintf(ln, sizeof(ln), "IMU %s SD %s", imu_ok ? "OK" : "--", sd_ok ? "OK" : "--");
            display.SetCursor(0, 52); display.WriteString(ln, Font_6x8, true);

            display.Update();
        }

        if(now < next_trel) continue;
        next_trel = now + 80;

        // --- consume encoder movement -----------------------------------
        // KY-040 gives 4 quadrature counts per detent. If one physical click
        // moves the colour by 4 steps instead of 1, your clone is 1-count --
        // change this shift to 0.
        int8_t d = enc1.delta;
        enc1.delta = 0;
        hue = (uint8_t)(hue + (int8_t)(d * 8));
        enc_total += d;
        if(d) {
            float g = g_mon_gain + (float)d * 0.02f;
            if(g < 0.f)  g = 0.f;
            if(g > 1.f)  g = 1.f;
            g_mon_gain = g;
        }


        bool enc_held = (enc1.psw == 0);              // active low
        bool joy_held = (dsy_gpio_read(&joy_sw) == 0);

        // --- joystick -> cursor cell ------------------------------------
        // Absolute mapping across full ADC travel, so this also shows you
        // whether the stick actually reaches both rails.
        uint16_t x = hw.adc.Get(0);
        uint16_t y = hw.adc.Get(1);
        uint8_t  col = (uint8_t)(x >> 14);            // 0..3
        uint8_t  row = (uint8_t)(y >> 14);            // 0..3
        if(col > 3) col = 3;
        if(row > 3) row = 3;
        uint8_t  cursor = (uint8_t)(row * 4 + col);

        // --- IMU: tilt -> ball cell, shake -> background speed -----------
        // +/-2g full scale = 16384 LSB/g. Using +/-0.5g (8192) as full
        // deflection, so ~30 degrees of tilt walks the ball edge to edge.
        // --- FSR: same 12-bit scale PMOR uses (GetFloat * 4095) so the
        // numbers here are directly comparable to its 1200..2600 velocity
        // window. Median-of-5 like PMOR, to kill the single-sample spikes
        // that would otherwise make the bar flicker.
        {
            static uint16_t fbuf[5] = {};
            static uint8_t  fidx = 0;
            fbuf[fidx] = (uint16_t)(hw.adc.GetFloat(2) * 4095.f);
            fidx = (uint8_t)((fidx + 1) % 5);
            uint16_t srt[5];
            for(uint8_t a=0;a<5;a++) srt[a]=fbuf[a];
            for(uint8_t a=0;a<5;a++)
                for(uint8_t b=(uint8_t)(a+1);b<5;b++)
                    if(srt[b]<srt[a]) { uint16_t t=srt[a]; srt[a]=srt[b]; srt[b]=t; }
            fsr12 = srt[2];                       // median
        }

        // ACCEL X ONLY. Lights one whole COLUMN, tracking tilt left<->right.
        // +/-2g full scale = 16384 LSB/g; using +/-0.5g (8192) as full
        // deflection, so ~30 degrees of tilt walks the column edge to edge.
        uint8_t axcol = 255;                     // 255 = no IMU, don't draw
        if(imu_ok) {
            int16_t ax = sbus.r16be(MPU6050, MPU_ACCEL_XOUT_H);
            last_ax = ax;
            int32_t c  = ((int32_t)ax + 8192) * 4 / 16384;
            if(c < 0) c = 0;
            if(c > 3) c = 3;
            axcol = (uint8_t)c;
        }

        // --- render ------------------------------------------------------
        for(uint8_t i=0;i<16;i++) pphase[i] += DRIFT[i];

        uint8_t cr, cg, cb;
        lavaColor(hue, cr, cg, cb);
        if(enc_held) { cr = cg = cb = 255; }          // encoder click = white

        uint8_t grb[48];

        // ---- FSR BAR ----------------------------------------------------
        // While the FSR reads anything, it takes over the whole grid as a
        // 16-step bar: each pad = 256 counts of the 0..4095 range.
        //   pads 0-4   BLUE  = below PMOR's 1200 velocity floor (wasted)
        //   pads 5-10  GREEN = inside PMOR's 1200..2600 window  (what you want)
        //   pads 11-15 RED   = above 2600, saturating
        // A normal press should light into GREEN and stop. If a light touch
        // already reaches RED your pulldown is too big; if a hard press never
        // leaves BLUE it is too small.
        if(fsr12 > 120) {
            // Canonical conditioning -- same header PMOR will use, so what
            // you feel here is exactly what the instrument will feel like.
            float t = fsr.Process(fsr12);

            uint32_t norm = (uint32_t)(t * 15.99f);
            if(norm > 15) norm = 15;
            uint8_t lit = (uint8_t)norm;              // 0..15, full bar
            for(uint8_t i=0;i<16;i++) {
                uint8_t r=0,g=0,b=0;
                if(i <= lit) {
                    r=230; g=120; b=0;    // amber -- the locked cubed curve
                }
                grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;
            }
            if(joy_held) fsr.Reset();                     // click = reset range

            if(!ntShow(grb)) ok = false;
            continue;
        }

        for(uint8_t i=0;i<16;i++) {
            uint8_t r,g,b;
            if(joy_held) {                            // stick click = flood
                r = cr; g = cg; b = cb;
            } else if(i == 15) {                      // SD verdict, always on
                r = sd_ok ? 0 : 180; g = sd_ok ? 180 : 0; b = 0;
            } else if((i & 3) == axcol) {             // accel X column, bright
                r = cr; g = cg; b = cb;
            } else if(i == cursor) {                  // joystick cursor — teal
                r = 20; g = 200; b = 160;
            } else {                                  // dim lava background
                lavaColor(pphase[i], r, g, b);
                r >>= 5; g >>= 5; b >>= 5;
            }
            grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;   // GRB order
        }

        if(!ntShow(grb)) ok = false;                  // SAMD09 watchdog -> re-init
    }
}
