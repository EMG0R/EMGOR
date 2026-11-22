// events.h -- sound events as a side output of the engines (never feeds back into audio).
// AUDIO plane pushes, CONTROL plane (firmware MIDI layer / host tools) pops. Single producer, single consumer.
#pragma once
#include <stdint.h>

enum PmorEventKind : uint8_t {
    EV_DRUM_ON = 1,   // a = pad 0..15,  b = velocity 1..127
    EV_SYN_ON,        // a = midi note,  b = velocity,  c = pitch-bend 14 bit (8192 = centre)
    EV_SYN_OFF,       // a = midi note
    EV_SMP_ON,        // a = slot 0..15, b = velocity
    EV_SEED,          // c = 28-bit seed in `seed` (transport start reseed)
};
struct PmorEvent { uint8_t kind, a, b, pad_; uint16_t c, pad2_; uint32_t seed; };

void pmorEventPush(uint8_t kind, uint8_t a, uint8_t b, uint16_t c);
void pmorEventPushSeed(uint32_t seed);      // EV_SEED
bool pmorEventPop(PmorEvent* out);          // false when empty
uint32_t pmorEventDrops(void);              // pushes lost to a full ring
// hz -> nearest MIDI note + 14-bit bend (+-2 semitones) for the microtonal remainder
void pmorHzToMidi(float hz, uint8_t* note, uint16_t* bend);
