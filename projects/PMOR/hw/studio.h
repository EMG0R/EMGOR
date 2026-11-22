// studio.h -- PMOR STUDIO glue: wires pmor_core's protocol (proto.cpp) to the CDC port, the USB-MIDI
// out (notes/CCs from the sound events), the external-clock follower and the speaker mute.
// CONTROL plane only. docs/PROTOCOL.md is the contract.
#pragma once
#include <stdint.h>

void studioInit(void);              // after patchInit(): protocol ops, QSPI settings. Call once before the main loop.
void studioPoll(uint32_t now_ms);   // every main-loop pass, right after clock_engine_poll()
void studioCdcRx(const uint8_t* d, uint32_t n);   // raw CDC bytes (from PollCdc)
void studioMidiService(uint32_t now_ms);          // events -> USB-MIDI; cheap, called from pmor_usb_service()

// AUDIO plane reads these (volatile flags only): speakers muted in STUDIO/EDIT except during an audition.
bool studioMuted(uint32_t sample_now);

// Manual clock-mode setting (for the settings menu): 0 AUTO, 1 INTERNAL, 2 FOLLOW, 3 MASTER. Persisted in QSPI (own record).
uint8_t studioClockMode(void);
void    studioSetClockMode(uint8_t m);
const char* studioClockModeName(uint8_t m);
