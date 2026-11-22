// pmor_hostctl.h -- PMOR MIDI (PROTOCOL section 7) -> core events for pmor.core~ (notes-only host), header-only so tests/test_parity.cpp and
// tests/test_hostctl.cpp run the exact code the external runs. Everything here executes on the render thread (core globals are single-threaded).
//
//   ch1 drum notes, ch3 sampler notes, ch2 synth notes (+ ch2 pitch bend), ch4 CC1..6 sensors (bipolar CC3..6 decode (d-63.5)/63.5),
//   ch5 CC = FX engage, ch6 CC = FX/perform state (CC1 effective FSR macro, CC2 tilt filter, CC3 (MSB) + CC35 (LSB) synth octave, 14 bit, 8192 = 0, +-2 octaves, CC4 env macro,
//   CC8..11 drum/sampler perform throws up/right/down/left, CC12 perform engine (127 = none), CC16..23 / 24..31 / 33,34,36..39 per-bank FX AMOUNT / LIDAR / MIX; CC35 is the octave LSB, never MIX).
//   ch6 CC2/3/4/8..12 are not emitted by firmware 6071de3 yet (FIRMWARE_TODO.md in the handoff): the host side is ready for them.
//   ch4 CC7 (master fader) is deliberately ignored: the exact FADER param (PD_GLOBAL 2) already reaches the core through P pushes, and a 7-bit CC must not fight it.
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "shared_state.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "params.h"
#include "pmor_notepad.h"

namespace pmorhc {

enum EvType : uint8_t { HE_PARAM, HE_DRUM, HE_SMP, HE_SYN_ON, HE_SYN_OFF, HE_FX_ON, HE_FX_OFF, HE_CTL, HE_BEND, HE_FXCC };
struct Ev { uint8_t t; uint16_t id; float v; };

struct State {
    float   bend = 0.f;               // ch2 pitch bend in semitones, queued before the note-on it belongs to
    int16_t padOf[128];               // note -> pad carrying it (-1 none)
    int16_t noteOf[16];               // pad -> note (-1 free)
    uint8_t octMsb = 64, octLsb = 0;  // ch6 CC3 / CC35: synth octave as 14 bit (7 bit steps were 38 cents)
    State() { reset(); }
    void reset() { bend = 0.f; octMsb = 64; octLsb = 0; for (auto& x : padOf) x = -1; for (auto& x : noteOf) x = -1; }
};

static inline float noteHz(int note, float bendSt) { return 440.f * exp2f((((float)note + bendSt) - 69.f) / 12.f); }

// Synth note at its exact pitch (any octave of the 5-octave scale table, microtones included). The core has no play-by-pitch call, so the pad is only a
// voice tag: a free pad (nearest in pitch) is chosen and its scale-table entry is held at the exact Hz for the duration of the trigger.
static inline void synOn(State& s, int note, float bendSt, float vel) {
    const float hz = noteHz(note, bendSt);
    if (s.padOf[note] >= 0) { synthNoteOff((uint8_t)s.padOf[note]); s.noteOf[s.padOf[note]] = -1; s.padOf[note] = -1; }
    int best = -1; float bestD = 1e9f;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int pd = 0; pd < 16; pd++) {
            if (pass == 0 && s.noteOf[pd] >= 0) continue;
            int deg, oct; scalePadToNote((uint8_t)pd, deg, oct);
            const float d = fabsf(log2f(scaleHz(deg, oct) / hz));
            if (d < bestD) { bestD = d; best = pd; }
        }
    if (s.noteOf[best] >= 0) s.padOf[s.noteOf[best]] = -1;          // all 16 busy: steal the nearest
    int deg, oct; scalePadToNote((uint8_t)best, deg, oct);
    float& cell = scale.hz[oct * SCALE_DEGREES + deg]; const float keep = cell;
    cell = hz; synthNoteOn((uint8_t)best, vel); cell = keep;
    s.padOf[note] = (int16_t)best; s.noteOf[best] = (int16_t)note;
}
static inline void synOff(State& s, int note) {
    int16_t pd = s.padOf[note & 127]; if (pd < 0) return;
    synthNoteOff((uint8_t)pd); s.noteOf[pd] = -1; s.padOf[note & 127] = -1;
}

