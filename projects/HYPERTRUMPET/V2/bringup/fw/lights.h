// Test 07: FCOB strip (data on D29 = PB14, driven by SPI2 + DMA with the MISO/MOSI functions swapped), NeoTrellis 4x4 (seesaw, I2C), audio-reactive lava-lamp lights, Seed audio passthrough.
// - Strip: WS2812 via SPI, 4 SPI bits per WS bit at a rate picked from the real SPI kernel clock (target 3.125 MHz): '1000' = 0 (T0H 0.32 us, T0L 0.96 us),
//   '1110' = 1 (T1H 0.96 us, T1L 0.32 us), period 1.28 us. WS2812B datasheet: T0H 0.4 +-0.15, T1H 0.8 +-0.15, T0L 0.85 +-0.15, T1L 0.45 +-0.15 us
//   (V5 parts: T0H 0.22 to 0.38, T1H 0.58 to 1.0), reset > 50 us (V5: > 280 us). The earlier 3-bit scheme gave T1H = 0.64 us, too short, so '1' read as '0'.
//   The data pin is D29 = PB14, which is SPI2_MISO (AF5). SPI_CFG2.IOSWP (HAL: SPI_IO_SWAP_ENABLE, RM0433 SPI_CFG2 bit 15, writable only while SPE = 0)
//   swaps the MISO and MOSI functions, so the master's transmit data comes out on PB14. libDaisy's SpiHandle sets IOSwap disabled and cannot be
//   told otherwise, so the pin is muxed through SpiHandle (direction TWO_LINES with PB14 as the 'miso' pin, so its AF5 setup runs) and the bit is set
//   right after Init and again before every frame (a libDaisy re-init after a DMA error would clear it). Soft NSS, no sclk / nss pins.
//   The DMA buffer sits in D2 SRAM (DMA_BUFFER_MEM_SECTION) and is cleared at run time, never given an initializer.
//   Refresh about 60 fps from the main loop; never from the audio callback.
// - NeoTrellis: protocol ported from Adafruit_Seesaw (BSD; https://github.com/adafruit/Adafruit_Seesaw, Adafruit_seesaw.h/.cpp,
//   seesaw_neopixel.cpp, Adafruit_NeoTrellis.cpp): status base 0x00 (HW_ID 0x01 = 0x55, SWRST 0x7F), neopixel base 0x0E
//   (PIN 0x01 = 3, SPEED 0x02 = 1, BUF_LENGTH 0x03, BUF 0x04, SHOW 0x05, GRB order), keypad base 0x10 (EVENT 0x01, INTENSET 0x02,
//   COUNT 0x04, FIFO 0x10; event byte = key<<2 | edge, edge 3 = pressed (rising), 2 = released (falling); seesaw key = row*8 + col).
//   I2C only from the main loop, one job per pass, writes in 24-byte chunks 2 ms apart (PMOR's lesson), reads with STOP + 500 us gap.
// - Audio: envelope follower (attack 5 ms, release 250 ms) and an onset detector, computed in the callback into volatile floats.
#pragma once
#include <math.h>
#include "tl_retry.h"
#include "per/spi.h"
#include "sys/dma.h"
#include "stm32h7xx_hal.h"

static const int STRIP_N = 157;
static const int STRIP_BYTES = 2048;                                  // 12 bytes per pixel (4 SPI bits per WS bit) = 1884, then zeros: a 164-byte (about 420 us) latch gap; 2048 is a multiple of the DMA FIFO burst
static uint8_t DMA_BUFFER_MEM_SECTION g_strip_buf[2048];             // declared WITHOUT an initializer; cleared in stripInit()
static daisy::SpiHandle g_spi;
static volatile bool g_strip_busy = false; static uint32_t g_strip_t0 = 0, g_strip_kicks = 0;
#ifdef TEST_DELAY
static volatile uint32_t g_strip_n = 0, g_strip_sg = 0, g_strip_sgmax = 0;   // frames sent by the blocking path, frames stretched past nominal + 40 us, worst stretch in us
static volatile uint32_t g_strip_done = 0, g_strip_cons = 0; static uint32_t g_strip_dmafail = 0;   // DMA mode: completions, consecutive stalls, times it fell back
#if defined(HX_MIN_DRONE) && !defined(HX_RIG)
static volatile bool g_strip_dma = false;   // drone-only build: the 08 strip path (CPU-fed register send, no HAL EOT wait), which the user saw lighting the strip
#else
static volatile bool g_strip_dma = true;    // rig build: DMA (measured: the CPU-fed send had stretched gaps on 24 % of frames with one fader effect on)
#endif
static volatile uint32_t g_mute_n = 0;                                // samples since StartAudio: output soft-mute at boot
static volatile float g_opk[2] = {0, 0};                              // output peaks since the last frame, after the output gain
#endif
static const int STRIP_LEAD = 64;   // zero bytes BEFORE the pixels (~164 us low) so the first WS bit doesn't ride the SPI start-up edge (first pixel showed green)
static uint32_t g_strip_hz = 0; static bool g_strip_ok = false;
#ifdef TEST_DELAY
static int g_strip_ps = 0;
#endif
#ifdef TEST_DELAY
[[maybe_unused]] static void stripEnd(void*, daisy::SpiHandle::Result r) { g_strip_busy = false; if(r == daisy::SpiHandle::Result::OK) { g_strip_done = g_strip_done + 1; g_strip_cons = 0; } }
#else
[[maybe_unused]] static void stripEnd(void*, daisy::SpiHandle::Result) { g_strip_busy = false; }
#endif

