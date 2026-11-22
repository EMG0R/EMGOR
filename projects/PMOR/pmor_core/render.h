// render.h -- the hardware-free audio entry point of pmor_core.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Renders one block (n <= BLOCK): clock -> sequencer -> engines -> FX -> metro -> master output.
// in0 = mono line-in for the sampler monitor (may be a zero buffer). L/R are overwritten.
// AUDIO plane. Everything hardware (joystick poll, USB clock engine, PO sync, CPU meter) is the
// caller's business -- see hw/audio_callback.cpp for the firmware's wrapper.
extern bool g_render_notes_only;   // true on the host: no sequencer/generative firing, only received notes sound
void pmorRenderBlock(const float* in0, float* L, float* R, size_t n);

// Absolute sample time of the start of the NEXT block (what the firmware stamps controls with).
uint32_t pmorSampleNow(void);

// Transport gate for follower modes: false = the grid keeps counting but no step fires (Stop).
// Default true (standalone / NEPTR behave exactly as before).
void pmorSetTransportRun(bool run);
bool pmorTransportRun(void);
// Metronome-only transport gate (any follow): false = no clicks until Start/Continue. Default true.
void pmorSetMetroGate(bool run);
// What the beat/step INDICATORS (OLED metro dot, playhead on OLED + pads) show: false while a followed transport is stopped. Always true standalone / master.
bool pmorFollowedRun(void);

// Restart every random stream (FX noise/glitch/stutter, sampler pan, generative dice, synth note
// character) from one 28-bit seed. 0 = the original fixed seeds. Also queues an EV_SEED event so
// the MIDI layer can publish it (docs/PROTOCOL.md section 7).
void pmorReseed(uint32_t seed28);
