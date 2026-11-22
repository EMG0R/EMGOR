// sensors.cpp — IMU (MPU6050) + lidar (VL53L1X) + FSR → SharedState g.
// VERBATIM from PMOR_V2.cpp:
//   sensor bus init            L4102–4104
//   MPU6050 init               L4106–4115   (magnetometer probe L4118–4135 DROPPED: no mag on the 6050)
//   lidar register shims       L290–291, init L4143–4145
//   50 ms sensor read          L4919–4938   (gyro reads L4926–4928 DROPPED — unused in v3)
//   sensor watchdog            L4945–4984   (mag watchdog L4961–4968 DROPPED)
//   FSR 8-sample median        L5021–5045   (FsrScale from _fsr_scale.h)
#include "hw.h"
#include "hw_priv.h"
#include "bbus.h"
#include "../config.h"
#include "../pmor_core/shared_state.h"
#include "VL53L1X_mini.h"
#include "_fsr_scale.h"
#include "daisy_seed.h"
using namespace daisy;

// Half-bit delays (us). Measured 2026-11-11 (sbus.r8 = 4 bytes): us=5 554-712 us, 3 379, 2 288, 1 201, 0 114 -- the byte time is ~28 us of GPIO
// overhead + 9 bits x 2 x us, so every us shaved is ~100 us per transaction. These transactions are what sits in front of the keypad poll.
// Both were 5 (trellis) / 3 (sensors); the seesaw and MPU6050/VL53L1X are all rated 400 kHz, bit time at us=2 is ~7 us (~140 kHz).
#ifndef SBUS_HALF_US
#define SBUS_HALF_US 1
#endif
#ifndef TBUS_HALF_US
#define TBUS_HALF_US 3
#endif
static VL53L1X_Mini lidar;
static FsrScale     g_fsr;          // 330R + cubed + full-span ceiling

// ---- IMU
static bool imu_ok = false;
// ---- lidar
static bool     lidar_ok   = false;
static uint16_t dist       = 0;
static uint32_t lidar_seen = 0;
// ---- FSR
static uint16_t fsr = 0;

bool     imuOk()       { return imu_ok; }
bool     lidarOk()     { return lidar_ok; }
uint16_t lidarMm()     { return dist; }
uint8_t  lidarStatus() { return lidar.lastStatus(); }

// Pump the USB plane around every lidar register op. If the sensor is
// unresponsive, each bit-banged transaction burns ~100ms in bus timeouts;
// a full re-init is ~40 ops ≈ 4-5 SECONDS — measured as 4.8s MIDI-clock
// holes every watchdog retry (2026-08-16). With the pump, even that
// pathological path can't gap the clock or starve USB audio.
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg (uint16_t reg, uint8_t* d, uint8_t n) { pmor_usb_service(); sbus.r16a(0x29,reg,d,n); }

// ============================================================
// boot
// ============================================================
void sensorsBusInit() {
    sbus.init(hw.GetPin(PIN_SBUS_SCL), hw.GetPin(PIN_SBUS_SDA));   // D13 / D14
    sbus.us = SBUS_HALF_US;
    tbus.init(hw.GetPin(PIN_TBUS_SCL), hw.GetPin(PIN_TBUS_SDA));   // D11 / D12
    tbus.us = TBUS_HALF_US;
}

static inline bool imuIdOk(uint8_t id) {
    // v2: accept the MPU6050 (returns 0x68) as well as the MPU9250 family
    // (0x71/0x73/0x70) and common clones (0x69/0x98). Before this, a 6050 was
    // rejected outright -- imu_ok never went true and the watchdog below
    // reinit-stormed it once a second.
    return (id==0x71||id==0x73||id==0x70||id==0x68||id==0x69||id==0x98);
}

void imuInit() {
    // IMU at 0x68 -- MPU6050 (6-axis) on v2, MPU9250 also accepted
    sbus.w8(0x68,0x6B,0x80); System::Delay(100); // DEVICE_RESET
    sbus.w8(0x68,0x6B,0x01); System::Delay(50);  // wake + PLL clock
    uint8_t imu_id = sbus.r8(0x68,0x75);
    imu_ok = imuIdOk(imu_id);
    // v3: the IMU is an MPU6050 (6-axis). It has NO magnetometer, so the
    // AK8963 bypass probe at 0x0C (v2 L4118–4135) is gone entirely — it only
    // ever cost failed transactions and 5 ms waitScl timeouts on this board.
}

