// note_pend.h -- timed note-offs for one-shot (drum / sampler) hits on USB-MIDI. Hardware-free, header-only.
// A retrigger note-off is sent ONLY when that same ch+pitch is still held (its scheduled off not yet sent);
// a hit on an idle pitch is a bare note-on.
#pragma once
#include <stdint.h>
struct NotePend {
    struct E { uint32_t at; uint8_t ch, note; bool used; };
    E t[32] = {};
    typedef void (*OnFn)(uint8_t ch, uint8_t n, uint8_t v);
    typedef void (*OffFn)(uint8_t ch, uint8_t n);
    void hit(uint32_t at, uint8_t ch, uint8_t n, uint8_t v, OnFn on, OffFn off) {
        E* slot = nullptr;
        for (auto& p : t) if (p.used && p.ch == ch && p.note == n) { slot = &p; break; }
        if (slot) off(ch, n);                                   // same pitch already sounding: retrigger
        else for (auto& p : t) if (!p.used) { slot = &p; break; }
        on(ch, n, v);
        if (slot) *slot = { at, ch, n, true }; else off(ch, n);  // table full: end it now
    }
    void service(uint32_t now, OffFn off) { for (auto& p : t) if (p.used && (int32_t)(now - p.at) >= 0) { off(p.ch, p.note); p.used = false; } }
    void clear() { for (auto& p : t) p.used = false; }
    // End of a session: every pending off is SENT (not dropped), then the table is empty. clear() alone left the receiver with stuck notes.
    void flush(OffFn off) { for (auto& p : t) if (p.used) { off(p.ch, p.note); p.used = false; } }
};
// Which synth notes the receiver currently believes are on (ch1). on/off mirror EV_SYN_ON / EV_SYN_OFF; flush ends every held note.
struct HeldNotes {
    bool on[128] = {};
    void flush(uint8_t ch, NotePend::OffFn off) { for (int n = 0; n < 128; n++) if (on[n]) { off(ch, (uint8_t)n); on[n] = false; } }
};
