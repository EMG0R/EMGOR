// bbus.h — bit-bang open-drain I2C. VERBATIM from PMOR_V2.cpp L137–281.
// Instances: tbus (NeoTrellis, D11/D12) and sbus (sensors, D13/D14) — defined in bbus.cpp.
#pragma once
#include <stdint.h>
#include "daisy_seed.h"

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
        scl(1); sda(1); daisy::System::Delay(2);
    }
    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { daisy::System::DelayUs(us); }

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
            uint32_t t0 = daisy::System::GetNow();
            while(!dsy_gpio_read(&scl_g) && daisy::System::GetNow() - t0 < 5)
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
    // n bytes from an 8-bit register in ONE transaction (the IMU's accel block 0x3B..0x40: one burst instead of three r16be)
    void rN(uint8_t a7, uint8_t reg, uint8_t* d, uint8_t n) {
        pmor_usb_service();
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) d[i]=byteR(i==n-1);
        stop();hd();
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
    //
    // Split form (keypad poll): ssCmd() sends the request, the caller does other work for >= 500 us, then ssGet1()/ssGetN() read the
    // reply (identical bytes on the wire to ssR1/ssRN, which are just the two halves with a spin between). Nothing else may touch
    // THIS bus between the halves.
    bool ssCmd(uint8_t a7, uint8_t base, uint8_t reg) { uint8_t cmd[2]={base,reg}; return send(a7,cmd,2); }
    uint8_t ssGet1(uint8_t a7) {
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    void ssGetN(uint8_t a7, uint8_t* out, uint8_t n) {
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) out[i]=byteR(i==n-1);
        stop();hd();
    }
    uint8_t ssR1(uint8_t a7, uint8_t base, uint8_t reg) {
        ssCmd(a7,base,reg);
        daisy::System::DelayUs(500);
        return ssGet1(a7);
    }
    void ssRN(uint8_t a7, uint8_t base, uint8_t reg, uint8_t* out, uint8_t n) {
        ssCmd(a7,base,reg);
        daisy::System::DelayUs(500);
        ssGetN(a7,out,n);
    }
};

extern BBus tbus; // NeoTrellis — D11=SCL, D12=SDA
extern BBus sbus; // Sensors    — D13=SCL, D14=SDA
