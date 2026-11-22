// Test 10: Valhalla-style stereo delay as the first DSP stage (additive, only compiled with TEST_DELAY).
// Chain: mic in L -> delay -> output gain -> out L / out R. The lights keep reading the dry mic.
// - Two 2.0 s float lines in SDRAM (DSY_SDRAM_BSS, cleared at run time), 48 kHz, Hermite fractional reads.
// - Mono in feeds both lines. Taps: L at 0.75 x time, R at 1.0 x time. Cross-fed feedback (L line gets R's tap, R line gets L's tap) = ping-pong smear.
// - Feedback fixed at 0.55. Feedback path: HP 120 Hz, LP 4.5 kHz, three allpass diffusers (g 0.5, 5.3/8.7/13.1 ms left and 6.1/9.7/14.3 ms right), soft clip.
// - Modulation: two slow LFOs (0.37 and 0.53 Hz) plus a filtered random wobble on the read positions. Depth follows accel Y (0.5 to 6 ms).
// - Time follows accel Y (80 to 1200 ms) through a one-pole slew of about 300 ms, applied per sample to the read position: a tilt is a tape-style pitch glide, never a zipper.
// - Dry/wet from fader 2, equal power, smoothed. Nothing here blocks; the host side only writes volatile floats.
#pragma once
#include <math.h>
#include "stm32h7xx_hal.h"
static const int DLY_MAX = 96000;                                   // 2.0 s at 48 kHz, per channel
static float DSY_SDRAM_BSS g_dlyL[DLY_MAX];
static float DSY_SDRAM_BSS g_dlyR[DLY_MAX];
// host -> audio (written from the main loop)
static volatile float g_dly_target_ms = 400.0f, g_dly_mod_ms = 2.0f, g_dly_mix = 0.5f;
// IMU accel Y in g, smoothed at the sensor rate (written from i2cPoll)
static volatile float g_dly_ay = 0.0f;
// audio -> host (telemetry)
static volatile float g_dly_ms_cur = 400.0f, g_cb_cpu = 0.0f, g_cb_cpu_pk = 0.0f;
static float g_cyc_per_sample = 8000.0f;

struct DlyAp { float buf[720]; int n, i; float g; };
struct DlyCh { float hp_lp = 0, lp = 0; DlyAp ap[3]; };
static DlyCh g_dc[2];
static int g_dw = 0;
static float g_time_s = 19200.0f, g_mod_s = 96.0f, g_dg = 0.707f, g_wg = 0.707f, g_wob[2] = {0, 0}, g_wob_t[2] = {0, 0}, g_ph[2] = {0, 0.37f};
static uint32_t g_rng = 12345u; static int g_wob_n = 0;

static void dlyInit() {
    memset(g_dlyL, 0, sizeof(g_dlyL)); memset(g_dlyR, 0, sizeof(g_dlyR));      // SDRAM BSS is not cleared by the startup code
    static const float ms[2][3] = { { 5.3f, 8.7f, 13.1f }, { 6.1f, 9.7f, 14.3f } };
    for(int c = 0; c < 2; c++) for(int k = 0; k < 3; k++) { DlyAp& a = g_dc[c].ap[k]; memset(a.buf, 0, sizeof(a.buf)); a.n = (int)(ms[c][k] * 48.0f); a.i = 0; a.g = 0.5f; }
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CYCCNT = 0; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;   // cycle counter for the CPU-load readout
    g_cyc_per_sample = (float)HAL_RCC_GetSysClockFreq() / 48000.0f;
}
static inline float dlyHermite(const float* b, float delay) {
    if(delay < 2.0f) delay = 2.0f; else if(delay > DLY_MAX - 4.0f) delay = DLY_MAX - 4.0f;
    float rp = (float)g_dw - delay; if(rp < 0) rp += DLY_MAX;
    int i1 = (int)rp; float f = rp - (float)i1;
    int i0 = i1 - 1; if(i0 < 0) i0 += DLY_MAX; int i2 = i1 + 1; if(i2 >= DLY_MAX) i2 -= DLY_MAX; int i3 = i2 + 1; if(i3 >= DLY_MAX) i3 -= DLY_MAX;
    float y0 = b[i0], y1 = b[i1], y2 = b[i2], y3 = b[i3];
    float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}
