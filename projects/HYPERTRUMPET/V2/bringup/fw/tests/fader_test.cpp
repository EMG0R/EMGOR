// Host test of fader_cal.h with the measured rest values. Build: c++ -std=c++14 -I. tests/fader_test.cpp -o /tmp/fader_test   (from bringup/fw)
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const int NCH = 3; static uint16_t g_bp[9];
static void putu(uint32_t) {} static void puts_(const char*) {} static void put(char) {}
static uint16_t linearize(int, uint16_t raw) {                       // the firmware's default A-taper curve (main.cpp defaultCal / linearize)
    if(raw <= g_bp[0]) return 0; if(raw >= g_bp[8]) return 65535;
    for(int k = 0; k < 8; k++) if(raw < g_bp[k + 1]) { float f = (float)(raw - g_bp[k]) / (float)(g_bp[k + 1] - g_bp[k]); return (uint16_t)((k + f) * (65535.0f / 8.0f)); }
    return 65535;
}
static volatile float g_simFader[3] = {-1, -1, -1};
#include "fader_cal.h"
static int fails = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while(0)
int main() {
    const float n = logf(0.15f) / logf(0.5f); for(int k = 0; k < 9; k++) g_bp[k] = (uint16_t)(powf(k / 8.0f, n) * 65535.0f + 0.5f);
    srand(1); uint32_t now = 0; bool everLive = false; float maxFs = 0;
    for(int f = 0; f < 2000; f++, now += 5) {                          // 10 s at rest, raw 24..42 per fader, 14 % settling bump at boot (raw up to 294 for 60 ms)
        uint16_t v[3]; for(int i = 0; i < 3; i++) v[i] = (uint16_t)(24 + rand() % 19); if(f < 12) v[1] = 294, v[2] = 250;
        faderStep(v, now); for(int i = 0; i < 3; i++) { if(g_fs[i] != 0.0f) everLive = true; if(g_fs[i] > maxFs) maxFs = g_fs[i]; }
    }
    CHECK(!everLive, "uncalibrated, at rest, with the boot settling bump: every fader reads EXACTLY 0 for 10 s");
    for(int f = 0; f < 400; f++, now += 5) { uint16_t v[3] = { 63700, 63640, 63770 }; faderStep(v, now); }
    CHECK(g_fs[0] == 1.0f && g_fs[1] == 1.0f && g_fs[2] == 1.0f, "at the top stop (raw 63640..63770) every fader reads EXACTLY 1.0");
    for(int f = 0; f < 400; f++, now += 5) { uint16_t v[3] = { 40, 30, 28 }; faderStep(v, now); }
    CHECK(g_fs[0] == 0.0f && g_fs[1] == 0.0f && g_fs[2] == 0.0f, "back at the bottom: exactly 0");
    for(int f = 0; f < 400; f++, now += 5) { uint16_t v[3] = { 32768, 32768, 32768 }; faderStep(v, now); }
    CHECK(g_fs[0] > 0.2f && g_fs[0] < 0.9f, "mid travel reads in between");
    return fails;
}
