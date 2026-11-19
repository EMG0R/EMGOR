// hyper_hw.h -- the small HAL surface that the imported PMOR drivers need.
// NEW file (HYPER_CORE, 2026-11-13), declarations only. It replaces PMOR's hw/hw.h + hw/hw_priv.h
// for the files copied into src/hal and src/drivers, so they build without the PMOR engine,
// sequencer, OLED, SD or clock_engine. No feature code lives here.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "daisy_seed.h"

extern daisy::DaisySeed hw;

// ---- USB service pump (every blocking driver calls this; see src/hal/usb_service.cpp)
extern "C" void pmor_usb_service(void);
void usbDelay(uint32_t ms);                 // Delay that keeps USB alive. Use instead of System::Delay after USB is up.

// Hooks the instrument supplies (weak defaults live in usb_service.cpp / dfu.cpp):
extern "C" void hyper_usb_extra_poll(void); // anything else to service each pump (default: nothing)
extern "C" void hyper_cdc_line(const char* line);   // one complete CDC text line (default: sets the DFU flag on "dfu")
extern "C" bool hyper_dfu_requested(void);  // true once a CDC line containing "dfu" arrived

// Pins for the hardware DFU escape (both low at power-on). Override with -D on the compile line.
#ifndef HYPER_DFU_PIN_A
#define HYPER_DFU_PIN_A daisy::seed::D2
#endif
#ifndef HYPER_DFU_PIN_B
#define HYPER_DFU_PIN_B daisy::seed::D24
#endif

void dfuBootGesture();
void dfuCheck();
void PollCdc(void);

// ---- NeoTrellis (src/drivers/trellis.cpp)
typedef void (*PadCb)(uint8_t pad, bool pressed);
bool ntInit();
void trellisBootInit();
bool trellisOk();
void trellisTick(uint32_t now_ms);
void trellisSetFrame(const uint8_t grb[48]);
void trellisPollKeys(uint32_t now_ms, PadCb cb);
void trellisSetRotation(uint8_t quarter_turns);
extern float g_trellis_brightness;