static inline float dlySoft(float x) { if(x > 3.0f) return 1.0f; if(x < -3.0f) return -1.0f; float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2); }   // tanh-like
static inline float dlyPsin(float p) { p -= (int)p; float x = p * 2.0f - 1.0f; float y = x < 0 ? 4.0f * x * (1.0f + x) : 4.0f * x * (1.0f - x); return -y; }       // parabolic sine, p in cycles
static inline float dlyFb(DlyCh& c, float x) {
    static const float aHp = 1.0f - expf(-6.2831853f * 120.0f / 48000.0f), aLp = 1.0f - expf(-6.2831853f * 4500.0f / 48000.0f);
    c.hp_lp += aHp * (x - c.hp_lp); float h = x - c.hp_lp;                  // high-pass 120 Hz
    c.lp += aLp * (h - c.lp); float y = c.lp;                               // low-pass 4.5 kHz
    for(int k = 0; k < 3; k++) { DlyAp& a = c.ap[k]; float d = a.buf[a.i]; float w = y + a.g * d; a.buf[a.i] = w; y = d - a.g * w; if(++a.i >= a.n) a.i = 0; }   // allpass diffusers
    return dlySoft(y);
}
// one block: in (mono), outputs the delayed stereo mix (before the output gain). Called from the audio callback.
static void dlyBlock(const float* in, float* oL, float* oR, size_t size) {
    static const float kT = 1.0f - expf(-1.0f / (0.30f * 48000.0f)), kM = 1.0f - expf(-1.0f / (0.60f * 48000.0f)), kG = 1.0f - expf(-1.0f / (0.020f * 48000.0f)), kW = 1.0f - expf(-6.2831853f * 3.0f / 48000.0f);
    static const float fb = 0.55f;
    float mix = g_dly_mix; mix = mix < 0 ? 0 : mix > 1 ? 1 : mix;
    const float dgT = cosf(mix * 1.5707963f), wgT = sinf(mix * 1.5707963f);   // equal-power crossfade targets
    const float tT = g_dly_target_ms * 48.0f, mT = g_dly_mod_ms * 48.0f;
    for(size_t n = 0; n < size; n++) {
        g_time_s += kT * (tT - g_time_s);                                    // hard slew: tape-style glide
        g_mod_s += kM * (mT - g_mod_s);
        g_dg += kG * (dgT - g_dg); g_wg += kG * (wgT - g_wg);
        if(++g_wob_n >= 14400) { g_wob_n = 0; for(int c = 0; c < 2; c++) { g_rng = g_rng * 1664525u + 1013904223u; g_wob_t[c] = ((g_rng >> 9) & 0xFFFF) / 32768.0f - 1.0f; } }   // new random target every 0.3 s
        for(int c = 0; c < 2; c++) g_wob[c] += kW * (g_wob_t[c] - g_wob[c]);
        g_ph[0] += 0.37f / 48000.0f; g_ph[1] += 0.53f / 48000.0f; if(g_ph[0] >= 1.0f) g_ph[0] -= 1.0f; if(g_ph[1] >= 1.0f) g_ph[1] -= 1.0f;
        float modL = g_mod_s * (0.8f * dlyPsin(g_ph[0]) + 0.2f * g_wob[0]), modR = g_mod_s * (0.8f * dlyPsin(g_ph[1]) + 0.2f * g_wob[1]);
        float tapL = dlyHermite(g_dlyL, 0.75f * g_time_s + modL), tapR = dlyHermite(g_dlyR, g_time_s + modR);
        float fL = dlyFb(g_dc[0], tapR), fR = dlyFb(g_dc[1], tapL);          // cross-fed (ping-pong)
        float x = in[n];
        g_dlyL[g_dw] = x + fb * fL; g_dlyR[g_dw] = x + fb * fR;
        if(++g_dw >= DLY_MAX) g_dw = 0;
        oL[n] = x * g_dg + tapL * g_wg; oR[n] = x * g_dg + tapR * g_wg;
    }
    g_dly_ms_cur = g_time_s / 48.0f;
}
