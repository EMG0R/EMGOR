// platform_hw.cpp -- pmor_core/platform.h on the Daisy.
#include "../pmor_core/platform.h"
#include "daisy_seed.h"
uint32_t pmorNowMs(void) { return daisy::System::GetNow(); }

// Free-running timer for the USB IRQ's MIDI RX stamps (usb/clock_engine.c). 32-bit TIM, differences are wrap-safe.
extern "C" uint32_t pmor_tick(void) { return daisy::System::GetTick(); }
extern "C" uint32_t pmor_tick_hz(void) { return daisy::System::GetTickFreq(); }

// DWT cycle counter (Cortex-M7). Enabled on first use; reads are 1 cycle.
uint32_t pmorCycles(void) {
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) { CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; }
    return DWT->CYCCNT;
}