void lidarInit() {
    // VL53L1X lidar (0x29)
    g_t_sens = System::GetNow();
    lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(40);
    lidar_seen = System::GetNow();
}

// ============================================================
// main-loop tick
// ============================================================
volatile uint32_t g_sns_us[4];
volatile uint32_t g_sns_err[3];   // cumulative: IMU id probe failures, lidar reads with a non-valid status, lidar init/reinit failures
static inline uint32_t snsT() { return System::GetTick(); }
static inline void snsMark(int i, uint32_t t0) { uint32_t d = (uint32_t)(((uint64_t)(System::GetTick() - t0) * 1000000ull) / System::GetTickFreq()); if (d > g_sns_us[i]) g_sns_us[i] = d; }
// One bounded bus step per call (<= one IMU burst / lidar burst, ~1 ms). The 50 ms sensor tick used to run as ONE ~6 ms block (IMU 3 reads +
// lidar dataReady x2 + 17-byte burst + clear + DSS + status), during which a pad hit sat unread in the seesaw FIFO; the stages below spread the
// same work over consecutive passes and the keypad poll runs in between.
enum { ST_IDLE = 0, ST_LIDAR_RDY, ST_LIDAR_BURST, ST_LIDAR_BURST2, ST_LIDAR_CLEAR, ST_LIDAR_FIN, ST_LIDAR_CAL, ST_SMOOTH_WD };
void sensorsTick(uint32_t now) {
    static uint32_t next_sensor = 0;
    static uint8_t  stage = ST_IDLE;
    uint32_t _t0 = snsT();
    switch(stage) {
    case ST_IDLE:
        if(now >= next_sensor) {
            next_sensor = now + 50;
            if(imu_ok) {
                // accel X/Y/Z are contiguous (0x3B..0x40): one 6-byte burst = one transaction and one coherent sample
                uint8_t b[6]; sbus.rN(0x68, 0x3B, b, 6);
                float ax=(int16_t)((b[0]<<8)|b[1])/16384.f;
                float ay=(int16_t)((b[2]<<8)|b[3])/16384.f;
                float az=(int16_t)((b[4]<<8)|b[5])/16384.f;
                // (gyro 0x43/0x45/0x47 reads dropped in v3 — nothing consumes them)
#if ACCEL_SWAP_XY
                { float t = ax; ax = ay; ay = t; }   // logical X = left/right, logical Y = front/back
#endif
#if ACCEL_INVERT_X
                ax = -ax;
#endif
#if ACCEL_INVERT_Y
                ay = -ay;
#endif
                g.accel_x = ax; g.accel_y = ay; g.accel_z = az;
            }
            snsMark(0, _t0);
            stage = lidar_ok ? ST_LIDAR_RDY : ST_SMOOTH_WD;
        }
        break;
    case ST_LIDAR_RDY:   { bool rdy = lidar_ok && lidar.dataReady(); snsMark(1, _t0); stage = rdy ? ST_LIDAR_BURST : ST_SMOOTH_WD; } break;
    case ST_LIDAR_BURST: lidar.readBurst();  snsMark(1, _t0); stage = ST_LIDAR_BURST2; break;
    case ST_LIDAR_BURST2: lidar.readBurst2(); snsMark(1, _t0); stage = ST_LIDAR_CLEAR; break;
    case ST_LIDAR_CLEAR: lidar.readClear();  snsMark(1, _t0); stage = ST_LIDAR_FIN;   break;
    case ST_LIDAR_FIN:   dist = lidar.readFinish(); { uint8_t st = lidar.lastStatus(); if(st != 9 && st != 6 && st != 8) g_sns_err[1] = g_sns_err[1] + 1; } lidar_seen = now; snsMark(1, _t0); stage = ST_LIDAR_CAL; break;
    case ST_LIDAR_CAL:   if(lidar.calStep()) stage = ST_SMOOTH_WD; snsMark(1, _t0); break;
    case ST_SMOOTH_WD:
        {
            {
                // 0..1 with 1 = touching, 0 = far / no reading (dist 0 = invalid)
                float L = (dist == 0) ? 0.f : 1.f - (float)dist / 600.f;
                if(L > 1.f) L = 1.f;
                if(L < 0.f) L = 0.f;
                // the sensor updates ~25x/s; smooth here so nothing downstream steps.
                // (effects add their own per-sample glide on top — see SIREN)
                static float lsm = 0.f;
                lsm += (L - lsm) * 0.35f;
                g.lidar = lsm;
            }

            // ── sensor watchdog — PROBE-ONLY in normal operation (2 cheap
            // reads per second). Reinit fires only after a sensor has been
            // LOST >1s (two consecutive failed probes). Lidar reinit (a
            // long blocking call) only if it was up, silent >5s, and at
            // least 10s since the last attempt — never an init storm.
            {
                static uint32_t next_schk = 0;
                static uint8_t  imu_bad = 0;
                static uint32_t lidar_retry = 0;
                static uint32_t imu_backoff = 1000;   // a dead/unplugged IMU costs ~50 ms of bus timeouts per probe: back off 1 s -> 32 s so it can't stall pads every second
                uint32_t _tw = snsT();
                if(now >= next_schk) {
                    next_schk = now + 1000;
                    static uint32_t next_imu = 0;
                    if(now >= next_imu) {
                    uint8_t id = sbus.r8(0x68, 0x75);
                    if(!imuIdOk(id)) g_sns_err[0] = g_sns_err[0] + 1;
                    if(imuIdOk(id)) { imu_bad = 0; imu_ok = true; imu_backoff = 1000; }
                    else if(imu_bad >= 4) { imu_ok = false; if(imu_backoff < 32000) imu_backoff *= 2; }   // lost: probe only, no re-wake storm
                    else if(++imu_bad >= 2) {
                        sbus.w8(0x68, 0x6B, 0x01);
                        // (v2 also re-armed the mag bypass 0x37 / AK8963 0x0C here — dropped, no mag)
                        id = sbus.r8(0x68, 0x75);
                        imu_ok = imuIdOk(id);
                        if(imu_ok) imu_bad = 0;
                    }
                    next_imu = now + imu_backoff;
                    }
                    // ONE re-init attempt, ever. Against an unresponsive
                    // sensor a full init is seconds of I2C timeouts; the
                    // old every-10s retry put a multi-second hole in the
                    // main loop forever (13s MIDI-clock gap measured
                    // 2026-08-16). A sensor that failed twice isn't
                    // coming back without a power cycle anyway.
                    static uint8_t lidar_reinits = 0;
                    if(lidar_ok && now - lidar_seen > 5000 && now >= lidar_retry) {
                        lidar_retry = now + 10000;
                        if(lidar_reinits++ >= 1) { lidar_ok = false; }
                        else {
                            lidar_ok = lidar.init();
                            if(lidar_ok) { lidar.startContinuous(40); lidar_seen = now; }
                        }
                    }
                }
                snsMark(2, _tw);
            }
        }
        stage = ST_IDLE;
        break;
    }
    // FSR: 8-sample circular median, sampled at 10ms intervals.
    // Gating prevents all 8 samples from landing in the same glitch window,
    // and median-of-8 requires 4+ simultaneous spikes to corrupt the output.
    {
        static uint16_t fsrbuf[8]={}; static uint8_t fsridx=0;
        static uint32_t fsr_next=0;
        if(System::GetNow() >= fsr_next) {
            fsr_next = System::GetNow() + 10;
            fsrbuf[fsridx]=(uint16_t)(hw.adc.GetFloat(0)*4095.f);
            fsridx=(fsridx+1)%8;
        }
        // Median of 8: copy, insertion-sort, take middle two average
        uint16_t s[8]; for(int i=0;i<8;i++) s[i]=fsrbuf[i];
        for(int i=1;i<8;i++){uint16_t k=s[i];int j=i-1;while(j>=0&&s[j]>k){s[j+1]=s[j];j--;}s[j+1]=k;}
        fsr=(uint16_t)(((uint32_t)s[3]+s[4])>>1);
        // v2: the old fixed 400..2600 linear window was tuned for the v1
        // 10k pulldown. With the 330R pulldown + big panel FSR the raw
        // values sit compressed outside that range, so it snapped
        // low-to-high with nothing usable in between.
        //
        // FsrScale auto-calibrates to whatever this sensor actually
        // produces and applies the cubed curve (an FSR is ~1/force, so
        // linear always feels like a hair trigger). See _fsr_scale.h.
        g.fsr = g_fsr.Process(fsr);
    }
}
