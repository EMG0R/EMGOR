// hx_sensors.h -- ONE abstraction from the bring-up wiring to hx::HxControls. NEW 2026-11-15. Main loop only (never the audio callback).
//
// Wiring (bring-up fw 0.4, confirmed on the bench):
//   faders  A3-A5 (D18-D20, pins 25-27)  64-cycle sample, A-taper (15 % at mid) -> linearized by a 9-point LUT (inverse of n = ln .15/ln .5)
//   pots    A6/A7 (D21/D22, pins 28/29)  810.5-cycle sample (500 k source impedance), C-taper reverse log (85 % at mid, n = 0.234) -> 9-point LUT
//   FSR     A8 (D23, pin 30)             FsrScale (PMOR): 330 R pulldown, 12-bit in, noise gate, learned span, CUBED curve
//   I2C     bit-banged D11 SCL / D12 SDA, ~100 kHz (bring-up drivers copied verbatim to drivers/): MPU6050 0x68, VL53L1X LIDAR1 0x30 (XSHUT D7), LIDAR2 0x29 (XSHUT D9)
// Optional (compile flags, OFF by default because they are not in the bring-up wiring and floating ADC pins would fake valve presses):
//   -DHX_HALLS=1  three 49E Hall valve sensors on A0-A2 (D15-D17), rest learned at boot, span learned while playing (UNVERIFIED)
//   -DHX_JOY=1    joystick on A9/A10 (D24/D25), centre learned at boot
// Not wired in this skeleton: encoders, pad clicks, NeoTrellis (shares the bit-bang bus; driver in HYPER_CORE/src/drivers/trellis.cpp is not linked).
// Any control can be overridden from USB (`ctl <name> <value>`), which is how the sim UI / a test host plays the instrument.
#pragma once
#include <math.h>
#include <string.h>
#include "daisy_seed.h"
#include "fx_mapping.h"
#include "fsr_scale.h"

// ---- shims the verbatim bring-up i2c_sensors.h expects (its JSON helpers are unused here)
static volatile bool g_cmd_hello = false;
static void put(char) {}
static void puts_(const char*) {}
static void putu(uint32_t) {}
#include "drivers/i2c_sensors.h"      // verbatim copy: g_bus, i2cProbe/i2cRd/i2cWr, imuInit, g_L[2] (VL53L1X x2), lidRetry, i2cBoot (bus + XSHUT + bring-up)

#ifndef HX_HALLS
#define HX_HALLS 0
#endif
#ifndef HX_JOY
#define HX_JOY 0
#endif

enum HxCtl : uint8_t { K_V0, K_V1, K_V2, K_F0, K_F1, K_F2, K_P0, K_P1, K_FSR, K_JX, K_JY, K_TX, K_TY, K_SHAKE, K_LID, K_PADS, K_N };
static const char* const kCtlName[K_N] = { "v0", "v1", "v2", "f0", "f1", "f2", "p0", "p1", "fsr", "jx", "jy", "tx", "ty", "shake", "lid", "pads" };

struct HxSensors {
    static const int kMax = 11;
    enum { FD = 0, PT = 3, FS = 5, HL = 6 };
    int n = 0, joy0 = -1; uint8_t kind[kMax];          // 0 fader 1 pot 2 fsr 3 hall 4 joy
    uint16_t raw[kMax] = {}; uint16_t bp[kMax][9]; float sm[kMax] = {};
    float rest[kMax] = {}, span[kMax] = {};
    FsrScale fsr;
    // IMU / lidar
    bool imuOk = false; float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;     // g, deg/s
    uint16_t mm[2] = {0, 0}; uint8_t lst[2] = {0, 0}; uint32_t lidT[2] = {0, 0}; float lidar = 0.f;
    uint32_t seq = 0, nextImu = 0, imuFail = 0;
    hx::HxControls ctl;
    uint32_t ovrMask = 0; float ovrVal[K_N] = {};

