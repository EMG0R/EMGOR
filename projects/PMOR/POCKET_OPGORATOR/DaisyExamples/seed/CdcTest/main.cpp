// CdcTest — isolation test: USB CDC + LED blink + buttonless DFU.
// STEP 2: audio passthrough ADDED, still NO OLED. Isolates whether StartAudio
// is what kills USB enumeration.
//
// Diagnostics:
//   LED blinks + /dev/cu.usbmodem*  => audio is fine; OLED was the culprit.
//   LED stops (or no serial)        => StartAudio() halts/disrupts USB.

#include <string.h>
#include "daisy_seed.h"

using namespace daisy;

static DaisySeed hw;
volatile bool    g_dfu_request = false;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

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

    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(500);
    hw.usb_handle.SetReceiveCallback(UsbCallback, UsbHandle::FS_INTERNAL);

    hw.StartAudio(AudioCallback);   // <-- variable under test

    bool led = false;
    while(1)
    {
        if(g_dfu_request)
        {
            System::Delay(50);
            System::ResetToBootloader(System::BootloaderMode::STM);
        }
        hw.SetLed(led);   // blink = proof of life past StartAudio
        led = !led;
        System::Delay(250);
    }
}
