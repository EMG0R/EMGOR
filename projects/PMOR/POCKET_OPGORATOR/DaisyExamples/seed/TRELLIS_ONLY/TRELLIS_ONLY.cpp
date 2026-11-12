// TRELLIS_ONLY — PMOR hardware bring-up step 1
//
// Does ONE thing: bring up the NeoTrellis on tbus (D11=SCL, D12=SDA) and run
// PMOR's idle lava-lamp animation. No USB, no OLED, no sensors, no SD, no audio.
//
// BBus and the NeoTrellis driver below are lifted VERBATIM from
// POCKETGORATOR_CODE.cpp (Aug 18 build) so this proves the real driver, not a
// reimplementation. The only edit: usbDelay() is a plain System::Delay here,
// since there is no USB stack to service.
//
// If the pads light and drift, the trellis + bit-banged I2C + 5V rail are all good.

#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;

// PMOR's BBus pumps the USB stack per-byte so a long bit-banged I2C write can
// never starve TinyUSB. There is no USB in this build, so it is a no-op — kept
// so the driver below stays byte-for-byte identical to POCKETGORATOR_CODE.cpp.
static inline void pmor_usb_service(void) {}

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
static inline void usbDelay(uint32_t ms) { System::Delay(ms); }  // no USB in this build

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
// ============================================================
// main
// ============================================================
int main(void)
{
    hw.Configure();
    hw.Init();

    // tbus: D11 = SCL, D12 = SDA  (BBus::init takes (scl, sda))
    tbus.init(hw.GetPin(11), hw.GetPin(12));

    // Onboard LED: slow blink = trellis init failed, solid = running.
    bool ok = ntInit();

    // PMOR's idle animation state (POCKETGORATOR_CODE.cpp:4088-4089)
    static const uint8_t DRIFT[16]={1,2,1,2, 2,1,2,1, 1,2,2,1, 2,1,1,2};
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    uint32_t next_trel = 0;
    bool     led = false;

    while(1)
    {
        uint32_t now = System::GetNow();

        if(!ok) {
            // trellis never answered — blink 2Hz and keep retrying every 2s
            if(now >= next_trel) { next_trel = now + 250; led = !led; hw.SetLed(led); }
            static uint32_t retry = 0;
            if(now >= retry) { retry = now + 2000; ok = ntInit(); }
            continue;
        }

        hw.SetLed(true);

        if(now >= next_trel) {
            next_trel = now + 80;   // same cadence as PMOR (:4206)

            for(uint8_t i=0;i<16;i++) pphase[i] += DRIFT[i];

            uint8_t grb[48];
            for(uint8_t i=0;i<16;i++) {
                uint8_t r,g,b;
                lavaColor(pphase[i], r, g, b);
                grb[i*3+0]=g; grb[i*3+1]=r; grb[i*3+2]=b;   // GRB order
            }

            if(!ntShow(grb)) {      // SAMD09 watchdog'd — re-init
                ok = false;
            }
        }
    }
}
