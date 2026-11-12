// test_notepad.cpp -- max/pmor_notepad.h: a synth note recorded from PMOR plays the pad with the same pitch (key, scale, microtonal bend).
#include <stdio.h>
#include <math.h>
#include <initializer_list>
#include "scale.h"
#include "../max/pmor_notepad.h"
static int fails = 0, passes = 0;
#define CHECK(c, ...) do { if (c) passes++; else { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    for (int mode = 0; mode < SCALE_MODES; mode++) for (float key : {0.f, 3.f, -5.f, 0.5f}) {
        scaleInit(); scale.mode = (uint8_t)mode; scale.key_semis = key; scaleRebuild();
        int bad = 0;
        for (uint8_t pad = 0; pad < 16; pad++) {
            int deg, oct; scalePadToNote(pad, deg, oct);
            const float hz = scaleHz(deg, oct), midi = 69.f + 12.f * log2f(hz / 440.f);
            const int note = (int)lrintf(midi); const float bend = midi - (float)note;           // what PMOR sends: nearest note + bend for the remainder
            const uint8_t got = pmorPadForNote(note, bend);
            int d2, o2; scalePadToNote(got, d2, o2);
            if (fabsf(log2f(scaleHz(d2, o2) / hz)) > 1e-4f) bad++;      // same pitch (several pads can share one)
        }
        CHECK(bad == 0, "mode %d key %.1f: %d pads mapped to a different pitch", mode, key, bad);
    }
    scaleInit(); scaleRebuild();
    CHECK(pmorPadForNote(0, 0.f) < 16 && pmorPadForNote(127, 0.f) < 16, "out-of-range notes clamp to a valid pad");
    printf("test_notepad: %d passed, %d failed\n", passes, fails); return fails ? 1 : 0;
}
