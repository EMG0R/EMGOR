// NotePend: retrigger note-off only when the same ch+pitch is still held.
#include "../pmor_core/note_pend.h"
#include <stdio.h>
static int ons, offs; static uint8_t lastOff = 255;
static void on(uint8_t, uint8_t, uint8_t) { ons++; }
static void off(uint8_t, uint8_t n) { offs++; lastOff = n; }
static int fails;
#define EXPECT(c, ...) do { if (c) printf("  ok   " __VA_ARGS__), puts(""); else { printf("  FAIL " __VA_ARGS__); puts(""); fails++; } } while (0)
int main() {
    NotePend p;
    for (int i = 0; i < 171; i++) { p.hit(1000u + i * 200 + 30, 0, 36 + (i % 8), 100, on, off); p.service(1000u + i * 200 + 150, off); }
    EXPECT(ons == 171 && offs == 171, "171 spaced hits: ons=%d offs=%d (one off each, no extra)", ons, offs);
    ons = offs = 0; NotePend q;
    q.hit(130, 0, 40, 100, on, off);
    EXPECT(ons == 1 && offs == 0, "first hit on an idle pitch: bare note-on (offs=%d)", offs);
    q.hit(150, 0, 40, 100, on, off);
    EXPECT(ons == 2 && offs == 1, "retrigger while held: one off then on (ons=%d offs=%d)", ons, offs);
    q.hit(150, 0, 41, 100, on, off);
    EXPECT(ons == 3 && offs == 1, "different pitch while another is held: no off (offs=%d)", offs);
    q.hit(150, 2, 40, 100, on, off);
    EXPECT(ons == 4 && offs == 1, "same pitch other channel: no off");
    q.service(1000, off);
    EXPECT(offs == 4, "all three pending offs fire once (offs=%d)", offs);
    q.service(2000, off);
    EXPECT(offs == 4, "no double offs");
    // session end mid-note: pending drum/sampler offs and held synth notes are SENT, not dropped
    {
        ons = offs = 0; NotePend r; HeldNotes h;
        r.hit(500, 0, 36, 100, on, off); r.hit(500, 2, 40, 100, on, off);
        h.on[60] = h.on[64] = true;
        NotePend legacy = r; legacy.clear();                    // the old behaviour (s_pend.clear()): nothing is sent
        EXPECT(offs == 0, "clear() alone sends no offs (the bug: %d)", offs);
        r.flush(off); h.flush(1, off);
        EXPECT(offs == 4, "flush sends all 2 pending + 2 held offs (offs=%d)", offs);
        r.flush(off); h.flush(1, off);
        EXPECT(offs == 4, "second flush sends nothing (offs=%d)", offs);
        r.service(9999, off);
        EXPECT(offs == 4, "nothing pending after flush");
    }
    printf("%s\n", fails ? "FAIL" : "PASS"); return fails ? 1 : 0;
}
