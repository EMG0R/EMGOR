// HYPERTRUMPET V2 bring-up firmware: one app, compile-time TEST selector.
// Streams newline-delimited JSON over USB CDC (libDaisy's own CDC, interrupt driven,
// so no tud_task() pump is needed and System::Delay is safe).
//   host -> seed (one command per line):  reset | hello | dfu
//   seed -> host:  {"t":"hello",...}  1 Hz and on request
//                  {"t":"s","n":seq,"ms":t,"v":[raw16...]}   200 Hz
//                  {"t":"st","min":[],"max":[],"pp":[],"ppmin":[],"cov":[],"win":N}  20 Hz
// Boot order follows the PMOR rule: USB first, settle, only then ADC and the rest.
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "daisy_seed.h"
#include "usbd_def.h"                                   // USBD_HandleTypeDef: the CDC state is read directly (see usbReady())
#include "hyper_hw.h"

#if defined(TEST_SEL_01_faders)
#include "tests/test_01_faders.h"
#elif defined(TEST_SEL_02_pots)
#include "tests/test_02_pots.h"
#elif defined(TEST_SEL_03_fsr)
#include "tests/test_03_fsr.h"
#elif defined(TEST_SEL_04_joystick)
#include "tests/test_04_joystick.h"
#elif defined(TEST_SEL_05_faders_imu_lidar)
#include "tests/test_05_faders_imu_lidar.h"
#elif defined(TEST_SEL_06_pots2w_enc)
#include "tests/test_06_pots2w_enc.h"
#elif defined(TEST_SEL_07_lights)
#include "tests/test_07_lights.h"
#elif defined(TEST_SEL_07a_strip_only)
#include "tests/test_07a_strip_only.h"
#elif defined(TEST_SEL_07b_audio_only)
#include "tests/test_07b_audio_only.h"
#elif defined(TEST_SEL_07c_trellis_only)
#include "tests/test_07c_trellis_only.h"
#elif defined(TEST_SEL_10_delay)
#include "tests/test_10_delay.h"
#elif defined(TEST_SEL_12_drone)
#include "tests/test_12_drone.h"
#elif defined(TEST_SEL_13_rig)
#include "tests/test_13_rig.h"
#elif defined(TEST_SEL_14_padfx)
#include "tests/test_14_padfx.h"
#elif defined(TEST_SEL_08_mic)
#include "tests/test_08_mic.h"
#elif defined(TEST_SEL_07d_compute_only)
#include "tests/test_07d_compute_only.h"
#else
#error "unknown TEST: add tests/test_<TEST>.h and a branch here"
#endif

#ifdef HX_D12
#define FW_VERSION "drone12-1.4"
#elif defined(HX_MIN_DRONE)
#define FW_VERSION "padfx-4.15"
#elif defined(HX_OUTSTAGE)
#define FW_VERSION "rig-2.6"
#elif defined(TEST_DELAY)
#define FW_VERSION "bringup-1.22"
#else
#define FW_VERSION "bringup-1.2"
#endif
using namespace daisy;

DaisySeed hw;
static AdcChannelConfig g_adc_cfg[12];        // kept alive so the ADC can be re-initialised
static uint32_t g_adc_re = 0;                 // ADC watchdog restarts since boot

struct ChanDef { const char* name; const char* kind; const char* label; int phys; Pin pin; };
#define X(n, k, l, ph, p) { n, k, l, ph, p },
static const ChanDef kCh[] = { TEST_CHANNELS(X) };
#undef X
static const int NCH = sizeof(kCh) / sizeof(kCh[0]);
static const int kMaxCh = 12;

// ---- tuning -----------------------------------------------------------------
static const uint32_t SAMPLE_MS   = 5;     // 200 Hz sample frames
static const int      STAT_EVERY  = 10;    // stats frame every 10th sample frame (20 Hz)
static const uint32_t HELLO_MS    = 1000;
static const int      WIN         = 200;   // rest-noise window = 1 s at 200 Hz

// ---- fader linearization: 9 raw breakpoints at physical positions k/8 -> lin 0..65535 (RAM only) ----
static uint16_t g_bp[kMaxCh][9];
static bool     g_inv[kMaxCh];                  // invert direction (lin = 65535 - lin), 2-wire pots / reversed calibration
static bool     g_calc[kMaxCh];                 // true once the host sent a custom cal
static bool is2w(int c) { return strcmp(kCh[c].kind, "pot2w") == 0; }
static bool isSlider(int c) { return strcmp(kCh[c].kind, "slider") == 0 || strcmp(kCh[c].kind, "pot") == 0 || is2w(c); }   // "linearized" channels: A-taper faders, C-taper pots
static void defaultCal(int c) {                 // inverse of a standard A (audio) taper: 15 % output at mid travel -> out = pos^n, n = ln(.15)/ln(.5)
    // faders: A taper, 15 % output at mid travel -> n = 2.737.  pots (C500K reverse-log): 85 % at mid rotation -> n = 0.234
    const float n = logf(strcmp(kCh[c].kind, "pot") == 0 ? 0.85f : 0.15f) / logf(0.5f);
    for(int k = 0; k < 9; k++) g_bp[c][k] = (uint16_t)(powf(k / 8.0f, n) * 65535.0f + 0.5f);
    g_calc[c] = false; g_inv[c] = false;
}
static uint16_t linearize(int c, uint16_t raw) {
    if(strcmp(kCh[c].kind, "fsr") == 0) {          // PMOR-style: cubed, full scale at 13/16 of raw, deadzone ignores the pulldown rest level
        const float dz = 0.015f * 65535.0f, fsr = 65535.0f * 13.0f / 16.0f;
        float x = ((float)raw - dz) / (fsr - dz); x = x < 0 ? 0 : x > 1 ? 1 : x;
        return (uint16_t)(x * x * x * 65535.0f);
    }
    if(!isSlider(c)) return raw;
    uint16_t out = 65535;
    if(is2w(c) && !g_calc[c]) {
        // default for a 2-wire pot + 47 k: invert the divider. Vout = Vcc*47k/(47k+Rx)  ->  Rx = 47k*(Vcc/V - 1), norm = Rx/500k clamped 0..1
        float r = raw < 1 ? 1.0f : (float)raw;
        float n = 47000.0f * (65535.0f / r - 1.0f) / 500000.0f;
        out = (uint16_t)((n < 0 ? 0 : n > 1 ? 1 : n) * 65535.0f);
    } else {
        const uint16_t* b = g_bp[c];
        if(raw <= b[0]) out = 0;
        else if(raw < b[8]) for(int k = 0; k < 8; k++) if(raw < b[k + 1]) {
            uint32_t span = b[k + 1] - b[k];
            float f = span ? (float)(raw - b[k]) / span : 0.0f;
            out = (uint16_t)((k + f) * (65535.0f / 8.0f)); break;
        }
    }
    return g_inv[c] ? (uint16_t)(65535 - out) : out;
}

