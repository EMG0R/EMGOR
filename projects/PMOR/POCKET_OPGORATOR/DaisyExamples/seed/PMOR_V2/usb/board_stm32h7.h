// board_stm32h7.h — minimal board glue between libDaisy's HAL init and
// TinyUSB's device controller driver (dcd_dwc2), for the Daisy Seed's
// USB_OTG_FS peripheral (PA11/PA12, internal full-speed PHY only).
//
// Why this exists instead of TinyUSB's hw/bsp/stm32h7 board support: that
// layer is written for ST's own Nucleo/Discovery boards and their
// board_init()/board_led_write()/etc. contract, and it initializes the MCU
// clock tree itself -- which would fight libDaisy's own System::Init()
// (different PLL config, different HSE/HSI choice, different D-cache/MPU
// setup). We let libDaisy own the clock tree entirely (DaisySeed::Init())
// and only add the handful of USB-specific bits libDaisy's own usbd_conf.c
// would normally set up for ST's USB device stack -- which we are not
// using (see README.md "why TinyUSB instead of libDaisy's usbd").
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enables USB_OTG_FS peripheral clock (HSI48-sourced, matching libDaisy's
// own RCC_USBCLKSOURCE_HSI48 choice in System::Init), configures PA11/PA12
// as AF10 (USB_OTG_FS_DM/DP) and PA9 as VBUS sense input, and enables/
// primes the OTG_FS_IRQn (and its associated EP1 IN/OUT IRQs) in the NVIC.
// Call AFTER hw.Init() (so the MPU/D-cache/clock tree are already up) and
// BEFORE tusb_init().
void board_usb_init(void);

// Reads the STM32's 96-bit factory-programmed unique ID (UID registers,
// base 0x1FF1E800 on STM32H7) and writes it as a UTF-16 hex string (no
// header/length prefix -- tud_descriptor_string_cb() adds that) into `buf`,
// up to `max_chars` UTF-16 code units. Returns the number of code units
// written. Gives every board a distinct USB serial number with zero
// provisioning.
size_t board_get_serial_utf16(uint16_t *buf, size_t max_chars);

// Wall-clock milliseconds since boot -- thin wrapper around HAL_GetTick()
// (the same tick libDaisy's System::GetNow() reads) so a plain .c file
// (clock_engine.c) can read it for CDC diagnostics without needing a
// C++-only System:: include. Added 2026-08-11 for the "diag" CDC command --
// see clock_engine.c's diagnostic-instrumentation comment for why this is
// reported ALONGSIDE g_sample_clock rather than instead of it: comparing
// the two against a host-side wall-clock reference is what tells apart "the
// audio domain is off" from "everything HSE-derived is off".
uint32_t board_get_now_ms(void);

#ifdef __cplusplus
}
#endif
