// tusb_config.h — Pocket OpGorator TinyUSB composite device configuration
//
// Device: UAC2 stereo capture-only (device->host) + USB-MIDI 1.0 + CDC-ACM,
// on the Daisy Seed's (STM32H750) dedicated USB_OTG_FS peripheral
// (PA11/PA12, internal FS PHY) -- full-speed (12 Mbps) only, no HS PHY is
// wired on this board. See README.md for the full rationale.
#pragma once

#include "audio_format.h"
#include "usb_dma_mem.h"

//--------------------------------------------------------------------+
// Board / MCU
//--------------------------------------------------------------------+
#define CFG_TUSB_MCU               OPT_MCU_STM32H7
#define CFG_TUSB_OS                OPT_OS_NONE

// Only rhport0 is wired on the Daisy Seed (USB_OTG_FS / PA11-PA12). Force
// full-speed explicitly -- the internal FS PHY cannot do high-speed, and we
// never want TinyUSB probing for a HS PHY that doesn't exist on this board.
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define BOARD_TUD_RHPORT            0
#define BOARD_DEVICE_RHPORT_SPEED   OPT_MODE_FULL_SPEED

// All TinyUSB-internal buffers that the DWC2 device controller driver
// touches directly (control transfer buffer, class FIFOs) go in the
// MPU-non-cacheable D2 RAM window libDaisy already sets up. See
// usb_dma_mem.h for why.
#define CFG_TUSB_MEM_SECTION        USB_DMA_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          TU_ATTR_ALIGNED(4)

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG              0
#endif

//--------------------------------------------------------------------+
// Device Configuration
//--------------------------------------------------------------------+
#define CFG_TUD_ENDPOINT0_SIZE      64

// Class drivers enabled in this composite: UAC2 audio, USB-MIDI, CDC-ACM --
// EXCEPT in the isolation build variants (Makefile
// `make midi-only` / `make midi-audio`, or `ISOLATION=midi`/`midi_audio`),
// which strip class drivers down to answer one question at a time: does
// USB-MIDI enumerate on this hardware at all (§ "USB bring-up isolation
// ladder" in README.md), mirroring the same isolation methodology that
// root-caused the OLED/USB-CDC co-enumeration failure documented in
// PocketPassthru-FLASHING.md §6. USB-MIDI is present in every variant --
// it's the one thing every isolation build exists to test.
#if defined(POCKET_OPGORATOR_ISOLATION_MIDI)
#define CFG_TUD_AUDIO               0
#define CFG_TUD_CDC                 0
#elif defined(POCKET_OPGORATOR_ISOLATION_MIDI_AUDIO)
#define CFG_TUD_AUDIO               1
#define CFG_TUD_CDC                 0
#else
#define CFG_TUD_AUDIO               1
#define CFG_TUD_CDC                 1
#endif
#define CFG_TUD_MIDI                1

#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_VENDOR              0

//--------------------------------------------------------------------+
// AUDIO (UAC2) — capture-only, function #1
//--------------------------------------------------------------------+
// Capture-only: EP IN (device -> host) enabled, EP OUT never enabled.
// A playback (OUT) interface would make the Daisy a second USB audio sink
// and pull it into WirePlumber's clock-master election on the Demiurge
// side, defeating direct-output mode. See design spec section 2.
#define CFG_TUD_AUDIO_ENABLE_EP_IN         1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT        0
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP   0   // N/A for an IN-only (mic-style) endpoint
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP  0   // no runtime AC control-change notifications needed yet

#define CFG_TUD_AUDIO_CTRL_BUF_SZ          64

// Endpoint 1 IN: sized for the worst-case drift-compensated packet (49
// samples, see audio_format.h). The class driver's own flow-control layer
// (CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL, default-on below) is what actually
// picks 47 vs 48 vs 49 samples per 1ms packet at runtime from FIFO fill
// level -- our job is just to make sure the EP and its software FIFO are
// big enough to never be the bottleneck.
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX      AUDIO_EP_IN_SZ_MAX

// Software FIFO backing the endpoint: 4 nominal blocks (~4ms) of headroom
// between the Daisy audio callback (usb_audio_stream_push, called every
// 48-sample block = 1ms) and the USB SOF-driven IN packet scheduler. Deep
// enough to absorb normal scheduling jitter without the flow-control logic
// having to run at the ragged edges of empty/full.
// 8 packets (~8ms) of headroom: the capture FIFO is now fed from the main
// loop (usb_audio_stream_task), whose worst gap is one OLED page blit plus
// loop overhead. 4 packets was measurably too tight (host-side dropouts).
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ   (AUDIO_EP_IN_SZ_MAX * 16)

#define CFG_TUD_AUDIO_EP_IN_FLOW_CONTROL       1

//--------------------------------------------------------------------+
// MIDI
//--------------------------------------------------------------------+
#define CFG_TUD_MIDI_RX_BUFSIZE     64
#define CFG_TUD_MIDI_TX_BUFSIZE     64
#define CFG_TUD_MIDI_EP_BUFSIZE     64

//--------------------------------------------------------------------+
// CDC (debug/config serial console)
//--------------------------------------------------------------------+
#define CFG_TUD_CDC_RX_BUFSIZE      256
// Bumped 256 -> 512 (2026-08-12, live-device debugging): tud_cdc_write()
// -> tu_edpt_stream_write() -> tu_fifo_write_n() silently truncates to
// whatever fits in this ring buffer and returns the short count -- this
// project's cdc_reply() (clock_engine.c) never checked that return value.
// The "diag" command's reply is ~360 chars (the longest this firmware
// sends), which silently truncated at exactly 256 bytes with the tail
// (dwc2_midi_kick_bytes onward) and the terminating "\r\n" dropped --
// caught live, mid-investigation, when a DIAG read over the wire came
// back missing its last few fields and no newline. Bumped again 512 ->
// 768 when the "HELP" command (added same day, see clock_engine.c) came
// in at 462 bytes -- comfortably under 512 already, but close enough
// that the NEXT command added without checking this margin could repeat
// the exact same bug. 768 gives real headroom above both current replies
// (diag ~380B, help ~462B) rather than reacting again the third time.
#define CFG_TUD_CDC_TX_BUFSIZE      768
#define CFG_TUD_CDC_EP_BUFSIZE      64