static inline void applyFxCc(State& s, uint8_t cc, uint8_t raw) {
    const float v = raw / 127.f;
    switch (cc) {
        case 1: fx.ext_fsr = v; break;
        case 2: fx.tilt_t = v; break;                                    // the tilt filter reads fx.tilt_t every block; g.on_fx_page stays false so nothing overwrites it
        case 3: case 35: {                                               // CC3 = MSB, CC35 = LSB of a 14 bit value; 8192 = 0 octaves, +-2 octaves at the ends
            if (cc == 3) s.octMsb = raw; else s.octLsb = raw;
            float o = ((float)((s.octMsb << 7) | s.octLsb) - 8192.f) * (2.f / 8191.f); syn.perf_octave = o < -2.f ? -2.f : o > 2.f ? 2.f : o; } break;
        case 4: syn.perf_env_macro = v; break;
        case 8: fx.perf_up = v; break; case 9: fx.perf_right = v; break; case 10: fx.perf_down = v; break; case 11: fx.perf_left = v; break;
        case 12: fx.perf_engine = raw < ENG_COUNT ? raw : 0xFF; break;
        default: {
            uint16_t idx = 0;
            if (cc >= 16 && cc < 24) idx = (uint16_t)(130 + cc - 16);
            else if (cc >= 24 && cc < 32) idx = (uint16_t)(140 + cc - 24);
            else if (cc > 32 && cc < 40 && cc != 35) idx = (uint16_t)(150 + cc - 32);
            if (idx) { uint16_t id = pmorParamId(PD_FX, idx); if (id) pmorParamSet(id, v); }
        }
    }
}

static inline void apply(State& s, const Ev& e) {
    switch (e.t) {
        case HE_PARAM:
            if ((e.id >> 10) == PD_FX && (e.id & 1023) < 128) break;      // FX engage is owned by ch5 (a P push of held=0 would kill an effect PMOR's loop keeps on)
            pmorParamSet(e.id, e.v); break;
        case HE_DRUM:   drumsTrigger((uint8_t)e.id, e.v); break;
        case HE_SMP:    samplerPlay((uint8_t)e.id, e.v); break;
        case HE_SYN_ON: synOn(s, e.id & 127, s.bend, e.v); break;
        case HE_SYN_OFF: synOff(s, e.id & 127); break;
        case HE_FX_ON:  fxPress((uint8_t)e.id); break;
        case HE_FX_OFF: fxRelease((uint8_t)e.id); break;
        case HE_BEND:   s.bend = e.v; break;
        case HE_FXCC:   applyFxCc(s, (uint8_t)e.id, (uint8_t)e.v); break;
        case HE_CTL:    switch (e.id) {
            case 1: g.fsr = e.v; break; case 2: g.lidar = e.v; break; case 3: g.accel_x = e.v; break;
            case 4: g.accel_y = e.v; break; case 5: g.joy_x = e.v; break; case 6: g.joy_y = e.v; break;
            default: break; } break;
    }
}

// raw MIDI -> event. false = not for the core (unknown channel / CC7 / seed CCs, which the caller handles).
static inline bool decode(int st, int d1, int d2, Ev& e) {
    const int ch = (st & 0x0F) + 1, kind = st & 0xF0;
    const bool on = kind == 0x90 && d2 > 0, off = kind == 0x80 || (kind == 0x90 && d2 == 0);
    if (ch == 1 && on)  { e = { HE_DRUM, (uint16_t)((d1 - 36) & 15), d2 / 127.f }; return true; }
    if (ch == 3 && on)  { e = { HE_SMP, (uint16_t)((d1 - 36) & 15), d2 / 127.f }; return true; }
    if (ch == 2 && kind == 0xE0) { e = { HE_BEND, 0, (((d2 << 7) | d1) - 8192) / 8191.f * 2.f }; return true; }   // +-2 st, 14 bit
    if (ch == 2 && on)  { e = { HE_SYN_ON, (uint16_t)(d1 & 127), d2 / 127.f }; return true; }
    if (ch == 2 && off) { e = { HE_SYN_OFF, (uint16_t)(d1 & 127), 0.f }; return true; }
    if (kind == 0xB0 && ch == 4 && d1 >= 1 && d1 <= 6) { e = { HE_CTL, (uint16_t)d1, d1 >= 3 ? d2 / 63.5f - 1.f : d2 / 127.f }; return true; }
    if (kind == 0xB0 && ch == 5) { e = { d2 >= 64 ? HE_FX_ON : HE_FX_OFF, (uint16_t)d1, 0.f }; return true; }
    if (kind == 0xB0 && ch == 6) { e = { HE_FXCC, (uint16_t)d1, (float)d2 }; return true; }
    return false;
}

}  // namespace pmorhc
