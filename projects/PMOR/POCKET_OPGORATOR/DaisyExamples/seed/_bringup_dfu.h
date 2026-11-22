// _bringup_dfu.h — buttonless DFU for PMOR bring-up test builds.
//
// Ports PMOR's MUST-PRESERVE trigger (POCKETGORATOR_CODE.cpp / clock_engine.c:995)
// to libDaisy's own USB CDC, so the little test firmwares can be reflashed with
// no BOOT+RESET dance:
//
//     printf 'dfu\n' > /dev/cu.usbmodem*
//
// The device re-enumerates as 0483:df11 and dfu-util takes over.
//
// PMOR does this over TinyUSB; these builds have no TinyUSB, so we use
// libDaisy's UsbHandle (CDC-only, interrupt-driven -- no tud_task() to pump,
// which is why a blocking System::Delay does not kill it here the way it does
// in PMOR).
//
// USAGE (order matters -- see the note below):
//     hw.Configure(); hw.Init();
//     bringup_dfu_init();          // BEFORE any slow peripheral init
//     ...
//     while(1) { bringup_dfu_poll();  ... }
//
// INIT ORDER: PocketPassthru-FLASHING.md proved USB must come up BEFORE the
// OLED, with a settle delay before the receive callback is registered, or the
// host gives up and the device vanishes from the bus. Same shape here: call
// bringup_dfu_init() first, and it does its own settle wait internally.

#pragma once
#include "daisy_seed.h"

// WHICH BOOTLOADER TO JUMP TO -- this MUST match how the app is linked:
//
//   APP_TYPE = BOOT_NONE  -> app lives in INTERNAL FLASH (0x08000000).
//                            Use STM: the ST ROM loader's DFU exposes exactly
//                            that region. (It exposes ONLY that region.)
//
//   APP_TYPE = BOOT_SRAM  -> app lives in QSPI (0x90040000), loaded by the
//                            Daisy bootloader. Use DAISY_INFINITE_TIMEOUT:
//                            the ROM loader has NO QSPI region, so writing
//                            there is rejected with
//                            "Last page ... is not writeable".
//                            INFINITE_TIMEOUT also removes the bootloader's
//                            2-second window race.
//
// Get this wrong and the reflash silently cannot work.
#ifndef BRINGUP_DFU_MODE
#define BRINGUP_DFU_MODE daisy::System::BootloaderMode::STM
#endif

namespace bringup
{
static volatile bool g_dfu_request = false;

// Any received chunk containing "dfu" arms the reset. Matched on the raw
// buffer rather than line-buffered, so `printf 'dfu'` with no newline works
// too -- same guarantee PMOR's trigger gives.
inline void DfuRxCallback(uint8_t* buf, uint32_t* len)
{
    uint32_t n = *len;
    for(uint32_t i = 0; i + 2 < n; i++)
    {
        if(buf[i] == 'd' && buf[i + 1] == 'f' && buf[i + 2] == 'u')
        {
            g_dfu_request = true;
            return;
        }
    }
}
} // namespace bringup

/** Bring up USB CDC and arm the `dfu` trigger. Call right after hw.Init(). */
inline void bringup_dfu_init(daisy::DaisySeed& hw)
{
    hw.StartLog(false);          // non-blocking: never waits for a host
    daisy::System::Delay(500);   // settle before arming RX (see note above)
    hw.usb_handle.SetReceiveCallback(bringup::DfuRxCallback,
                                     daisy::UsbHandle::FS_INTERNAL);
}

/** Poll once per main-loop iteration. Reboots to ROM DFU when triggered. */
inline void bringup_dfu_poll()
{
    if(bringup::g_dfu_request)
    {
        bringup::g_dfu_request = false;
        daisy::System::Delay(50);   // let the CDC ack drain before we vanish
        // MUST be a DAISY mode, NOT STM. BootloaderMode::STM boots the ST ROM
        // loader, whose DFU exposes ONLY internal flash (0x08000000) -- it has
        // no QSPI region, so writing a BOOT_SRAM app to 0x90040000 is rejected
        // with "Last page ... is not writeable". INFINITE_TIMEOUT also removes
        // the bootloader's 2-second window race so the catcher cannot miss it.
        daisy::System::ResetToBootloader(BRINGUP_DFU_MODE);
    }
}
