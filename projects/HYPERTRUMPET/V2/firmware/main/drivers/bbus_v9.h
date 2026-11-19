/* Port of HYPER_CORE/src/hal/bbus.h (PMOR_V3_live hw/bbus.h, commit ff86e4c) to libDaisy v9.
 * origin  : ~/Documents/_EMGOR_SYNTH/______2026NEW/HYPER_CORE/src/hal/bbus.h
 * changes : dsy_gpio (removed in libDaisy v9) -> daisy::GPIO open-drain; pmor_usb_service() pump dropped
 *           (libDaisy CDC is interrupt driven); only the primitives (start/stop/byteW/byteR/waitScl/recover)
 *           are kept, the transaction helpers with ack reporting live in i2c_sensors.h.
 *           Logic, 5 us half-bit (about 100 kHz), 5 ms patient clock-stretch wait and the 9-clock recover() are unchanged.
 * licence : Emory (own work) */
#pragma once
#include <stdint.h>
#include "daisy_seed.h"

struct BBus {
    daisy::GPIO scl_g, sda_g;
    uint8_t     us = 5;          // half-bit delay: 2 per bit -> 10 us/bit = 100 kHz
    bool        stretch_err = false;

    void init(daisy::Pin sp, daisy::Pin dp) {
        scl_g.Init(sp, daisy::GPIO::Mode::OPEN_DRAIN, daisy::GPIO::Pull::NOPULL);
        sda_g.Init(dp, daisy::GPIO::Mode::OPEN_DRAIN, daisy::GPIO::Pull::NOPULL);
        scl(1); sda(1); daisy::System::Delay(2);
    }
    void scl(int v) { scl_g.Write(v); }
    void sda(int v) { sda_g.Write(v); }
    void hd()       { daisy::System::DelayUs(us); }

    // slave may hold SCL low (clock stretch): wait for it, patiently (<= 5 ms), flag a timeout
    void waitScl() {
        for(uint32_t i = 10000; i && !scl_g.Read(); --i) {}
        if(!scl_g.Read()) {
            uint32_t t0 = daisy::System::GetNow();
            while(!scl_g.Read() && daisy::System::GetNow() - t0 < 5) {}
            if(!scl_g.Read()) stretch_err = true;
        }
    }
    // 9 clocks lets a slave stuck mid-byte finish, then STOP resets every state machine
    void recover() {
        sda(1);
        for(int i = 0; i < 9; i++) { scl(0); hd(); scl(1); hd(); }
        stop(); hd();
    }
    void start() { sda(1); hd(); scl(1); hd(); sda(0); hd(); scl(0); hd(); }
    void stop()  { sda(0); hd(); scl(1); hd(); sda(1); hd(); }

    bool byteW(uint8_t b) {
        for(int i = 7; i >= 0; i--) {
            sda((b >> i) & 1); hd();
            scl(1); waitScl(); hd();
            scl(0); hd();                    // data hold after SCL falls (L1X is picky)
        }
        sda(1); hd();
        scl(1); waitScl(); hd();
        bool ack = (sda_g.Read() == 0);
        scl(0); hd(); return ack;
    }
    uint8_t byteR(bool nack) {
        uint8_t v = 0; sda(1);
        for(int i = 7; i >= 0; i--) {
            hd(); scl(1); waitScl(); hd();
            if(sda_g.Read()) v |= (1 << i);
            scl(0);
        }
        sda(nack ? 1 : 0); hd(); scl(1); waitScl(); hd(); scl(0); hd(); sda(1);
        return v;
    }
};
