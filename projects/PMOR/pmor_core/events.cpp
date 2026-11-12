// events.cpp -- see events.h.
#include "events.h"
#include <math.h>
#include "platform.h"

#define EV_N 256u   // power of two
static PmorEvent s_ring[EV_N];
static volatile uint32_t s_w = 0, s_r = 0;
static volatile uint32_t s_drops = 0;

void pmorEventPush(uint8_t kind, uint8_t a, uint8_t b, uint16_t c) {
    uint32_t w = s_w;
    if ((uint32_t)(w - s_r) >= EV_N) { s_drops = s_drops + 1; return; }
    PmorEvent& e = s_ring[w & (EV_N - 1)];
    e.kind = kind; e.a = a; e.b = b; e.pad_ = 0; e.c = c; e.pad2_ = 0; e.seed = 0;
    PMOR_MEM_BARRIER();
    s_w = w + 1;
}
bool pmorEventPop(PmorEvent* out) {
    uint32_t r = s_r;
    if (r == s_w) return false;
    PMOR_MEM_BARRIER();
    *out = s_ring[r & (EV_N - 1)];
    PMOR_MEM_BARRIER();
    s_r = r + 1;
    return true;
}
uint32_t pmorEventDrops(void) { return s_drops; }
void pmorHzToMidi(float hz, uint8_t* note, uint16_t* bend) {
    if (hz < 8.f) hz = 8.f;
    float m = 69.f + 12.f * log2f(hz / 440.f);
    int n = (int)floorf(m + 0.5f);
    if (n < 0) n = 0;
    if (n > 127) n = 127;
    float frac = m - (float)n;                         // -0.5..+0.5 semitone
    float bf = frac / 2.f * 8191.f; int b = 8192 + (int)(bf + (bf >= 0.f ? 0.5f : -0.5f));   // +-2 semitone bend range, rounded (was truncated)
    if (b < 0) b = 0;
    if (b > 16383) b = 16383;
    *note = (uint8_t)n; *bend = (uint16_t)b;
}
void pmorEventPushSeed(uint32_t seed) {
    uint32_t w = s_w;
    if ((uint32_t)(w - s_r) >= EV_N) { s_drops = s_drops + 1; return; }
    PmorEvent& e = s_ring[w & (EV_N - 1)];
    e.kind = EV_SEED; e.a = e.b = e.pad_ = 0; e.c = e.pad2_ = 0; e.seed = seed;
    PMOR_MEM_BARRIER();
    s_w = w + 1;
}