    static void defaultBp(uint16_t* b, float nexp) { for (int k = 0; k < 9; k++) b[k] = (uint16_t)(powf(k / 8.0f, nexp) * 65535.0f + 0.5f); }
    static float linearize(const uint16_t* b, uint16_t r) {     // piecewise-linear inverse of the taper, 0..1 (same as bring-up)
        if (r <= b[0]) return 0.f; if (r >= b[8]) return 1.f;
        for (int k = 0; k < 8; k++) if (r < b[k + 1]) { uint32_t s = b[k + 1] - b[k]; float f = s ? (float)(r - b[k]) / s : 0.f; return (k + f) * 0.125f; }
        return 1.f;
    }
    bool setCal(int ch, const uint16_t* v) {                    // host calibration (9 raw breakpoints, ascending)
        if (ch < 0 || ch >= n || kind[ch] > 1) return false;
        for (int k = 1; k < 9; k++) if (v[k] < v[k - 1]) return false;
        if (v[8] <= v[0]) return false; memcpy(bp[ch], v, sizeof bp[ch]); return true;
    }
    void resetCal() { for (int i = 0; i < n; i++) { if (kind[i] == 0) defaultBp(bp[i], logf(0.15f) / logf(0.5f)); else if (kind[i] == 1) defaultBp(bp[i], 0.234f); } }
    int  ctlByName(const char* s) const { for (int i = 0; i < K_N; i++) if (!strcmp(s, kCtlName[i])) return i; return -1; }
    void setOverride(int k, float v) { ovrMask |= 1u << k; ovrVal[k] = v; }
    void releaseOverrides() { ovrMask = 0; }

    // call after USB is up (PMOR rule). Starts the ADC and brings up I2C + both lidars.
    void init(daisy::DaisySeed& hw) {
        using namespace daisy; using namespace daisy::seed;
        AdcChannelConfig cfg[kMax]; n = 0;
        auto add = [&](Pin p, uint8_t k, bool slow) { cfg[n].InitSingle(p, slow ? AdcChannelConfig::SPEED_810CYCLES_5 : AdcChannelConfig::SPEED_64CYCLES_5); kind[n++] = k; };
        add(A3, 0, false); add(A4, 0, false); add(A5, 0, false);          // faders
        add(A6, 1, true);  add(A7, 1, true);                              // pots: 810.5-cycle sample
        add(A8, 2, false);                                                // FSR
#if HX_HALLS
        add(A0, 3, false); add(A1, 3, false); add(A2, 3, false);
#endif
#if HX_JOY
        joy0 = n; add(A9, 4, false); add(A10, 4, false);
#endif
        hw.adc.Init(cfg, n, AdcHandle::OVS_32); hw.adc.Start();
        resetCal(); fsr.Reset();
        System::Delay(30);
        for (int i = 0; i < n; i++) { raw[i] = hw.adc.Get(i); rest[i] = raw[i]; span[i] = 4000.f; sm[i] = 0.f; }
        i2cBoot();                                                        // bus, XSHUT D7/D9, LIDAR1 -> 0x30, LIDAR2 0x29, scan
        imuOk = imuInit();
    }

