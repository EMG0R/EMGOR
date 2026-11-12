// pmor_notepad.h -- synth MIDI note (the firmware sends the RESOLVED pitch, PROTOCOL section 7 ch2) -> the pad the core should trigger.
// The core has no play-by-pitch call: pick the pad whose scale note is nearest in pitch (16 pads = 2 octaves + 2 degrees of the
// current key/scale). `bendSt` = the ch2 pitch bend (+-2 st) that precedes a microtonal-key note-on. Header-only, tested by tests/test_notepad.cpp.
#pragma once
#include <math.h>
#include <stdint.h>
#include "scale.h"

static inline uint8_t pmorPadForNote(int note, float bendSt) {
    const float target = 440.f * exp2f((((float)note + bendSt) - 69.f) / 12.f);
    uint8_t best = 0; float bestD = 1e9f;
    for (uint8_t pd = 0; pd < 16; pd++) {
        int deg, oct; scalePadToNote(pd, deg, oct);
        const float d = fabsf(log2f(scaleHz(deg, oct) / target));
        if (d < bestD) { bestD = d; best = pd; }
    }
    return best;
}