// tunables g_lcap / g_lfloor / g_lsens live in main.cpp (set by the lcap, lfloor, lsens commands)
// ---- audio ----
static volatile float g_env = 0, g_envfast = 0; static volatile uint32_t g_onsets = 0;
static volatile float g_mic_peak = 0, g_mic_ms = 0; static volatile uint32_t g_mic_clips = 0;   // mic meter: peak since the last frame, smoothed mean square, clipped-sample count
[[maybe_unused]] static void audioCb(daisy::AudioHandle::InputBuffer in, daisy::AudioHandle::OutputBuffer out, size_t size) {
    static float sr = 48000.0f, ef = 0, es = 0, ee = 0; static uint32_t cool = 0, tick = 0;
    static float aA = 1.0f - expf(-1.0f / (0.005f * 48000.0f)), aR = 1.0f - expf(-1.0f / (0.250f * 48000.0f));
    static float fA = 1.0f - expf(-1.0f / (0.001f * 48000.0f)), fR = 1.0f - expf(-1.0f / (0.015f * 48000.0f));
    static float sA = 1.0f - expf(-1.0f / (0.040f * 48000.0f)), sR = 1.0f - expf(-1.0f / (0.120f * 48000.0f));
    (void)sr;
#ifdef TEST_DELAY
    if(!g_b2a) g_b2a = System::GetNow();                                  // boot to first audio callback (ms)
    uint32_t cyc0 = DWT->CYCCNT;                                          // CPU load of this callback
    { static uint32_t lastStart = 0; if(lastStart && (cyc0 - lastStart) > (uint32_t)(1.5f * (float)size * g_cyc_per_sample)) g_xrunGap = g_xrunGap + 1; lastStart = cyc0; }   // callback started more than 1.5 block periods after the previous one: a deadline was missed
    static float chkPh = 0.0f; float opkL = 0, opkR = 0;
    static float dtmpL[128], dtmpR[128]; size_t dn = size > 128 ? 128 : size;
    static float toneBuf[128], tonePh = 0.0f, toneV = 0.0f; const float* chainIn = in[0];
    if(g_toneAmp > 0.0f) { const float inc = g_toneHz / 48000.0f; for(size_t i = 0; i < dn; i++) { toneBuf[i] = g_toneAmp * sinf(6.2831853f * tonePh); tonePh += inc * (1.0f + 0.03f * sinf(toneV)); toneV += 6.2831853f * 4.0f / 48000.0f; if(toneV > 6.2831853f) toneV -= 6.2831853f; if(tonePh >= 1.0f) tonePh -= 1.0f; } chainIn = toneBuf; }   // 4 Hz +-3 % vibrato: a perfectly steady sine nulls against its own delayed copies (comb), which is not a dropout   // `sim tone`: a steady test tone replaces the mic at the start of the chain
    if(g_noiseAmp > 0.0f) { static uint32_t nrng = 0x2545F491u; for(size_t i = 0; i < dn; i++) { nrng = nrng * 1664525u + 1013904223u; toneBuf[i] = g_noiseAmp * ((float)(nrng >> 8) * (1.0f / 8388608.0f) - 1.0f); } chainIn = toneBuf; }   // `sim noise <dbfs>`: white noise (rms about 0.577 x amp) instead of the mic: broadband, no comb nulls
    {   // mic NOISE GATE (web UI toggle). Peak follower 0.5 ms / 30 ms; opens above the threshold, closes 4 dB below it after a 50 ms hold;
        // gain opens in 1 ms and closes over 80 ms (no chatter, no clicks). Off = the signal is untouched.
        static float ngEnv = 0.0f, ngG = 1.0f; static uint32_t ngHold = 0; static float ngBuf[128];
        static const float eA = 1.0f - expf(-1.0f / (0.0005f * 48000.0f)), eR = 1.0f - expf(-1.0f / (0.030f * 48000.0f));
        static const float gA = 1.0f - expf(-1.0f / (0.001f * 48000.0f)), gR = 1.0f - expf(-1.0f / (0.080f * 48000.0f));
        if(g_ngOn) {
            const float thO = powf(10.0f, g_ngThr / 20.0f), thC = thO * 0.631f;
            for(size_t i = 0; i < dn; i++) {
                const float x = chainIn[i], a = fabsf(x); ngEnv += (a > ngEnv ? eA : eR) * (a - ngEnv);
                float tgt; if(ngEnv > thO) { ngHold = 2400; tgt = 1.0f; } else if(ngHold) { ngHold--; tgt = 1.0f; } else tgt = (ngEnv > thC && ngG > 0.5f) ? 1.0f : 0.0f;
                ngG += (tgt > ngG ? gA : gR) * (tgt - ngG); ngBuf[i] = x * ngG;
            }
            chainIn = ngBuf;
        } else { ngG = 1.0f; ngHold = 0; }
    }
    float bsq = 0.0f;
    fxChain(chainIn, dtmpL, dtmpR, dn);
#endif
    const float sens = (float)g_lsens; (void)sens; float bpk = 0, bms = 0; uint32_t clip = 0;
#ifdef TEST_MIC
    static float og = g_og_lin; static const float ogA = 1.0f - expf(-1.0f / (0.020f * 48000.0f));   // output gain: one-pole ramp, 20 ms, no zipper
#endif
    for(size_t i = 0; i < size; i++) {
        float x = in[0][i];                                           // line in, left channel only drives the lights
#ifdef TEST_MIC
        og += ogA * (g_og_lin - og);
#ifdef TEST_DELAY
        float oL = dtmpL[i], oR = dtmpR[i];
        { const float m = 0.5f * (oL + oR); oL = m; oR = m; }   // 2026-11-17: MONO SUM to both outs. The rig listens on one channel (one wireless TX), so every effect must be on L AND R. Stereo outputs (reverb/delay/autopan) are summed, not dropped.
        if(g_chk) { chkPh += 440.0f / 48000.0f; if(chkPh >= 1.0f) chkPh -= 1.0f; float t = 0.1f * sinf(6.2831853f * chkPh); oL = g_chk == 1 ? t : 0.0f; oR = g_chk == 2 ? t : 0.0f; }   // probe: -20 dBFS 440 Hz on one channel only, after the chain, before the gain
        uint32_t mn = g_mute_n; float mg = mn < 9600 ? 0.0f : mn < 14400 ? (float)(mn - 9600) / 4800.0f : 1.0f; g_mute_n = mn < 14400 ? mn + 1 : mn;   // hold 0 for 200 ms after StartAudio, then ramp in over 100 ms (power-on pop)
        oL *= og * mg; oR *= og * mg;
#if defined(HX_MIN_DRONE) || defined(HX_OUTSTAGE)
        limiterSample(oL, oR);              // output limiter at the very end (ceiling -1 dBFS)
#endif
#ifdef HX_MIN_DRONE
        capSample(oL);
#endif
        // reverb (stereo) times the output gain, which stays last in the chain
        bsq += oL * oL + oR * oR;
        { float aL = fabsf(oL), aR = fabsf(oR); opkL = aL > opkL ? aL : opkL; opkR = aR > opkR ? aR : opkR; }     // level meter BEFORE the mute
        if(g_mute) { out[0][i] = 0.0f; out[1][i] = 0.0f; } else { out[0][i] = oL; out[1][i] = oR; }
#else
        out[0][i] = in[0][i] * og; out[1][i] = in[0][i] * og;         // dry mono: in L to out L and out R, times the output gain (meter below measures the input)
#endif
        bms += x * x; if(fabsf(x) > bpk) bpk = fabsf(x); if(fabsf(x) >= 0.99f) clip++;
#else
        out[0][i] = in[0][i]; out[1][i] = in[1][i];                   // unity passthrough
#endif
        float a = fabsf(x);
        ee += (a > ee ? aA : aR) * (a - ee);
        ef += (a > ef ? fA : fR) * (a - ef);
        es += (a > es ? sA : sR) * (a - es);
        tick++;
        if(tick - cool > 4800 && ef > es * 1.7f + 0.0003f) { cool = tick; g_onsets = g_onsets + 1; }   // >= 100 ms apart
    }
    g_env = ee; g_envfast = ef;
#ifdef TEST_DELAY
    {   // output level BEFORE the mute stage: block RMS over both channels; with a steady input tone, a block more than 20 dB under the running level is a dropout
        static float lvl = 0.0f; static int warm = 0; const float rms = sqrtf(bsq / (float)(2 * size));
        if(g_toneAmp > 0.0f || g_noiseAmp > 0.0f) { if(warm < 500) { lvl += (rms - lvl) * 0.05f; warm++; } else { if(rms < 0.1f * lvl) g_olDrops = g_olDrops + 1; else lvl += 0.002f * (rms - lvl); } } else { warm = 0; lvl = 0.0f; }
        if(rms < g_olMin) g_olMin = rms; g_olSum = g_olSum + rms; g_olN = g_olN + 1; g_olLvl = lvl;
    }
    g_opk[0] = opkL > g_opk[0] ? opkL : g_opk[0]; g_opk[1] = opkR > g_opk[1] ? opkR : g_opk[1];
#endif
#ifdef TEST_DELAY
    { const uint32_t used = DWT->CYCCNT - cyc0; float load = (float)used / ((float)size * g_cyc_per_sample); if(used > g_cbMax) g_cbMax = used; if(load > 1.0f) g_xrunOver = g_xrunOver + 1; fxCpu(load); }
#endif
#ifdef TEST_MIC
    if(bpk > g_mic_peak) g_mic_peak = bpk;
    g_mic_ms = g_mic_ms + 0.1f * (bms / (float)size - g_mic_ms); g_mic_clips = g_mic_clips + clip;
#endif
}

