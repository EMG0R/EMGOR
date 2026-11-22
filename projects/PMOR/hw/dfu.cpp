// dfu.cpp — the two ways into the bootloader on an enclosed device.
//   boot gesture   PMOR_V2.cpp L3899–3906  (pins changed: D2 + D24, see below)
//   CDC "dfu"      PMOR_V2.cpp L4341–4344 / L4371–4374
#include "hw.h"
#include "hw_priv.h"
#include "../config.h"
#include "daisy_seed.h"
extern "C" {
#include "tusb.h"
#include "usb/clock_engine.h"
}
using namespace daisy;

// WHICH BOOTLOADER TO JUMP TO -- this MUST match how the app is linked
// (same rule as ../_bringup_dfu.h BRINGUP_DFU_MODE):
//   APP_TYPE = BOOT_SRAM -> app lives in QSPI (0x90040000), loaded by the
//   Daisy bootloader. Use DAISY_INFINITE_TIMEOUT: the ST ROM loader has NO
//   QSPI region, so writing there is rejected with "Last page ... is not
//   writeable". INFINITE_TIMEOUT also removes the bootloader's 2-second
//   window race. Get this wrong and the reflash silently cannot work.
#ifndef PMOR_DFU_MODE
#define PMOR_DFU_MODE System::BootloaderMode::DAISY_INFINITE_TIMEOUT
#endif

// ── HARDWARE DFU ESCAPE HATCH: hold BOTH buttons at power-on ──
// The CDC `dfu` command is the normal way in, but it is useless in the
// exact situation you need it most: if USB fails to enumerate, there is
// no serial port to send it to, and recovery needs physical BOOT+RESET
// on a device that is meant to live inside a sealed enclosure. This
// gesture depends on nothing but GPIO, so it works even when USB, the
// OLED, or the whole audio path is broken.
// enc SW = D2, joystick SW = D24; both are active-low with pull-ups.
// (v2 read D17, which is now the joystick Y wiper — an analog pin.)
// Read them directly — the control objects are not initialised this
// early, and deliberately so: fewer moving parts.
void dfuBootGesture() {
    GPIO b1, b2;
    b1.Init(seed::D2,  GPIO::Mode::INPUT, GPIO::Pull::PULLUP);   // PIN_ENC_SW
    b2.Init(seed::D24, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);   // PIN_JOY_SW
    System::Delay(0);                  // let the pull-ups settle
    if(!b1.Read() && !b2.Read())       // both held == both low
        System::ResetToBootloader(PMOR_DFU_MODE);
}

// Buttonless DFU (MUST PRESERVE — enclosed device, no physical
// access to BOOT/RESET without disassembly). Checked every loop
// iteration, same as the source project's own main().
void dfuCheck() {
    if(clock_engine_dfu_requested()) {
        System::Delay(50);
        System::ResetToBootloader(PMOR_DFU_MODE);
    }
}
