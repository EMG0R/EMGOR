// PocketPassthru — Pocket OpGorator
// Raw audio passthrough (no DSP), OLED, and BUTTONLESS USB reflash via SERIAL.
//
// Buttonless flashing:
//   Board enumerates as a USB CDC serial port (/dev/cu.usbmodem*).
//   Send the 3 bytes "dfu" to that port and System::ResetToBootloader(STM)
//   drives PG3 -> BOOT0 high, then resets -> STM32H7 ROM DFU comes up with
//   NO button press. Reflash, done.
//
// IMPORTANT: USB is initialized FIRST (right after hw.Init), BEFORE the OLED
// SPI and audio. Initializing the OLED before USB prevented enumeration.
//
// (Future: USB MIDI I/O in parallel = composite CDC+MIDI descriptor — separate step.)
//
// Synthesis is intentionally commented out — input goes straight to output.

#include <string.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;

using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed     hw;
MyOled        display;
volatile bool g_dfu_request = false;

// ================================================================
// AUDIO — RAW PASSTHROUGH (no DSP)
// ================================================================
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i]; // L: input straight to output
        out[1][i] = in[1][i]; // R: input straight to output
    }

    // --- SYNTHESIS DISABLED ---
    // for(size_t i = 0; i < size; i++)
    // {
    //     float sig = osc.Process();   // oscillator / sample playback / effects
    //     out[0][i] = sig;
    //     out[1][i] = sig;
    // }
}

// USB CDC receive: look for the trigger sequence "dfu"
void UsbCallback(uint8_t* buf, uint32_t* len)
{
    for(uint32_t i = 0; i + 2 < *len; i++)
        if(buf[i] == 'd' && buf[i + 1] == 'f' && buf[i + 2] == 'u')
            g_dfu_request = true;
}

int main(void)
{
    hw.Configure();
    hw.Init();

    // ---- USB CDC serial FIRST (built-in micro-USB / FS) — carries "dfu" trigger ----
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(500);
    hw.usb_handle.SetReceiveCallback(UsbCallback, UsbHandle::FS_INTERNAL);

    // ---- OLED (hardware SPI1: DC=D3, RST=D4; data on default SPI1 pins) ----
    // NOTE: with the OLED active, the USB CDC serial port does NOT enumerate on
    // bus-powered/dock connections (OLED 12V boost + audio + USB browns out the
    // port). Proven not pins / not SPI peripheral. To get OLED + buttonless
    // serial together, power the board from a high-current/direct port or a
    // powered hub. See PocketPassthru-FLASHING.md.
    MyOled::Config oc;
    oc.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oc.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oc);
    display.Fill(false);
    display.SetCursor(20, 16); display.WriteString("HELLO", Font_11x18, true);
    display.SetCursor(8,  38); display.WriteString("EMORY", Font_11x18, true);
    display.Update();

    // ---- Audio passthrough ----
    hw.StartAudio(AudioCallback);

    bool led = false;
    while(1)
    {
        if(g_dfu_request)
        {
            System::Delay(50);
            // Buttonless DFU: PG3 -> BOOT0 high + system reset -> ROM DFU
            System::ResetToBootloader(System::BootloaderMode::STM);
        }
        hw.SetLed(led);   // blink = proof of life past OLED + audio init
        led = !led;
        System::Delay(250);
    }
}