// ---- lava-lamp field: 1D value noise with domain warping ----
static inline float hash1(int n) { uint32_t h = (uint32_t)n * 0x9E3779B1u; h ^= h >> 15; h *= 0x85EBCA77u; h ^= h >> 13; return (h & 0xFFFF) / 65535.0f; }
static inline float vnoise(float x) { int i = (int)floorf(x); float f = x - i; f = f * f * (3 - 2 * f); float a = hash1(i), b = hash1(i + 1); return a + (b - a) * f; }
static inline float sstep(float a, float b, float x) { float t = (x - a) / (b - a); t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3 - 2 * t); }
struct RGB { float r, g, b; };
static const RGB kBlue = { 70 / 255.0f, 150 / 255.0f, 255 / 255.0f };   // Trellis key flash (non-rig builds only)
static float g_capBoost = 1.0f;                                         // strip brightness multiplier from the level (stripFrame)
static float g_gamma[256];
#ifndef STRIP_FROM_END
#define STRIP_FROM_END 0   // 0: pixel 0 is the near (mouthpiece) end and motion flows toward the bell; 1: the strip is wired from the bell end
#endif
static RGB g_px[STRIP_N]; static float g_px_s[STRIP_N]; static RGB g_tr[16]; static float g_flash[16];
static float g_resid[STRIP_N][3];                                      // temporal dithering residue (cap 64 leaves few LSBs)

// 2026-11-18 strip v4 (padfx-4.11): OFF AT REST. Lights only while playing. The gate is the web UI light threshold (lthr) on the input envelope g_env, with
// 4 dB of hysteresis (opens above lthr, closes below lthr - 4), so it does not flicker at the edge. g_act = the gate smoothed (attack 25 ms, release 260 ms);
// below 0.003 every pixel is exactly 0. g_lvl = the dynamic level (lthr .. lthr + 24 dB, 0..1), smoothed 40 ms up / 250 ms down.
// SPEED follows g_lvl: colour drift 0.04 .. 0.6 palette steps/s, motion 6 .. 105 px/s. Palette = hot pink / warm yellow / light blue blended along the strip
// with value-noise domain warping, three incommensurate noise clocks: no fixed loop. Brightness follows g_lvl (35 % .. 100 %) times g_act.
static float g_act = 0, g_lvl = 0, g_envs = 0, g_pp = 0, g_pw = 0, g_ps = 0; static bool g_open = false;
static const float kPal[2][3] = { { 255, 90, 190 }, { 110, 200, 255 } };   // 4.13: pink + light blue only (no yellow)
static inline void palCol(float p, float* o) {
    p -= 2.0f * floorf(p / 2.0f); int i = (int)p; if(i > 1) i = 1; const int j = 1 - i; const float f = sstep(0.0f, 1.0f, p - (float)i);
    for(int c = 0; c < 3; c++) o[c] = (kPal[i][c] + (kPal[j][c] - kPal[i][c]) * f) * (1.0f / 255.0f);
}
static void lightsCompute(float dt) {
    const float db = 20.0f * log10f(g_env + 1e-7f), thr = g_lthr;       // threshold from the web UI (lthr)
    static float hold = 0.0f;   // 4.13: close only after 10 dB below thr AND 350 ms hold, so note tails stay lit
    if(db > thr) { g_open = true; hold = 0.35f; } else if(g_open) { if(db < thr - 10.0f) { hold -= dt; if(hold <= 0.0f) g_open = false; } else hold = 0.35f; }
    float l = (db - thr) / 24.0f; l = l < 0 ? 0 : l > 1 ? 1 : l;
    if(!g_open) l = 0.0f;
    g_act += ((g_open ? 1.0f : 0.0f) - g_act) * (1.0f - expf(-dt / (g_open ? 0.020f : 0.450f)));
    g_lvl += (l - g_lvl) * (1.0f - expf(-dt / (l > g_lvl ? 0.040f : 0.250f)));
    g_envs = g_lvl; g_capBoost = 1.0f + 1.0f * g_lvl;
    const bool dark = g_act < 0.003f;
    g_ps += dt * (20.0f + 280.0f * g_lvl); if(g_ps > 1.0e6f) g_ps -= 1.0e6f;   // 4.14: travel position in px; speed 20 .. 300 px/s follows loudness
    const float bri = g_act * (0.70f + 0.30f * g_lvl);   // 4.13: high floor, no dark colours while lit
    for(int i = 0; i < STRIP_N + 16; i++) {
        bool strip = i < STRIP_N; float x = strip ? (float)(STRIP_FROM_END ? (STRIP_N - 1 - i) : i) : ((i - STRIP_N) + 0.5f) / 16.0f * STRIP_N;   // x = distance from the near (mouthpiece) end
        RGB c = { 0, 0, 0 };
        if(!dark) {
            // 4.15: sparse dots travelling mouthpiece -> bell, one every 40 px, black between. Head 2-3 px, short fading tail.
            const float SP = 40.0f, u = x - g_ps; const float fi = floorf(u / SP); const float f = u - fi * SP, d = SP - f;
            const float head = fmaxf(expf(-f * f / 1.6f), expf(-d * d / 1.6f)), tail = d < 12.0f ? 0.55f * expf(-d / 3.0f) : 0.0f; const float v = head > tail ? head : tail;
            const int ci = ((int)fi) & 1; const float k = bri * v;          // alternate pink / light blue per dot
            c.r = kPal[ci][0] * (1.0f / 255.0f) * k; c.g = kPal[ci][1] * (1.0f / 255.0f) * k; c.b = kPal[ci][2] * (1.0f / 255.0f) * k;
        }
        if(!strip) { int k = i - STRIP_N; c.r += (kBlue.r - c.r) * g_flash[k]; c.g += (kBlue.g - c.g) * g_flash[k]; c.b += (kBlue.b - c.b) * g_flash[k]; }
        if(c.r > 1) c.r = 1; if(c.g > 1) c.g = 1; if(c.b > 1) c.b = 1;
#if defined(TEST_DELAY) && (!defined(HX_MIN_DRONE) || defined(HX_RIG))
        if(strip) { g_px[i] = c; g_px_s[i] = 1.0f; }                     // rig builds: the Trellis is the pad menu (its own code owns g_tr[]), lights on the strip only
#else                                                                   // 08 / drone-only: lights on the strip AND the Trellis
        if(strip) { g_px[i] = c; g_px_s[i] = 1.0f; } else { g_tr[i - STRIP_N] = c; }
#endif
    }
    for(int k = 0; k < 16; k++) g_flash[k] *= expf(-dt / 0.3f);        // key flashes decay
}
static inline uint8_t gam(float c) { c = c < 0 ? 0 : c > 1 ? 1 : c; return (uint8_t)(c * 255.0f + 0.5f); }

