// fx_midi.h -- FX automation over MIDI (hardware-free; hw/studio.cpp wires it to USB-MIDI). docs/PROTOCOL.md "FX over MIDI".
//   ch5 (idx 4): CC n = engage of FX id n (bank*16+pad), 127 / 0.            out: the EFFECTIVE state (held | automation | MIDI-in)  in: Live drives the FX
//   ch6 (idx 5): CC 1 = FX amount (effective FSR macro), CC 16+k = bank k AMOUNT, CC 24+k = bank k LIDAR, CC 32+k (k=1..7) = bank k MIX
// Rules: only changes are sent (<= FXMIDI_RATE_HZ total); PMOR never re-sends a value it just received; an incoming value equal to what we
// last sent, or any incoming CC for an id within FXMIDI_ECHO_MS of our own send, is an echo and ignored; a genuine incoming edit takes that FX
// over from PMOR's own loop (the loop is paused for that FX until its next physical hold) and is never recorded.
#pragma once
#include <stdint.h>

#define FXMIDI_CH_ENGAGE 4
#define FXMIDI_CH_PARAM  5
#define FXMIDI_RATE_HZ   200
#define FXMIDI_ECHO_MS   100

typedef void (*FxMidiCc)(uint8_t ch, uint8_t cc, uint8_t val);
void fxMidiReset(void);                                                  // session ended / started: forget baselines (no emission until re-armed)
void fxMidiPoll(uint32_t now_ms, FxMidiCc out);                          // emit what changed since the last call (rate limited)
void fxMidiIn(uint32_t now_ms, uint8_t ch, uint8_t cc, uint8_t val);     // an incoming CC (ch 0-based)
