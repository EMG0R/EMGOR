// usb_descriptors.h — interface/endpoint numbering for the Pocket OpGorator
// composite USB device (UAC2 capture + USB-MIDI + CDC-ACM).
//
// Numbering is fixed here (not auto-generated) so main.c, the UAC2 control
// request handlers, and the descriptor tables in usb_descriptors.c all agree
// without depending on macro-expansion order.
#pragma once

// Needed for CFG_TUD_AUDIO / CFG_TUD_CDC below -- both this header and
// usb_descriptors.c are always used alongside "tusb.h" (which pulls in
// tusb_config.h) in every translation unit that includes this file, but
// including it directly here too makes that non-obvious dependency
// explicit and this header self-sufficient on its own.
#include "tusb_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Interface numbers
//--------------------------------------------------------------------+
// Isolation builds (see README.md "USB bring-up isolation ladder" and
// Makefile POCKET_OPGORATOR_ISOLATION_MIDI / _MIDI_AUDIO) strip class
// drivers to answer "does USB-MIDI even enumerate on this hardware"
// with zero confounds, before trusting the full composite. tusb_config.h
// sets CFG_TUD_AUDIO / CFG_TUD_CDC to 0 for those builds; the interface
// numbering here follows suit so the config descriptor in
// usb_descriptors.c (also #if-guarded the same way) and this enum never
// disagree about which interfaces exist.
enum {
#if CFG_TUD_AUDIO
  ITF_NUM_AUDIO_CONTROL = 0,  // UAC2 AudioControl (AC)
  ITF_NUM_AUDIO_STREAMING,    // UAC2 AudioStreaming (AS) -- capture, alt 0/1
#endif
  ITF_NUM_MIDI_COMM,          // USB-MIDI 1.0 AudioControl (trivial, per spec)
  ITF_NUM_MIDI_STREAMING,     // USB-MIDI 1.0 MIDIStreaming (MS)
#if CFG_TUD_CDC
  ITF_NUM_CDC_COMM,           // CDC-ACM control interface
  ITF_NUM_CDC_DATA,           // CDC-ACM data interface
#endif
  ITF_NUM_TOTAL
};

//--------------------------------------------------------------------+
// Endpoint numbers
//--------------------------------------------------------------------+
// Full-speed OTG_FS on STM32H750 gives us plenty of endpoint numbers (the
// DWC2 core here exposes more FIFOs than we need for 4 logical pipes); pick
// low, sequential numbers for readability. IN/OUT share a number on a given
// endpoint index (bit 0x80 sets direction) -- STM32's DWC2 port supports
// same-numbered IN+OUT pairs (it is not in TinyUSB's
// CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY exception list).
#define EPNUM_AUDIO_IN     0x01  // -> 0x81, UAC2 capture ISO IN
#define EPNUM_MIDI         0x02  // -> 0x02 OUT / 0x82 IN, USB-MIDI bulk
#define EPNUM_CDC_DATA     0x03  // -> 0x03 OUT / 0x83 IN, CDC bulk data
#define EPNUM_CDC_NOTIF    0x04  // -> 0x84, CDC interrupt notification

//--------------------------------------------------------------------+
// UAC2 entity IDs (arbitrary but must be unique within the AC interface)
//--------------------------------------------------------------------+
#define UAC2_ENTITY_CLOCK             0x01  // Clock Source: internal, fixed 48kHz
#define UAC2_ENTITY_INPUT_TERMINAL    0x02  // Input Terminal: analog-in (PCM3060 ADC, stereo)
#define UAC2_ENTITY_FEATURE_UNIT      0x03  // Feature Unit: mute/volume, master + L + R
#define UAC2_ENTITY_OUTPUT_TERMINAL   0x04  // Output Terminal: USB streaming (host-bound)

#ifdef __cplusplus
}
#endif