// ---- strip ----
#ifdef TEST_DELAY
#include "ws2812_dma.h"                                                // register-level DMA2 S6 -> SPI2 (libDaisy's DmaTransmit never completed)
static void stripHwInit(int ps);
#endif
[[maybe_unused]] static void stripInit() {
    memset(g_strip_buf, 0, sizeof(g_strip_buf));
    for(int i = 0; i < 256; i++) g_gamma[i] = powf(i / 255.0f, 2.2f);
    uint32_t f = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
    int ps = 0; float best = 1e12f;
    for(int p = 0; p < 8; p++) { float hz = f / (float)(2 << p), d = fabsf(hz - 3.125e6f); if(d < best) { best = d; ps = p; } }
    g_strip_hz = f / (2 << ps);
#ifdef TEST_DELAY
    g_strip_ps = ps;
#endif
#ifdef TEST_DELAY
    stripHwInit(ps);
}
static void stripHwInit(int ps) {
#endif
    daisy::SpiHandle::Config c;
    c.periph = daisy::SpiHandle::Config::Peripheral::SPI_2; c.mode = daisy::SpiHandle::Config::Mode::MASTER; c.direction = daisy::SpiHandle::Config::Direction::TWO_LINES;      // TWO_LINES so libDaisy muxes the 'miso' pin (PB14); HAL's transmit path still runs transmit-only
    c.nss = daisy::SpiHandle::Config::NSS::SOFT; c.datasize = 8;
    c.baud_prescaler = (daisy::SpiHandle::Config::BaudPrescaler)ps;
    c.pin_config.miso = daisy::seed::D29;                              // PB14 = SPI2_MISO, AF5; carries the transmit data once IOSWP is set. mosi / sclk / nss stay unset
    g_strip_ok = g_spi.Init(c) == daisy::SpiHandle::Result::OK;
    if(g_strip_ok) SPI2->CFG2 |= SPI_CFG2_IOSWP;                       // SPE is 0 after Init: swap MISO and MOSI functions
#ifdef TEST_DELAY
    if(g_strip_ok && g_strip_dma) wsDmaInit();
#endif
}
[[maybe_unused]] static void stripFrame() {
    float cap = (float)g_lcap * ledRamp() * g_capBoost; if(cap > 150.0f) cap = 150.0f;   // volume-mapped brightness, hard ceiling 150/255 (boost/BMS current headroom)
    for(int i = 0; i < STRIP_N; i++) {
        float ch[3] = { g_px[i].g, g_px[i].r, g_px[i].b };                // WS2812 order: G, R, B
        uint8_t* o = &g_strip_buf[STRIP_LEAD + i * 12];
        float lv[3], mx = 0.0f; for(int k = 0; k < 3; k++) { lv[k] = g_gamma[gam(ch[k])] * g_px_s[i] * cap; if(lv[k] > mx) mx = lv[k]; }
        for(int k = 0; k < 3; k++) {
            // hue guard (2026-11-17): at dim levels a minor channel (the red or green in a blue pixel) is a fraction of one 8-bit step, and
            // dithering it flickers the pixel red / green. Channels under 1/4 of the pixel's main channel, or under 1.0 step, are dropped.
            if(lv[k] < 0.25f * mx || lv[k] < 1.0f) { lv[k] = (k == 2 && mx >= 1.0f && lv[k] >= 0.25f * mx) ? 1.0f : 0.0f; g_resid[i][k] = 0.0f; }
            float lin = lv[k] + g_resid[i][k];
            int q = (int)lin; if(q > 255) q = 255; g_resid[i][k] = lin - q;
            for(int n = 0; n < 4; n++) {                                          // 2 WS bits per output byte, MSB first: 0 -> 1000, 1 -> 1110
                uint8_t hi = ((q >> (7 - 2 * n)) & 1) ? 0xE : 0x8, lo = ((q >> (6 - 2 * n)) & 1) ? 0xE : 0x8;
                o[k * 4 + n] = (uint8_t)((hi << 4) | lo);
            }
        }
    }
    if(!(SPI2->CR1 & SPI_CR1_SPE)) SPI2->CFG2 |= SPI_CFG2_IOSWP;      // keep the swap across any libDaisy re-init
    SCB_CleanDCache_by_Addr((uint32_t*)g_strip_buf, STRIP_BYTES);     // push CPU writes to RAM before the DMA reads them (in case the D2 region is cacheable under the bootloader MPU)
    g_strip_busy = true; g_strip_t0 = daisy::System::GetNow();
#ifdef TEST_DELAY
    if(g_strip_dma) {                                                 // DMA from the non-cacheable D2 buffer: the audio IRQs can no longer stretch the WS2812 bit stream
        if(!wsDmaStart(g_strip_buf, STRIP_BYTES)) { g_strip_busy = false; g_strip_cons = g_strip_cons + 1; }
        return;
    }
#endif
#ifdef TEST_DELAY
    {   // polled send (no HAL EOT wait). Stretch = the longest gap between two byte writes beyond the FIFO time (16 B ~ 41 us): the FIFO ran dry and the line idled low.
        const uint32_t gap = wsPolledSend(g_strip_buf, STRIP_BYTES);
        const float hz = g_cyc_per_sample * 48000.0f, fifoUs = 16.0f * 8.0f / (float)g_strip_hz * 1e6f, overUs = (float)gap / hz * 1e6f - fifoUs;
        g_strip_n = g_strip_n + 1; if(overUs > 0.0f) { g_strip_sg = g_strip_sg + 1; if((uint32_t)overUs > g_strip_sgmax) g_strip_sgmax = (uint32_t)overUs; }
    }
#else
    g_spi.BlockingTransmit(g_strip_buf, STRIP_BYTES, 20);            // blocking: ~5.3 ms. The libDaisy DMA path sent ONE frame then never signalled done (measured: fr 1, kicks climbing, strip frozen on frame 1)
#endif
    g_strip_busy = false;
}

