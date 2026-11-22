// usb_dma_mem.h — D2-RAM placement for USB-DMA-visible buffers.
//
// STM32H750's OTG core (and, more importantly, the Cortex-M7 D-cache sitting
// in front of every SRAM access) means any buffer the USB peripheral reads
// or writes directly must live somewhere the CPU's cache can't silently
// diverge from what's actually in RAM. libDaisy already solves this for its
// own DMA users: `System::ConfigureMpu()` (src/sys/system.cpp) marks the
// first 32KB of D2 RAM (0x30000000, MPU_REGION_NUMBER0) *non-cacheable*, and
// `DMA_BUFFER_MEM_SECTION` (src/daisy_core.h) places a symbol in the
// `.sram1_bss` section, which the stock libDaisy linker scripts
// (core/STM32H750IB_flash.lds etc.) map into that same D2/SRAM1 region.
//
// We reuse the exact same section name here (deliberately not `#include
// "daisy_core.h"`, since this header is consumed by tusb_config.h which is
// compiled as C by TinyUSB's C sources, and DMA_BUFFER_MEM_SECTION's home
// header pulls in C++-only libDaisy declarations). Any buffer TinyUSB's
// device controller driver (dcd_dwc2) DMAs into/out of -- the endpoint 0
// control buffer, the UAC2 IN software FIFO, MIDI/CDC endpoint buffers --
// must carry this attribute, and CFG_TUD_MEM_SECTION in tusb_config.h wires
// it in globally for TinyUSB's own internal buffers.
//
// No new linker-script section is required: `.sram1_bss` already exists in
// every libDaisy example's .lds. If you ever port this scaffold off
// libDaisy's linker scripts, you must add an equivalent NOLOAD output
// section mapped into the MPU-non-cacheable window, or fall back to
// SCB_CleanInvalidateDCache_by_Addr() around every USB transfer instead.
#pragma once

#define USB_DMA_MEM_SECTION __attribute__((section(".sram1_bss")))