    // ~200 Hz (every 5 ms). Same scheduling as bring-up: IMU on even frames (100 Hz), LIDAR1/LIDAR2 on frames 1/3 mod 4 (50 Hz each).
    void poll(daisy::DaisySeed& hw, uint32_t now) {
        using namespace daisy;
        seq++;
        for (int i = 0; i < n; i++) raw[i] = hw.adc.Get(i);
        for (int i = 0; i < n; i++) {
            if (kind[i] <= 1) { float a = kind[i] ? 0.15f : 0.3f; sm[i] += (linearize(bp[i], raw[i]) - sm[i]) * a; }
        }
        for (int k = 0; k < 3; k++) ctl.fader[k] = sm[FD + k];
        ctl.pot[0] = sm[PT]; ctl.pot[1] = sm[PT + 1];
        ctl.fsr = fsr.Process((uint16_t)(raw[FS] >> 4));                 // 16-bit oversampled -> 12-bit scale PMOR was tuned on; cubed
#if HX_HALLS
        for (int k = 0; k < 3; k++) {
            int i = HL + k; float d = fabsf((float)raw[i] - rest[i]);
            if (d > span[i]) span[i] = d; else span[i] -= 0.02f;           // slow shrink so a one-off spike does not pin the span
            if (span[i] < 3000.f) span[i] = 3000.f;
            float v = d / span[i]; ctl.valve[k] = v > 1.f ? 1.f : (v < 0.03f ? 0.f : v);
        }
#endif
#if HX_JOY
        { float jx = ((float)raw[joy0] - rest[joy0]) / 32768.f, jy = ((float)raw[joy0 + 1] - rest[joy0 + 1]) / 32768.f;
          ctl.joyX = fabsf(jx) < 0.08f ? 0.f : jx; ctl.joyY = fabsf(jy) < 0.08f ? 0.f : jy; }
#endif
        // ---- I2C (bit-banged, ms per transaction: main loop only)
        if (!imuOk && now >= nextImu) { nextImu = now + 2000; imuOk = imuInit(); return; }
        lidRetry(now);
        if (imuOk && (seq & 1) == 0) {
            uint8_t b[14];
            if (i2cRd(ADDR_IMU, 0x3B, b, 14)) {
                imuFail = 0; int16_t v[7]; for (int i = 0; i < 7; i++) v[i] = (int16_t)(b[2 * i] << 8 | b[2 * i + 1]);
                ax = v[0] / 16384.f; ay = v[1] / 16384.f; az = v[2] / 16384.f; gx = v[4] / 131.f; gy = v[5] / 131.f; gz = v[6] / 131.f;
                float roll = atan2f(ay, az) * 57.29578f, pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f;   // orientation vs mounting UNVERIFIED
                float tx = roll / 45.f, ty = pitch / 45.f;
                ctl.tiltX = tx < -1.f ? -1.f : (tx > 1.f ? 1.f : tx); ctl.tiltY = ty < -1.f ? -1.f : (ty > 1.f ? 1.f : ty);
                float gm = sqrtf(gx * gx + gy * gy + gz * gz), sh = (gm - 30.f) / 300.f; ctl.shake = sh < 0.f ? 0.f : (sh > 1.f ? 1.f : sh);
            } else if (++imuFail >= 5) { imuOk = false; nextImu = now + 2000; }
        } else if ((seq & 3) == 1 || (seq & 3) == 3) {
            int k = ((seq & 3) == 1) ? 0 : 1; Lid& L = g_L[k];
            if (L.st == DEV_OK) {
                lidSel(k);
                if (L.drv.dataReady()) { mm[k] = L.drv.read(false); lst[k] = L.drv.rawStatus(); lidT[k] = now; }
            }
        }
        // hand lidar = LIDAR1 (0x30): v1 range 56..333 mm, 1 = close. Invalid / stale (> 400 ms) = no hand.
        bool valid = g_L[0].st == DEV_OK && lst[0] == 9 && now - lidT[0] < 400 && mm[0] > 0;
        float t = valid ? (333.f - (float)mm[0]) / (333.f - 56.f) : 0.f; t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        lidar = t; ctl.lidar = t;
    }

    // final controls = sensors with host overrides applied
    hx::HxControls out() const {
        hx::HxControls c = ctl; uint32_t m = ovrMask; const float* o = ovrVal;
        if (m & (1u << K_V0)) c.valve[0] = o[K_V0]; if (m & (1u << K_V1)) c.valve[1] = o[K_V1]; if (m & (1u << K_V2)) c.valve[2] = o[K_V2];
        if (m & (1u << K_F0)) c.fader[0] = o[K_F0]; if (m & (1u << K_F1)) c.fader[1] = o[K_F1]; if (m & (1u << K_F2)) c.fader[2] = o[K_F2];
        if (m & (1u << K_P0)) c.pot[0] = o[K_P0];   if (m & (1u << K_P1)) c.pot[1] = o[K_P1];
        if (m & (1u << K_FSR)) c.fsr = o[K_FSR];    if (m & (1u << K_JX)) c.joyX = o[K_JX]; if (m & (1u << K_JY)) c.joyY = o[K_JY];
        if (m & (1u << K_TX)) c.tiltX = o[K_TX];    if (m & (1u << K_TY)) c.tiltY = o[K_TY]; if (m & (1u << K_SHAKE)) c.shake = o[K_SHAKE];
        if (m & (1u << K_LID)) c.lidar = o[K_LID];  if (m & (1u << K_PADS)) c.pads = (uint16_t)o[K_PADS];
        return c;
    }
};