// ---- NeoTrellis (seesaw) ----
static DevState g_tl = DEV_MISSING; static uint8_t g_tl_fail = 0; static uint32_t g_tl_next = 0, g_tl_key_t = 0, g_tl_push_t = 0, g_tl_stage_t = 0; static uint8_t g_tl_stage = 0;
#ifdef TEST_DELAY
static uint32_t g_tl_show_t = 0;
#define TL_SHOW_OK(n) ((n) - g_tl_show_t >= 5)      // test 10: never read keys within 5 ms of a SHOW, the seesaw bit-bangs the NeoPixels then and does not answer
#define TL_MARK_SHOW(n) (g_tl_show_t = (n))
#define TL_FAIL_MAX 30                               // 300 ms of failed key reads before it is called an error (was 5 = 50 ms)
#else
#define TL_SHOW_OK(n) true
#define TL_MARK_SHOW(n) ((void)0)
#define TL_FAIL_MAX 5
#endif
static uint16_t g_keys = 0; static uint32_t g_tl_events = 0; static uint8_t g_tl_hw = 0;
// Trellis orientation: logical pad index (pad 1 = top-left as the player sees it = logical 0) from the physical seesaw key/pixel index.
// TL_ROT = number of 90-degree turns; set from the boot marker (logical pad 1 white, pad 4 red, pad 13 green, pad 16 blue for 4 s).
#ifndef TL_ROT
#define TL_ROT 0
#endif
#ifdef TEST_DELAY
static inline int tlMap(int p) { int r = p >> 2, c = p & 3; for(int i = 0; i < TL_ROT; i++) { int t = r; r = c; c = 3 - t; } return r * 4 + c; }
#else
static inline int tlMap(int p) { return p; }   // non-test-10 builds keep the original orientation (08_mic stays byte-identical to the known-good build)
#endif
static const uint8_t ADDR_TL = 0x2E;
#ifdef TEST_DELAY
static uint32_t g_tr_err[5] = {0, 0, 0, 0, 0}; static uint8_t g_tr_last = 0;      // [0] bus stretch / recover, [1] NACK on the write address, [2] NACK on a data byte, [3] NACK on the read address, [4] absurd FIFO count (readback bad); last = type of the latest error
static bool ssWrite(uint8_t base, uint8_t reg, const uint8_t* d, uint8_t n) {
    g_bus.start(); const bool a = g_bus.byteW(ADDR_TL << 1); bool dd = g_bus.byteW(base); dd = g_bus.byteW(reg) && dd;
    for(uint8_t i = 0; i < n; i++) dd = g_bus.byteW(d[i]) && dd;
    g_bus.stop(); g_bus.hd();
    if(!a) { g_tr_err[1]++; g_tr_last = 1; } else if(!dd) { g_tr_err[2]++; g_tr_last = 2; }
    const bool st = g_bus.stretch_err; const bool r = i2cFin(a && dd); if(st) { g_tr_err[0]++; g_tr_last = 5; }
    return r;
}
#else
static bool ssWrite(uint8_t base, uint8_t reg, const uint8_t* d, uint8_t n) {
    g_bus.start(); bool ok = g_bus.byteW(ADDR_TL << 1); ok = g_bus.byteW(base) && ok; ok = g_bus.byteW(reg) && ok;
    for(uint8_t i = 0; i < n; i++) ok = g_bus.byteW(d[i]) && ok;
    g_bus.stop(); g_bus.hd(); return i2cFin(ok);
}
#endif
#ifdef TEST_DELAY
static bool ssRead(uint8_t base, uint8_t reg, uint8_t* d, uint8_t n) {              // write prefix, STOP, 500 us gap (the seesaw needs 250..500 us), new START, read
    g_bus.start(); const bool a = g_bus.byteW(ADDR_TL << 1); bool dd = g_bus.byteW(base); dd = g_bus.byteW(reg) && dd; g_bus.stop(); g_bus.hd();
    daisy::System::DelayUs(500);
    g_bus.start(); const bool a2 = g_bus.byteW((ADDR_TL << 1) | 1);
    for(uint8_t i = 0; i < n; i++) d[i] = g_bus.byteR(i == n - 1);
    g_bus.stop(); g_bus.hd();
    if(!a) { g_tr_err[1]++; g_tr_last = 1; } else if(!dd) { g_tr_err[2]++; g_tr_last = 2; } else if(!a2) { g_tr_err[3]++; g_tr_last = 3; }
    const bool st = g_bus.stretch_err; const bool r = i2cFin(a && dd && a2); if(st) { g_tr_err[0]++; g_tr_last = 5; }
    return r;
}
#else
static bool ssRead(uint8_t base, uint8_t reg, uint8_t* d, uint8_t n) {              // write prefix, STOP, 500 us gap, new START, read
    g_bus.start(); bool ok = g_bus.byteW(ADDR_TL << 1); ok = g_bus.byteW(base) && ok; ok = g_bus.byteW(reg) && ok; g_bus.stop(); g_bus.hd();
    daisy::System::DelayUs(500);
    g_bus.start(); ok = g_bus.byteW((ADDR_TL << 1) | 1) && ok;
    for(uint8_t i = 0; i < n; i++) d[i] = g_bus.byteR(i == n - 1);
    g_bus.stop(); g_bus.hd(); return i2cFin(ok);
}
#endif
[[maybe_unused]] static bool trellisInit() {
    if(!i2cProbe(ADDR_TL)) { g_tl = DEV_MISSING; return false; }
    uint8_t ff = 0xFF; ssWrite(0x00, 0x7F, &ff, 1);                                  // software reset
    daisy::System::Delay(100);
    bool found = false;
    for(int r = 0; r < 20 && !found; r++) { uint8_t c = 0; ssRead(0x00, 0x01, &c, 1); g_tl_hw = c; if(c == 0x55) found = true; else daisy::System::Delay(10); }   // SAMD09
    if(!found) { g_tl = DEV_ERROR; return false; }
    uint8_t pin = 3, spd = 1, len[2] = { 0, 48 };
    ssWrite(0x0E, 0x01, &pin, 1); ssWrite(0x0E, 0x02, &spd, 1); ssWrite(0x0E, 0x03, len, 2);
    for(int k = 0; k < 16; k++) { uint8_t cmd[2] = { (uint8_t)((k / 4) * 8 + (k % 4)), 0x19 }; ssWrite(0x10, 0x01, cmd, 2); }   // press + release events (STATE | ACTIVE falling+rising)
    uint8_t one = 1; ssWrite(0x10, 0x02, &one, 1);                                    // INTENSET
    g_tl = DEV_OK; g_tl_fail = 0; g_tl_stage = 0; return true;
}
#ifdef TEST_DELAY
// Test 10 version: a failed read is retried (short back-off), never latched; the error state needs 1.5 s of continuous failure. LED frames at 30 Hz, only the changed half of the
// 48-byte buffer is written and a frame with no change costs no I2C at all. Counters feed the tr{} telemetry.
static TlRetry g_tlr; static uint32_t g_tl_up_ms = 0, g_tl_recs = 0, g_tl_late = 0, g_tl_hold_until = 0, g_tl_fail_t0 = 0, g_tl_shows = 0, g_tl_writes = 0, g_tl_keyreads = 0, g_tl_reinits = 0; static uint8_t g_tl_last[48]; static bool g_tl_lastValid = false; static uint8_t g_tl_mask = 0;
static volatile uint32_t g_lpmax_tr = 0;                       // longest main-loop pass (us) since the last tr{} frame, written in main()
static void trellisPoll(uint32_t now) {
    if(g_tl != DEV_OK) {
        if(g_tlr.due(now)) {                                             // one short probe (bus recovery + address probe, < 1 ms when absent), back-off 150 ms .. 5 s (tl_retry.h)
            g_tl_reinits++; if(g_bus.sdaStuck()) { g_bus.recover(); g_tl_recs++; } DevState w = g_tl; trellisInit(); g_tlr.result(now, g_tl == DEV_OK);   // 2026-11-17: no blind 9-clock bursts every retry (suspected Trellis twitch); recover only when SDA is held low
            if(g_tl == DEV_OK) { g_tl_up_ms = now; g_tl_lastValid = false; }       // full LED redraw on the next frame
            if(w != g_tl) g_cmd_hello = true;
        }
        return;
    }
    if(g_tl_stage == 0 && now - g_tl_key_t >= 10 && now - g_tl_show_t >= 5 && (int32_t)(now - g_tl_hold_until) >= 0) {          // keys at 100 Hz, never within 5 ms of a SHOW (the seesaw bit-bangs the NeoPixels then and does not answer)
        g_tl_key_t = now; uint8_t cnt = 0; g_tl_keyreads++;
        bool ok = ssRead(0x10, 0x04, &cnt, 1);
        if(ok && cnt > 32) { g_tr_err[4]++; g_tr_last = 4; ok = false; }                // FIFO count that cannot be real: a corrupted read
        if(!ok) {
            if(g_tl_fail == 0) g_tl_fail_t0 = now;
            if(g_tl_fail < 255) g_tl_fail++;
            g_tl_hold_until = now + (g_tl_fail < 3 ? 0 : 20);                              // short retry, then back off 20 ms
            if(now - g_tl_fail_t0 >= 1500) { g_tl = DEV_ERROR; g_tlr.next = now + 2000; g_tlr.backoff = 2000; g_cmd_hello = true; }
            return;
        }
        g_tl_fail = 0;
        if(cnt > 0) {
            uint8_t n = cnt > 8 ? 8 : cnt, buf[8];
            if(ssRead(0x10, 0x10, buf, n)) for(uint8_t i = 0; i < n; i++) {
                uint8_t edge = buf[i] & 3, num = buf[i] >> 2, idx = (num / 8) * 4 + (num % 8);
                if(idx > 15) continue;
                if(edge == 3) { const int L = tlMap(idx); g_keys |= (1u << L); g_flash[L] = 1.0f; g_tl_events++; }   // rising = pressed
                else if(edge == 2) g_keys &= ~(1u << tlMap(idx));                                      // falling = released
            }
        }
        return;
    }
    if(g_tl_stage == 0 && now - g_tl_push_t >= 33) { if(now - g_tl_push_t > 66) g_tl_late++; g_tl_push_t = now; g_tl_stage = 1; g_tl_stage_t = 0; }   // LED frame at 30 Hz, 3 jobs of which only the needed ones run
    if(g_tl_stage && now - g_tl_stage_t >= 2) {
        static uint8_t gb[48];
        if(g_tl_stage == 1) {                                                           // build the 48-byte GRB buffer, find the changed halves
            float cap = (float)g_lcap * ledRamp();
            for(int k = 0; k < 16; k++) { RGB q = g_tr[tlMap(k)]; if(false) { const int L = tlMap(k);   /* orientation marker: verified 2026-11-17 (pad 1 white at top-left, TL_ROT 0); set true to show it again */ q.r = q.g = q.b = 0.0f; if(L == 0) { q.r = q.g = q.b = 1.0f; } else if(L == 3) q.r = 1.0f; else if(L == 12) q.g = 1.0f; else if(L == 15) q.b = 1.0f; } float ch[3] = { q.g, q.r, q.b }; for(int c = 0; c < 3; c++) { float v = g_gamma[gam(ch[c])] * cap + 0.5f; gb[k * 3 + c] = v > 255 ? 255 : (uint8_t)v; } }
            g_tl_mask = 0; if(!g_tl_lastValid || memcmp(gb, g_tl_last, 24)) g_tl_mask |= 1; if(!g_tl_lastValid || memcmp(gb + 24, g_tl_last + 24, 24)) g_tl_mask |= 2;
            if(!g_tl_mask) { g_tl_stage = 0; return; }                                  // nothing changed: no I2C this frame
            if(g_tl_mask & 1) { uint8_t h[26]; h[0] = 0; h[1] = 0; memcpy(h + 2, gb, 24); if(!ssWrite(0x0E, 0x04, h, 26)) g_tl_lastValid = false; g_tl_writes++; }
        } else if(g_tl_stage == 2) { if(g_tl_mask & 2) { uint8_t h[26]; h[0] = 0; h[1] = 24; memcpy(h + 2, gb + 24, 24); if(!ssWrite(0x0E, 0x04, h, 26)) g_tl_lastValid = false; g_tl_writes++; } }
        else { if(ssWrite(0x0E, 0x05, nullptr, 0)) { memcpy(g_tl_last, gb, 48); g_tl_lastValid = true; } else g_tl_lastValid = false; g_tl_shows++; g_tl_show_t = now; g_tl_stage = 0; return; }   // SHOW
        g_tl_stage++; g_tl_stage_t = now;
    }
}
static void putTrellisTele(uint32_t now) {                       // tr{}: state, error counts by type, last error, rates over the last window, longest main-loop pass
    static uint32_t t0 = 0, ev0 = 0, sh0 = 0, wr0 = 0, kr0 = 0, lpmax = 0;
    static float evs = 0, shs = 0, wrs = 0, krs = 0;
    if(now - t0 >= 1000) { const float dt = (float)(now - t0) * 0.001f; evs = (g_tl_events - ev0) / dt; shs = (g_tl_shows - sh0) / dt; wrs = (g_tl_writes - wr0) / dt; krs = (g_tl_keyreads - kr0) / dt; ev0 = g_tl_events; sh0 = g_tl_shows; wr0 = g_tl_writes; kr0 = g_tl_keyreads; t0 = now; lpmax = g_lpmax_tr; g_lpmax_tr = 0; }
    puts_(",\"tr\":{\"ok\":"); put(g_tl == DEV_OK ? '1' : '0'); puts_(",\"hw\":"); putu(g_tl_hw); puts_(",\"err\":["); for(int i = 0; i < 5; i++) { if(i) put(','); putu(g_tr_err[i]); }
    puts_("],\"last\":"); putu(g_tr_last); puts_(",\"fail\":"); putu(g_tl_fail); puts_(",\"re\":"); putu(g_tl_reinits); puts_(",\"late\":"); putu(g_tl_late); puts_(",\"rec\":"); putu(g_tl_recs); puts_(",\"up\":"); putu(g_tl_up_ms); puts_(",\"ev\":"); putu((uint32_t)(evs * 10)); puts_(",\"sh\":"); putu((uint32_t)shs); puts_(",\"wr\":"); putu((uint32_t)wrs); puts_(",\"kr\":"); putu((uint32_t)krs);
    puts_(",\"lp\":"); putu(lpmax / 1000); put('.'); putu((lpmax % 1000) / 100); puts_("}");
}
#else
[[maybe_unused]] static void trellisPoll(uint32_t now) {
    if(g_tl != DEV_OK) { if(now >= g_tl_next) { g_tl_next = now + 2000; DevState w = g_tl; trellisInit(); if(w != g_tl) g_cmd_hello = true; } return; }
    if(g_tl_stage == 0 && now - g_tl_key_t >= 10 && TL_SHOW_OK(now)) {          // keys at 100 Hz
        g_tl_key_t = now; uint8_t cnt = 0;
        if(!ssRead(0x10, 0x04, &cnt, 1)) { if(++g_tl_fail >= TL_FAIL_MAX) { g_tl = DEV_ERROR; g_tl_next = now + 2000; g_cmd_hello = true; } return; }
        g_tl_fail = 0;
        if(cnt > 0) {
            uint8_t n = cnt > 8 ? 8 : cnt, buf[8];
            if(ssRead(0x10, 0x10, buf, n)) for(uint8_t i = 0; i < n; i++) {
                uint8_t edge = buf[i] & 3, num = buf[i] >> 2, idx = (num / 8) * 4 + (num % 8);
                if(idx > 15) continue;
                if(edge == 3) { const int L = tlMap(idx); g_keys |= (1u << L); g_flash[L] = 1.0f; g_tl_events++; }   // rising = pressed
                else if(edge == 2) g_keys &= ~(1u << tlMap(idx));                                      // falling = released
            }
        }
        return;
    }
    if(g_tl_stage == 0 && now - g_tl_push_t >= 66) { g_tl_push_t = now; g_tl_stage = 1; g_tl_stage_t = 0; }   // LED frame at ~15 fps, 3 jobs
    if(g_tl_stage && now - g_tl_stage_t >= 2) {
        static uint8_t gb[48];
        if(g_tl_stage == 1) {                                                           // build the 48-byte GRB buffer once per frame
            float cap = (float)g_lcap * ledRamp();
            for(int k = 0; k < 16; k++) { float ch[3] = { g_tr[k].g, g_tr[k].r, g_tr[k].b }; for(int c = 0; c < 3; c++) { float v = g_gamma[gam(ch[c])] * cap + 0.5f; gb[k * 3 + c] = v > 255 ? 255 : (uint8_t)v; } }
            uint8_t h[26]; h[0] = 0; h[1] = 0; memcpy(h + 2, gb, 24); ssWrite(0x0E, 0x04, h, 26);
        } else if(g_tl_stage == 2) { uint8_t h[26]; h[0] = 0; h[1] = 24; memcpy(h + 2, gb + 24, 24); ssWrite(0x0E, 0x04, h, 26); }
        else { ssWrite(0x0E, 0x05, nullptr, 0); TL_MARK_SHOW(now); g_tl_stage = 0; return; }   // SHOW
        g_tl_stage++; g_tl_stage_t = now;
    }
}
#endif