// ---- encoders + buttons (test 06): 4x quadrature state machine polled every main-loop pass, buttons debounced 15 ms ----
#ifdef TEST_ENC
struct Enc { daisy::GPIO a, b, sw; uint8_t prev = 0; int32_t cnt = 0; uint32_t bad = 0, presses = 0; bool btn = false, raw = false; uint32_t t_raw = 0; };
static Enc g_enc[2];
static void encInit() {
    static const daisy::Pin pins[2][3] = { { daisy::seed::D1, daisy::seed::D2, daisy::seed::D3 }, { daisy::seed::D4, daisy::seed::D5, daisy::seed::D6 } };
    for(int i = 0; i < 2; i++) {
        g_enc[i].a.Init(pins[i][0], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
        g_enc[i].b.Init(pins[i][1], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
        g_enc[i].sw.Init(pins[i][2], daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
        g_enc[i].prev = (g_enc[i].a.Read() << 1) | g_enc[i].b.Read();
    }
}
static void encPoll(uint32_t now) {
    static const int8_t tab[16] = { 0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0 };   // index (prev<<2 | new), Gray code; 0 on a double step = counted as bad below
    for(int i = 0; i < 2; i++) {
        Enc& e = g_enc[i];
        uint8_t s = (e.a.Read() << 1) | e.b.Read();
        if(s != e.prev) {
            int8_t d = tab[(e.prev << 2) | s];
            if(d == 0) e.bad++; else e.cnt -= d;               // 4 counts per detent (KY-040 style)
            e.prev = s;
        }
        bool down = !e.sw.Read();                                // active low
        if(down != e.raw) { e.raw = down; e.t_raw = now; }
        else if(down != e.btn && now - e.t_raw >= 15) { e.btn = down; if(down) e.presses++; }
    }
}
#endif

// ---- joystick (test 06): centre from the first 200 ms, radial deadzone, auto-span, button on D10 ----
#ifdef TEST_JOY
static int g_jx = -1, g_jy = -1;                               // channel indexes of the "joy" channels
static float g_jc[2], g_jmin[2], g_jmax[2];
static volatile int g_joydz = 6;                              // percent
static volatile bool g_recenter = true;                       // true at boot: average the first 200 ms
static uint32_t g_jt0 = 0; static float g_jsum[2]; static int g_jn = 0; static bool g_jready = false;
static int16_t g_jn_out[2];                                    // normalized -1000..1000
static daisy::GPIO g_jsw; static bool g_jbtn = false, g_jraw = false; static uint32_t g_jtraw = 0, g_jpress = 0;
static void joyInit() {
    for(int i = 0; i < NCH; i++) if(strcmp(kCh[i].kind, "joy") == 0) { if(g_jx < 0) g_jx = i; else if(g_jy < 0) g_jy = i; }
    g_jsw.Init(daisy::seed::D10, daisy::GPIO::Mode::INPUT, daisy::GPIO::Pull::PULLUP);
}
static void joyStep(uint32_t now, const uint16_t* v) {
    if(g_jx < 0 || g_jy < 0) return;
    if(g_recenter) { g_recenter = false; g_jready = false; g_jt0 = now; g_jsum[0] = g_jsum[1] = 0; g_jn = 0; }
    if(!g_jready) {                                            // 50 ms for the ADC to settle, then average 200 ms
        if(now - g_jt0 >= 50) { g_jsum[0] += v[g_jx]; g_jsum[1] += v[g_jy]; g_jn++; }
        if(now - g_jt0 >= 250 && g_jn > 0) { for(int a = 0; a < 2; a++) { g_jc[a] = g_jsum[a] / g_jn; g_jmin[a] = g_jc[a] - 20000.0f; g_jmax[a] = g_jc[a] + 20000.0f; } g_jready = true; }
        g_jn_out[0] = g_jn_out[1] = 0; return;
    }
    float n[2];
    for(int a = 0; a < 2; a++) {
        float r = a ? v[g_jy] : v[g_jx];
        if(r < g_jmin[a]) { g_jmin[a] = r; }
        if(r > g_jmax[a]) { g_jmax[a] = r; }               // running min/max: auto-span
        n[a] = r < g_jc[a] ? (r - g_jc[a]) / (g_jc[a] - g_jmin[a]) : (r - g_jc[a]) / (g_jmax[a] - g_jc[a]);
    }
    float rad = sqrtf(n[0] * n[0] + n[1] * n[1]), dz = g_joydz / 100.0f;
    for(int a = 0; a < 2; a++) {
        float o = 0;
        if(rad > dz) o = n[a] * ((rad - dz) / (1.0f - dz)) / rad;                        // radial deadzone, direction kept
        o = o < -1 ? -1 : o > 1 ? 1 : o; g_jn_out[a] = (int16_t)(o * 1000.0f);
    }
}
static void joyButton(uint32_t now) {
    bool down = !g_jsw.Read();
    if(down != g_jraw) { g_jraw = down; g_jtraw = now; }
    else if(down != g_jbtn && now - g_jtraw >= 15) { g_jbtn = down; if(down) g_jpress++; }
}
#endif

// ---- Hall valves (test 06): rest captured at boot, pole sign + span learned from the press, depth 0..1 ----
#ifdef TEST_HALL
static int g_hi[3] = { -1, -1, -1 };
static float g_hrest[3], g_hspan[3]; static int8_t g_hsign[3]; static int16_t g_hdepth[3];
static bool g_hready = false; static uint32_t g_ht0; static float g_hsum[3]; static int g_hn;
static volatile bool g_hrecal = true; static volatile uint8_t g_hfull = 0;   // bit i = capture valve i's current press as full
static void hallInit() { int k = 0; for(int i = 0; i < NCH && k < 3; i++) if(strcmp(kCh[i].kind, "hall") == 0) g_hi[k++] = i; }
static void hallStep(uint32_t now, const uint16_t* v) {
    if(g_hi[2] < 0) return;
    if(g_hrecal) { g_hrecal = false; g_hready = false; g_ht0 = now; g_hn = 0; for(int k = 0; k < 3; k++) { g_hsum[k] = 0; g_hsign[k] = 0; g_hspan[k] = 6000.0f; } }
    if(!g_hready) {                                            // 50 ms settle, then average 200 ms
        if(now - g_ht0 >= 50) { for(int k = 0; k < 3; k++) g_hsum[k] += v[g_hi[k]]; g_hn++; }
        if(now - g_ht0 >= 250 && g_hn > 0) { for(int k = 0; k < 3; k++) g_hrest[k] = g_hsum[k] / g_hn; g_hready = true; }
        for(int k = 0; k < 3; k++) g_hdepth[k] = 0;
        return;
    }
    for(int k = 0; k < 3; k++) {
        float dev = (float)v[g_hi[k]] - g_hrest[k];
        if(g_hsign[k] == 0 && fabsf(dev) > 1500.0f) g_hsign[k] = dev > 0 ? 1 : -1;      // pole detected from the first real press
        if(g_hfull & (1 << k)) { if(fabsf(dev) > 1500.0f) { g_hsign[k] = dev > 0 ? 1 : -1; g_hspan[k] = fabsf(dev); } g_hfull &= ~(1 << k); }
        float d = g_hsign[k] ? dev * g_hsign[k] : 0.0f;
        if(d > g_hspan[k]) g_hspan[k] = d;                                              // auto-span: largest travel seen
        float x = d / g_hspan[k], dz = 0.03f;
        x = x <= dz ? 0.0f : (x - dz) / (1.0f - dz); x = x > 1 ? 1 : x;
        g_hdepth[k] = (int16_t)(x * 1000.0f);
    }
}
#endif

// ---- stats ------------------------------------------------------------------
static uint16_t g_win[kMaxCh][WIN];
static int      g_wpos = 0, g_wfill = 0;
static uint16_t g_min[kMaxCh], g_max[kMaxCh], g_ppmin[kMaxCh];
static uint32_t g_cov[kMaxCh];                // 32 bins of 2048 counts each

static void resetStats() {
    for(int c = 0; c < kMaxCh; c++) { g_min[c] = 65535; g_max[c] = 0; g_ppmin[c] = 65535; g_cov[c] = 0; }
    g_wpos = 0; g_wfill = 0;
}

// ---- USB CDC ----------------------------------------------------------------
#ifdef TEST_LIGHTS
#ifdef TEST_MIC
#if defined(HX_MIN_DRONE) || defined(HX_OUTSTAGE)
#define OG_MAX10 360
#define OG_DEF10 0
static volatile float g_og_lin = 1.0f;                          // test 12: master -40..+36 dB, default 0 dB
#else
#define OG_MAX10 60
#define OG_DEF10 -120
static volatile float g_og_lin = 0.25118864f;
#endif                  // output gain, linear (default -12 dB); ramped inside the audio callback
// BOOT BREADCRUMB: stage codes written during init into a variable that is NOT zeroed at reset (.axi_bss is NOLOAD). The next boot reports the last stage the PREVIOUS boot reached (hello `bc`).
// 10 hw.Init, 20 USB up, 30 sensors, 40 i2c, 50.. fxGlueInit (one code per effect: 51 drone, 52 mod, 53 reverb, 54 grain, 55 tape, 56 freeze, 57 harmony, 58 trace, 59 ring, 5A glitch, 5B wah, 5C modjoy), 60 params, 70 lights, 80 adc, 99 main loop.
struct BootCrumb { uint32_t magic, stage, seq; };
static BootCrumb g_bc __attribute__((section(".axi_bss")));
static uint32_t g_bcPrevStage = 0xFFFFFFFFu, g_bcPrevSeq = 0;
#define BC(n) do { g_bc.stage = (n); } while(0)
static volatile uint32_t g_usbre = 0, g_txOkAt = 0, g_txDrop = 0, g_b2a = 0; static volatile bool g_reboot = false;   // g_b2a: ms from app start to the first audio callback; g_reboot: `reboot` command
// LED SOFT START: strip + Trellis brightness ramps 0 -> 1 over 1.5 s from the first LED frame, so the LED inrush cannot brown out a battery boost / BMS at power-on.
static uint32_t g_ledT0 = 0; static inline float ledRamp() { const uint32_t now = System::GetNow(); if(!g_ledT0) g_ledT0 = now ? now : 1; const float t = (float)(now - g_ledT0) * (1.0f / 1500.0f); return t >= 1.0f ? 1.0f : t; }   // USB self-heal count, last successful transmit (ms), frames dropped (host not reading / not configured); see usbWatch()
static uint32_t g_rsr = 0;                                    // RCC->RSR at boot: bit 16 LPWR, 17 WWDG1, 18 IWDG1, 19 SFT, 20 POR/PDR, 21 PIN, 22 BOR, 23 D1, 24 D2, 25 CPU
static volatile int g_og_db10 = OG_DEF10;                          // output gain in tenths of a dB, -400..+60
#ifdef TEST_DELAY
static void fxParamsRx(const char* line); static void fxParamsInit(); static void fxParamsPoll(uint32_t now);   // fx_glue_params.h (included below): p / pd / preset commands, parsed in the main loop
#endif
static volatile int g_og_req = OG_DEF10;                           // pending request from the host (set in the CDC ISR, applied in the main loop)
#endif
static volatile int g_lcap = 64, g_lfloor = 14, g_lsens = 8; static volatile float g_lthr = -40.0f; static volatile bool g_ngOn = false; static volatile float g_ngThr = -50.0f;   // mic noise gate (web UI toggle + threshold, dBFS)   // g_lthr: reactive-lighting threshold, dBFS (web UI, next to master volume)     // strip/trellis brightness cap (0..64 of 255), floor (% of cap), audio sensitivity
#endif
static volatile bool g_cmd_reset = false, g_cmd_hello = false, g_cmd_scan = false, g_cal_pending = false;
#ifdef TEST_DELAY
static volatile int g_chk = 0;   // output probe: 0 off, 1 tone on L only, 2 tone on R only
static volatile uint32_t g_lp33 = 0;   // main-loop passes longer than the 33 ms light frame period
static volatile uint32_t g_lpmax = 0;   // longest main-loop period in microseconds since the last frame
#endif
static volatile int g_cmd_audio = -1;   // diag: "aoff" stops audio, "aon" restarts it (main loop), to see if the ADC freeze follows the running audio
static char g_calline[160];
static volatile bool g_dfu = false;

static void rxCb(uint8_t* buf, uint32_t* len) {            // ISR context: tiny line parser
    static char line[160]; static uint8_t n = 0;
    for(uint32_t i = 0; i < *len; i++) {
        char c = (char)buf[i];
        if(c == '\r') continue;
        if(c == '\n') {
            line[n] = 0;
#ifdef TEST_DELAY
            if(!strncmp(line, "p ", 2) || !strncmp(line, "pd ", 3) || !strncmp(line, "preset", 6) || !strncmp(line, "fx ", 3) || !strncmp(line, "focus ", 6) || !strncmp(line, "pad ", 4) || !strncmp(line, "cap", 3) || !strncmp(line, "nosave", 6) || !strncmp(line, "fcal", 4) || !strncmp(line, "sim", 3) || !strncmp(line, "mute", 4) || !strncmp(line, "state", 5)) fxParamsRx(line);   // before the strstr matches below ("preset reset" contains "reset")
            else
#endif
            if(!strncmp(line, "cal", 3)) { if(!g_cal_pending) { strcpy(g_calline, line); g_cal_pending = true; } }   // parsed in the main loop
#ifdef TEST_DELAY
            else if(!strncmp(line, "chk", 3)) g_chk = strstr(line, "off") ? 0 : strchr(line, 'L') ? 1 : strchr(line, 'R') ? 2 : 0;   // output probe: chk L / chk R / chk off
#endif
            else if(strstr(line, "dfu"))        g_dfu = true;
            else if(strstr(line, "reboot")) g_reboot = true;
            else if(strstr(line, "reset")) g_cmd_reset = true;
            else if(strstr(line, "hello")) g_cmd_hello = true;
            else if(strstr(line, "scan"))  g_cmd_scan = true;
#ifdef TEST_MIC
            else if(!strncmp(line, "ogain", 5)) { float d = (float)atof(line + 5); int t = (int)(d * 10.0f + (d < 0 ? -0.5f : 0.5f)); g_og_req = t < -400 ? -400 : t > OG_MAX10 ? OG_MAX10 : t; }
#endif
#ifdef TEST_LIGHTS
            else if(!strncmp(line, "aoff", 4)) g_cmd_audio = 0;
            else if(!strncmp(line, "aon", 3)) g_cmd_audio = 1;
            else if(!strncmp(line, "lcap", 4))   { int v = atoi(line + 4); g_lcap = v < 0 ? 0 : v > 64 ? 64 : v; g_cmd_hello = true; }
            else if(!strncmp(line, "lfloor", 6)) { int v = atoi(line + 6); g_lfloor = v < 0 ? 0 : v > 30 ? 30 : v; g_cmd_hello = true; }
            else if(!strncmp(line, "ngthr", 5))  { float v = (float)atof(line + 5); g_ngThr = v < -90.0f ? -90.0f : v > -10.0f ? -10.0f : v; g_cmd_hello = true; }
            else if(!strncmp(line, "ngate", 5))  { g_ngOn = atoi(line + 5) != 0; g_cmd_hello = true; }
            else if(!strncmp(line, "lthr", 4))   { float v = (float)atof(line + 4); g_lthr = v < -70.0f ? -70.0f : v > 0.0f ? 0.0f : v; g_cmd_hello = true; }
            else if(!strncmp(line, "lsens", 5))  { int v = atoi(line + 5); g_lsens = v < 1 ? 1 : v > 60 ? 60 : v; g_cmd_hello = true; }
#endif
#ifdef TEST_HALL
            else if(!strncmp(line, "valvecal", 8)) { if(strstr(line, "full")) { int n = atoi(strstr(line, "full") + 4); g_hfull |= n >= 1 && n <= 3 ? (1 << (n - 1)) : 7; } else g_hrecal = true; g_cmd_hello = true; }
#endif
#ifdef TEST_JOY
            else if(strstr(line, "recenter")) g_recenter = true;
            else if(!strncmp(line, "joydz", 5)) { int d = atoi(line + 5); if(d >= 0 && d <= 50) g_joydz = d; g_cmd_hello = true; }
#endif
            n = 0;
        } else if(n < sizeof(line) - 1) line[n++] = c;
    }
}
extern "C" bool hyper_dfu_requested(void) { return g_dfu; }

static uint8_t g_tx[2][4600];
static int     g_cur = 0;
static size_t  g_len = 0;
static uint8_t* g_p;
static void put(char c) { if(g_len < sizeof(g_tx[0]) - 1) g_p[g_len++] = (uint8_t)c; }
static void puts_(const char* s) { while(*s) put(*s++); }
static void putu(uint32_t v) { char t[11]; int n = 0; do { t[n++] = '0' + v % 10; v /= 10; } while(v); while(n) put(t[--n]); }
static void putarr16(const uint16_t* a) { put('['); for(int i = 0; i < NCH; i++) { if(i) put(','); putu(a[i]); } put(']'); }
static void putarr32(const uint32_t* a) { put('['); for(int i = 0; i < NCH; i++) { if(i) put(','); putu(a[i]); } put(']'); }

static void ensure(size_t n);                 // defined below, next to sendFrame()
#ifndef TEST_DELAY
#define SEC_T0(v)
#define SEC_T1(i, v)
#endif
#ifdef TEST_DELAY
static volatile float g_dly_ay = 0.0f;   // accel Y in g, smoothed (written by i2c_sensors.h)
static volatile float g_lid_mm = 0.0f; static volatile uint32_t g_lid_ms = 0; static volatile uint8_t g_lid_st = 0;   // last lidar reading (mm, millis, raw status 9 = valid), written by i2c_sensors.h
#endif
#ifdef TEST_I2C
#include "i2c_sensors.h"
#endif
#ifdef TEST_DELAY
#ifdef HX_D12
#include "fx_glue_d12.h"
#elif defined(HX_MIN_DRONE)
#include "fx_glue_min.h"
#else
#include "fx_glue_chain.h"
#endif                       // per-effect glue (fx_glue_reverb.h, fx_glue_drone.h): gate, mapping, telemetry
#endif
#ifdef TEST_LIGHTS
#include "lights.h"
#endif
#ifdef TEST_MIC
// Output gain persisted in QSPI with libDaisy PersistentStorage (PMOR pattern). The app image sits at 0x90040000 (BOOT_SRAM, loaded to SRAM at boot, so the QSPI is
// free to erase and write), the Daisy bootloader's region is below 0x40000, PMOR keeps its own data at 0x7C0000 and 0x7F0000. This record uses the 4 KB sector at
// QSPI offset 0x700000 (0x90700000): far above the app (ends before 0x90100000) and clear of PMOR's offsets. Saved at most once per second after a change, from the
// main loop only, never from the audio callback.
#ifndef TEST_DELAY
struct OutRec { uint32_t magic; int32_t db10; bool operator==(const OutRec& o) const { return magic == o.magic && db10 == o.db10; } bool operator!=(const OutRec& o) const { return !(*this == o); } };
static PersistentStorage<OutRec> g_store(hw.qspi);
static const uint32_t OG_QSPI_OFFSET = 0x700000u, OG_MAGIC = 0x4F475231u;   // "OGR1"
static uint32_t g_og_dirty_ms = 0; static bool g_og_dirty = false, g_og_loaded = false;
static void ogApply(int db10) { g_og_db10 = db10; g_og_lin = powf(10.0f, db10 / 200.0f); }
static void ogInit() {
    OutRec def = { OG_MAGIC, -120 };
    g_store.Init(def, OG_QSPI_OFFSET);
    OutRec& r = g_store.GetSettings();
    if(r.magic == OG_MAGIC && r.db10 >= -400 && r.db10 <= 60) { ogApply(r.db10); g_og_loaded = true; } else ogApply(-120);
    g_og_req = g_og_db10;
}
static void ogPoll(uint32_t now) {
    if(g_og_req != g_og_db10) { ogApply(g_og_req); g_og_dirty = true; g_og_dirty_ms = now; g_cmd_hello = true; }
    if(g_og_dirty && now - g_og_dirty_ms >= 1000) {                       // debounce: one save per second at most
        OutRec& r = g_store.GetSettings(); r.magic = OG_MAGIC; r.db10 = g_og_db10; g_store.Save(); g_og_dirty = false;
    }
}
#else
static void ogInit() { fxParamsInit(); }      // test 10: name-keyed, versioned, CRC-checked record for the output gain + every effect param (fx_glue_params.h)
static void ogPoll(uint32_t now) { fxParamsPoll(now); }
#endif
#endif

// VDDA readback: ADC3 reads the internal VREFINT (polled, once per 2 s, independent of the ADC1 DMA). If the analog supply sags when audio starts, this shows it.
static ADC_HandleTypeDef g_h3; static bool g_h3_ok = false; static uint32_t g_vdda_mv = 0, g_vraw = 0;
static void vddaInit() {
    __HAL_RCC_ADC3_CLK_ENABLE();
    g_h3.Instance = ADC3; g_h3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2; g_h3.Init.Resolution = ADC_RESOLUTION_16B; g_h3.Init.ScanConvMode = ADC_SCAN_DISABLE;
    g_h3.Init.EOCSelection = ADC_EOC_SINGLE_CONV; g_h3.Init.LowPowerAutoWait = DISABLE; g_h3.Init.ContinuousConvMode = DISABLE; g_h3.Init.NbrOfConversion = 1;
    g_h3.Init.DiscontinuousConvMode = DISABLE; g_h3.Init.ExternalTrigConv = ADC_SOFTWARE_START; g_h3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_h3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR; g_h3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN; g_h3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE; g_h3.Init.OversamplingMode = DISABLE;
    if(HAL_ADC_Init(&g_h3) != HAL_OK) return;
    HAL_ADCEx_Calibration_Start(&g_h3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);
    ADC_ChannelConfTypeDef c = {0}; c.Channel = ADC_CHANNEL_VREFINT; c.Rank = ADC_REGULAR_RANK_1; c.SamplingTime = ADC_SAMPLETIME_810CYCLES_5; c.SingleDiff = ADC_SINGLE_ENDED; c.OffsetNumber = ADC_OFFSET_NONE;
    if(HAL_ADC_ConfigChannel(&g_h3, &c) == HAL_OK) g_h3_ok = true;
}
static void vddaRead() {
    if(!g_h3_ok) return;
    HAL_ADC_Start(&g_h3);
    if(HAL_ADC_PollForConversion(&g_h3, 5) == HAL_OK) { g_vraw = HAL_ADC_GetValue(&g_h3); if(g_vraw) g_vdda_mv = (uint32_t)(3300.0f * (float)(*VREFINT_CAL_ADDR) / (float)g_vraw); }
    HAL_ADC_Stop(&g_h3);
}
static void adcStart() {
    static bool started = false;
    if(started) hw.adc.Stop();                         // a restart must stop the ADC and its DMA first
    started = true;
    hw.adc.Init(g_adc_cfg, NCH, AdcHandle::OVS_32);    // 32x hardware oversampling, 16-bit result
    hw.adc.Start();
}
// ---- USB HEALTH (2026-11-17 audit). libDaisy's CDC_Transmit_FS dereferences hUsbDeviceFS.pClassData WITHOUT a null check: after a USB bus reset (host sleep / wake, port reset, hub glitch) the CDC class
// is de-initialised (pClassData = NULL) and a transmit then reads / WRITES through a null pointer (TxBuffer / TxLength at address ~0: ITCM RAM). Our 200 Hz telemetry hits exactly that. usbReady() gates every
// transmit: only a CONFIGURED device with live class data transmits. usbWatch() (main loop) re-initialises the CDC when the device has been unconfigured for 3 s, or when a host that was receiving stops
// accepting data for 3 s (stuck TxState). Re-init count = st.usbre. The onboard LED shows the state (slow = configured, fast = not).
extern "C" USBD_HandleTypeDef hUsbDeviceFS;
static bool g_usbSeen = false; static uint32_t g_usbCfgAt = 0, g_usbReAt = 0, g_usbBack = 8000;   // g_usbCfgAt starts at 5 s: a first-enumeration grace of 8 s (the Mac needs a few seconds after a DFU leave) static bool g_usbSeen = false;
static inline bool usbReady() { return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != nullptr; }
static void rxCb(uint8_t* buf, uint32_t* len);                 // defined below
static void usbWatch(uint32_t now) {
    // A CONFIGURED device is healthy whether or not a host has the port open or closed it (the host not reading is normal): never re-init for that. Re-init only when the device is NOT configured:
    // a host that was configured before and is now gone / lost enumeration (> 8 s, back-off 8, 16, 30 s), or a first enumeration that never completed (> 45 s, then every 120 s).
    const bool rdy = usbReady(); if(rdy) { g_usbCfgAt = now; g_usbSeen = true; g_usbBack = 8000; return; }
    const uint32_t need = g_usbSeen ? g_usbBack : 45000;
    if(now - g_usbCfgAt > need && now - g_usbReAt > (g_usbSeen ? g_usbBack : 120000)) {
        hw.usb_handle.DeInit(UsbHandle::FS_INTERNAL); System::Delay(2); hw.usb_handle.Init(UsbHandle::FS_INTERNAL); hw.usb_handle.SetReceiveCallback(rxCb, UsbHandle::FS_INTERNAL);
        g_usbre = g_usbre + 1; g_usbReAt = now; if(g_usbSeen) g_usbBack = g_usbBack < 30000 ? g_usbBack * 2 : 30000;
    }
}
                // USB TX frames / hello parts dropped because the host was not reading (the firmware never waits on the host)
static void beginFrame() { g_p = g_tx[g_cur]; g_len = 0; }
static void sendFrame() {
    if(!g_len) return;
    // OK means the previous transfer finished, so the other buffer is free. ERR = busy: drop this frame.
    if(!usbReady()) { g_txDrop = g_txDrop + 1; return; }
    if(hw.usb_handle.TransmitInternal(g_tx[g_cur], g_len) == UsbHandle::Result::OK) { g_cur ^= 1; g_txOkAt = System::GetNow(); } else g_txDrop = g_txDrop + 1;
}

// Never let a message straddle the frame limit: if it would not fit, send what is there (or drop it when USB is busy) and start a fresh frame.
static void ensure(size_t n) { if(g_len && g_len + n > 2000) { sendFrame(); beginFrame(); } }

// A hello part is never dropped and never shares a transfer: flush the frame, then send the part alone, retrying while USB is busy (30 ms cap).
static bool sendWhole() {
    if(!g_len) return true;
    uint32_t t0 = System::GetNow();
#if defined(HX_MIN_DRONE) || defined(HX_OUTSTAGE)
    const uint32_t cap = 3;                                // a host that has stopped reading must not stall the main loop: 3 ms, then drop and count
#else
    const uint32_t cap = 30;
#endif
    if(!usbReady()) { g_txDrop = g_txDrop + 1; return false; }
    do { if(hw.usb_handle.TransmitInternal(g_tx[g_cur], g_len) == UsbHandle::Result::OK) { g_cur ^= 1; g_txOkAt = System::GetNow(); return true; } } while(System::GetNow() - t0 < cap);
    g_txDrop = g_txDrop + 1; return false;
}
#ifdef TEST_DELAY
#include "fader_cal.h"
#ifdef HX_D12
#include "fx_controls_d12.h"
#include "fx_glue_params_d12.h"
#elif defined(HX_MIN_DRONE)
#include "fx_controls_min.h"
#include "fx_glue_params_min.h"
#else
#include "fx_controls.h"
#include "fx_glue_params.h"
#endif
#endif
static uint16_t g_hello_len[2] = {0, 0};
static void putHello(uint32_t now) {
    puts_("{\"t\":\"hello\",\"test\":\""); puts_(TEST_ID_STR); puts_("\",\"fw\":\""); puts_(FW_VERSION);
    puts_("\",\"rate\":"); putu(1000 / SAMPLE_MS); puts_(",\"adc_bits\":16,\"ovs\":32,\"win\":"); putu(WIN);
    puts_(",\"ms\":"); putu(now); puts_(",\"rst\":"); putu(g_rsr); puts_(",\"bc\":["); putd((int32_t)g_bcPrevStage); put(','); putu(g_bcPrevSeq); put(']'); puts_(",\"ch\":[");
    for(int i = 0; i < NCH; i++) {
        if(i) put(',');
        puts_("{\"n\":\""); puts_(kCh[i].name); puts_("\",\"k\":\""); puts_(kCh[i].kind);
        puts_("\",\"pin\":\""); puts_(kCh[i].label); puts_("\",\"phys\":"); putu(kCh[i].phys); put('}');
    }
    puts_("],\"cal\":[");
    for(int i = 0; i < NCH; i++) { if(i) put(','); put('['); if(isSlider(i) && !(is2w(i) && !g_calc[i])) for(int k = 0; k < 9; k++) { if(k) put(','); putu(g_bp[i][k]); } put(']'); }
    puts_("],\"calc\":["); for(int i = 0; i < NCH; i++) { if(i) put(','); put(g_calc[i] ? '1' : '0'); }
    puts_("],\"inv\":["); for(int i = 0; i < NCH; i++) { if(i) put(','); put(g_inv[i] ? '1' : '0'); }
    puts_("],\"split\":1}\n");   // core line: the page builds its channel groups from this; the extras follow in a second line (hello2)
}

static void putHello2(uint32_t now) {                          // extras: adc, enc, hall, joy, mic gain, lights, i2c
    puts_("{\"t\":\"hello2\",\"ms\":"); putu(now); puts_(",\"hl\":["); putu(g_hello_len[0]); put(','); putu(g_hello_len[1]); puts_("]");   // byte lengths of the previous core / extras lines
    puts_(",\"adc\":{\"vdda\":"); putu(g_vdda_mv); puts_(",\"vraw\":"); putu(g_vraw); puts_(",\"re\":"); putu(g_adc_re); puts_(",\"dbg\":["); putu(ADC1->ISR); put(','); putu(ADC1->CR); put(','); putu(DMA1_Stream2->NDTR); put(','); putu(DMA1_Stream2->CR); puts_("],\"m0\":"); putu(DMA1_Stream2->M0AR); puts_(",\"buf\":["); { uint16_t* bp = hw.adc.GetPtr(0); for(int i = 0; i < NCH; i++) { if(i) put(','); putu(bp[i]); } } puts_("],\"cfg\":["); putu(ADC1->SQR1); put(','); putu(ADC1->CFGR); put(','); putu(ADC1->CFGR2); put(','); putu(RCC->D3CCIPR); puts_("]}");
#ifdef TEST_ENC
    puts_(",\"enc\":2");
#endif
#ifdef TEST_HALL
    puts_(",\"hall\":{\"rest\":["); for(int k = 0; k < 3; k++) { if(k) put(','); putu((uint32_t)g_hrest[k]); } puts_("],\"sign\":["); for(int k = 0; k < 3; k++) { if(k) put(','); putd(g_hsign[k]); } puts_("],\"span\":["); for(int k = 0; k < 3; k++) { if(k) put(','); putu((uint32_t)g_hspan[k]); } puts_("],\"ready\":"); put(g_hready ? '1' : '0'); puts_("}");
#endif
#ifdef TEST_JOY
    puts_(",\"joy\":{\"dz\":"); putu(g_joydz); puts_(",\"c\":["); putu((uint32_t)g_jc[0]); put(','); putu((uint32_t)g_jc[1]); puts_("],\"mm\":["); putu((uint32_t)g_jmin[0]); put(','); putu((uint32_t)g_jmax[0]); put(','); putu((uint32_t)g_jmin[1]); put(','); putu((uint32_t)g_jmax[1]); puts_("],\"ready\":"); put(g_jready ? '1' : '0'); puts_("}");
#endif
#ifdef TEST_MIC
    puts_(",\"ogain\":"); putd(g_og_db10); puts_(",\"ogsaved\":"); put(g_og_dirty ? '0' : '1'); puts_(",\"ogloaded\":"); put(g_og_loaded ? '1' : '0');
#endif
#ifdef TEST_DELAY
    fxGlueHello(); fxParamsHello();
#endif
#ifdef TEST_LIGHTS
    putLightsHello();
#endif
#ifdef TEST_I2C
    putI2cHello();
#endif
    puts_("}\n");
}

#ifdef TEST_DELAY
// Effect controls (main loop, never the audio callback): faders through fader_cal.h (smooth, linearize, per-fader endpoints, exact 0 and 1 dead zones);
// fader 1 = reverb mix, fader 2 = drone mix; accel Y (g_dly_ay) = reverb WARP / MODDEPTH.
static void rvControls(const uint16_t* v, const uint16_t* lin, uint32_t now) {
    faderStep(v, now);
    fxControlsTick(lin, now);
#ifdef HX_MIN_DRONE
    capPoll();
#endif
    fxClearPoll();
    fxShedPoll(now);
}
#endif

int main(void) {
    g_rsr = RCC->RSR; RCC->RSR |= RCC_RSR_RMVF;
    if(g_bc.magic == 0xB007C0DEu) { g_bcPrevStage = g_bc.stage; g_bcPrevSeq = g_bc.seq; } g_bc.magic = 0xB007C0DEu; g_bc.seq = g_bcPrevSeq + 1; BC(1);
    BC(10); hw.Init(true);                               // 480 MHz boost, as PMOR V3 and the libDaisy Seed3 DevKits do. clocks, board detect (Seed 3 via PH6), codec strap
    dfuBootGesture();                            // both DFU keys low at power-on -> bootloader
    hw.usb_handle.Init(UsbHandle::FS_INTERNAL);  // USB BEFORE any heavy load (PMOR rule)
    hw.usb_handle.SetReceiveCallback(rxCb, UsbHandle::FS_INTERNAL); BC(20);
    // (the 500 ms host-enumeration wait is gone: USB enumerates from its interrupt while the rest of init runs; a battery boot must not wait for a host)

    for(int i = 0; i < NCH; i++)      // 500k pots need the longest sample time (high source impedance; add 100 nF wiper to DGND)
        g_adc_cfg[i].InitSingle(kCh[i].pin, (strcmp(kCh[i].kind, "pot") == 0 || is2w(i)) ? AdcChannelConfig::SPEED_810CYCLES_5 : AdcChannelConfig::SPEED_64CYCLES_5);
    // ADC start moved AFTER the bus, audio, SPI and NeoTrellis bring-up (see below): in fw 1.0 the ADC read a constant 32767 on every channel when it
    // was started first and lightsInit() ran afterwards.
#ifdef TEST_JOY
    BC(30); joyInit();
#endif
#ifdef TEST_HALL
    hallInit();
#endif
    resetStats();
    for(int c = 0; c < NCH; c++) defaultCal(c);
#ifdef TEST_ENC
    encInit();
#endif
#ifdef TEST_I2C
    BC(40); i2cBoot();                                   // bus init + boot scan (USB already up)
#endif
#ifdef TEST_DELAY
    BC(50); fxGlueInit(); BC(60);                                // effect arenas in SDRAM, cycle counter on, before the audio starts
#endif
#ifdef TEST_MIC
    ogInit();                                    // load the saved output gain before the audio starts
#endif
#ifdef TEST_LIGHTS
    BC(70); lightsInit();                                // strip, audio, NeoTrellis
#endif
#ifdef TEST_L_AUDIO
    System::Delay(300);                          // let the SAI clocks and the codec settle before the ADC is calibrated
#endif
    BC(80); adcStart();                                  // ADC last, after every DMA user is configured
    vddaInit();

    BC(99); uint32_t seq = 0, next = System::GetNow(), nextHello = 0;
    for(;;) {
        uint32_t now = System::GetNow();
#ifdef TEST_DELAY
        { static uint32_t lastUs = 0; const uint32_t us = System::GetUs(); if(lastUs) { const uint32_t d = us - lastUs; if(d > g_lpmax) g_lpmax = d; if(d > g_lpmax_tr) g_lpmax_tr = d; if(d > 33000) g_lp33++; } lastUs = us; }   // longest main-loop pass since the last frame
#endif
#ifdef TEST_DELAY
        usbWatch(now);
#endif
        if(g_reboot) { System::Delay(30); NVIC_SystemReset(); }          // `reboot`: software reset (boot-to-audio test)
        if(g_dfu) {                              // buttonless reflash
            beginFrame(); puts_("{\"t\":\"dfu\"}\n"); sendFrame();
            System::Delay(100);
            dfuCheck();                          // never returns if the bootloader is present
            g_dfu = false;                       // BOOT_NONE+no ROM DFU path: ignore
        }
#ifdef TEST_JOY
        SEC_T0(ps0); joyButton(now);
#endif
#ifdef TEST_MIC
        { SEC_T0(og0); ogPoll(now); SEC_T1(7, og0); }
#endif
#ifdef TEST_LIGHTS
        { SEC_T0(lt0); lightsPoll(now); SEC_T1(1, lt0); }                         // strip frames at ~60 fps (DMA), NeoTrellis one I2C job per pass
#endif
#ifdef TEST_ENC
        encPoll(now);                            // every pass (tens of kHz when idle)
        SEC_T1(0, ps0);
#endif
        if(g_cmd_reset) { g_cmd_reset = false; resetStats(); }
        if(g_cal_pending) {                      // "cal <ch> b0..b8"  |  "calreset"
            char* p = g_calline; bool ok = false;
            if(!strncmp(p, "calinv", 6)) { char* e; long ch = strtol(p + 6, &e, 10); long v = strtol(e, &e, 10); ok = ch >= 0 && ch < NCH && isSlider((int)ch); if(ok) g_inv[ch] = v != 0; }
            else if(!strncmp(p, "calreset", 8)) { for(int c = 0; c < NCH; c++) defaultCal(c); ok = true; }
            else {
                p += 3; char* e; long ch = strtol(p, &e, 10); uint32_t v[9]; ok = e != p && ch >= 0 && ch < NCH && isSlider((int)ch);
                for(int k = 0; ok && k < 9; k++) { p = e; v[k] = (uint32_t)strtol(p, &e, 10); if(e == p || v[k] > 65535 || (k && v[k] < v[k - 1])) ok = false; }
                if(ok && v[8] <= v[0]) ok = false;
                if(ok) { for(int k = 0; k < 9; k++) g_bp[ch][k] = (uint16_t)v[k]; g_calc[ch] = true; }
            }
            (void)ok; g_cal_pending = false; g_cmd_hello = true;
        }
#ifdef TEST_I2C
        if(g_cmd_scan) { g_cmd_scan = false; i2cScan(); g_cmd_hello = true; }
        if(g_cmd_audio == 0) { g_cmd_audio = -1; hw.StopAudio(); } else if(g_cmd_audio == 1) { g_cmd_audio = -1;
#ifdef TEST_DELAY
            g_mute_n = 0;
#endif
            hw.StartAudio(audioCb); }
#endif
        if((int32_t)(now - next) < 0) continue;
        next += SAMPLE_MS; if((int32_t)(now - next) > 50) next = now + SAMPLE_MS;

        uint16_t v[kMaxCh], lin[kMaxCh];
        for(int i = 0; i < NCH; i++) {
            v[i] = hw.adc.Get(i); lin[i] = linearize(i, v[i]);
            if(v[i] < g_min[i]) g_min[i] = v[i];
            if(v[i] > g_max[i]) g_max[i] = v[i];
            g_cov[i] |= 1u << (v[i] >> 11);
            g_win[i][g_wpos] = v[i];
        }
        g_wpos = (g_wpos + 1) % WIN; if(g_wfill < WIN) g_wfill++;
#ifdef TEST_DELAY
        { SEC_T0(rc0); rvControls(v, lin, now); SEC_T1(3, rc0); }
#endif
#ifdef TEST_L_AUDIO
        { static bool again = false; if(!again && now > 2500) { again = true; adcStart(); } }       // second calibration once audio has been running a while
#endif
        { static uint32_t vt = 0; if(now - vt >= 2000) { vt = now; vddaRead(); } }
        {   // ADC watchdog: every channel identical and unchanged for 2 s means the ADC DMA stopped; restart it and count it
            static uint16_t pv[kMaxCh]; static int same = 0; bool eq = NCH >= 3, ch = false;
            for(int i = 0; i < NCH; i++) { if(v[i] != v[0]) eq = false; if(v[i] != pv[i]) ch = true; pv[i] = v[i]; }
            (void)ch; if(eq) { if(++same >= 200) { same = 0; g_adc_re++; adcStart(); g_cmd_hello = true; } } else same = 0;
        }
#ifdef TEST_HALL
        hallStep(now, v);
        for(int k = 0; k < 3; k++) if(g_hi[k] >= 0) lin[g_hi[k]] = (uint16_t)(g_hdepth[k] * 65.535f);
#endif
#ifdef TEST_JOY
        joyStep(now, v);
        if(g_jx >= 0 && g_jy >= 0) { lin[g_jx] = (uint16_t)(32768 + g_jn_out[0] * 32.767f); lin[g_jy] = (uint16_t)(32768 + g_jn_out[1] * 32.767f); }   // normalized -1..1 centred on 32768
#endif
        seq++;
        hw.SetLed((seq / 100) & 1);              // 1 Hz heartbeat = the firmware is running (does not encode the USB state)

        beginFrame();
        puts_("{\"t\":\"s\",\"n\":"); putu(seq); puts_(",\"ms\":"); putu(now); puts_(",\"v\":"); putarr16(v); puts_(",\"l\":"); putarr16(lin);
#ifdef TEST_ENC
        puts_(",\"e\":["); for(int i = 0; i < 2; i++) { if(i) put(','); putd(g_enc[i].cnt); put(','); put(g_enc[i].btn ? '1' : '0'); put(','); putu(g_enc[i].presses); put(','); putu(g_enc[i].bad); } put(']');
#endif
#ifdef TEST_DELAY
        { puts_(",\"fs\":["); for(int i = 0; i < 3; i++) { if(i) put(','); putu((uint32_t)(g_fs[i] * 1000.0f)); } put(']'); if((seq & 3) == 0) fxGlueTelemetry(); puts_(",\"fcl\":"); putu(g_fvalid | (g_flearn ? 8u : 0u)); puts_(",\"lp\":"); putu(g_lpmax); g_lpmax = 0; puts_(",\"lp33\":"); putu(g_lp33); puts_(",\"sgf\":["); putu(g_strip_n); put(','); putu(g_strip_sg); put(','); putu(g_strip_sgmax); put(']'); if((seq & 15) == 0) putTrellisTele(now);
#ifdef HX_MIN_DRONE
          moTelemetry();
#endif
          puts_(",\"op\":["); putu((uint32_t)(g_opk[0] * 1000.0f)); put(','); putu((uint32_t)(g_opk[1] * 1000.0f)); puts_("],\"chk\":"); putu((uint32_t)g_chk); g_opk[0] = 0; g_opk[1] = 0; }
#endif
#ifdef TEST_MIC
        puts_(",\"ad\":["); putu(DMA1_Stream2->NDTR); put(','); putu(ADC1->ISR); put(','); putu(DMA1_Stream2->M0AR); put(','); { uint16_t* bp = hw.adc.GetPtr(0); putu(bp[0]); put(','); putu(bp[NCH - 1]); } put(']');   // live ADC/DMA state every frame
#endif
#ifdef TEST_MIC
        { float pk = g_mic_peak; g_mic_peak = 0; float ms = g_mic_ms; int pk10 = pk > 1e-5f ? (int)(200.0f * log10f(pk)) : -1000, rms10 = ms > 1e-10f ? (int)(100.0f * log10f(ms)) : -1000;
          puts_(",\"m\":["); putd(pk10); put(','); putd(rms10); put(','); putu(g_mic_clips); put(']'); }
#endif
#ifdef TEST_LIGHTS
        { float e = g_env * (float)g_lsens * 1000.0f; if(e > 1000) e = 1000; puts_(",\"a\":["); putu((uint32_t)e); put(','); putu(g_onsets); puts_("],\"k\":"); putu(g_keys); }
#endif
#ifdef TEST_HALL
        puts_(",\"h\":["); for(int k = 0; k < 3; k++) { if(k) put(','); putd(g_hdepth[k]); } put(']');
#endif
#ifdef TEST_JOY
        puts_(",\"j\":["); putd(g_jn_out[0]); put(','); putd(g_jn_out[1]); put(','); put(g_jbtn ? '1' : '0'); put(','); putu(g_jpress); put(']');
#endif
        puts_("}\n");
#ifdef TEST_I2C
        { SEC_T0(ip0); uint32_t rescan = 0; i2cPoll(now, seq, &rescan); SEC_T1(4, ip0); if(rescan) { i2cScan(); g_cmd_hello = true; } }
#endif
        if(seq % STAT_EVERY == 0) {
            ensure(900);
            uint16_t pp[kMaxCh];
            for(int i = 0; i < NCH; i++) {
                uint16_t lo = 65535, hi = 0;
                for(int k = 0; k < g_wfill; k++) { uint16_t x = g_win[i][k]; if(x < lo) lo = x; if(x > hi) hi = x; }
                pp[i] = hi - lo;
                if(g_wfill == WIN && pp[i] < g_ppmin[i]) g_ppmin[i] = pp[i];
            }
            puts_("{\"t\":\"st\",\"min\":"); putarr16(g_min); puts_(",\"max\":"); putarr16(g_max);
            puts_(",\"pp\":"); putarr16(pp); puts_(",\"ppmin\":"); putarr16(g_ppmin);
            puts_(",\"cov\":"); putarr32(g_cov); puts_(",\"fill\":"); putu(g_wfill); puts_("}\n");
        }
#ifdef TEST_LIGHTS
        if(seq % 20 == 0) { ensure(480); lightsStreamStrip(now); ensure(160); lightsStreamKeys(now); }
#endif
        {   // hello dump: split over two main-loop passes (one blocking send each) so a slow host cannot stall the loop for 90 ms (3 sends x 30 ms); part 1 = core line, part 2 = extras
            static uint8_t hs = 0;
            if(hs == 0 && (g_cmd_hello || now >= nextHello)) { g_cmd_hello = false; nextHello = now + HELLO_MS; hs = 1; }
            if(hs == 1) { SEC_T0(hl0); sendWhole(); beginFrame(); putHello(now); g_hello_len[0] = (uint16_t)g_len; sendWhole(); beginFrame(); hs = 2; SEC_T1(5, hl0); }
            else if(hs == 2) { SEC_T0(hl1); putHello2(now); g_hello_len[1] = (uint16_t)g_len; sendWhole(); beginFrame(); hs = 0; SEC_T1(5, hl1); }
        }
        { SEC_T0(sf0); sendFrame(); SEC_T1(6, sf0); }
    }
}
