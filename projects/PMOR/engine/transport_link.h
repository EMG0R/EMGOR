// transport_link.h -- CONTROL plane. The one place that decides what a tap MEANS and whose tempo
// the instrument (and its screen) follows, for each clock_mode (usb/clock_engine.h):
//
//   INTERNAL  standalone / unplugged / following a dead bus: local tap analysis (the SHARED
//             analyzer, demiurge-transport.h), local grid snap, BPM editor live. Full function.
//   MASTER    same local analysis; tempo + role go out as CC116/118/119/120/121 (human gesture).
//   SLAVE     a tap is an EVENT: it goes to the Pi (CC112, with its ISR age) and the Pi analyses
//             it ONCE and broadcasts the result. The local clock is untouched until the
//             broadcast tempo comes back; the screen shows the RECEIVED tempo, never a local guess.
#pragma once
#include <stdint.h>

// An encoder press, stamped in the audio ISR (hw/controls.cpp encoderTakePressSample()).
void transportTap(uint32_t tap_sample);

// Every main-loop pass: tempo authority (adopt the bus tempo ONLY while SLAVE -- an INTERNAL
// PMOR keeps whatever tempo you set), and the display fields g.bpm_shown / g.clock_mode.
void transportTick();