// ---- top level ----
static uint32_t g_ls_t = 0; static uint32_t g_ls_frames = 0;
#ifdef TEST_DELAY
#ifndef HX_MIN_DRONE
#include "fx_pads.h"
#endif                       // NeoTrellis pads -> effects (v1 map), LEDs
#endif
static void lightsInit() {
#ifdef TEST_L_STRIP
    stripInit();
#endif
#ifdef TEST_L_AUDIO
    hw.SetAudioBlockSize(48);
#ifdef TEST_DELAY
    g_mute_n = 0;
#endif
    hw.StartAudio(audioCb);                                            // audio before the NeoTrellis, which goes last (PMOR boot order)
#endif
#ifdef TEST_L_TRELLIS
#ifdef TEST_DELAY
    g_bus.recover();                                                     // 9-clock bus recovery; no blocking wait: the retries every 150 ms during the first 3 s cover the seesaw power-up
#endif
    trellisInit();
#endif
}
static void lightsPoll(uint32_t now) {
#ifdef TEST_L_STRIP
#ifdef TEST_DELAY
    if(g_strip_busy && g_strip_dma && wsDmaPoll()) { g_strip_busy = false; g_strip_done = g_strip_done + 1; g_strip_cons = 0; }   // EOT polled, no IRQ
#endif
#ifdef TEST_DELAY
#define STRIP_TIMEOUT_MS (g_strip_dma ? 40u : 12u)
#else
#define STRIP_TIMEOUT_MS 12u
#endif
    if(g_strip_busy && now - g_strip_t0 > STRIP_TIMEOUT_MS) { g_strip_busy = false; g_strip_kicks++;   // DMA timeout 40 ms: a frame is 5.2 ms but completion is polled from the main loop (passes up to ~10 ms), so only a really stuck transfer counts
#ifdef TEST_DELAY
        if(g_strip_dma) wsDmaAbort();
        g_strip_cons = g_strip_cons + 1;
#endif
    }
#ifdef TEST_DELAY
    if(g_strip_dma && g_strip_cons >= 3) {                                 // DMA never completes: fall back to blocking transmit for good (re-init clears a stuck HAL state) and say so in the telemetry
        g_strip_dma = false; g_strip_dmafail++; stripHwInit(g_strip_ps);
    }
#endif   // a frame is ~5.9 ms: if the DMA-done callback never fires, don't freeze on one frame
    if(g_strip_ok && !g_strip_busy && now - g_ls_t >= 33) { float dt = (now - g_ls_t) * 0.001f; if(dt > 0.1f) dt = 0.1f; g_ls_t = now; lightsCompute(dt); stripFrame(); g_ls_frames++; }
#else
    if(now - g_ls_t >= 16) { float dt = (now - g_ls_t) * 0.001f; if(dt > 0.1f) dt = 0.1f; g_ls_t = now; lightsCompute(dt); }
#endif
#ifdef TEST_DELAY
#ifndef HX_MIN_DRONE
    padPoll(now);
#endif
                                  // after the lights compute, before the LED frame is built: the pads own g_tr[]
#endif
#ifdef TEST_L_TRELLIS
    { SEC_T0(tt0); trellisPoll(now); SEC_T1(2, tt0); }
#endif
}
static void putHex2(uint8_t v) { static const char* H = "0123456789abcdef"; put(H[v >> 4]); put(H[v & 15]); }
static uint8_t dispc(float c, float s) { c *= s; c = c < 0 ? 0 : c > 1 ? 1 : c; return (uint8_t)(powf(c, 0.45f) * 255.0f + 0.5f); }
static void lightsStreamStrip(uint32_t now) {                          // 10 Hz: 53 decimated strip pixels, RRGGBB hex at display brightness (about 360 bytes)
    puts_("{\"t\":\"lt\",\"ms\":"); putu(now); puts_(",\"fr\":"); putu(g_ls_frames); puts_(",\"s\":\"");
    for(int i = 0; i < STRIP_N; i += 3) { putHex2(dispc(g_px[i].r, g_px_s[i])); putHex2(dispc(g_px[i].g, g_px_s[i])); putHex2(dispc(g_px[i].b, g_px_s[i])); }
    puts_("\"}\n");
}
static void lightsStreamKeys(uint32_t now) {                           // separate line: the 16 Trellis pixels (about 130 bytes)
    puts_("{\"t\":\"lk\",\"ms\":"); putu(now); puts_(",\"k\":\"");
    for(int k = 0; k < 16; k++) { putHex2(dispc(g_tr[k].r, 1)); putHex2(dispc(g_tr[k].g, 1)); putHex2(dispc(g_tr[k].b, 1)); }
    puts_("\"}\n");
}
static void putLightsHello() {
    puts_(",\"lights\":{\"cap\":"); putu(g_lcap); puts_(",\"floor\":"); putu(g_lfloor); puts_(",\"sens\":"); putu(g_lsens); puts_(",\"lthr\":"); putd((int)(g_lthr * 10.0f)); puts_(",\"ng\":["); put(g_ngOn ? '1' : '0'); put(','); putd((int)(g_ngThr * 10.0f)); puts_("]");
    puts_(",\"strip\":{\"n\":"); putu(STRIP_N); puts_(",\"hz\":"); putu(g_strip_hz); puts_(",\"ok\":"); put(g_strip_ok ? '1' : '0'); puts_(",\"iosw\":"); put((SPI2->CFG2 & SPI_CFG2_IOSWP) ? '1' : '0'); puts_(",\"kicks\":"); putu(g_strip_kicks);
#ifdef TEST_DELAY
    puts_(",\"sg\":"); putu(g_strip_sg); puts_(",\"sgmax\":"); putu(g_strip_sgmax); puts_(",\"n\":"); putu(g_strip_n); puts_(",\"dma\":"); put(g_strip_dma ? '1' : '0'); puts_(",\"done\":"); putu(g_strip_done); puts_(",\"fb\":"); putu(g_strip_dmafail);
    puts_(",\"eot\":"); putu(g_ws_eot); puts_(",\"ndz\":"); putu(g_ws_ndz); puts_(",\"werr\":"); putu(g_ws_err);
    puts_(",\"dbg\":["); for(int i = 0; i < 6; i++) { if(i) put(','); putu(g_ws_dbg[i]); } puts_("],\"dbga\":["); for(int i = 0; i < 6; i++) { if(i) put(','); putu(g_ws_dbgabort[i]); } puts_("]");
#endif
    puts_(",\"fr\":"); putu(g_ls_frames); { uint32_t ck = 0; for(int b = STRIP_LEAD; b < STRIP_LEAD + STRIP_N * 12; b++) ck += g_strip_buf[b] * (uint32_t)(b & 255); puts_(",\"ck\":"); putu(ck); } puts_(",\"pin\":\"D29\"}");
    puts_(",\"trellis\":{\"s\":\""); puts_(g_tl == DEV_OK ? "ok" : g_tl == DEV_ERROR ? "error" : "missing"); puts_("\",\"hw\":"); putu(g_tl_hw); puts_("},\"audio\":\"on\"}");
}
